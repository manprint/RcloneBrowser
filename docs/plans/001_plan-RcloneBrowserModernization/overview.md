# Rclone Browser modernization — Plan overview

Authored 2026-10-21 by agent-1:opus (strong supervisor). Folder: `docs/plans/001_plan-RcloneBrowserModernization/`.
Execution starts at [STATE.md](STATE.md); all live readiness and progress are there.

## Goal and reference scenario

Goal: make the manprint/RcloneBrowser fork a maintained Rclone Browser 2.x for rclone ≥ 1.75.0. The work has five parts:
- replace every human-readable rclone output parser with structured output: JSON logs, `lsjson`, and the rc API;
- fix the upstream bugs listed below;
- add an rcd-based backend;
- integrate the useful fork work;
- deliver the F3 features: bisync, serve, copyurl/copyto, path entry, search, shortcuts, dual pane, i18n (Italian), S3 headers/tags, and BSD CI.

Reference scenario. On the final HEAD, inside the Docker dev image with Qt 6.4.2 and the host rclone 1.75.2, all of the following hold:
1. `scripts/dev.sh build qt64` and `scripts/dev.sh build qt610` compile with `-Werror` and no warnings.
2. `scripts/dev.sh test` and `scripts/dev.sh e2e` pass, and every test listed in STATE.md §11 is discovered and executed.
3. With `RCLONE_VERBOSE=1`, `RCLONE_STATS=5s` and `RCLONE_STATS_ONE_LINE=true` set in the environment, a copy started through `MainWindow::addTransfer`'s JobWidget path shows:
   - size, bandwidth, ETA, transfers, checks and elapsed time;
   - per-file progress for names containing `/`, `[ ]`, `( )`, quotes and non-ASCII characters;
   - for a failing copy, the error count and the last error;
   - when the user adds `--log-file X`, the same stats, with X receiving a rendered text log.
4. Cancelling a running transfer leaves no rclone process alive and the destination unchanged 3 s later.
5. Folder listing handles names with a trailing space, `[ ] ( )` and `:`, and entries of unknown size (Size -1).
6. The app's rcd starts on 127.0.0.1 with credentials passed only through the environment. It lists folders through `operations/list`, creates a remote through the native config flow (rc state machine, OAuth cancel cleans up), and quits cleanly.
7. Each F3 feature has a passing e2e test named in its phase.
8. G-CI: every GitHub Actions job triggered by the push is green on the pushed HEAD (today 5 build jobs; 13.2 adds OpenBSD/NetBSD). CodeQL (0.3) runs on schedule or `workflow_dispatch` only, and one user-dispatched run must be green. This requires the user.

Exclusions:
- no PRs or comments against kapitainsky/RcloneBrowser;
- no tag, release, push or merge by the executor (user actions only);
- transfers are not moved to rc async jobs (U3);
- no S3 tag viewing or editing UI, only tags set at upload;
- no notarization of macOS builds.

## Decisions

| ID | Decision and consequence | Authority/source | Supersedes |
|----|--------------------------|------------------|------------|
| U1 | Minimum rclone is 1.75.0. An older or unparsable rclone gets a blocking dialog that offers Preferences and download help. No compatibility code is kept for older versions. | User Q 2026-10-21 | analysis "min 1.60" |
| U2 | This plan covers F0–F3. | User Q 2026-10-21 | "F3 separate plan" proposal |
| U3 | Transfers (copy/move/sync/bisync/copyto/copyurl/check/dedupe) stay rclone CLI processes and are monitored through JSON log stats. rcd is used for listing, config, serve, version and VFS. | User Q 2026-10-21 | analysis "async sync/* jobs" |
| U4 | Product name is "Rclone Browser". VERSION is numeric (`2.0.0`); tags are v2.0.0-beta.1 after phase 4 and v2.0.0 after phase 7. Every tag needs explicit user approval. Bundle id is `io.github.manprint.rclonebrowser`. Update checks point at manprint/RcloneBrowser. | User Q 2026-10-21 | — |
| U5 | Full autonomous: true. WIP commits: on. Neither authorizes a push, tag, merge or release. | User 2026-10-21 "setta nel piano wip commit e autonomous:true" | — |
| U6 | Work is local. The executor makes no network calls. Downloads (fork refs, Qt 6.10 image, MinIO image, extra apt packages) are user-run preparation commands (STATE.md §3 "Prefetch"). | User 2026-10-21 "devi lavorare in locale" | — |
| D1 | The app owns rclone logging for every process whose output it parses. Arguments pass through `PrepareTransferArgs`, which strips user log/stats/verbosity flags and appends `--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER` (`-vv` when the user asked for debug). The app never emits `--log-level` (R1). | supervisor | job_options "always verbose" block |
| D2 | One environment factory, `RcloneEnvironment(mode)`, replaces `UseRclonePassword`. In `Parsed` mode it removes the logging/stats `RCLONE_*` variables in I-2, keeps the others, and adds `RCLONE_CONFIG_PASS` when set. `Interactive` mode (config terminal) keeps the user's environment plus the password. | supervisor (R1) | `UseRclonePassword` |
| D3 | `RcloneLogParser` is a pure QtCore function that maps one line to a `RcloneLogEvent` (Stats / Log / Text). A line that is not JSON becomes Text, so Go panics stay visible. Levels are normalized to DEBUG/INFO/NOTICE/ERROR/CRITICAL. | supervisor (R1, R2) | job_widget regex chain |
| D4 | `TransferMonitor` (pure QtCore QObject) owns the stats, errors and active files. JobWidget only renders. The output panel shows rendered log events, uses `setMaximumBlockCount(10000)`, and prints the final stats block once. | supervisor | `mLines` clear logic |
| D5 | Listing uses one `rclone lsjson` process per folder with SeparateChannels and the streaming `LsjsonParser`. `Item.size` becomes `qint64`, where -1 means unknown (shown blank). `Item.modified` is local `yyyy-MM-dd HH:mm:ss`. A failure shows one Special row "⚠ <message>". Export uses `lsjson -R --files-only` with the same parser. The remote list uses `listremotes --long --json`. | supervisor (R3) | lsd+lsl regexes |
| D6 | rcd transport is `rclone rcd --rc-addr 127.0.0.1:0`. Credentials are 24 random bytes in base64url, passed via `RCLONE_RC_USER` and `RCLONE_RC_PASS`. The port is read from the "Serving remote control on" log line. HTTP is POST JSON with Basic auth through QNetworkAccessManager. Shutdown is `core/quit`, then kill after 3 s. | supervisor (R4) | unix socket idea (Qt has no client) |
| D7 | Mounts stay separate rclone processes whose lifetime is independent of rcd. Their rc credentials move from argv to the environment. "Refresh cache" calls `vfs/refresh` on the mount's own rc. | supervisor | mount via rcd mount/mount |
| D8 | The native config UI drives `config/create` / `config/update` with `opt.nonInteractive=true`, answering each returned `Option`, and `all=true` for new remotes. An OAuth step (`config_is_local=true`) runs as a long request while the UI polls `config/oauthstatus` for `authUrl` and opens it. Cancel calls `config/oauthstop`, and a new remote left partial is deleted with `config/delete`. The terminal config stays available as "Advanced (terminal)". | supervisor (R5) | terminal-only config |
| D9 | The version comes from `rclone version` (first line), parsed by an anchored regex (`v(\d+)\.(\d+)\.(\d+)` plus an optional suffix) into `RcloneVersion`. `compareVersion` is deleted. | supervisor | `compareVersion` (stoi throws on "beta") |
| D10 | The test harness adds a static library `rclone-browser-core` (every source except main.cpp), QtTest tests under `tests/`, CTest labels `unit` and `e2e`. e2e tests are registered only when `-DRB_TEST_RCLONE=<path>` is given. Every target uses the `rb_warnings` interface target (`-Werror` / `/WX`). | supervisor | — |
| D11 | Local gates run in Docker. `rb-build:24.04` (Qt 6.4.2) is required at every closure. `rb-build:qt610` (aqt Qt 6.10.3, like CI) is required at G-REL and G-FINAL, and run at P<N> when present. Host rclone 1.75.2 is bind-mounted read-only, so there is no download. | supervisor (U6) | — |
| D12 | G-CI (every GitHub Actions job triggered by the push green on the pushed HEAD, plus one dispatched CodeQL run) requires a user push. It is required before any tag and at G-FINAL, and does not block phase closure. Units that edit `Q_OS_WIN`/`Q_OS_MACOS`-only code are marked CI-compiled and get supervisor review. | supervisor (U5, U6) | — |
| D13 | Fork code is ported from locally fetched refs (`refs/remotes/<fork>/*`): Nicolaius by cherry-pick; totza2010 by reimplementation (its progress/regex commits are superseded by D3); hrpv only codeql.yml and dependabot.yml; xrp001 only the header/tag-upload idea; iamnhx only the OpenBSD/NetBSD CI jobs; TommyandTina only the path-entry idea, made generic. | supervisor | — |
| D14 | The `main_window.cpp` refactor is a no-behavior-change unit pair: the constructor is split into named setup methods, and the tasks/queue/scheduler methods move to `main_window_tasks.cpp`. | supervisor (user F2 item) | — |
| D15 | i18n uses `qt_add_translations` with `translations/rclone-browser_it.ts`. The language is chosen in Preferences ("System", "English", "Italiano") and applies after a restart. | supervisor, user-delegated ("Vai con le tue consigliate") | — |
| D16 | F3 UX defaults: dual pane is a tab with two RemoteWidgets in a QSplitter, with drag-drop and "Copy to other pane". The path field becomes editable with Go and adds the typed path as a root. The shortcut map is in phase_11. Search is a filter field over the tasks list and the jobs list. | supervisor, user-delegated | — |

## Open questions

None block READY. User sign-off is requested in unit 0.4 (triage) and for tags. Neither blocks code units.

## Architecture

```
MainWindow ──▶ RcloneDaemon (QProcess rcd) ──▶ RcClient (QNAM, JSON POST, Basic auth)
   │                         ▲                      ├─ operations/list ─▶ ItemModel (fallback: lsjson process)
   │                         │                      ├─ config/* ─▶ ConfigFlow ─▶ RemoteConfigDialog
   │                         │                      └─ serve/* ─▶ ServeManager ─▶ ServeWidget
   ├─ addTransfer ─▶ PrepareTransferArgs + RcloneEnvironment(Parsed) ─▶ QProcess(rclone copy …)
   │                    └─ JobWidget ◀── TransferMonitor ◀── RcloneLogParser (per line)
   └─ MountWidget ─▶ QProcess(rclone mount --rc …; creds via env) ─▶ RcClient(mount port) vfs/refresh, mount/unmount
```

New modules are flat files in `src/` (the existing convention), all compiled into `rclone-browser-core`. Pure QtCore logic is kept apart from widgets so it is unit-testable:
- P1: `rclone_log` (parser and data model), `rclone_format` (sizes, rates, durations), `rclone_process` (arguments, environment, config-error classifier from 3.3), `transfer_monitor`;
- P2: `lsjson_parser`, `export_runner`;
- P3: `instance_lock`, `terminal_command`, `rclone_version`, `update_check`;
- P5: `rc_client`, `rclone_daemon`, `mount_launch`;
- P6: `config_flow`, `remote_config_dialog`;
- P7: `main_window_tasks.cpp` and `main_window_p.h` (moved code only);
- P8: `remote_actions`, `url_upload_dialog`;
- P9: `serve_manager`, `serve_widget`;
- P10: `path_entry`, `list_filter`;
- P11: `item_mime`, `dual_pane_widget`;
- P12: `i18n`;
- P13: `upload_headers`, `remote_icons`.

## Interfaces and compatibility

| Surface | Exact name/type | Default | Errors/conflicts | Compatibility |
|---------|-----------------|---------|------------------|---------------|
| Transfer args | `TransferArgs PrepareTransferArgs(const QStringList &)` | appends D1 flags | flag with missing value → dropped and listed in `ignored` | user "extra options" keep working; log flags are ignored and reported in the output panel |
| Child env | `QProcessEnvironment RcloneEnvironment(RcloneEnvMode = Parsed, const QProcessEnvironment &base = systemEnvironment())`, `void ApplyRcloneEnvironment(QProcess*, RcloneEnvMode = RcloneEnvMode::Parsed)` | Parsed | — | user RCLONE_* settings other than I-2 are preserved |
| Settings | `Settings/rcloneUpdateRepo` (string) | `rclone/rclone` | invalid → error label in Preferences, check skipped | new key |
| Settings | `Settings/language` (`system`/`en`/`it`) | `system` | unknown → system | new key |
| Task file | `JobOptions::classVersion` 8→9 (8.1 bisync), 9→10 (13.1 upload headers, S3 tags) | — | newer file read by an older app → existing "stored version is newer" error | files of every earlier version load unchanged (I-7) |
| rc | POST `http://127.0.0.1:<port>/<path>`, JSON body, `Authorization: Basic` | timeout 30 s; config/create OAuth step 15 min | HTTP ≥400 → `RcReply.error` from body `error` | rclone ≥ 1.75 |
| CLI | `rclone lsjson`, `listremotes --long --json`, `--use-json-log` | — | exit≠0 → stderr last line as message | rclone ≥ 1.75 |

## Phase map

| Logical phase | File | Sub-phase IDs | Depends on | Assignment |
|---------------|------|---------------|------------|------------|
| 0 Foundation | [phase_01.md](phase_01.md) | 0.1, 0.2, 0.3, 0.4 | none | agent-2, agent-3 |
| 1 JSON transfer engine | [phase_02.md](phase_02.md) | 1.1–1.4 | P0 | agent-2 |
| 2 Structured listing | [phase_03.md](phase_03.md) | 2.1, 2.2 | P1 | agent-3, agent-2 |
| 3 Tier-1 fixes and version policy | [phase_04.md](phase_04.md) | 3.1–3.3 | P1, P2 | agent-3, agent-2 |
| 4 Fork integration, docs, beta readiness | [phase_05.md](phase_05.md) | 4.1, 4.2 | P3 | agent-2, agent-3 |
| 5 rc backend core | [phase_06.md](phase_06.md) | 5.1–5.3 | P4 | agent-2 |
| 6 Native remote configuration | [phase_07.md](phase_07.md) | 6.1, 6.2, 6.3 | P5 | agent-2 |
| 7 main_window scoped refactor | [phase_08.md](phase_08.md) | 7.1, 7.2 | P6 | agent-2, agent-3 |
| 14 Upstream issue sweep (revision 11; runs after P7) | [phase_15.md](phase_15.md) | 14.1–14.5 | P7 | agent-2 |
| 8 Bisync, copyurl, copyto | [phase_09.md](phase_09.md) | 8.1, 8.2 | P14 | agent-2 |
| 9 Serve | [phase_10.md](phase_10.md) | 9.1 | P14 | agent-2 |
| 10 Navigation and productivity | [phase_11.md](phase_11.md) | 10.1–10.3 | P8, P9 | agent-2, agent-3 |
| 11 Dual pane | [phase_12.md](phase_12.md) | 11.1, 11.2 | P10 | agent-2 |
| 12 i18n Italian | [phase_13.md](phase_13.md) | 12.1–12.3 | P11 | agent-2, agent-3 |
| 13 Ecosystem extras and final | [phase_14.md](phase_14.md) | 13.1, 13.2 | P12 | agent-2, agent-3 |

No live status in this table. Phases run in ID order with one state writer, except phase 14 (revision 11), which runs after P7 and before P8: the execution order is 0–7, 14, 8–13. Phases 8 and 9 depend only on P14, so a blocked serve feature does not stall bisync/copyurl. From phase 10 on, each phase depends on its predecessor: they extend the same `remote_widget.ui`/`main_window.ui`, the shortcut map and T-MW-SNAPSHOT, and 12.2 must wrap the strings of every earlier feature.

## Reuse map

| Need | Path and symbol | Contract to preserve |
|------|-----------------|----------------------|
| Transfer choke point | `src/main_window.cpp — MainWindow::addTransfer` | signature unchanged; every transfer goes through it |
| Args from UI/tasks | `src/job_options.cpp — JobOptions::getOptions` | field→flag mapping unchanged except the removed always-verbose block (D1) |
| Extra options split | regex `" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"` in getOptions/addNewMount | quoting semantics unchanged |
| Settings | `src/utils.cpp — GetSettings, IsPortableMode, GetRclone, GetRcloneConf` | portable-mode paths unchanged |
| Item tree | `src/item_model.cpp — ItemModel::load` merge lambda `rcloneFinished` | merge/insert/remove/sort semantics unchanged |
| Task persistence | `src/list_of_job_options.cpp — operator<< / operator>>` | append-only fields, gated by `actualVersion` |
| Jobs tab insertion | `addTransfer` / `addNewMount` / `addStream` pattern (`ui.jobs->insertWidget(0, widget)` + HLine) | job count / sort / buttons bookkeeping |
| Mount rc | `src/mount_widget.cpp` unmount through rc, `src/mount_dialog.cpp` rc flags | rc port allocation via `global.usedRcPorts` |

## Research and evidence

Scope: the rclone CLI/rc behavior the design depends on, checked against rclone 1.75.2 (the version the user requires), plus the shape of the fork deltas. Qt behavior is established API, with no external question except R6.

| ID | Question / required or optional | Evidence state | Affected decisions / units / tests |
|----|---------------------------------|----------------|------------------------------------|
| R1 | JSON log/stats shape and env interaction (required) | CONFIRMED | D1–D4, 1.1–1.4, T-LOG-*, T-ARGS-*, T-E2E-JOB-* |
| R2 | Error and failure reporting in JSON mode (required) | CONFIRMED | D3, D4, 1.3, T-MON-ERR |
| R3 | lsjson / listremotes output and exit codes (required) | CONFIRMED | D5, 2.1, 2.2 |
| R4 | rcd port discovery, env credentials, auth, quit, operations/list (required) | CONFIRMED | D6, D7, 5.1–5.3 |
| R5 | rc config state machine and OAuth endpoints (required) | CONFIRMED | D8, 6.1, 6.2 |
| R6 | Qt LinguistTools availability in the images (required for 12.1) | CONFIRMED absent in `rb-build:24.04` (local check 2026-10-21); the fix is the P-5 image rebuild | D15, 0.1, 12.1 |
| R7 | Fork deltas (required for 4.1, 13.x, 0.3) | CONFIRMED (GitHub compare, 2026-10-21); the commits are not in the local clone, P-1 fetches them | D13 |
| R8 | Encrypted-config error texts (required, 3.3) | CONFIRMED (local) | 3.3, T-CFG-*, fixtures `config_errors/` |
| R9 | Terminal emulator launch syntax (required, 3.1) | CONFIRMED for gnome-terminal (local); konsole/xfce4-terminal/xterm are established conventions, tested with fake terminals only (accepted residual risk) | 3.1, T-TERM-* |
| R10 | `vfs/refresh` and `vfs/list` over a VFS host's rc (required, 5.3) | CONFIRMED (local, serve webdav as VFS host) | D7, 5.3 |
| R11 | rcd with an encrypted config; `config/unlock` (required, 5.2) | CONFIRMED (local) | 5.2, T-E2E-DAEMON-* |
| R12 | `operations/list` parameters, `_filter`, connection strings, error texts (required, 5.2) | CONFIRMED (local); Drive option names string-checked only | 5.2, T-E2E-RCLIST-* |
| R13 | rc config answers must be echoed in `parameters` (required, 6.1) | CONFIRMED (local); its "echo all" decision superseded by R18 | D8, 6.1, T-E2E-CONFIG-SFTP |
| R18 | rc config echo rules, Update password defaults, `config_fs_advanced` loop (required, 6.1; found in the 6.1 pre-dispatch review, revision 10) | CONFIRMED (local probes, isolated containers) | 6.1, 6.2, T-CFG-PARAMS, T-CFG-LOOP, T-E2E-CONFIG-SFTP, T-E2E-CONFIG-UPDATE |
| R19 | isolated protocol servers for real-protocol e2e (required, 6.3; revision 11) | CONFIRMED (local probe, internal Docker network) | 6.3, 14.3, G-SERVERS |
| R14 | bisync flags and exit codes; copyto/copyurl output (required, 8.x) | CONFIRMED (local) | D1 (`--color NEVER`), 8.1, 8.2 |
| R15 | `serve/*` rc endpoints (required, 9.1) | CONFIRMED (local) | 9.1, T-E2E-SERVE-* |
| R16 | `--header-upload` reaches the S3 PutObject request (required, 13.1); tag storage by a real S3 server (optional) | CONFIRMED on the wire (local `serve s3`); storage UNVERIFIED, optional G-S3-MINIO (P-3) | 13.1, T-E2E-S3-TAGS |

### R1 — JSON log/stats shape and environment interaction
- Sources: local `rclone` 1.75.2 (static, `/usr/bin/rclone`) and `rclone help flags`; rclone 1.60.1 in ubuntu:24.04 Docker for contrast.
- Applicability: rclone 1.75.2 on linux/amd64. The JSON field set is the same as rc `core/stats` (R4).
- Dates: probes run 2026-10-21.
- Documented fact: `--use-json-log` emits one JSON object per line with `time`, `level`, `msg` and `source`, optionally `object` and `objectType`. Stats lines carry `stats{bytes, checks, deletedDirs, deletes, elapsedTime, errors, eta, fatalError, listed, renames, retryError, serverSideCopies, serverSideCopyBytes, serverSideMoveBytes, serverSideMoves, speed, totalBytes, totalChecks, totalTransfers, transferTime, transfers, transferring[]}`. Each `transferring[]` item has `bytes, dstFs, eta, group, name, percentage, size, speed, speedAvg, srcFs`. `msg` holds the human stats block.
- Local check: the first stats line has `eta:null` and `speed:0`. The final line has no `transferring`. The per-file "Copied (new)" message is `info` with `object` set to the file name. With the host env (`RCLONE_VERBOSE=1`, `RCLONE_STATS=5s`, `RCLONE_STATS_ONE_LINE=true`) the app's `--verbose` turns into DEBUG (24 debug lines), and adding `--log-level INFO` fails with `critical "Can't set -v and --log-level"`. With a clean env, `-v` gives info+notice only.
- Decision impact: D1 (never `--log-level`; strip flags), D2 (strip env), D3. Rejected: keeping text parsing with more regexes (the root cause of #141/#142/#160).
- Unresolved: none.

### R2 — Errors in JSON mode
- Local check (1.75.2):
  - The failed copy into a read-only dir produced one `error` line per file (`msg "Failed to copy: …"`, `object` = file) and `error "Attempt 1/1 failed with 6 errors and: …"`.
  - The final stats line had `errors:6, retryError:true, lastError:"…"`.
  - The process ended with `notice "Failed to copy with 6 errors: last error was: …"` and exit 1.
  - With a bad remote there was a single `critical` line ("Failed to create file system …"), no stats, exit 1.
- Decision impact: the TransferMonitor error model (D4). The final status comes from the exit code. A `critical` line becomes the last error even without stats.

### R3 — lsjson and listremotes
- Local check:
  - `lsjson <dir>` writes to stdout `[`, then one object per line (`,`-terminated except the last), then `]`. Fields are `Path, Name, Size, MimeType, ModTime (RFC3339 with ns and offset), IsDir`. A local dir has Size 4096.
  - Missing dir: stdout is only `[`, stderr has text ERROR/NOTICE lines, exit 3.
  - Unknown remote: stderr `CRITICAL`, exit 1.
  - `listremotes --long --json` writes an array of `{name, type, source, description}`.
- Documented fact: Size is -1 when unknown (Google Docs, upstream #140).
- Decision impact: D5. Line-streaming parse tolerates `[`/`]`/trailing commas. Error text is the last non-empty stderr line.

### R4 — rcd
- Local check (1.75.2):
  - `rclone rcd --rc-addr 127.0.0.1:0 --use-json-log` logs `notice "Serving remote control on http://127.0.0.1:PORT/"`.
  - With `RCLONE_RC_USER`/`RCLONE_RC_PASS` in the env, argv carries no credentials and `/proc/PID/environ` is mode 0400.
  - A request without auth gets 401.
  - `core/quit` makes the process exit in under 2 s.
  - `core/version` returns `version` and `decomposed:[1,75,2]`.
  - `operations/list {fs, remote}` returns `{list:[lsjson items]}`.
  - `job/stop` makes `job/status` report `finished:true, error:"context canceled"`. This is not used per U3, but recorded.
- Decision impact: D6, D7. Rejected: argv credentials (visible to other local users through the process list), and a unix socket (QNetworkAccessManager cannot use it).

### R5 — Config via rc
- Sources: `rclone rc --loopback rc/list` help (1.75.2) and local probes.
- Documented fact: `config/create` takes `{name, type, parameters, opt{obscure, noObscure, nonInteractive, continue, all, state, result}}`. `config/oauthstatus` returns `{status:"running"|"stopped", authUrl}`. `config/oauthstop` stops the server. `config/unlock {configPassword}` unlocks the config.
- Local check:
  - Responses have the form `{State, Option, Error, Result}`.
  - `Option` has `Name, Help, Default, Required, IsPassword, Exclusive, Type, Examples[{Value, Help}]`. `State == ""` means done.
  - sftp with `all:true` first asks `host`.
  - drive asks `config_shared_client_id` and then `config_is_local`. Answering `true` blocks the request, and oauthstatus shows `running` with `authUrl http://127.0.0.1:53682/auth?state=…`.
  - oauthstop makes the blocked request return HTTP 500 `"config failed to refresh token: oauth authentication was cancelled"` and leaves `[g] type = drive` in the config.
  - `config/providers` returns 69 providers, each with `Name, Prefix, Description, Hide, Options[{Name, Help, Default, DefaultStr, Required, IsPassword, Advanced, Exclusive, Type, Examples…}]`.
- Decision impact: D8, including the cleanup on cancel.

### R6 — Qt LinguistTools in local images
- Local check (2026-10-21, `docker run --network none rb-build:24.04`): `/usr/lib/x86_64-linux-gnu/cmake/` has `Qt6Network` and `Qt6Test` but no `Qt6LinguistTools`. There is no `lupdate`/`lrelease` in `/usr/lib/qt6/bin` or `/usr/lib/qt6/libexec`.
- Supervisor inference (not verified offline): on Ubuntu 24.04 the CMake package comes from `qt6-tools-dev` and the binaries from `qt6-l10n-tools`. In CI, aqt and install-qt-action Qt 6.10 installs include qttools, so LinguistTools is expected there. 12.1 verifies this through G-CI.
- Decision impact: 0.1's Dockerfile lists both packages; the user rebuilds the image (P-5). CMake uses `find_package(Qt6 OPTIONAL_COMPONENTS LinguistTools)`, so builds without it stay green and skip translations. 12.x gates require it. If apt rejects the package names, the user reports it and the supervisor revises 0.1.

### R7 — Fork deltas (GitHub compare against manprint master, 2026-10-21)
- totza2010/master is 19 ahead and 12 behind, with its own Qt6 port. Useful pieces:
  - `045ee4f9a` custom rclone update repo plus validation UI;
  - `a4beb0fc5` QDate-based update check plus error handling;
  - `5d5656008` CI VERSION/tag check;
  - `89d99104b`/`6b6cbe5e5` progress regexes, superseded by D3;
  - teldrive icon.
- Nicolaius/my-customizations is 1 ahead and 0 behind: `cc483cce5` (icon multiplier, sorted remotes, hide tabs/remote icons, tab colors, stream player chooser).
- hrpv/master is 24 ahead: `.github/workflows/codeql.yml` and `.github/dependabot.yml` (plus action major bumps, which are not taken directly).
- xrp001 has commits `036882904` (upload headers) and `74fdeb738` (S3 tags → `X-Amz-Tagging` header).
- iamnhx has commit `a2e96c64a`: FreeBSD, OpenBSD and NetBSD builds.
- TommyandTina has commit `5049b4cd9`: editable path plus Go, and Internet Archive handling.

### R8–R16 — local probes (rclone 1.75.2, 2026-10-21)
- Sources: the host `rclone` 1.75.2, temporary configs and directories under the scratchpad, servers bound to 127.0.0.1 only; no external network. Each phase file carries the exact strings and commands under "Local design context"; this section records the conclusions.
- R8 (phase_04): the no-password, wrong-password and failed-command cases have distinct stderr texts (`not allowed to ask for password - set RCLONE_CONFIG_PASS`, `Couldn't decrypt configuration, most likely wrong password.`, `password command failed: exit status 1`); `RCLONE_PASSWORD_COMMAND` works; `rclone version` never loads the config. Impact: 3.3 classifies the error instead of prompting on any `RCLONE_CONFIG_PASS` mention.
- R9 (phase_04): `gnome-terminal --wait -- COMMAND…` works on the host (3.52.0) and `-e "<string>"` does not (root cause of #163/#172/#239). Impact: argv-style launch per terminal; ptyxis/kgx are not candidates.
- R10 (phase_06): `vfs/refresh {"recursive":"true"}` (a JSON string; a boolean is rejected) returns `{"result":{"":"OK"}}` on a serve-webdav rc. Impact: "Refresh cache" (5.3) and Docker tests without FUSE.
- R11 (phase_06): rcd starts with an encrypted config; the first config call fails with HTTP 500 carrying the R8 text; `config/unlock` fixes it. Impact: 5.2 `ensureRcUnlocked`.
- R12 (phase_06): `operations/list` needs `fs = "<remote>:<path>"` with `remote = ""`; `_filter.ExcludeRule` matches the CLI hidden-file flags; connection-string options are accepted. Impact: 5.2 listing parity.
- R13 (phase_07): answering only through `opt.result` stores a password in clear text; echoing all answers in `parameters` stores it obscured; `config/update` with `all:true` offers current values as Default. Impact: ConfigFlow request rules.
- R18 (phase_07, revision 10): echo only non-default answers (a password only when non-empty, never `config_*`), so the conf holds only non-default keys and passwords stay obscured. In Update, an empty password answer must be sent as `DefaultStr`, because `""` overwrites the password with an obscured empty string. `config_fs_advanced=true` with `all:true` never ends, because the `State` strings repeat. Impact: ConfigFlow request rules, no "Show advanced options", the loop guard.
- R14 (phase_09): bisync without prior listings exits 7 and needs `--resync`; JSON messages carry ANSI codes unless `--color NEVER`; long absolute paths break bisync lock names (README note). Impact: D1 flag, 8.1 e2e with short relative paths.
- R15 (phase_10): `serve/types`, `serve/start`, `serve/list`, `serve/stop`, `serve/stopall`; unknown types and params are rejected with HTTP 500 texts; `read_only` blocks webdav writes. Impact: ServeManager contract.
- R16 (phase_14): `--header-upload "X-Amz-Tagging: …"` appears in the PutObject request headers dumped by `--dump headers` against an anonymous local `rclone serve s3`. `serve s3 --auth-key` returned `InvalidAccessKeyId` (unexplained), so tests use anonymous access. Impact: 13.1 builds tags as a header; storage on real S3 is optional evidence.
- R19 (phase_07 6.3, revision 11): local isolated servers (internal Docker network): `atmoz/sftp:alpine`, `delfer/alpine-ftp-server` (needs a chroot to accept writes), `httpd:2.4-alpine` + `apr-util-dbm_gdbm` (mod_dav lock DB) and `ghcr.io/coollabsio/minio` (the `minio/minio` image is gone) pass mkdir/copy/lsjson/moveto/purge with rclone 1.75.2. Impact: 6.3 harness, G-SERVERS, 14.3.
- Unresolved: R9 non-gnome terminals and R16 tag storage, both accepted as residual risk with the stated tests; neither blocks READY.

## Invariants

| ID | Meaning | Guarding test |
|----|---------|---------------|
| I-1 | Every rclone process whose output is parsed gets its environment from `RcloneEnvironment(Parsed)`, and its args (transfers) from `PrepareTransferArgs`. No call site uses `QProcessEnvironment::systemEnvironment()` directly for rclone. | T-ARGS-*, T-ENV-*, review grep in P1 |
| I-2 | These variables are removed in Parsed mode: RCLONE_VERBOSE, RCLONE_LOG_LEVEL, RCLONE_LOG_FILE, RCLONE_LOG_FILE_COMPRESS, RCLONE_LOG_FILE_MAX_AGE, RCLONE_LOG_FILE_MAX_BACKUPS, RCLONE_LOG_FILE_MAX_SIZE, RCLONE_LOG_FORMAT, RCLONE_LOG_SYSTEMD, RCLONE_USE_JSON_LOG, RCLONE_STATS, RCLONE_STATS_LOG_LEVEL, RCLONE_STATS_ONE_LINE, RCLONE_STATS_ONE_LINE_DATE, RCLONE_STATS_ONE_LINE_DATE_FORMAT, RCLONE_STATS_FILE_NAME_LENGTH, RCLONE_PROGRESS, RCLONE_PROGRESS_TERMINAL_TITLE, RCLONE_QUIET, RCLONE_SYSLOG, RCLONE_SYSLOG_FACILITY, RCLONE_COLOR. All other variables are kept. | T-ENV-STRIP |
| I-3 | The parser never throws or crashes on arbitrary bytes. Non-JSON input is a Text event with the raw line. | T-LOG-GARBAGE |
| I-4 | Progress values shown are clamped to 0–100. Unknown size (-1 or 0 with unknown total) shows an indeterminate bar. | T-MON-CLAMP |
| I-5 | rc credentials never appear in the argv of rclone or rcd. The single exception is the user mount script's documented positional args (D7 refinement in phase_06, deprecated in the README). | T-DAEMON-ARGV, T-MOUNT-ARGV |
| I-6 | The executor never pushes, tags, merges or performs network access (U5, U6), except what the user authorized on 2026-10-21 (STATE.md §3, revision 11): pushes, `gh`, releases and merges on manprint/RcloneBrowser, image pulls/builds, documentation. Never a write to kapitainsky/RcloneBrowser. | review at every P<N> |
| I-7 | Task files of every earlier classVersion (≤ 8; ≤ 9 after 13.1) load with identical field values. | T-JO-COMPAT, T-JO-COMPAT-V9 |
| I-8 | `-Werror`/`/WX` stays enabled for every target, including tests. | G-BUILD64 log grep |
| I-9 | No test, gate, fixture recorder or probe uses the host's rclone configuration or remotes. Every rclone run uses a temporary config with isolated temporary remotes (temp local dirs, `memory`, 127.0.0.1 servers). Containers never mount the host `$HOME` or `~/.config/rclone`; dev.sh sets `RCLONE_CONFIG=/tmp/home/rclone-empty.conf`; `rbInitTestEnvironment()` sets `RCLONE_CONFIG` to a temp file. Host-side rclone runs only `version`. (User 2026-10-21, revision 2.) Revision 11: real-protocol servers run only through `scripts/dev.sh servers` on a per-run `--internal` Docker network with no published ports; the client container joins only that network. | G-U0.1 check (`docker run` args contain `-e RCLONE_CONFIG=` and no `.config/rclone`), review at every P<N> |

## Risks

| Risk | Design mitigation | Verification |
|------|-------------------|--------------|
| Library split breaks the MSVC PCH, the macOS bundle or the Windows exe | 0.2 keeps exe-only items (main.cpp, QRC, rc, icns) in the exe target and applies the PCH macro to core sources only | G-CI (Windows/macOS) before beta tag; supervisor review |
| Windows/macOS-only code cannot compile locally | D12 CI-compiled marking; such edits kept minimal | G-CI |
| A JSON-schema change in a future rclone | parser treats missing fields as defaults; fixtures re-recordable by script | T-LOG-* with field-removal cases |
| A user relies on `--log-file` | captured and written as rendered text log | T-E2E-JOB-LOGFILE |
| rcd fails to start (port, AV, policy) | listing falls back to the lsjson process; config falls back to terminal | T-E2E-DAEMON-FAIL |
| OAuth flow leaves a partial remote | cleanup on cancel/failure for new remotes | T-E2E-CONFIG-OAUTH-CANCEL |
| i18n wrapping changes behavior (string comparisons on UI text) | 12.2 deny list forbids wrapping identifiers, settings keys, flags and compared strings | G-I18N checker, T-MW-SNAPSHOT unchanged, 12.2 review after diff |

## Verification strategy

- G-BASE (before 0.1): the existing Docker build of the baseline.
- Each sub-phase runs its focused G-U gate: the named ctest `-R` set with an expected test count.
- P<N> runs the full local gates G-BUILD64, G-UNIT, G-E2E and G-SMOKE, plus G-BUILD610 when the image exists, and G-I18N from P12 on.
- G-REL (before tags v2.0.0-beta.1 after P4 and, revision 11, v2.0.0 after P14): local gates on both images plus G-CI. G-SERVERS (revision 11, local only) is required at P6 and every later P<N>.
- G-FINAL at P13: all local gates on both images, G-CI, and the reference-scenario e2e checklist in STATE.md §3.

STATE.md §3 is the command registry.

## Model-assignment summary

| Unit(s) | Implementer | Supervisor review beyond the P<N> phase review |
|---------|-------------|-----------------------------------------------|
| 0.2, 4.1, 5.1, 6.1 | agent-2:sonnet | before (contract walk-through) and after the diff |
| 1.2, 1.4, 2.2, 5.2, 7.1, 11.1, 11.2 | agent-2:sonnet | after the diff |
| 0.1, 0.3, 1.1, 1.3, 3.2, 3.3, 5.3, 6.2, 8.1, 8.2, 9.1, 10.1, 12.1, 13.1 | agent-2:sonnet | none (CI-compiled hunks of 0.3, 3.2, 5.3 in focus at P<N>) |
| 7.2, 12.2 | agent-3:haiku | after the diff |
| 0.4, 2.1, 3.1, 4.2, 10.2, 10.3, 12.3, 13.2 | agent-3:haiku | none (CI-compiled hunks of 3.1, 13.2 in focus at P<N>) |
| 6.3, 14.1–14.5 (revision 11) | agent-2:sonnet | after the diff |
| P0–P14 | agent-1:opus | phase review |
