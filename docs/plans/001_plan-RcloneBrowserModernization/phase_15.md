# Phase 14 — Upstream issue sweep (revision 11)

Intent:
- Every open upstream issue (129, snapshot `inputs/upstream-open-issues-bodies.json`) is verified against the fork. Still-present bugs are fixed with a test. Small features that do not collide with a pending phase are implemented. The rest is answered in the README or deferred with a recorded rationale (user, 2026-10-21: "le issue upstream vanno verificate e risolte, se occorre").
- `docs/triage/upstream-issues.md` gets a "Fork status" for all 129 issues.
Prerequisites: P7 (main window split, T-MW-SNAPSHOT, `main_window_tasks.cpp`) and 6.3 (G-SERVERS harness). P8 and P9 depend on P14 (revision 11), so the execution order is P7 → P14 → P8.
Phase closure: P14; review by agent-1:opus. P14 is the v2.0.0 readiness point (revision 11 moves it from P7).

## State and ownership contract
Read STATE.md §0–1 first. Open a unit before editing it. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. Tests use only temp configs, temp local dirs and the `dev.sh servers` harness (I-9); the network rules are the user authorizations in STATE.md §3.

## Local design context
Plan revision: 11.
- Sweep input: a read-only analysis workflow (wf_7aa437c8-baa, 7 analysts, 98 issues in 7 groups) produced hypotheses per issue (status, evidence, fix proposal). They come from reading code only, and some are visibly wrong. They are **hypotheses, not findings**.
- Rules for every unit of this phase:
  - R-REPRO: every bug item starts with a test that reproduces the reported behavior and fails on the unit base. If the test passes on the base, the item is "not reproduced in the fork": the test stays as a regression guard, no code changes for it, and the review row records it.
  - R-ROW: no triage row moves to "fixed" without a test ID or a recorded repro. `cannot-determine` items get a repro attempt, never a speculative fix.
  - R-DEDUP: an issue owned by a pending unit (8.x–13.x) keeps that owner; it is not reimplemented here.
  - R-I18N: new user-visible strings use `tr()` (they are wrapped again, if needed, by 12.2).
  - R-SNAP: a unit that changes MainWindow widgets or actions re-records the T-MW-SNAPSHOT fixture in the same unit, and the review checks the fixture diff line by line against the intended change (supervisor-approved re-record; 7.1's no-re-record rule applies only to the refactor units).
- Shared helper (14.1): NEW `src/command_line.h/.cpp`:
  ```cpp
  QString quoteArgPosix(const QString &a);    // '' for empty; unquoted if only [A-Za-z0-9_@%+=:,./-]; else '…' with ' → '\''
  QString quoteArgWindows(const QString &a);  // CommandLineToArgvW rules: unquoted if non-empty and no space, tab, '"' or cmd metachar & | < > ^ ( ) % !;
                                              // else "…" with each '"' → \" and the backslashes before a '"' or before the closing quote doubled
  enum class Shell { Posix, Windows };
  QString commandLineForClipboard(const QString &program, const QStringList &args, Shell shell);
  Shell nativeShell();                        // Windows on Q_OS_WIN, else Posix
  ```
  The program is quoted like an argument.
- Current code (anchors):
  - `src/job_widget.cpp` and `src/mount_widget.cpp`: `ui.copy` → `clipboard->setText(mArgs.join(" "))`; `src/stream_widget.cpp` has the same pattern.
  - `src/job_options.cpp` `JobOptions::getOptions()`: extra options split per line, then on spaces outside double quotes, with every `"` removed; then `--include`, `--exclude`, `--filter` lines.
  - Remote operations (create folder, move, rename, delete, purge) are lambdas in the `RemoteWidget` constructor (`src/remote_widget.cpp`).
  - The queue already has Up/Down/Purge buttons (`ui.buttonUpQueue`, `ui.buttonDownQueue`, `ui.buttonPurgeQueue`).

## Sub-phases

### 14.1 Command lines and job widget (#226, #194, #176, #123, #169, #83)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff.
- **Files:** NEW `src/command_line.h/.cpp` (registered); WRITE `src/job_widget.cpp/.ui`, `src/mount_widget.cpp`, `src/stream_widget.cpp`, `src/main_window_tasks.cpp` (task "Copy command"), `src/main_window.ui` if a button is added; NEW `tests/tst_command_line.cpp` (unit), NEW `tests/tst_jobwidget_layout.cpp` (unit, offscreen); the T-MW-SNAPSHOT fixture if MainWindow changes (R-SNAP).
- **Change:**
  - #226: every "copy command" site copies `commandLineForClipboard(<rclone path>, args, nativeShell())` instead of `join(" ")`. The text pasted into cmd.exe (Windows) or sh (POSIX) runs the same argv.
  - #194: the mount widget's copy button already exists; it uses the same helper. It contains no rc credentials (I-5: the mount argv has none).
  - #176/#123: the Tasks tab gains "Copy command" (button or context-menu action, objectName `actionCopyTaskCommand` or `buttonCopyTaskCommand`). It copies the command the task's Run would start: the same args builder as `runItem` (no new builder), quoted with the helper.
  - #169: the per-file progress area of the job widget keeps its space when it has nothing to show (`QSizePolicy::setRetainSizeWhenHidden(true)` on the widgets that toggle, or an always-visible widget that is cleared). The job widget's height no longer changes between "a file is transferring" and "no file is transferring".
  - #83: R-REPRO first: at a widget width of 600 px, the status label ("Running") and the "Show output" button must not overlap the info/source/dest fields. Fix the layout only if the test reproduces the overlap.
- **Unit tests:**
  - `tests/tst_command_line.cpp`:
    - T-CMD-POSIX: `abc` → `abc`; `` → `''`; `a b` → `'a b'`; `it's` → `'it'\''s'`; `--exclude` → `--exclude`; `._*` → `'._*'`.
    - T-CMD-WIN: `r:\Downloads\Redump - Sega - Dreamcast\` → `"r:\Downloads\Redump - Sega - Dreamcast\\"`; `a"b` → `"a\"b"`; `C:\x\y` → `C:\x\y`; `a&b` → `"a&b"`; `` → `""`.
    - T-CMD-ISSUE226: the #226 argv (program `N:\rclone\rclone.exe`, `move`, the source with spaces and a trailing backslash, the dest `MediaDB:00 - Online Media/Giochi/Rom & Bios Sets/Sega Dreamcast/`) gives one line whose Windows parse (a test-local CommandLineToArgvW implementation) equals the argv.
  - `tests/tst_jobwidget_layout.cpp` (offscreen, a JobWidget fed with the 1.4 stats fixtures):
    - T-JW-STABLE: `sizeHint().height()` is equal with a per-file transfer shown and with none (#169).
    - T-JW-OVERLAP: at width 600 the status label and `showOutput` geometries do not intersect `info`, `source` or `dest` (#83).
  - Gate G-U14.1: `scripts/dev.sh test qt64 -- -R 'tst_command_line|tst_jobwidget_layout|tst_mainwindow_snapshot'` passes with 5 functions (+1 snapshot).
- **e2e tests:** T-E2E-CMD-ROUNDTRIP (in `tests/tst_e2e_terminal.cpp` or a new `tests/tst_e2e_cmdline.cpp`): the POSIX clipboard text of a local copy job (temp dirs with `a b.txt` and `it's.txt`) run through `sh -c` with the real rclone copies both files. Gate G-U14.1e runs it.
- **Done:** gates pass; R-REPRO outcome of #83 recorded; reviews; unit closed; completion commit.

### 14.2 Transfer and task options (#179, #144, #139, #80, #98, #118)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff.
- **Files:** WRITE `src/job_options.cpp` (only if #179 reproduces), `src/main_window_tasks.cpp`, `src/main_window.h/.cpp/.ui`, `src/preferences_dialog.*`, `src/job_widget.*`, `src/utils.*` (settings key defaults, if needed); NEW `tests/tst_task_actions.cpp` (unit) and `tests/tst_e2e_exclude.cpp` (e2e); the T-MW-SNAPSHOT fixture (R-SNAP).
- **Change:**
  - #179 (R-REPRO): a copy job between temp dirs built from `JobOptions` with the Exclude tab text `._*` and, in a second case, the extra options `--exclude "._*"` must not copy `._a` and must copy `b.txt`; also with a non-empty Include text (rclone applies filters in order, so check the generated order). Fix only what the test reproduces (for example the order of `--include`/`--exclude`, or quote handling in extra options).
  - #144: Preferences checkbox "Ask before running a task" (key `Settings/confirmTaskRun`, default `false`). When on, Run, Dry run and running several tasks (below) ask one `QMessageBox::question` naming the task(s); No cancels.
  - #139: "Duplicate task" for the selected task: a copy of its `JobOptions` with a new unique id and description `<old> (copy)`, persisted through `ListOfJobOptions`, shown in the list. Tasks files of every classVersion still load (I-7).
  - #80/#98: the tasks list allows extended selection. Run with several tasks selected starts each in list order (or adds each to the queue when the queue is in use, like Run does for one task). Edit stays enabled only for a single selection.
  - #118: a finished or failed transfer job widget gets "Restart" (objectName `restart`): it starts the same transfer again as a new job (same args, source, dest, transfer mode) through the existing `addTransfer` path; a running job has no Restart.
- **Unit tests:** `tests/tst_task_actions.cpp` (offscreen MainWindow, as T-MW-SNAPSHOT does, with temp task files):
  - T-TASK-DUP: duplicating a task gives 2 tasks with different ids, the copy's description ends with `(copy)`, and the options are otherwise equal; the tasks file reloads with both.
  - T-TASK-MULTI: with 2 tasks selected, Run creates 2 jobs (or 2 queue entries) in list order; Edit is disabled.
  - T-TASK-CONFIRM: with `Settings/confirmTaskRun=true` and the message box answered No (a `QTimer` that rejects the active modal), no job starts; answered Yes, it starts.
  - Gate G-U14.2: `scripts/dev.sh test qt64 -- -R 'tst_task_actions|tst_mainwindow_snapshot'` passes with 3 functions (+1 snapshot).
- **e2e tests:** `tests/tst_e2e_exclude.cpp`: T-E2E-EXCLUDE (the #179 cases above with the real rclone) and T-E2E-JOB-RESTART (a failing copy to a missing dest dir under a read-only parent, then Restart creates a second job widget with the same args). Gate G-U14.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_exclude` passes with 2 functions.
- **Done:** gates pass; the R-REPRO outcome of #179 recorded; reviews; unit closed; completion commit.

### 14.3 Remote operations (#171, #220)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff.
- **Files:** NEW `src/remote_ops.h/.cpp` (pure argv builders; registered); WRITE `src/remote_widget.cpp` (the move and create-folder lambdas call the builders and add a timeout), `src/remote_widget.ui` only if a label lives there; NEW `tests/tst_remote_ops.cpp` (unit); WRITE `tests/servers/tst_srv_protocols.cpp` (new function).
- **Change:**
  - #171 (R-REPRO with the real rclone on temp dirs): moving folder `F` (path `p/F`) to the location `L` must give `L/F` with the whole tree, and `p/F` must no longer exist (not even empty). Upstream ran `rclone move p/F L`, which moves only the contents. The builder `folderMoveArgs(remote, srcPath, destParent)` gives `moveto <remote>:p/F <remote>:L/F --delete-empty-src-dirs` (plus the default options the code already adds); the dialog text says "Move 'F' into:". A file move keeps its current behavior.
  - #220: the create-folder dialog asks for the new folder's name: title "Create folder in '<parent>'", label "Folder name:", an empty input; OK is disabled for an empty name or one containing `/`, and the created path is `<parent>/<name>` (`mkdirArgs(remote, parent, name)`). The mkdir process gets a 60 s timeout: on expiry it is killed and an error box says the operation timed out.
- **Unit tests:** `tests/tst_remote_ops.cpp`: T-OPS-MOVEARGS (folder and file cases, paths with spaces and brackets), T-OPS-MKDIRARGS (root parent, nested parent, the rejected names). Gate G-U14.3: `scripts/dev.sh test qt64 -- -R tst_remote_ops` passes with 2 functions.
- **e2e tests:** T-E2E-OPS-MOVEFOLDER (temp dirs, the #171 oracle, in a new `tests/tst_e2e_remote_ops.cpp`; gate G-U14.3e) and T-PROTO-MOVEFOLDER (the same oracle on the 4 server rows in `tst_srv_protocols`; G-SERVERS now has 5 functions).
- **Done:** gates pass (G-SERVERS included); reviews; unit closed; completion commit.

### 14.4 Startup, rclone path, terminal, tray, window size (#159, #145, #209, #175, #208, #201, #156, #228)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff.
- **Files:** WRITE `src/main_window.*`, `src/main.cpp`, `src/preferences_dialog.*`, `src/utils.*`, the terminal launcher from 3.1; NEW `tests/tst_startup.cpp` (unit) and `tests/tst_e2e_startup.cpp` (e2e, Linux block); the T-MW-SNAPSHOT fixture (R-SNAP).
- **Change:**
  - #159 (R-REPRO): the app started with the rclone path set to a non-rclone executable (`/bin/false`, or a script printing `fclone v1.0`) or to a missing file must stay alive, show the version problem, and let the user open Preferences; it must not quit.
  - #145/#209: one helper `QString rcloneExecutableProblem(const QString &path)` (empty when the file exists and is executable, else a message naming the path). "Config" (terminal and native), the daemon start and the version check use it first: a problem shows a message with a button that opens Preferences, where the rclone path can be changed (#209: the path field must stay editable after first run).
  - #175: the terminal launcher's search order is: the Preferences value "Terminal command" (new key `Settings/terminalCommand`, default empty), then `$TERMINAL`, then the existing candidates; FreeBSD/OpenBSD/NetBSD use the same Linux path. The error text says the Preferences field can be set; it does not suggest `$TERM` (a terminal type, not a program).
  - #208 (R-REPRO): check whether "start minimised to tray" already exists (P7 context names a "start minimised to tray" block). If it exists and works offscreen with a tray, no code change: answered in 14.5. If not, add the Preferences checkbox (key `Settings/startMinimisedToTray`).
  - #201 (R-REPRO): with fresh settings and no remotes, the Jobs and Tasks tabs and their lists are enabled. Fix only what reproduces.
  - #156/#228 (R-REPRO): `MainWindow::minimumSizeHint().width()` after construction (offscreen, default font) must be ≤ 1024. Fix by layout policy (no fixed minimum widths), not by shrinking fonts.
- **Unit tests:** `tests/tst_startup.cpp`: T-START-RCLONEPATH (`rcloneExecutableProblem` for an executable, a missing file, a directory and a non-executable file), T-START-TERMINAL (the candidate order for a set Preferences value, a set `$TERMINAL`, neither), T-START-MINWIDTH (#156), T-START-TABS (#201). Gate G-U14.4: `scripts/dev.sh test qt64 -- -R 'tst_startup|tst_mainwindow_snapshot'` passes with 4 functions (+1 snapshot).
- **e2e tests:** `tests/tst_e2e_startup.cpp`: T-E2E-START-BADRCLONE: the app binary, offscreen, with a temp `XDG_CONFIG_HOME` whose settings point rclone to the `fclone` script, and then to a missing file, is alive after 8 s in both cases (#159). Gate G-U14.4e.
- **Done:** gates pass; R-REPRO outcomes recorded; reviews; unit closed; completion commit.

### 14.5 README answers and triage (all remaining issues)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews the triage table against the evidence (every "fixed" row cites a test ID or a unit).
- **Files:** WRITE `README.md` (new section "FAQ and known limitations"), `docs/triage/upstream-issues.md`; NEW `scripts/check-triage.py` (stdlib only).
- **Change:**
  - README answers (one short entry each, linking the issue): #133 (`--fast-list` per remote or in default options), #210 (Windows drive letters), #235 (100% before the end of a transfer), #252 (exclude patterns are relative to the source root), #180 (several mounts at once), #132 (listing pre-warm via `vfs/refresh`, the "Refresh cache" button and the mount script env), #116 (a global bandwidth limit through default rclone options), #213 and #247 (headless use: `rclone rcd --rc-web-gui`), #219 (build with distro Qt, not Anaconda), #244 (Wayland), #119 (FTP servers without MLSD: `disable_mlsd = true` through "Advanced (terminal)…"), #153 (sftp outside `$HOME`: a path starting with `/`), #215 (the mount dialog remembers the last options), #238 (duplicate names on Drive: `rclone dedupe`), #190 (git inside a mount: VFS cache mode).
  - `docs/triage/upstream-issues.md`: every one of the 129 issues gets a non-empty "Fork status": `fixed in <unit> (<test ID>)`, `not reproduced (<test ID>)`, `answered (README#faq-…)`, `planned in <unit>`, `deferred: <one-line rationale>`, `obsolete`, or `no action (announcement)` (#253). The 30 issues outside the sweep groups keep their earlier disposition and get their status the same way. The deferred set includes the M/L features (#19, #42/#121/#188 multiselect, #52, #76, #87, #108, #143, #158 auto-remount, #181 CLI, #204 queue manager beyond the existing Up/Down, #214 single instance, #218, #221, #249, …), #186 (macOS dock only; cannot be verified here) and #168 (no stderr in the report), each with its rationale.
  - `scripts/check-triage.py docs/triage/upstream-issues.md inputs/upstream-open-issues-bodies.json` exits 0 only if every issue number in the snapshot has exactly one row, every row has a Fork status, and every `fixed in`/`not reproduced` status names a test ID that exists as a `T_…` function in `tests/`.
- **Unit tests:** N/A (documentation); the check script is the oracle. Gate G-U14.5: `python3 -I scripts/check-triage.py docs/triage/upstream-issues.md docs/plans/001_plan-RcloneBrowserModernization/inputs/upstream-open-issues-bodies.json` exits 0 and prints `issues=129`.
- **e2e tests:** N/A (documentation only).
- **Done:** G-U14.5 passes; review; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-BUILD610, G-UNIT, G-E2E, G-SMOKE, G-SERVERS.
- README obligation: the 14.5 FAQ section plus the user-visible changes of 14.1–14.4 (copy command quoting, task duplicate/multi-run/confirm, job restart, folder move semantics, create-folder dialog, terminal command and rclone path checks). CHANGELOG: the `## [2.0.0]` section lists them.
- After P14: G-REL for `v2.0.0` (local gates on both images plus G-CI on the same HEAD); with the user's release authorization (STATE.md §3) the supervisor tags `v2.0.0` on that HEAD, then fast-forwards the fork's `master` to it.
