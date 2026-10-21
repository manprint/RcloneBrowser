# Phase 2 — Structured listing (fixes #107, #130, #140; hardens #160 names)

Intent: folder listing, export and the remote list read rclone's JSON output. Names with a trailing space, `[ ] ( )`, `:` or quotes, and entries of unknown size, are listed correctly. A listing failure is visible in the tree instead of being an empty folder.
Prerequisites: P1. This phase reuses `parseRfc3339` from 1.1 and `ApplyRcloneEnvironment` from 1.2.
Phase closure: P2; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6).

## Local design context
Plan revision: 5.
- R3 (rclone 1.75.2, local probes):
  - `lsjson <dir>` writes to stdout `[`, then one object per line (`,`-terminated except the last), then `]`.
  - Fields: `Path, Name, Size, MimeType, ModTime, IsDir`. `ModTime` is RFC3339 with nanoseconds and an offset. Size -1 means unknown.
  - Logs go to stderr as text: `yyyy/MM/dd HH:mm:ss LEVEL : msg`.
  - A missing dir gives stdout `[` only and exit 3. An unknown remote gives a CRITICAL line on stderr and exit 1.
  - `listremotes --long --json` writes a JSON array of `{name, type, source, description}`.
- D5:
  - one `lsjson` process per folder, with SeparateChannels;
  - `Item.size` is `qint64`, where -1 means unknown and is shown blank;
  - `Item.modified` is local `yyyy-MM-dd HH:mm:ss`, the same text format as the lsl/lsd output it replaces, so sorting by string still orders by time;
  - a failure gives one Special row `⚠ <message>`;
  - export uses `lsjson -R --files-only`;
  - the remote list uses `listremotes --long --json`.
- I-1: every rclone process here gets `ApplyRcloneEnvironment(p)` (Parsed).
- Current code (anchors):
  - `src/item_model.cpp — ItemModel::load` starts two processes, `lsd` and `lsl --max-depth 1`, with the same option list `GetRcloneConf() << GetRemoteModeRcloneOptions() << GetShowHidden() << GetDefaultOptionsList("defaultRcloneOptions") << mRemote + ":" + parent->path.path()`. It parses them with `mRegExpFolder`/`mRegExpFile` (the file regex requires `\d+`, so Size -1 never matches → #140). The merge lambda `rcloneFinished` runs when both processes finish (Loading1 → Loading2 → Ready).
  - `getNiceSize(quint64)` sits in an anonymous namespace; values 0–9 render "0" (bug).
  - Export: `src/export_dialog.cpp — ExportDialog::getOptions` starts with `"lsl"`. `src/remote_widget.cpp` export handler (around the `ExportDialog e(remote, path, this)` line) parses `ProgressDialog::outputAvailable` chunks with the lsl regex. Lines split across chunks are lost, and the per-chunk UTF-8 decode can corrupt multibyte characters.
  - `src/progress_dialog.cpp` constructor forces `MergedChannels` and emits `outputAvailable(QString)` from `readAll()`.
  - `src/main_window.cpp — MainWindow::rcloneListRemotes` parses `listremotes --long` text with `line.split(':')`. On failure, `getConfigPassword(p)` reads stderr for `RCLONE_CONFIG_PASS`.

### Contract (NEW `src/lsjson_parser.h/.cpp`)
```cpp
#pragma once
#include <QtCore>
#include <optional>

struct ListEntry {
  QString path;        // "Path" (relative to the listed root; recursive listings contain '/')
  QString name;        // "Name"
  bool isDir = false;  // "IsDir"
  qint64 size = -1;    // "Size"; -1 unknown
  QDateTime modTime;   // "ModTime" via parseRfc3339; invalid when missing/unparsable
  QString mimeType;    // "MimeType"; may be empty
};
std::optional<ListEntry> lsjsonEntryFromObject(const QJsonObject &o);   // nullopt if "Name" missing or not a string
std::optional<ListEntry> parseLsjsonLine(QByteArrayView line);           // trims whitespace and one trailing ','; "[" / "]" / "" → nullopt
QString lsjsonModifiedText(const QDateTime &t,
                           const QTimeZone &tz = QTimeZone::systemTimeZone()); // "yyyy-MM-dd HH:mm:ss"; invalid → ""

class LsjsonStreamParser {
public:
  void feed(QByteArrayView chunk);      // appends; parses every complete '\n'-terminated line
  void finish();                        // parses the buffered remainder (no trailing newline)
  QVector<ListEntry> takeEntries();     // returns and clears the parsed entries
  int skippedLines() const;             // non-empty lines that are not "[", "]" and not a JSON object (an object without a string Name is dropped, not counted; revision 5)
};

struct RemoteInfo { QString name, type, source, description; };
std::optional<QVector<RemoteInfo>> parseListRemotesJson(const QByteArray &json); // nullopt if not a JSON array; skips elements without string name/type

QString lastRcloneStderrMessage(const QByteArray &stderrBytes);
// last non-empty line; strips a leading "yyyy/MM/dd HH:mm:ss LEVEL : " prefix
// (regex ^\d{4}/\d\d/\d\d \d\d:\d\d:\d\d [A-Z]+\s*: ); returns "" if none
```
Move `getNiceSize` to `src/rclone_format.h/.cpp` as `QString formatNiceSize(qint64 size)`:
- negative gives "";
- 0–9 give the decimal number (bug fix);
- otherwise the existing algorithm: largest prefix with `size >= 10 * base`, giving `"%1 %2"` trimmed.
- Examples: -1→"", 0→"0", 5→"5", 10239→"10239", 10240→"10 K", 1073741824→"1024 M", 10737418240→"10 G".

## Sub-phases

### 2.1 lsjson / listremotes parsers
- **Model:** agent-3:haiku
- **Assignment:** implementer. agent-1:opus reviews at P2.
- **Files:**
  - NEW `src/lsjson_parser.h/.cpp`, registered in `SOURCE`/`OTHER`.
  - WRITE `src/rclone_format.h/.cpp`: add `formatNiceSize`.
  - NEW `tests/tst_lsjson_parser.cpp` (unit, GUILESS).
  - NEW `tests/fixtures/synthetic/lsjson_variants.jsonl` with README entry.
  - READ `tests/fixtures/rclone-1.75/{lsjson_tree.json,lsjson_recursive.json,lsjson_missing.*,listremotes.json}` (recorded in 1.1).
- **Change:**
  - Preconditions: P1 DONE.
  - Contract: above.
  - Synthetic `lsjson_variants.jsonl`, one line each:
    - (a) `{"Path":"Doc","Name":"Doc","Size":-1,"MimeType":"application/vnd.google-apps.document","ModTime":"2026-10-21T10:00:00.000Z","IsDir":false},`;
    - (b) `{"Path":"x","Name":"x","Size":12,"IsDir":false}` (no ModTime);
    - (c) `{"Path":"y","Size":1}` (no Name);
    - (d) `[`;
    - (e) `]`;
    - (f) `2026/10/21 10:00:00 NOTICE: something`;
    - (g) an empty line.
  - Steps:
    1. S1 — Implement the parsers and `formatNiceSize`, and register them. Expected: it builds.
    2. S2 — Write the tests, then run `scripts/dev.sh test`.
  - Failure handling: if the recorded fixtures contradict R3, escalate to agent-1:opus.
- **Unit tests:** `tests/tst_lsjson_parser.cpp`:
  - T-LSJ-TREE: feed `lsjson_tree.json` in one chunk, then `finish()`.
    - Entries: `file 1.bin`, `file 2.bin`, `file 3.bin` (isDir false, size 1500000), `dir [x] (y)` (isDir true), `12345 ` (isDir true, name with a trailing space preserved), `a:b.txt` (size 1).
    - Every modTime is valid.
    - `skippedLines() == 0`.
  - T-LSJ-CHUNKS: feed the same bytes in 7-byte chunks, including one split inside the UTF-8 `ñ` of the recursive fixture. The entry list equals the one-chunk result (compare path, size and isDir).
  - T-LSJ-RECURSIVE: `lsjson_recursive.json` contains the path `dir [x] (y)/ñ"quote'.txt` and the path `12345 /trail.txt`. No entry has isDir true.
  - T-LSJ-VARIANTS:
    - (a) gives size -1;
    - (b) gives an invalid modTime;
    - (c) gives nullopt;
    - (d), (e) and (g) give nullopt and are not counted as skipped;
    - (f) counts as 1 skipped;
    - in total, 2 entries and `skippedLines() == 1`.
  - T-LSJ-MODTEXT: `lsjsonModifiedText(parseRfc3339("2026-10-21T17:19:56.651+02:00"), QTimeZone::utc()) == "2026-10-21 15:19:56"`. An invalid time gives "".
  - T-LSJ-MISSING: feeding `lsjson_missing.stdout` gives 0 entries. `lastRcloneStderrMessage(lsjson_missing.stderr)` is non-empty, does not start with a digit, and contains `directory not found`.
  - T-LSJ-REMOTES: `listremotes.json` gives 2 remotes; `loc` has type `local` and description `test local`; `mem` has type `memory`. Input `{}` gives nullopt. Input `[{"name":"a"}]` gives an empty vector.
  - T-FMT-NICESIZE: data-driven, every example in the contract.
  - Gate G-U2.1: `scripts/dev.sh test qt64 -- -R tst_lsjson_parser` passes with 8 functions.
- **e2e tests:** N/A — pure parsers on recorded output. The real process is exercised in 2.2.
- **Done:** G-U2.1 passes; unit closed; completion commit.

### 2.2 ItemModel, export and remote list on JSON
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff, focusing on the load/merge lifecycle (process count bookkeeping, `isDeleted` path, Special-row handling), the export streaming, and every `Item::Special` consumer.
- **Files:**
  - WRITE `src/item_model.h/.cpp`: `Item::size` becomes `qint64 size = -1`. Remove `mRegExpFolder`/`mRegExpFile` and the anonymous `getNiceSize`. Rewrite `load`. In `data()`, column 1 uses `formatNiceSize`.
  - WRITE `src/progress_dialog.h/.cpp`: add the signal `rawOutputAvailable(const QByteArray &)` and the constructor parameter `QProcess::ProcessChannelMode channelMode = QProcess::MergedChannels` (appended last).
  - WRITE `src/export_dialog.cpp` (`getOptions`) and the `src/remote_widget.cpp` export handler; NEW `src/export_runner.h/.cpp` (registered).
  - WRITE `src/main_window.cpp` (`rcloneListRemotes`).
  - Inspect every `Item::Special`, `->size` and `getNiceSize` use; locate them with literal search.
  - NEW `tests/tst_e2e_listing.cpp` (e2e, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 2.1 DONE.
  - Contract:
    - `ItemModel::load(parentIndex, parent)`:
      - Create one `QProcess *ls` with `setProcessChannelMode(QProcess::SeparateChannels)` and `ApplyRcloneEnvironment(ls)`.
      - Args: `"lsjson" << "--no-mimetype" << GetRcloneConf() << GetRemoteModeRcloneOptions() << GetShowHidden() << GetDefaultOptionsList("defaultRcloneOptions") << mRemote + ":" + parent->path.path()`.
      - A `LsjsonStreamParser` (heap, owned by the lambda captures and deleted in finish) is fed from `readyReadStandardOutput`. Each entry becomes an `Item {parent, isFolder = isDir, name, modified = lsjsonModifiedText(modTime), size = isDir ? -1 : size}` appended to `cache`.
      - stderr is accumulated into a `QByteArray`.
      - Set `parent->state = Item::Loading2` before `start`, so the single `finished` takes the existing `Loading2 → Ready` path in `rcloneFinished` unchanged.
      - Process counters are incremented by 1, not 2. The `finished` handler calls `parser->finish()`, appends the remaining entries, then runs the existing merge.
      - On `exitCode != 0` or `CrashExit`, the cache is cleared and holds exactly one Item: `state = Item::Special`, `isFolder = false`, `name = "⚠ " + (msg.isEmpty() ? QString("rclone lsjson failed (exit %1)").arg(code) : msg)` with `msg = lastRcloneStderrMessage(stderr)`. The merge then replaces the previous children with this row.
      - The loading spinner Item, the timer and the `isDeleted` branch are unchanged.
    - Special-row safety: every code path that expands or loads an item, or treats it as a file (double click, transfer, delete, rename, getIcon), must ignore `Item::Special`. The implementer lists each consumer and its guard in the evidence. Where one is missing, add `if (item->state == Item::Special) return;` (or the local equivalent).
    - `ProgressDialog`:
      - in `readyRead`, read `QByteArray raw = process->readAll();`, emit `rawOutputAvailable(raw)`, then keep the existing QString path (`QString::fromUtf8(raw)`, trim, append, `outputAvailable`);
      - use `channelMode` instead of the hard-coded `MergedChannels`;
      - when `channelMode == SeparateChannels`, also connect `readyReadStandardError` to `ui.output->appendPlainText(QString::fromUtf8(process->readAllStandardError()))`.
      - Existing callers are unchanged.
    - Export:
      - `ExportDialog::getOptions` starts with `"lsjson" << "-R" << "--files-only" << "--no-mimetype"`; the rest is unchanged.
      - The handler sets `ApplyRcloneEnvironment(process)` and passes `QProcess::SeparateChannels` to ProgressDialog. It feeds a `LsjsonStreamParser` (owned by the ProgressDialog through a QObject wrapper or a captured `std::shared_ptr`) from `rawOutputAvailable`, and calls `finish()` from the process's `finished`.
      - For each entry, write `"<path>"\n` (text mode), or `"<path>","<lsjsonModifiedText(modTime)>",<size>\n` (CSV mode).
      - This is the same quoting as today, with `Path` replacing the lsl name column.
    - `rcloneListRemotes`:
      - args `"listremotes" << GetRcloneConf() << GetDefaultOptionsList("defaultRcloneOptions") << "--long" << "--json" << "--ask-password=false"`, with `ApplyRcloneEnvironment(p)`;
      - on code 0, `parseListRemotesJson(stdout)`; nullopt is treated as a failure, giving the existing error branch;
      - per remote, `name` and `type` feed the existing icon/tooltip code;
      - the tooltip adds `"\n\ndescription: " + description` when the description is non-empty.
      - The `getConfigPassword` path is unchanged.
  - Steps:
    1. S1 — ItemModel and `formatNiceSize` wiring, plus the Special-row consumer audit. Expected: it builds; `git grep -n "mRegExpFile\|getNiceSize" src` is empty.
    2. S2 — ProgressDialog and export. Expected: it builds.
    3. S3 — rcloneListRemotes. Expected: it builds.
    4. S4 — Write the e2e tests. Run `scripts/dev.sh test`, `scripts/dev.sh e2e` and `scripts/dev.sh smoke`. Send the diff to agent-1:opus.
  - Recovery boundary: after S1.
  - Failure handling: if `GetShowHidden()` returns an option `lsjson` rejects, escalate with the exact rclone message. Do not drop the option silently.
- **Unit tests:** N/A beyond 2.1 — the ItemModel needs a real process; its behavior is asserted by the e2e tests below.
- **e2e tests:** `tests/tst_e2e_listing.cpp`.
  - Helper `loadModel(dir)`:
    - `SetRclone(rbRclone())`; `SetRcloneConf(<tmp>/t.conf)`, where t.conf contains `[loc]\ntype = local\n`;
    - `IconCache icons; ItemModel model(&icons, "loc", nullptr); QModelIndex root = model.addRoot("/", dir);` (the same calls RemoteWidget makes for a non-Windows remote);
    - trigger the load the way the view does (`model.rowCount(root)` or `fetchMore`; the implementer records which public call starts `load`);
    - wait with `QTRY_VERIFY_WITH_TIMEOUT(!model.isLoading(root), 20000)`.
  - T-E2E-LIST-NAMES (#107, #130, #160):
    - the temp dir holds `12345 /` (dir), `dir [x] (y)/` (dir), `a:b.txt`, `x "q".txt`, `Ünï.txt` and `t.txt`;
    - the model children, by name, equal exactly that set (dir names without a trailing `/`);
    - `12345 ` keeps its trailing space;
    - for `t.txt`, column 1 shows `formatNiceSize(size)` and column 2 matches `^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d$`.
  - T-E2E-LIST-UNKNOWNSIZE (#140): a local remote cannot produce Size -1. So the entry→Item conversion is a public static function, `static Item *ItemModel::itemFromEntry(const ListEntry &e, Item *parent)`, used by `load` and by this test. Feed it the synthetic `lsjson_variants.jsonl` line (a), insert the result under a root, and assert that `size == -1` and that `data(col 1, DisplayRole)` is an empty string for this non-folder item.
  - T-E2E-LIST-ERROR: list `/nonexistent-<uuid>`. Expect exactly one child, `state == Item::Special`, with a name starting `⚠ ` that contains `directory not found`. The process counter returns to its value before the load.
  - T-E2E-LIST-REFRESH: after the initial load, create `new.txt` and delete `t.txt`, then call `refresh` (the existing public reload API). The children update without duplicates.
  - T-E2E-EXPORT:
    - the export logic is extracted into a free function, `ProgressDialog *startExport(const QString &remote, const QString &path, const QStringList &exportOptions, const QString &dst, bool onlyNames, QWidget *parent)`, in NEW `src/export_runner.h/.cpp`; the RemoteWidget handler calls it;
    - the test calls it with remote `loc`, the temp tree and the options of `ExportDialog::getOptions` for default dialog values (`lsjson -R --files-only --no-mimetype <target>`), then waits for the process to finish;
    - CSV mode writes one line per file;
    - the recursive path `dir [x] (y)/inner.txt` is present, the line count equals the file count, and the export file has no lines that are not CSV.
  - T-E2E-REMOTES: run `rbRclone() listremotes --long --json --ask-password=false --config <tmp conf with [mem] type = memory and [loc] type = local, description = d>` with `ApplyRcloneEnvironment`, then `parseListRemotesJson(stdout)`. This is the call `rcloneListRemotes` makes. Expect 2 remotes, `loc` with description `d`. A full MainWindow is not required.
  - Gate G-U2.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_listing` passes with 6 functions.
- **Done:** Special-consumer audit and review recorded; G-U2.2e passes; smoke passes; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- Assertions: every test listed for 2.1–2.2 is discovered and passes; `git grep -n '"lsl"\|"lsd"' src` is empty.
- README obligation: P2 updates the "Known limitations"/FAQ text (if present) that mentions listing of special names, or records that no section mentions it.
- Supervisor agent-1:opus reviews. Close P2.
