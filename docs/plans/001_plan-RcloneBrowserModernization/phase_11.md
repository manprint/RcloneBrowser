# Phase 10 — Navigation and productivity (#16, #21, #94; Internet Archive)

Intent:
- The path field of a remote view is editable: typing a path and pressing Enter or Go opens it as a root (#16). This also makes remotes whose root cannot be listed usable, such as Internet Archive items.
- The Tasks and Jobs lists have a search filter (#21).
- Consistent keyboard shortcuts cover the main actions (#94), scoped so that several visible remote views never conflict.
Prerequisites: P8, P9 (both touch `remote_widget.ui` and `main_window.ui`; the shortcut map covers their new actions).
Phase closure: P10; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6).

## Local design context
Plan revision: 1; revision 27 (agent-1:opus, before 10.1, on the code after P7–P9) replaces the points it names; the text below is already updated.
- **Revision 27** (superseded → now):
  - the fixed tabs were "0 Remotes, 1 Jobs, 2 Tasks, 3 Queue, then remote tabs" → there is also 4 Scheduler; remote tabs start at index 5, so the tab shortcuts are Ctrl+1 to Ctrl+5;
  - "MainWindow (window) Ctrl+W closes the current remote tab (index ≥ 4)" → dropped: every RemoteWidget already has `QShortcut(QKeySequence::Close)` (Ctrl+W; Cmd+W on macOS; also Ctrl+F4 on Windows); 10.3 makes it go through `tabCloseRequested` because today it only calls `removeTab` and the widget is never deleted (the tab's × deletes it);
  - "every RemoteWidget action gets `Qt::WidgetWithChildrenShortcut`" → keeps `Qt::WindowShortcut`, added to the RemoteWidget itself: the rule served several visible views (the dual pane, skipped by revision 25); only the current remote tab is visible, Qt never matches a shortcut of a hidden widget, and WidgetWithChildrenShortcut would stop F5, F2, Del… after a click on the tab bar;
  - uniqueness was checked per scope → over the union of every scope that can be active at once (below);
  - `normalizeEnteredPath(text, isLocal)` with the remote "passed separately" → `normalizeEnteredPath(text, remote, isLocal)`; the path field focus rule, the Drive mode switch and the typed roots, Go moving focus to the tree, the root path rules (`QDir`) and the error-row oracle are now explicit;
  - gates: G-CI is dispatched after every 10.x commit (MSVC `/W4 /WX` applies to src since B-T4 and finds warnings the Linux gates cannot).
- D16: the path field becomes editable with Go and adds the typed path as a root. Search is a filter field over the tasks list and the jobs list. The shortcut map is below.
- D13: the TommyandTina idea (editable path, Internet Archive) is reimplemented generically. No code is taken.
- Current code (anchors):
  - `src/remote_widget.ui`: `QLineEdit path` (readOnly true) and `QTreeView tree`.
  - Actions with shortcuts:

    | Action | Shortcut |
    |--------|----------|
    | `refresh` | F5 |
    | `mkdir` | F7 |
    | `rename` | F2 |
    | `purge` | Del |
    | `upload` | Alt+U |
    | `download` | Alt+D |

  - `src/remote_widget.cpp`:
    - `isLocal` is `remoteType == "local"` and is forced false outside Windows: on Linux and macOS a local remote has the single root `"/"` like any remote (`addRoot("/", root)`, `mRootIndex`, expanded, `initialModelLoading`); on Windows a local remote has one root per `QDir::drives()` (`addRoot(toNativeSeparators(path), path)`) and no `mRootIndex`.
    - For a non-local remote `root` is an empty string; `Item::path` is a `QDir`, so that root's `model->path(idx).path()` reads `"."`. `QDir` cleans paths (no `.`/`..`, no double separators).
    - `ui.path`: `setText(remote + ":" + path)` on a single selection (native separators when `isLocal`), `clear()` on an empty or multiple selection and when a Drive mode switch ends, and the blinking "*****    Please wait - finishing background jobs    *****" while `switchRemoteType` waits for running listings.
    - `switchRemoteType` (Drive "Shared with me", "Trash" and back) clears the rows of `mRootIndex` and refreshes it.
    - `QShortcut *close = new QShortcut(QKeySequence::Close, this)` calls `tabs->removeTab(tabs->indexOf(this))`.
    - Actions added by P8/P9: `actionCopyTo`, `actionUploadFromUrl`, `actionServe` (Tools menu and context menu). The error row of a failed listing is an `Item::Special` row named `"⚠ " + <rclone's error>` (`errorItem` in `src/item_model.cpp`); `model->isSpecial(index)`.
  - `src/main_window.ui`: `tasksListWidget`, `queueListWidget` (QListWidget); `jobs` (a QVBoxLayout of JobWidget/MountWidget/StreamWidget separated by HLines); tabs 0 Remotes, 1 Jobs, 2 Tasks, 3 Queue, 4 Scheduler, remote tabs from 5; `preferences` Alt+P; `quit` Ctrl+Q; `actionDelete` (no shortcut); `actionServes` "Serves…" (File menu, no shortcut); menu bar "&File" (Alt+F) and "&Help" (Alt+H). `actionRun`, `actionEdit` and `actionDryRun` sit only on the Tasks tab buttons and the tasks context menu (not in the File or tray menus).
  - `MainWindow::listTasks` (`src/main_window_tasks.cpp`) fills `tasksListWidget`; `MainWindow::sortJobs` (`src/main_window.cpp`) reorders `jobs`. The tab × runs `slotCloseTab` (which picks the next current tab), `deleteLater()` on the widget and `removeTab`.
  - Totals before phase 10: G-UNIT 25, G-E2E 18; T-MW-SNAPSHOT fixture sha256 `fccc386d…`.
- Shortcut map (revision 27). Every RemoteWidget action with a shortcut keeps `Qt::WindowShortcut` and is added to the RemoteWidget itself (`addAction`): only the current remote tab is visible, a hidden widget never matches a shortcut, and the shortcut also works with the button row hidden. Window-wide actions are `QAction`s added to the MainWindow. The Tasks tab actions keep `Qt::WindowShortcut`; their buttons are on the Tasks page, hidden on other tabs. Existing shortcuts are unchanged. `actionUploadFromUrl` and `actionServes` get none.

  | Scope | Action | Shortcut |
  |-------|--------|----------|
  | RemoteWidget | focus the path field and select its text (new `actionGoToPath`) | Ctrl+L |
  | RemoteWidget | `getInfo` | Alt+I |
  | RemoteWidget | `getSize` | Alt+S |
  | RemoteWidget | `getTree` | Alt+T |
  | RemoteWidget | `link` | Alt+L |
  | RemoteWidget | `mount` | Alt+M |
  | RemoteWidget | `stream` | Alt+V |
  | RemoteWidget | `move` | F6 |
  | RemoteWidget | `actionCopyTo` (8.2) | Shift+F5 |
  | RemoteWidget | `actionServe` (9.1) | Alt+E |
  | MainWindow (window) | Remotes, Jobs, Tasks, Queue and Scheduler tabs | Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4, Ctrl+5 |
  | MainWindow (window) | focus the search field of the current tab (Tasks or Jobs) | Ctrl+F |
  | MainWindow (window) | refresh the remote list (`ui.refresh`) | Ctrl+R |
  | MainWindow, Tasks tab | `actionRun` | Ctrl+Return |
  | MainWindow, Tasks tab | `actionEdit` | Ctrl+E |
  | MainWindow, Tasks tab | `actionDryRun` | Ctrl+Shift+Return |
  | RemoteWidget (existing `QShortcut`) | close this remote tab like its × (through `tabCloseRequested`) | `QKeySequence::Close` (Ctrl+W; Cmd+W on macOS; also Ctrl+F4 on Windows) |

- Before assigning, the implementer checks each new shortcut against all existing ones (a `.ui` grep plus code `setShortcut` and `QShortcut`). Uniqueness holds over the union of every scope that can be active at once: the RemoteWidget actions and its Close shortcut, the Tasks tab actions, the MainWindow window actions (`preferences` Alt+P, `quit` Ctrl+Q and the new ones) and the menu bar mnemonics Alt+F and Alt+H. Two active shortcuts with one key make Qt emit `activatedAmbiguously` and run neither. If a collision is found, escalate instead of choosing another key.

## Sub-phases

### 10.1 Editable path with Go (#16); rootless remotes
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P10.
- **Files:**
  - WRITE `src/remote_widget.h/.cpp/.ui`: `path` editable, a `QToolButton goPath` ("Go") after `path`, and `actionGoToPath`.
  - NEW `src/path_entry.h/.cpp`, registered.
  - NEW `tests/tst_path_entry.cpp` (unit) and `tests/tst_e2e_path_entry.cpp` (e2e, `QTEST_MAIN`).
- **Change:**
  - Preconditions: P8 and P9 DONE.
  - Contract:
    ```cpp
    QString normalizeEnteredPath(const QString &text, const QString &remote, bool isLocal);  // trim; stripRemotePrefix(text, remote); isLocal: QDir::fromNativeSeparators + QDir::cleanPath; else: collapse runs of '/' and strip a trailing '/' except the root "/"; "" stays "" (revision 27: remote passed explicitly)
    QString stripRemotePrefix(const QString &text, const QString &remote);  // "r:a/b" with remote "r" → "a/b"; other prefixes untouched
    bool remoteTypeNeedsPathEntry(const QString &remoteType);       // true for "internetarchive"
    ```
    - `ui.path`:
      - readOnly false; `setPlaceholderText("Type a path and press Enter")`;
      - every write from the selection handler and the end of a Drive mode switch (`setText` and `clear`) goes through one helper that does nothing while the field has focus; the helper stores the last text, and Esc restores it;
      - while `switchRemoteType` waits (the blinking text), the field is read-only and Enter and Go do nothing; it is editable again when the switch ends.
    - Enter or Go with `p = normalizeEnteredPath(text, remote, isLocal)`:
      - empty: no-op;
      - otherwise, if a top-level root `r` exists with `model->path(r).path() == QDir(p).path()`, select it; else `QModelIndex idx = model->addRoot(isLocal ? QDir::toNativeSeparators(p) : p, p)`, select it and expand it, which triggers load;
      - then the tree gets the focus, so the selection handler writes `remote:<path>` into the field;
      - `""` (the home of the remote, the default root, path `"."`) and `"/"` are different roots: on sftp `r:` is the home folder and `r:/` the filesystem root;
      - a listing failure shows the existing error row under that root: one `Item::Special` row named `"⚠ " + <rclone's error>` (P2/P5 behavior).
    - A Drive mode switch (`switchRemoteType`) removes the typed roots (every top-level root except `mRootIndex`) before it refreshes `mRootIndex` (revision 27: they would keep the old view's content).
    - Remotes for which `remoteTypeNeedsPathEntry` is true:
      - the `"/"` root is still added but not auto-expanded;
      - the path field gets the placeholder "Enter an Internet Archive item identifier and press Go";
      - the path field gets focus on open.
    - `actionGoToPath` (Ctrl+L) focuses `path` and selects all.
  - Steps:
    1. S1 — `path_entry` plus unit tests.
    2. S2 — RemoteWidget wiring.
    3. S3 — e2e; full gates.
- **Unit tests:** `tests/tst_path_entry.cpp`:
  - T-PATH-NORMALIZE (remote `"r"`):
    - `("  a//b/ ", "r", false)` gives `a/b`;
    - `("r:/a//b/", "r", false)` gives `/a/b`;
    - `("/", "r", false)` gives `/`; `("//", "r", false)` gives `/`;
    - `("", "r", false)` and `("  ", "r", false)` give `""`;
    - `("/tmp/x/../y", "r", true)` gives `/tmp/y`; `("s:a", "r", false)` gives `s:a`.
  - T-PATH-PREFIX: `("r:a/b", "r")` gives `a/b`; `("s:a", "r")` is unchanged; `("r:", "r")` gives `""`.
  - T-PATH-ROOTLESS: `internetarchive` is true; `drive` and `local` are false.
  - Gate G-U10.1: `scripts/dev.sh test qt64 -- -R tst_path_entry` passes with 3 functions.
- **e2e tests:** `tests/tst_e2e_path_entry.cpp`. A RemoteWidget is constructed as in `MainWindow` (`new RemoteWidget(&icons, "loc", "local", nullptr)`) with conf `[loc] type = local` and `SetRclone(rbRclone())`.
  - On Linux `isLocal` is false for this widget, so the e2e exercises the remote branch with absolute local paths; the file is in the `UNIX AND NOT APPLE` block of `tests/CMakeLists.txt`.
  - T-E2E-PATH-GO: set `path` text to `<tmp>/sub` and click `goPath`. Within 20 s the model has a top-level root whose path is `<tmp>/sub`, its children include `inner.txt`, it is the current index, and the field reads `loc:<tmp>/sub`.
  - T-E2E-PATH-DUP: Go on the same path twice leaves a single root for that path.
  - T-E2E-PATH-MISSING: Go on `<tmp>/nope` gives a root whose only child is an `Item::Special` row whose name starts with `"⚠ "` and contains `directory not found`.
  - Gate G-U10.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_path_entry` passes with 3 functions.
  - Totals after 10.1: G-UNIT 26, G-E2E 19.
- **Done:** gates pass; unit closed; completion commit; G-CI dispatched on it.

### 10.2 Search filter for Tasks and Jobs (#21)
- **Model:** agent-3:haiku
- **Assignment:** implementer. agent-1:opus reviews at P10.
- **Files:**
  - WRITE `src/main_window.ui`: `QLineEdit tasksFilter` above `tasksListWidget`, and `QLineEdit jobsFilter` above the jobs area, both with a clear button and the placeholder "Filter".
  - WRITE `src/main_window.cpp` / `src/main_window_tasks.cpp`.
  - NEW `src/list_filter.h/.cpp`, registered.
  - NEW `tests/tst_list_filter.cpp` (unit) and `tests/tst_e2e_filter.cpp` (e2e, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 10.1 DONE.
  - Contract:
    ```cpp
    bool filterMatches(const QString &filter, const QStringList &fields);  // split filter on whitespace; every token must occur (case-insensitive) in at least one field; empty filter → true
    ```
    - Tasks: on `tasksFilter` textChanged and after every `listTasks()`, each `tasksListWidget` item is hidden unless `filterMatches(filter, {item->text(), item->toolTip()})`. Selection of hidden items is cleared.
    - Jobs: on `jobsFilter` textChanged and after each job insertion or `sortJobs`, each job widget and its following HLine separator are hidden unless the filter matches `{widget info text, source, dest}`. Each job widget class gets `QStringList filterFields() const` returning its info, source and dest texts (JobWidget, MountWidget, StreamWidget).
    - Ctrl+F focuses the filter of the current tab (Tasks or Jobs). That shortcut is assigned in 10.3, and this unit exposes the widgets. The job count label keeps counting all jobs.
  - Steps:
    1. S1 — `list_filter` plus tests.
    2. S2 — UI wiring. Run `scripts/dev.sh test e2e smoke`, and re-record T-MW-SNAPSHOT under supervisor approval (2 new widgets; record the diff in STATE.md §8).
- **Unit tests:** `tests/tst_list_filter.cpp`, T-FILTER-MATCH:
  - `("", {...})` is true;
  - `("gd photos", {"Copy gd:Photos", "to /x"})` is true;
  - `("gd backup", {"Copy gd:Photos"})` is false;
  - `("PHO", {"photos"})` is true.
  - Gate G-U10.2: `scripts/dev.sh test qt64 -- -R tst_list_filter` passes with 1 function.
- **e2e tests:** `tests/tst_e2e_filter.cpp`, T-E2E-FILTER-TASKS:
  - construct MainWindow (as T-MW-SNAPSHOT does) with a test tasks file containing 3 saved tasks with distinct descriptions (written by `ListOfJobOptions` into the test config dir before construction);
  - set `tasksFilter` to the second description;
  - exactly 1 visible item, with that description.
  - Gate G-U10.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_filter` passes.
  - Totals after 10.2: G-UNIT 27, G-E2E 20.
- **Done:** gates pass; snapshot change recorded; unit closed; completion commit; G-CI dispatched on it.

### 10.3 Keyboard shortcuts (#94)
- **Model:** agent-3:haiku
- **Assignment:** implementer. agent-1:opus reviews at P10.
- **Files:**
  - WRITE `src/remote_widget.cpp/.ui` (contexts and new shortcuts) and `src/main_window.cpp/.ui` (window shortcuts as `QAction`s added to the main window, plus the tab and Ctrl+W logic).
  - NEW `tests/tst_shortcuts.cpp` (unit, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 10.2 DONE.
  - Contract: the shortcut map in the local design context (revision 27). The tab shortcuts call `ui.tabs->setCurrentIndex(n)` for n 0–4. The RemoteWidget's Close shortcut emits `tabs->tabCloseRequested(tabs->indexOf(this))` instead of calling `removeTab`, so it runs the same path as the tab's × (`slotCloseTab`, `deleteLater`, `removeTab`). Ctrl+F focuses `tasksFilter` or `jobsFilter` for tab 2 or 1, and does nothing otherwise.
  - Steps:
    1. S1 — Collision check (recorded), then assignments.
    2. S2 — Tests; re-record T-MW-SNAPSHOT under supervisor approval (only the action shortcut columns may change; record the diff); full gates.
- **Unit tests:** `tests/tst_shortcuts.cpp`:
  - T-SC-REMOTE-CONTEXT: for a RemoteWidget (as in 10.1) inside a `QTabWidget`, every `QAction` with a non-empty shortcut has `shortcutContext() == Qt::WindowShortcut` and `associatedObjects()` containing the RemoteWidget. The new map entries exist with the exact key sequences. Its `QShortcut` with `QKeySequence::Close`, when activated, emits `tabCloseRequested` with the RemoteWidget's index (QSignalSpy on the QTabWidget).
  - T-SC-UNIQUE: over the union of the RemoteWidget actions and its Close shortcut keys, the Tasks tab actions, the MainWindow window actions and Alt+F and Alt+H, no two entries share a key sequence (`QKeySequence` compare, every sequence of `shortcuts()` counted).
  - T-SC-MAINWINDOW: the MainWindow actions for Ctrl+1–5, Ctrl+F, Ctrl+R and Ctrl+Return exist, with the expected sequences. Triggering the Ctrl+2 action programmatically sets the current index to 1.
  - Gate G-U10.3: `scripts/dev.sh test qt64 -- -R tst_shortcuts` passes with 3 functions.
- **e2e tests:** N/A — shortcuts invoke existing actions whose behavior is covered elsewhere. Dispatch is guarded by the context and uniqueness tests, and key-event delivery under the offscreen platform is not a reliable oracle.
  - Totals after 10.3: G-UNIT 28, G-E2E 20.
- **Done:** G-U10.3 passes; README shortcut table written (P10 obligation); unit closed; completion commit; G-CI dispatched on it.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present. Revision 27: also G-SERVERS, check-triage, the leftover-process check and G-CI on the P10 commit (as at P8 and P9).
- README obligation (P10): "Navigation" (editable path, Internet Archive item entry), "Search" and the full "Keyboard shortcuts" table (the map above plus the existing shortcuts).
- Supervisor agent-1:opus reviews. Close P10.
