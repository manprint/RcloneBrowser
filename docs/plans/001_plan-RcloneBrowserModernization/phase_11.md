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
Plan revision: 1.
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

  - `src/remote_widget.cpp`: `ui.path->setText(...)` on selection change; `model->addRoot("/", root)` for non-Windows remotes.
  - `src/main_window.ui`: `tasksListWidget`, `queueListWidget` (QListWidget); `jobs` (a QVBoxLayout of JobWidget/MountWidget/StreamWidget separated by HLines); tabs 0 Remotes, 1 Jobs, 2 Tasks, 3 Queue, then remote tabs; `preferences` Alt+P; `quit` Ctrl+Q; `actionDelete` Delete.
  - `MainWindow::listTasks` fills `tasksListWidget`; `sortJobs` reorders `jobs`.
- Shortcut map. Every action created in a RemoteWidget gets `setShortcutContext(Qt::WidgetWithChildrenShortcut)` and is added to the RemoteWidget itself (`addAction`), so a shortcut fires only for the focused view. Existing shortcuts are unchanged.

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
  | MainWindow (window) | Remotes, Jobs, Tasks and Queue tabs | Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4 |
  | MainWindow (window) | focus the search field of the current tab (Tasks or Jobs) | Ctrl+F |
  | MainWindow (window) | refresh the remote list (`ui.refresh`) | Ctrl+R |
  | MainWindow, Tasks tab | `actionRun` | Ctrl+Return |
  | MainWindow, Tasks tab | `actionEdit` | Ctrl+E |
  | MainWindow, Tasks tab | `actionDryRun` | Ctrl+Shift+Return |
  | MainWindow (window) | close the current remote tab | Ctrl+W (only when the current tab index ≥ 4) |

- Before assigning, the implementer checks each new shortcut against all existing ones (a `.ui` grep plus code `setShortcut`). If a collision is found, escalate instead of choosing another key.

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
    QString normalizeEnteredPath(const QString &text, bool isLocal);  // trim; strip a leading "<remote>:" prefix if it equals the current remote (passed separately below); local: QDir::fromNativeSeparators + cleanPath; remote: collapse "//" and strip trailing '/' except root "/"; "" stays ""
    QString stripRemotePrefix(const QString &text, const QString &remote);  // "r:a/b" with remote "r" → "a/b"; other prefixes untouched
    bool remoteTypeNeedsPathEntry(const QString &remoteType);       // true for "internetarchive"
    ```
    - `ui.path`:
      - readOnly false; `setPlaceholderText("Type a path and press Enter")`;
      - selection changes update its text only while the field has no focus;
      - Esc restores the last selection's path.
    - Enter or Go with normalized text `p`:
      - empty: no-op;
      - otherwise, if a top-level root with path `p` exists, select it; else `QModelIndex idx = model->addRoot(p, p)`, select it and expand it, which triggers load;
      - a listing failure shows the existing Special `⚠` row under that root (P2/P5 behavior).
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
  - T-PATH-NORMALIZE:
    - `"  a//b/ "` with remote gives `a/b`;
    - `"/"` gives `/`;
    - `""` gives `""`;
    - local `/tmp/x/../y` gives `/tmp/y`.
  - T-PATH-PREFIX: `("r:a/b", "r")` gives `a/b`; `("s:a", "r")` is unchanged; `("r:", "r")` gives `""`.
  - T-PATH-ROOTLESS: `internetarchive` is true; `drive` and `local` are false.
  - Gate G-U10.1: `scripts/dev.sh test qt64 -- -R tst_path_entry` passes with 3 functions.
- **e2e tests:** `tests/tst_e2e_path_entry.cpp`. A RemoteWidget is constructed as in `MainWindow` (`new RemoteWidget(&icons, "loc", "local", nullptr)`) with conf `[loc] type = local` and `SetRclone(rbRclone())`.
  - T-E2E-PATH-GO: set `path` text to `<tmp>/sub` and click `goPath`. Within 20 s the model has a top-level root whose path is `<tmp>/sub`, its children include `inner.txt`, and it is the current index.
  - T-E2E-PATH-DUP: Go on the same path twice leaves a single root for that path.
  - T-E2E-PATH-MISSING: Go on `<tmp>/nope` gives a root whose only child is the Special row `⚠ …directory not found`.
  - Gate G-U10.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_path_entry` passes with 3 functions.
- **Done:** gates pass; unit closed; completion commit.

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
- **Done:** gates pass; snapshot change recorded; unit closed; completion commit.

### 10.3 Keyboard shortcuts (#94)
- **Model:** agent-3:haiku
- **Assignment:** implementer. agent-1:opus reviews at P10.
- **Files:**
  - WRITE `src/remote_widget.cpp/.ui` (contexts and new shortcuts) and `src/main_window.cpp/.ui` (window shortcuts as `QAction`s added to the main window, plus the tab and Ctrl+W logic).
  - NEW `tests/tst_shortcuts.cpp` (unit, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 10.2 DONE.
  - Contract: the shortcut map in the local design context. The tab shortcuts call `ui.tabs->setCurrentIndex(n)`. Ctrl+W calls the existing tab-close path for index ≥ 4. Ctrl+F focuses `tasksFilter` or `jobsFilter` for tab 2 or 1, and does nothing otherwise.
  - Steps:
    1. S1 — Collision check (recorded), then assignments.
    2. S2 — Tests; re-record T-MW-SNAPSHOT under supervisor approval (only the action shortcut columns may change; record the diff); full gates.
- **Unit tests:** `tests/tst_shortcuts.cpp`:
  - T-SC-REMOTE-CONTEXT: for a RemoteWidget (as in 10.1), every `QAction` with a non-empty shortcut has `shortcutContext() == Qt::WidgetWithChildrenShortcut` and `associatedObjects()` containing the RemoteWidget. The new map entries exist with the exact key sequences.
  - T-SC-UNIQUE: within one RemoteWidget, and within the MainWindow's window-context actions, no two actions share a key sequence.
  - T-SC-MAINWINDOW: the MainWindow actions for Ctrl+1–4, Ctrl+F, Ctrl+R, Ctrl+W and Ctrl+Return exist, with the expected sequences. Triggering the Ctrl+2 action programmatically sets the current index to 1.
  - Gate G-U10.3: `scripts/dev.sh test qt64 -- -R tst_shortcuts` passes with 3 functions.
- **e2e tests:** N/A — shortcuts invoke existing actions whose behavior is covered elsewhere. Dispatch is guarded by the context and uniqueness tests, and key-event delivery under the offscreen platform is not a reliable oracle.
- **Done:** G-U10.3 passes; README shortcut table written (P10 obligation); unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation (P10): "Navigation" (editable path, Internet Archive item entry), "Search" and the full "Keyboard shortcuts" table (the map above plus the existing shortcuts).
- Supervisor agent-1:opus reviews. Close P10.
