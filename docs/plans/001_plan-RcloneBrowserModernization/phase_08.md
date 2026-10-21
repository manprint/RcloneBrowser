# Phase 7 — main_window scoped refactor (no behavior change)

Intent:
- The 2,000-line `MainWindow` constructor becomes a short sequence of named setup methods.
- The tasks/queue/scheduler methods live in `main_window_tasks.cpp`.
- Behavior is provably unchanged (golden widget-state snapshot, full suites, moved-code review).
Prerequisites: P6.
Phase closure: P7; review by agent-1:opus. Revision 11: the v2.0.0 readiness point moves to P14 (the issue sweep, phase_15.md), which runs right after P7.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6).

## Local design context
Plan revision: 1.
- D14: a no-behavior-change unit pair. Lines are moved, not rewritten. Allowed edits:
  - wrapping a contiguous block in a method;
  - turning a constructor local used by later blocks into a private member named `m<Name>` (or recomputing it when it is a cheap getter such as `GetSettings()`);
  - adding the declarations to `main_window.h`;
  - adding `#include`s.
- Not allowed: renaming, reordering statements, changing lambdas' capture lists other than `this`-equivalence, or "fixing" code on the way (record such findings in STATE.md §10 for a later task).
- Current code (anchors, line hints are from the plan baseline and will have shifted):
  - `MainWindow::MainWindow` spans about 21–2180.
  - Its top-level comment markers include dark mode, Preferences button, open remote, jobs buttons, initial sortByTime, layout spacers, statusTips, initial queue status, intercept tab closure, auto mount tasks, start minimised to tray.
  - Free functions and methods after it: `autoStartMounts`, `quitApp`, `setTasksButtons`, `setQueueButtons`, `rcloneGetVersion`, `rcloneConfig`, `rcloneListRemotes`, `getConfigPassword`, `canClose`, `closeEvent`, `restoreSchedulersFromFile`, `addTasksToQueue`, `listTasks`, `runItem`, `editSelectedTask`, `saveQueueFile`, `saveSchedulerFile`, `addSavedTransfer`, `addTransfer`, `runScript`, `addNewMount`, `addScheduler`, `addStream`, `slotCloseTab`, `sortJobs`.
- Target constructor shape, in this order and each a contiguous original block:
  1. `setupWindowAndTheme()`: window flags, power-saving object, dark mode, fonts, icons;
  2. `setupSettingsDefaults()`: first-run keys, macOS .DS_Store default;
  3. `setupActions()`: Preferences, buttons and actions wiring, button texts and statusTips;
  4. `setupRemotesTab()`: remotes list, open, config, refresh;
  5. `setupJobsTab()`: jobs buttons, sort, stop/clean;
  6. `setupTasksTab()`: tasks list, run, edit, delete, dry-run;
  7. `setupQueue()`: queue buttons, script, initial status;
  8. `setupScheduler()`;
  9. `setupTabs()`: tab close interception, non-closable tabs;
  10. `setupTrayAndStartup()`: tray, auto mounts, start minimised, version check kick-off.
- The implementer maps actual blocks to these names. If a block does not fit, it gets its own `setup<Name>()` in place, recorded in the mapping. No method exceeds 400 lines.
- Golden snapshot oracle: a test written before any refactor (7.1 S1) serializes the widget and action state of a freshly constructed MainWindow, and the expected text is committed. After each refactor unit, the same test must produce byte-identical output.

## Sub-phases

### 7.1 Golden snapshot test; constructor split
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff. The review uses `git diff --color-moved=plain --color-moved-ws=allow-indentation-change`: every removed constructor line reappears in a setup method, and the only non-moved hunks are method signatures, calls, member declarations and promoted locals.
- **Files:**
  - NEW `tests/tst_mainwindow_snapshot.cpp` (unit, `QTEST_MAIN`, Linux only: revision 13) and `tests/fixtures/synthetic/mainwindow_snapshot.txt`.
  - WRITE `src/main_window.h/.cpp`.
- **Change:**
  - Preconditions: P6 DONE.
  - Revision 13 (supersedes the registration, environment and recording lines of 7.1): (a) `tst_mainwindow_snapshot` is registered only when `CMAKE_SYSTEM_NAME STREQUAL "Linux"`: the constructor has platform branches and CI runs unit tests on Windows, macOS and FreeBSD; the Windows/macOS compile of the split is checked by G-CI dispatched on the 7.1 commit, before 7.2. (b) `scripts/dev.sh` does not forward host variables, so the test always writes its actual snapshot next to its binary (`mainwindow_snapshot.actual.txt`) and S1 records the fixture by copying that file; `RB_RECORD_SNAPSHOT=1` still works for direct runs. (c) the test uses `SetRclone("/bin/false")` always (identical on every runner, no rcd from a unit test); the snapshot skips objectNames starting with `qt_` or `_q_` (Qt internals that differ between Qt versions), escapes `\`, newlines and `|`, and replaces the app version and the test temp dir with `<VERSION>` and `<TESTDIR>`. (d) after the window is destroyed the test checks that no `rcd --rc-addr 127.0.0.1:0` process is left. The fixture must also pass on qt610 (CI Linux runs Qt 6.10.3).
  - Snapshot contract, `QString mainWindowSnapshot(QWidget *w)`:
    - for every `QWidget` descendant with a non-empty objectName, sorted by objectName then class name, one line `W|<objectName>|<className>|<isEnabled>|<isVisibleTo(w)>|<text>|<toolTip>|<checked>`;
      - text: from QAbstractButton/QLabel/QLineEdit, else empty;
      - checked: for checkable QAbstractButton, else `-`;
    - then for every `QAction` descendant with a non-empty objectName, sorted, `A|<objectName>|<text>|<isEnabled>|<isCheckable>|<isChecked>|<shortcut().toString()>`;
    - then `T|<i>|<tabText(i)>` for `ui.tabs`.
    - Environment: `rbInitTestEnvironment()`; `SetRclone(rbRclone())` when set, else `/bin/false` (the snapshot is taken synchronously right after construction, before any event processing, so the rclone result does not affect it); `QT_QPA_PLATFORM=offscreen`.
    - The test constructs `MainWindow w;` and compares the snapshot to the fixture.
    - With `RB_RECORD_SNAPSHOT=1` in the environment it writes the fixture instead and calls `QSKIP("recorded")`. Recording is done only in S1.
  - Steps:
    1. S1 — Write the test and record the fixture with the unrefactored code (`RB_RECORD_SNAPSHOT=1 scripts/dev.sh test qt64 -- -R tst_mainwindow_snapshot`). Then run without the variable. Expected: it passes against the fixture. If construction blocks (modal dialog) or the output is non-deterministic across two runs, escalate to agent-1:opus before refactoring.
    2. S2 — Split the constructor per the context. Expected: it builds; the snapshot test passes unchanged.
    3. S3 — Full `scripts/dev.sh test e2e smoke`. Send the diff to agent-1:opus.
  - Recovery boundary: after S1 (fixture committed with the unit; record the fixture hash in STATE.md §6).
  - Failure handling: a snapshot mismatch after the split is a refactor bug. Fix the code; never re-record the fixture after S1. Re-recording needs a supervisor-approved revision.
- **Unit tests:** T-MW-SNAPSHOT, as above. Gate G-U7.1: `scripts/dev.sh test qt64 -- -R tst_mainwindow_snapshot` passes (1 function, not skipped). The constructor body is ≤ 40 lines (`awk` line count recorded).
- **e2e tests:** the full G-E2E suite and G-SMOKE, which must not regress. A new e2e is N/A: there is no behavior change, and the snapshot plus suites are the oracle.
- **Done:** moved-code review recorded; G-U7.1 passes; full suites pass; unit closed; completion commit.

### 7.2 Move tasks/queue/scheduler methods to main_window_tasks.cpp
- **Model:** agent-3:haiku
- **Assignment:** implementer. agent-1:opus reviews after the diff with the same moved-code check (method bodies must be byte-identical apart from leading context).
- **Files:**
  - NEW `src/main_window_tasks.cpp`, registered in `SOURCE`.
  - WRITE `src/main_window.cpp`.
  - WRITE `src/main_window.h` only if a file-local helper must become a private static member.
- **Change:**
  - Preconditions: 7.1 DONE.
  - Move verbatim, in their current order: `setTasksButtons`, `setQueueButtons`, `restoreSchedulersFromFile`, `addTasksToQueue`, `listTasks`, `runItem`, `editSelectedTask`, `saveQueueFile`, `saveSchedulerFile`, `addSavedTransfer`, `addScheduler`, `runScript`, plus the setup methods `setupTasksTab`, `setupQueue` and `setupScheduler` from 7.1.
  - File-local helpers (anonymous namespace, `static` functions) used by both files go to NEW `src/main_window_p.h` as `inline` functions. Helpers used only by moved code move with it.
  - `main_window_tasks.cpp` starts with the same include block as `main_window.cpp` (keep the PCH include first).
  - Steps:
    1. S1 — Move and build. Expected: it builds with `-Werror`; `wc -l src/main_window.cpp` is reduced by at least the moved line count.
    2. S2 — Run `scripts/dev.sh test e2e smoke`, including T-MW-SNAPSHOT.
- **Unit tests:** T-MW-SNAPSHOT re-run (G-U7.1 command), unchanged fixture.
- **e2e tests:** the full G-E2E and G-SMOKE. A new e2e is N/A, for the same reason as 7.1.
- **Done:** review recorded; gates pass; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present. G-CI is debt: Windows/macOS compile the moved `#ifdef` code, and this is required before the v2.0.0 tag (G-REL).
- README obligation: "Development" gains a short "Source layout" note (main window split, core library, tests). P7 owns it, together with a CHANGELOG section `## [2.0.0] - unreleased` listing the P5–P7 changes (rc backend, native config, mount credentials, refactor) above the beta.1 section.
- After P7 the next unit is 14.1 (revision 11). There is no tag at P7. G-SERVERS is a required gate (revision 11).
