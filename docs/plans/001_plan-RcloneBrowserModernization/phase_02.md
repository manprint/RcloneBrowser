# Phase 1 — JSON transfer engine (fixes #141, #142, #160; supersedes PRs #246, #225, #212 and totza2010's progress regexes)

Intent: transfer jobs are monitored from rclone JSON logs. The app owns the logging flags and the environment. Stats, per-file progress, errors and the output panel work for any filename and any user `RCLONE_*` environment. `--log-file` produces a rendered log without breaking the GUI.
Prerequisites: P0 (core library, test harness, `scripts/dev.sh`).
Phase closure: P1; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6).

## Local design context
Plan revision: 1.
- R1/R2 (rclone 1.75.2, local probes 2026-10-21). With `--use-json-log` every line is one JSON object `{time, level, msg, source[, object, objectType][, stats]}`.
  - The stats object has: bytes, checks, deletedDirs, deletes, elapsedTime, errors, eta (null until known), fatalError, listed, renames, retryError, serverSideCopies, serverSideCopyBytes, serverSideMoveBytes, serverSideMoves, speed, totalBytes, totalChecks, totalTransfers, transferTime, transfers, transferring[] (absent on the final line), and lastError (only after an error).
  - Each item of transferring[] has: bytes, dstFs, eta, group, name, percentage, size, speed, speedAvg, srcFs.
  - Each per-file success is `info "Copied (new)"` with `object` set to the file path.
  - A bad remote gives a single `critical` line and exit 1.
  - With `RCLONE_VERBOSE=1` in the env, the app's `-v` becomes DEBUG, and any `--log-level` aborts with "Can't set -v and --log-level".
- D1: the app owns the logging flags (`PrepareTransferArgs`). It never emits `--log-level`, and it always passes `--color NEVER`.
- R14 (local probe, bisync, 1.75.2): bisync writes ANSI SGR color codes inside JSON `msg` values (`\u001b[31m…`). `--color NEVER` removes them.
- D2: the env factory `RcloneEnvironment(mode)` replaces `UseRclonePassword`.
- D3: the parser is pure and never throws (I-3).
- D4: TransferMonitor is pure; JobWidget only renders.
- I-1: every parsed rclone process gets its env from `RcloneEnvironment(Parsed)`; transfers also get their args from `PrepareTransferArgs`.
- I-2: variables removed in Parsed mode: RCLONE_VERBOSE, RCLONE_LOG_LEVEL, RCLONE_LOG_FILE, RCLONE_LOG_FILE_COMPRESS, RCLONE_LOG_FILE_MAX_AGE, RCLONE_LOG_FILE_MAX_BACKUPS, RCLONE_LOG_FILE_MAX_SIZE, RCLONE_LOG_FORMAT, RCLONE_LOG_SYSTEMD, RCLONE_USE_JSON_LOG, RCLONE_STATS, RCLONE_STATS_LOG_LEVEL, RCLONE_STATS_ONE_LINE, RCLONE_STATS_ONE_LINE_DATE, RCLONE_STATS_ONE_LINE_DATE_FORMAT, RCLONE_STATS_FILE_NAME_LENGTH, RCLONE_PROGRESS, RCLONE_PROGRESS_TERMINAL_TITLE, RCLONE_QUIET, RCLONE_SYSLOG, RCLONE_SYSLOG_FACILITY, RCLONE_COLOR.
- I-3: arbitrary bytes produce a Text event, never a crash.
- I-4: displayed progress is clamped to 0–100; unknown size gives an indeterminate bar.
- File convention: flat `src/` files in snake_case, added to `SOURCE` (.cpp) and `OTHER` (.h, or `MOC` when the header has `Q_OBJECT`) in `src/CMakeLists.txt`. Tests go in `tests/tst_<name>.cpp`, registered with `rb_add_test(<name> unit|e2e)`; e2e registration sits inside `if(RB_TEST_RCLONE)`.

### Data model (NEW `src/rclone_log.h`)
```cpp
#pragma once
#include <QtCore>
#include <optional>

struct RcloneTransferItem {
  QString name;                  // path relative to srcFs; may contain '/', ':', brackets, quotes
  qint64 bytes = 0;
  qint64 size = -1;              // -1 = unknown
  int percentage = 0;            // as reported by rclone (may exceed 100)
  double speed = 0, speedAvg = 0;
  std::optional<qint64> eta;     // seconds; nullopt when null or missing
};
struct RcloneStats {
  qint64 bytes = 0, totalBytes = 0, checks = 0, totalChecks = 0;
  qint64 transfers = 0, totalTransfers = 0, errors = 0, deletes = 0, renames = 0, listed = 0;
  qint64 serverSideCopies = 0, serverSideMoves = 0;
  double elapsedTime = 0, transferTime = 0, speed = 0;
  std::optional<qint64> eta;
  bool fatalError = false, retryError = false;
  QString lastError;
  bool hasTransferring = false;  // the key was present
  QVector<RcloneTransferItem> transferring;
};
enum class RcloneLogLevel { Debug, Info, Notice, Error, Critical, Unknown };
struct RcloneLogEvent {
  enum class Kind { Log, Stats, Text };
  Kind kind = Kind::Text;
  QDateTime time;                // invalid for Text
  RcloneLogLevel level = RcloneLogLevel::Unknown;
  QString msg;                   // Log/Stats: leading/trailing '\n' stripped; Text: raw line without trailing "\r\n"/"\n"
  QString object, source;
  RcloneStats stats;             // meaningful for Kind::Stats
};
Q_DECLARE_METATYPE(RcloneStats)
Q_DECLARE_METATYPE(RcloneLogEvent)

RcloneLogEvent parseRcloneLogLine(QByteArrayView line);
RcloneStats parseRcloneStats(const QJsonObject &stats);      // reused by rc core/stats (phase 5)
RcloneLogLevel parseRcloneLevel(QStringView level);
QString rcloneLevelName(RcloneLogLevel level);                // "DEBUG","INFO","NOTICE","ERROR","CRITICAL",""
QDateTime parseRfc3339(const QString &s);                     // invalid on failure
QString renderRcloneLogEvent(const RcloneLogEvent &e,
                             const QTimeZone &tz = QTimeZone::systemTimeZone());
```
Parsing rules:
1. Strip trailing `\r`/`\n`. If the first non-space byte is not `{`, the result is Text.
2. `QJsonDocument::fromJson`. Unless it is an object with string `level` and string `msg`, the result is Text.
3. If `stats` is an object, the result is Stats; otherwise Log.
4. Levels, case-insensitive: `debug`→Debug, `info`→Info, `notice`|`warning`→Notice, `error`→Error, `critical`|`fatal`|`alert`|`emergency`→Critical, anything else→Unknown.
5. Integer fields are read with `qRound64(v.toDouble())`. Missing fields keep their defaults. `eta` that is null or missing is nullopt.
6. `parseRfc3339`: keep at most 3 fraction digits (pad to 3), then `QDateTime::fromString(s, Qt::ISODateWithMs)`. So `2026-10-21T17:19:56.651053406+02:00` parses to msec 651 with offset +7200.
7. Strip ANSI SGR sequences (`\x1B\[[0-9;]*m`) from `msg` and `object`. bisync colors its messages unless `--color NEVER` is given (R14); the app always passes it, and this rule is the defensive second layer.
8. Render (rclone text style):
   - Log: `yyyy/MM/dd HH:mm:ss` (time converted to `tz`), a space, the level name left-justified to width 6, `": "`, then `object + ": "` if the object is non-empty, then msg.
   - Stats: the same prefix with level NOTICE and no object, then msg (multi-line kept).
   - Text: the raw msg.
   - Example: info, `object "a b.txt"`, msg "Copied (new)" at 17:19:56+02:00 rendered in UTC gives `2026/10/21 15:19:56 INFO  : a b.txt: Copied (new)`.

### Formatting (NEW `src/rclone_format.h/.cpp`)
`QString formatBytes(qint64)`:
- a negative value gives "-";
- below 1024 gives "N B";
- otherwise binary units KiB…EiB with exactly 3 decimals, rounded half up (QString::number(v,'f',3)).
- Examples: 0→"0 B", 1023→"1023 B", 1024→"1.000 KiB", 1536→"1.500 KiB", 4500003→"4.292 MiB", 1073741824→"1.000 GiB".

`QString formatRate(double bps)` is `formatBytes(qRound64(bps)) + "/s"`.

`QString formatDuration(std::optional<qint64> s)`:
- nullopt or a negative value gives "-";
- otherwise d/h/m/s parts with leading zero-parts omitted.
- Examples: 0→"0s", 65→"1m5s", 3600→"1h0m0s", 3723→"1h2m3s", 90061→"1d1h1m1s".

`QString formatElapsed(double s)` is `formatDuration(qint64(s))` (truncated).

`QString formatPercent(qint64 part, qint64 total)`: total ≤ 0 gives "-"; otherwise `qBound(0, part*100/total, 100)` followed by "%".

### Process contract (NEW `src/rclone_process.h/.cpp`)
```cpp
enum class RcloneEnvMode { Parsed, Interactive };
const QStringList &RcloneLoggingEnvVars();                          // exactly I-2
QProcessEnvironment RcloneEnvironment(RcloneEnvMode mode = RcloneEnvMode::Parsed,
    const QProcessEnvironment &base = QProcessEnvironment::systemEnvironment());
void ApplyRcloneEnvironment(QProcess *p, RcloneEnvMode mode = RcloneEnvMode::Parsed);
struct TransferArgs { QStringList args; QString logFile; bool debug = false; QStringList ignored; };
TransferArgs PrepareTransferArgs(const QStringList &args,
    const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment());
```
- `RcloneEnvironment` copies `base`. In Parsed mode it removes every I-2 variable. In both modes, a non-empty `GetRclonePassword()` (NEW in utils.h/.cpp: returns `gRclonePassword`) is inserted as `RCLONE_CONFIG_PASS`.
- `PrepareTransferArgs` scans the tokens:
  - `--` stops the scan. The rest is copied verbatim, and the app flags are inserted before the `--`.
  - Boolean flags are dropped and appended to `ignored`, including the `=value` form: `-q --quiet -P --progress --progress-terminal-title --use-json-log --stats-one-line --stats-one-line-date --log-systemd --syslog --log-file-compress`.
  - Verbosity tokens are dropped and listed: `-v` (+1), `-vv` (+2), `-vvv` (+3), `--verbose` (+1), `--verbose=N` (verbosity becomes max(current, N)).
  - Value flags consume `--x value` or `--x=value`, and are dropped and listed (as `"--x value"` or as the original `--x=value` token): `--log-level --log-file --log-format --stats --stats-log-level --stats-file-name-length --stats-one-line-date-format --syslog-facility --log-file-max-age --log-file-max-backups --log-file-max-size --color`.
  - A value flag in last position, with no value, is dropped and listed alone.
  - `--log-level`'s value (uppercased) and `--log-file`'s value are recorded.
  - If no `--log-file` was given, `logFile` becomes `env.value("RCLONE_LOG_FILE")`. If no `--log-level` was given, the level comes from `RCLONE_LOG_LEVEL`.
  - `debug = verbosity >= 2 || level == "DEBUG"`.
  - Appended: `--use-json-log`, then `-vv` if debug else `-v`, then `--stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER`.
  - Combined short flags such as `-vP` are passed through unchanged (documented limitation).

## Sub-phases

### 1.1 Log parser, formatting helpers, fixture recorder
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P1.
- **Files:**
  - NEW `src/rclone_log.h/.cpp`, `src/rclone_format.h/.cpp`, registered in `SOURCE`/`OTHER`.
  - NEW `scripts/record-fixtures.sh`.
  - NEW `tests/fixtures/rclone-1.75/` (generated) and `tests/fixtures/synthetic/{panic.txt,stats_variants.jsonl,README.md}`.
  - NEW `tests/tst_rclone_log.cpp`, `tests/tst_rclone_format.cpp`, registered as `unit`.
- **Change:**
  - Preconditions: P0 DONE.
  - Contract: the parser and formatting rules above.
  - Fixture recorder: `scripts/record-fixtures.sh <rclone> <outdir>`, bash, `set -euo pipefail`, run through `scripts/dev.sh fixtures` (container, `--network none`, non-root):
    - Exit 5 if `id -u` is 0. Unset every `RCLONE_*` variable. `W=$(mktemp -d)`.
    - Tree under `$W/src`:
      - `file 1.bin`, `file 2.bin`, `file 3.bin` (1500000 bytes each, `head -c 1500000 /dev/urandom`);
      - `dir [x] (y)/ñ"quote'.txt` containing `hi`;
      - `12345 /trail.txt` (directory with a trailing space) containing `t`;
      - `a:b.txt` containing `c`.
    - `$W/empty.conf` is empty. `$W/remotes.conf` contains `[mem]\ntype = memory\n\n[loc]\ntype = local\ndescription = test local\n`.
    - `C="--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER --config $W/empty.conf"` (the exact flags `PrepareTransferArgs` appends).
    - Scenarios. Each stores the combined stdout+stderr as `<name>.jsonl`, except where stated, and the exit code in `<name>.exit`:
      - `copy_ok`: `copy src dst $C --bwlimit 1M --transfers 2`.
      - `sync_checks`: `sync src dst $C --checksum`, run after copy_ok.
      - `copy_errors`: `mkdir ro && chmod 555 ro`, then `copy src ro/x $C --retries 1 --low-level-retries 1`, then `chmod 755 ro`.
      - `config_fail`: `copy nope: dst $C`.
      - `lsjson_tree`: stdout only, as `lsjson_tree.json`: `lsjson src --config $W/empty.conf`.
      - `lsjson_recursive`: stdout only: `lsjson -R --files-only src`.
      - `lsjson_missing`: `.stdout`, `.stderr` and `.exit` kept separately: `lsjson src/nope`.
      - `listremotes`: stdout: `listremotes --long --json --config $W/remotes.conf`.
      - `rcd_start`: stderr. Run `RCLONE_RC_USER=u RCLONE_RC_PASS=p rclone rcd --rc-addr 127.0.0.1:0 --use-json-log --config $W/empty.conf` in the background for 1.5 s, then kill it.
    - Afterwards, replace `$W` with `/FIXTURE` in every output. Write `<outdir>/README.md` with the rclone version line, the date and the scenario commands.
  - Synthetic fixtures, hand-written with a README entry each:
    - `panic.txt`: 4 lines starting `panic: runtime error: invalid memory address or nil pointer dereference`, `[signal SIGSEGV: segmentation violation code=0x1 addr=0x0 pc=0x0]`, an empty line, and `goroutine 1 [running]:`.
    - `stats_variants.jsonl`, one case per line in this order:
      - (a) a stats line whose transferring item has `size:-1, percentage:0`;
      - (b) an item with `bytes:150, size:100, percentage:150`;
      - (c) `{"level":"notice","msg":"m","stats":{"bytes":5}}`;
      - (d) `[1,2]`;
      - (e) `{"foo":1}`;
      - (f) `{"level":"warning","msg":"w"}`;
      - (g) `{"level":"error","msg":"e","object":"o"}`;
      - (h) a truncated line `{"level":"info","msg":"x`;
      - (i) `{"level":"info","msg":"\u001b[32mBisync successful\u001b[0m","object":"\u001b[36mp\u001b[0m"}`.
  - Steps:
    1. S1 — Implement rclone_log and rclone_format. Register them in CMake. Expected: they build.
    2. S2 — Write record-fixtures.sh and run `scripts/dev.sh fixtures`. Expected: all files exist; `copy_ok.exit` is 0; `copy_errors.exit` and `config_fail.exit` are 1; `lsjson_missing.exit` is 3; no file contains the temp path.
    3. S3 — Write the synthetic fixtures and the tests. Run `scripts/dev.sh test`. Expected: the new tests pass.
  - Recovery boundary: after S2 (generated artifacts are committed with the unit).
  - Failure handling: if a fixture contradicts R1 (for example a renamed field), stop and escalate to agent-1:opus. Do not adapt the tests to the new shape.
- **Unit tests:**
  - `tests/tst_rclone_log.cpp`, `QTEST_GUILESS_MAIN`. Read the fixtures through `rbFixture()` and split them on `\n`.
    - T-LOG-COPY (copy_ok):
      - there are ≥ 2 Stats events, and the first Stats has `eta == std::nullopt`;
      - at least one Stats has `hasTransferring && !transferring.isEmpty()`, with every name in the 6 expected relative paths;
      - the last Stats has `!hasTransferring`, `transfers == totalTransfers == 6` and `bytes == totalBytes`;
      - the Log events with msg "Copied (new)" have exactly the object set {`file 1.bin`, `file 2.bin`, `file 3.bin`, `dir [x] (y)/ñ"quote'.txt`, `12345 /trail.txt`, `a:b.txt`};
      - there are no Debug-level events.
    - T-LOG-CHECKS (sync_checks): the last Stats has `checks == totalChecks == 6` and `transfers == 0`.
    - T-LOG-ERRORS (copy_errors): ≥ 6 Error events with a non-empty object; the last Stats has `errors ≥ 1` and a non-empty `lastError` that contains "permission denied".
    - T-LOG-CONFIGFAIL: exactly one non-empty event; it is a Critical Log whose msg contains `didn't find section in config file`; there are no Stats.
    - T-LOG-GARBAGE (I-3):
      - each panic.txt line gives Text with msg equal to the raw line;
      - a 512-byte buffer of bytes 0..255 gives Text;
      - variants (d), (e) and (h) give Text;
      - (f) gives a Notice Log;
      - (g) gives an Error Log with object "o".
    - T-LOG-VARIANTS:
      - (a) is Stats with item size -1 and eta nullopt;
      - (b) has item bytes 150 and percentage 150, unclamped at parse level;
      - (c) has bytes 5 and every other field at its default;
      - (i) is an Info Log with msg `Bisync successful` and object `p`.
    - T-LOG-TIME: `parseRfc3339("2026-10-21T17:19:56.651053406+02:00")` has msec 651 and `offsetFromUtc` 7200. `"2026-10-21T15:22:53.56219+00:00"` has msec 562. `"garbage"` is invalid.
    - T-LOG-RENDER: with tz UTC:
      - the Log example renders exactly as in the contract;
      - the Stats render starts with `2026/10/21 15:19:56 NOTICE: `;
      - a Text event renders as its raw msg.
  - `tests/tst_rclone_format.cpp`: T-FMT-BYTES, T-FMT-DURATION and T-FMT-PERCENT, as data-driven tables with every example above, plus `formatPercent(5, 0) == "-"` and `formatPercent(150, 100) == "100%"`.
  - Gate G-U1.1: `scripts/dev.sh test qt64 -- -R 'tst_rclone_(log|format)'` reports 2/2 passed. The QtTest output lists every T-LOG-*/T-FMT-* function name.
- **e2e tests:** N/A — the parser is pure. Real rclone behavior is captured by the recorded fixtures, which are re-recordable.
- **Done:** files, fixtures and tests exist; G-U1.1 passes; unit closed; completion commit.

### 1.2 Environment factory and transfer argument ownership
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff, focusing on I-1 across every call site and the JobOptions change.
- **Files:**
  - NEW `src/rclone_process.h/.cpp`, registered in CMake.
  - WRITE `src/utils.h/.cpp`: add `GetRclonePassword()` and delete `UseRclonePassword`.
  - WRITE every `UseRclonePassword(` call site; list them first with `tokensave_callers` / literal search `UseRclonePassword(`. Known files: `item_model.cpp`, `main_window.cpp`, `remote_widget.cpp`, `delete_progress_dialog.cpp`, plus any others the search finds.
  - WRITE `src/job_options.cpp`, `JobOptions::getOptions`: delete the block from `// always verbose` through `<< "0";` (`--verbose`, `--stats 1s`, `--stats-file-name-length 0`). Keep `--dry-run`.
  - WRITE `src/main_window.cpp`, `MainWindow::addTransfer`.
  - NEW `tests/tst_rclone_process.cpp` (unit), `tests/tst_e2e_env.cpp` (e2e).
- **Change:**
  - Preconditions: 1.1 DONE.
  - Contract: the Process contract above.
  - Scope: transfer output parsing stays as today until 1.4. JobWidget still receives the args for its "copy command" display.
  - Steps:
    1. S1 — Implement rclone_process plus `GetRclonePassword`. Expected: it builds.
    2. S2 — Replace every `UseRclonePassword(p)` with `ApplyRcloneEnvironment(p)`. The exception is `MainWindow::rcloneConfig`, which uses `ApplyRcloneEnvironment(p, RcloneEnvMode::Interactive)`. Delete `UseRclonePassword`. Expected: `git grep -n UseRclonePassword` is empty; it builds.
    3. S3 — In `addTransfer`:
       - compute `TransferArgs ta = PrepareTransferArgs(args);` before the JobWidget is constructed;
       - pass `ta.args` instead of `args` to the JobWidget constructor;
       - start with `transfer->start(GetRclone(), ta.args + GetRcloneConf(), QIODevice::ReadOnly);`;
       - store `ta` for 1.4 in a local passed to the widget later. In this unit, only add a `// 1.4 wires ta.logFile / ta.ignored` comment.
       Edit `getOptions` per Files. Expected: it builds. Transfers still show stats, because the existing regexes do not match JSON; the GUI stats are temporarily empty between 1.2 and 1.4, which is accepted and noted in STATE.md §6.
    4. S4 — List every `JobOptions::getOptions` caller. For each one that does not end in `addTransfer`, record the consumer. If such a consumer parses rclone output, escalate to agent-1:opus before closing. Expected: the caller list is recorded in the evidence.
    5. S5 — Tests, then `scripts/dev.sh test e2e`.
  - Failure handling: a call site that launches a non-rclone program (`runScript`) is not changed. Any `QProcessEnvironment::systemEnvironment()` left in `src/` outside `rclone_process.cpp` is either justified in the evidence (non-rclone process) or fixed.
- **Unit tests:** `tests/tst_rclone_process.cpp` (GUILESS):
  - T-ARGS-STRIP: input `copy a b -q --log-file /x.log --progress --stats=5s --stats-one-line --transfers 4 -vv` gives:
    - args exactly `copy a b --transfers 4 --use-json-log -vv --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER`;
    - logFile `/x.log`, debug true;
    - ignored exactly `[-q, --log-file /x.log, --progress, --stats=5s, --stats-one-line, -vv]`.
  - T-ARGS-DEFAULT: `copy a b` gets `-v` appended, debug false, ignored empty.
  - T-ARGS-COLOR: `copy a b --color ALWAYS` gives ignored `[--color ALWAYS]`, and the args contain exactly one `--color`, followed by `NEVER`.
  - T-ARGS-LOGLEVEL: `sync a b --log-level=DEBUG` gives debug true and ignored `[--log-level=DEBUG]`.
  - T-ARGS-DASHDASH: `copy a b -- -q` gives `copy a b --use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER -- -q`.
  - T-ARGS-MISSINGVALUE: `copy a b --log-file` gives args without it, logFile "", ignored `[--log-file]`.
  - T-ARGS-ENVLOGFILE: env `RCLONE_LOG_FILE=/e.log` gives logFile `/e.log`.
  - T-ENV-STRIP: the base has every I-2 variable plus `RCLONE_CONFIG=/c`, `RCLONE_PASSWORD_COMMAND=x` and `PATH`. Parsed mode contains no I-2 key and keeps the other three. Interactive mode keeps every key.
  - T-ENV-PASS: `SetRclonePassword("pw")` gives `RCLONE_CONFIG_PASS=pw` in both modes. After `SetRclonePassword("")` it is absent.
  - Gate G-U1.2: `scripts/dev.sh test qt64 -- -R tst_rclone_process` passes, with 9 test functions listed.
- **e2e tests:** `tests/tst_e2e_env.cpp`, T-E2E-ENV-POLLUTED:
  - base env plus `RCLONE_VERBOSE=1`, `RCLONE_STATS=5s`, `RCLONE_STATS_ONE_LINE=true`;
  - `RcloneEnvironment(Parsed, base)` and `PrepareTransferArgs({"copy", src, dst}).args`, with a 1-file temp tree;
  - expected: exit 0, ≥ 1 line parses to Stats, 0 Debug events, and no line contains `Can't set -v and --log-level`.
  - Gate G-U1.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_env` passes.
- **Done:** I-1 grep evidence recorded; review recorded; G-U1.2 and G-U1.2e pass; unit closed; completion commit.

### 1.3 TransferMonitor (pure model)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P1.
- **Files:** NEW `src/transfer_monitor.h` (Q_OBJECT → `MOC` list) and `src/transfer_monitor.cpp`; NEW `tests/tst_transfer_monitor.cpp` (unit).
- **Change:**
  - Preconditions: 1.1 DONE.
  - Contract:
    ```cpp
    class TransferMonitor : public QObject {
      Q_OBJECT
    public:
      enum class Result { Running, Finished, Error, Stopped };
      Q_ENUM(Result)
      explicit TransferMonitor(QObject *parent = nullptr);
      void consumeLine(QByteArrayView line);          // parse; empty lines ignored
      void consumeEvent(const RcloneLogEvent &e);
      void finish(int exitCode, QProcess::ExitStatus status, bool stoppedByUser);
      bool hasStats() const; const RcloneStats &stats() const;
      QString lastStatsText() const;                  // msg of the last Stats event
      qint64 errorCount() const;  QString lastError() const;  QStringList recentErrors() const;
      Result result() const;
    signals:
      void statsUpdated(const RcloneStats &stats);
      void activeTransfersChanged(const QVector<RcloneTransferItem> &items);
      void logEvent(const RcloneLogEvent &e);         // Log and Text only
      void errorsChanged(qint64 count, const QString &last);
      void finished(TransferMonitor::Result result);
    };
    int clampedPercent(const RcloneTransferItem &item);  // free function
    ```
  - Semantics:
    - A Stats event stores the stats and emits `statsUpdated`. It then emits `activeTransfersChanged(stats.hasTransferring ? stats.transferring : {})`. If `stats.errors` or `lastError` changed, it emits `errorsChanged`.
    - A Log event at Error or Critical level appends the error text (`object.isEmpty() ? msg : object + ": " + msg`) to a list capped at the most recent 20, and emits `errorsChanged`. Every Log and Text event emits `logEvent`.
    - `errorCount()` is `max(stats.errors, number of Error/Critical log events)`.
    - `lastError()` is `stats.lastError` if non-empty, otherwise the last error text, otherwise "".
    - `finish()`: stoppedByUser gives Stopped. Otherwise `exitCode == 0 && status == NormalExit && !stats.fatalError` gives Finished, and anything else gives Error. It emits `finished` once; later calls are ignored.
    - `clampedPercent`: `size <= 0` gives -1; otherwise `qBound(0, int(bytes * 100 / size), 100)`.
  - Steps:
    1. S1 — Implement and register. Expected: it builds.
    2. S2 — Tests, then `scripts/dev.sh test`.
- **Unit tests:** `tests/tst_transfer_monitor.cpp` (GUILESS; `qRegisterMetaType<RcloneStats>()` etc.):
  - T-MON-COPY: feed copy_ok. The `statsUpdated` count equals the Stats line count. The last `activeTransfersChanged` argument is empty. `finish(0, NormalExit, false)` gives Finished and `errorCount()` 0.
  - T-MON-ERR: feed copy_errors. `errorCount() ≥ 6`. `lastError()` equals the final stats lastError. `recentErrors().size()` is ≤ 20 and `recentErrors().first()` contains `Failed to copy`. `finish(1, NormalExit, false)` gives Error.
  - T-MON-CRITICAL: feed config_fail. No `statsUpdated`. `errorCount()` is 1. `lastError()` contains `didn't find section`. Error.
  - T-MON-CLAMP (I-4): {150,100}→100, {0,-1}→-1, {50,0}→-1, {5,10}→50.
  - T-MON-STOP: `finish(9, CrashExit, true)` gives Stopped. A second `finish(0, …)` changes nothing (spy count 1).
  - T-MON-TEXT: panic.txt gives 3 `logEvent` of Kind Text (the empty line is ignored).
  - Gate G-U1.3: `scripts/dev.sh test qt64 -- -R tst_transfer_monitor` passes, with 6 functions.
- **e2e tests:** N/A — pure model; covered end-to-end in 1.4.
- **Done:** G-U1.3 passes; unit closed; completion commit.

### 1.4 JobWidget rendering, log file, cancel; addTransfer wiring
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff, focusing on the JobWidget lifecycle (process deletion, finish ordering, stopped status) and on the status strings used by `sortJobs` and the notifications.
- **Files:**
  - WRITE `src/job_widget.h/.cpp`: remove `NormalizeRcloneStatsLine`, all `rx*` regexes, `mLines` and the blank-line handling. Add `TransferMonitor *mMonitor` and `QFile *mLogFile = nullptr`.
  - Add public members `TransferMonitor *monitor() const`, `void setLogFile(const QString &path)` and `void noteIgnoredArgs(const QStringList &ignored)`.
  - WRITE `src/main_window.cpp` (`addTransfer`): call `widget->noteIgnoredArgs(ta.ignored)`; `if (!ta.logFile.isEmpty()) widget->setLogFile(ta.logFile);` before `start`.
  - NEW `tests/tst_jobwidget_render.cpp` (unit, `QTEST_MAIN`) and `tests/tst_e2e_jobwidget.cpp` (e2e, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 1.2 and 1.3 DONE.
  - Contract (render; UI names from `job_widget.ui`, all QLineEdit except the QLabel `progress_info`, the QPlainTextEdit `output` and the QFormLayout `progress`):
    - `statsUpdated(s)` sets:
      - `size`: `formatBytes(s.bytes) + ", " + formatPercent(s.bytes, s.totalBytes)`;
      - `totalsize`: `formatBytes(s.totalBytes)`;
      - `bandwidth`: `formatRate(s.speed)`;
      - `eta`: `formatDuration(s.eta)`;
      - `transferred`: `"%1 / %2, %3"` with transfers, totalTransfers and `formatPercent(transfers, totalTransfers)`;
      - `checks`: `"%1 / %2, %3"` with checks, totalChecks and percent, plus `", Listed %4"` when `listed > 0`;
      - `elapsed`: `formatElapsed(s.elapsedTime)`;
      - `progress_info`: `"(" + formatPercent(s.bytes, s.totalBytes) + ")"`, with the existing green style unless errors > 0.
    - `errorsChanged(n, last)`:
      - `errors` text is `"0"` when n == 0, otherwise `QString("%1 — %2").arg(n).arg(last)`;
      - tooltip is `recentErrors().join("\n")`;
      - when n > 0, use the existing red styles for `errors` and `progress_info`.
    - `activeTransfersChanged(items)`:
      - for each item: `updateProgress(item.name, clampedPercent(item), tooltip)`, where tooltip is `"File name: <name>\nSize: <bytes> / <size or 'unknown'>\nSpeed: <rate> (avg <rate>)\nETA: <dur>"`;
      - in `updateProgress`, a value of -1 gives `bar->setRange(0, 0)` and anything else gives `bar->setRange(0, 100); bar->setValue(v)`;
      - then remove every row whose name is not among the items (same deletion code as today).
      - `mActive` stays keyed by the full name, so names with `/` or `:` work (#160).
    - `logEvent(e)`: `ui.output->appendPlainText(renderRcloneLogEvent(e))`, and if `mLogFile` is set, write the same line plus `\n` to it. The constructor calls `ui.output->setMaximumBlockCount(10000)`.
    - `setLogFile(path)`: open `QIODevice::Append | QIODevice::Text` and write `# Rclone Browser transfer log <ISO datetime>: <mArgs.join(" ")>\n`. On failure, append `Cannot write log file <path>: <errorString>` to `output`. The file is flushed after each line and closed at finish.
    - `noteIgnoredArgs(list)`: if non-empty, append `Rclone Browser manages rclone logging; ignored options: <list.join(" ")>` to `output`.
    - Process `readyRead`: `while (mProcess->canReadLine()) mMonitor->consumeLine(mProcess->readLine());`. Process `finished(code, st)`:
      1. drain the remaining bytes;
      2. call `mMonitor->finish(code, st, mJobFinalStatus == "stopped")`;
      3. if `lastStatsText()` is non-empty, render `RcloneLogEvent{Kind::Stats, time = QDateTime::currentDateTime(), level = Notice, msg = lastStatsText()}` once and append it to `output` and to the log file. A run with no Stats event (for example a `critical` config failure, R2) appends nothing here;
      4. run the existing UI branch: Finished uses today's `status == 0` branch; Error uses the error branch; Stopped keeps "stopped" and `2_transfer_stopped`;
      5. emit `finished(info, mJobFinalStatus)` exactly as today.
    - `cancel()` is unchanged (kill + waitForFinished).
  - Scope: the constructor signature, the signals, `getStatus` values and the `showDetails`/`showOutput`/copy/cancel buttons are unchanged.
  - Steps:
    1. S1 — Rewrite the JobWidget internals per the contract. Expected: it builds; `git grep -n "rxSize\|NormalizeRcloneStatsLine" src` is empty.
    2. S2 — Wire `addTransfer`. Expected: it builds.
    3. S3 — Write the tests. Run `scripts/dev.sh test`, `scripts/dev.sh e2e` and `scripts/dev.sh smoke`.
  - Recovery boundary: after S1.
  - Failure handling: if an e2e is flaky on timing, increase only the wait ceilings (≤ 60 s), never the assertions. Two failures on the same oracle go to agent-1:opus.
- **Unit tests:** `tests/tst_jobwidget_render.cpp`:
  - T-JOB-RENDER:
    - construct a JobWidget with a never-started `QProcess` and args `{"copy","s","d"}`;
    - feed every copy_ok line to `widget.monitor()->consumeLine()`;
    - compute `fs = parseRcloneStats(...)` of the last stats line;
    - expect `findChild<QLineEdit*>("transferred")->text() == "6 / 6, 100%"`, `size == formatBytes(fs.bytes) + ", 100%"`, `errors == "0"`;
    - expect 0 `QProgressBar` children after the final line;
    - expect the output to contain 6 lines with `: Copied (new)`.
  - T-JOB-RENDER-ERR: feed copy_errors. The `errors` text starts with a number ≥ 1 followed by ` — ` and contains `permission denied`.
  - Gate G-U1.4: `scripts/dev.sh test qt64 -- -R tst_jobwidget_render` passes, with 2 functions.
- **e2e tests:** `tests/tst_e2e_jobwidget.cpp`.
  - Setup for every case:
    - polluted env (`RCLONE_VERBOSE=1`, `RCLONE_STATS=5s`, `RCLONE_STATS_ONE_LINE=true`) passed through `RcloneEnvironment(Parsed, env)`;
    - args from `PrepareTransferArgs`;
    - `QProcess` with MergedChannels started on `rbRclone()`;
    - sample the widget texts every 200 ms;
    - wait on `QSignalSpy(&w, &JobWidget::finished)` for up to 60 s.
  - T-E2E-JOB-STATS:
    - 4 files of 1.5 MB named `a.bin`, `dir [x] (y)/b.bin`, `c:d.bin` and `é "q".bin`, copied with `--bwlimit 1M`;
    - at some sample, `size` contains `iB` and `bandwidth` ends with `/s`, and at least one `QProgressBar` exists;
    - final status `finished`;
    - the output has ≥ 4 `Copied (new)` lines and no ` DEBUG : ` line.
  - T-E2E-JOB-ERRORS: read-only destination. Status `error`; `errors` text matches `^[1-9]\d* — ` and contains `permission denied`.
  - T-E2E-JOB-LOGFILE (#142):
    - user args include `--log-file <tmp>/t.log`, passed through `PrepareTransferArgs`, so the test calls `noteIgnoredArgs` and `setLogFile` as `addTransfer` does;
    - at some sample, `size` is non-empty;
    - after the finish, t.log contains `Copied (new)` and a ` NOTICE: ` stats block;
    - the output contains `ignored options: --log-file`.
  - T-E2E-JOB-CANCEL (#117):
    - 10 files of 1.5 MB with `--bwlimit 500K`;
    - after the first non-empty `size` sample, record `pid = p->processId()`, call `w.cancel()`, and expect `p->state() == QProcess::NotRunning` within 5 s;
    - on Linux, `!QFileInfo::exists("/proc/" + pid)`;
    - the count of destination files is the same before and after a 3 s wait;
    - `w.getStatus() == "2_transfer_stopped"`.
  - Gate G-U1.4e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_jobwidget` passes, with 4 functions.
- **Done:** review recorded; G-U1.4 and G-U1.4e pass; smoke passes; README obligation handled at P1; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present. G-CI is debt.
- Assertions: every test listed for 1.1–1.4 is discovered and passes. Review grep for I-1: `git grep -n "systemEnvironment" src/` shows only `rclone_process.cpp` and the recorded non-rclone exceptions.
- README obligation: add a "Transfers and logging" subsection under the existing usage text. It explains that log, stats and verbosity options in "extra options" are managed by the app, that `--log-file` is honored as a text log, and that `RCLONE_*` logging variables are ignored. Owner: P1.
- Every sub-phase must be DONE. Supervisor agent-1:opus reviews. Close P1 with commit `phase(1): …`.
