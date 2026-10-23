# Phase 2 — Serves in the Jobs tab

Intent: every serve started from Rclone Browser appears in the Jobs tab like a mount, with Copy URL,
Open in browser, Stop/Close and Restart; the separate "Serves" window and File > "Serves…" are gone.
Prerequisites: P1 DONE.
Phase closure: P2; review by agent-1:opus.

## State and ownership contract
Read STATE.md first. In a fresh session reconcile the active unit, actual diff, step checkpoint, scope,
and commit reference before selecting work. In a delegated worker session, follow the coordinator's
assigned OPEN unit; do not take over state/commits or revert active work. Open before edits; checkpoint
after meaningful batches and before yielding. Close after tests/review/coherence, commit when configured
(full-autonomous:true), then continue only within scope. Missing design goes to agent-1:opus.
Restrict code searches to `RcloneBrowser/` (the index also covers worktrees `wt-*`).

## Local design context
Plan revision: 1.
- U1: remove `ServesWindow`, `MainWindow::showServes/servesWindow`, `mServesWindow`, `actionServes`
  (File menu). The "Serve…" dialog (`ServeDialog`) stays modal and unchanged in this phase.
- U2: an ended serve stays in Jobs as finished (Close, "Clean not running"); Restart starts it again
  with the same `ServeParams` (password included) held **only in the widget's memory** (I-2).
- U3 (part): "Open in browser" button for http, webdav, webdav-webui.
- D3: NEW `ServeJobWidget` (`src/serve_job_widget.{h,cpp}`, built in code, no .ui).
- D4: daemon is the source of truth. A widget in state Serving whose id is missing from the newest
  `serve/list` becomes Finished ("Stopped"). Daemon leaving Running ⇒ every Serving/Starting widget
  becomes Finished ("Backend stopped"). A listed id with no widget is adopted as a widget without Restart
  (serves started by another rc client), **only while no start is pending** (`mPendingServes` empty): the
  reply of a pending start may still be on its way, and adopting its id then would duplicate the widget
  (e.g. during Restart, whose widget has an empty id while Starting). ServeManager emits `started`
  before its own `refresh()`, so the list after a start always finds the widget. A later `started` for an
  adopted id attaches the params (Restart enabled).
- D5: `quint64 ServeManager::start(const ServeParams&)` returns a token; signals
  `started(quint64 token, const ServeEntry &entry)`, `startFailed(quint64 token, const QString &message)`;
  `error(const QString&)` still emitted (after `startFailed`) for the message box. Starts are FIFO, one
  in flight (prepares D6 in phase 3).
- D9: `serveBrowserUrl(e)` = `serveUrl(e)` with an unspecified host (`0.0.0.0`, `::`, `[::]`, empty)
  replaced by `127.0.0.1`; only used for Open in browser. `serveOpensInBrowser(type)` is true for
  `http`, `webdav`, `webdav-webui` (`kWebuiFlavor`).
- D12: serves count in `mJobCount`/"Jobs (N)"; sort, filter, Clean not running and the >75 oldest-job
  removal include them; "Stop all jobs" does not stop them; canClose/quitApp finish running serve
  widgets locally (no rc call, the backend quits with the app) and never wait for them.
- I-2 (plan 001 I-5 kept): credentials only in rc POST bodies; `ServeEntry` has no password; nothing logs params.
- I-6: Serving ⇔ id in newest list while daemon Running.
- I-7: every type-switch over Jobs widgets handles ServeJobWidget.

### ServeJobWidget contract
```
class ServeJobWidget : public QWidget {
  Q_OBJECT
public:
  enum class State { Starting, Serving, Finished, Error };
  // displayType: "webdav-webui" for that flavor, else entry.type. Empty params.type = adopted (no Restart).
  ServeJobWidget(const ServeEntry &entry, const QString &displayType,
                 const ServeParams &params, QWidget *parent = nullptr);
  bool isRunning = true;              // State Starting or Serving (MountWidget convention)
  State state() const;
  QDateTime getStartDateTime();
  QString getStatus();                // "0_zserve_starting" | "0_zserve_serving" | "1_zserve_finished" | "1_zserve_error"
  QStringList filterFields() const;   // {displayType, entry.fs, serveUrl(entry)}
  QString serveId() const;            // "" while Starting after a restart
  ServeParams params() const;
  bool canRestart() const;            // !params.type.isEmpty()
  void adoptParams(const ServeParams &p, const QString &displayType);
  void markStarting();                // Finished/Error → Starting; id cleared; isRunning = true
  void markRunning(const ServeEntry &e); // Starting/Serving → Serving with e (new id/addr, start time now)
  void markFinished(const QString &reason); // Starting/Serving → Finished, emits finished() once; else no-op
  void markStartFailed(const QString &message); // Starting → Error, emits finished(); else no-op
public slots:
  void cancel();                      // Serving: emit stopRequested(id), disable, text "  Stopping"; Finished/Error: emit closed()
signals:
  void stopRequested(const QString &id);
  void restartRequested();
  void openUrlRequested(const QUrl &url);
  void finished();
  void closed();
};
```
Layout (object names are test anchors): top row `QToolButton showDetails` (checkable, flat; text and
colour like MountWidget: "  Serving"/"  Starting" green bold, "  Finished" black (white style when
`Settings/iconsColour` = white), "  Error" red), `QLineEdit info` read-only = `"<displayType> <fs>"`,
`QToolButton copyUrl` (icon `qbutton_icons/copy`), `QToolButton openBrowser` (icon `qbutton_icons/link`,
hidden unless `serveOpensInBrowser(displayType)`), `QToolButton restart` (icon used by `JobWidget`'s
`ui.restart`; visible only when `canRestart()`, enabled only when Finished/Error), `QToolButton cancel`
(icon `qbutton_icons/cancel`; tooltip "Stop" while Serving, "Close" when Finished/Error, disabled while
Starting/Stopping). Hidden details panel (`QWidget details`, toggled by `showDetails`): form rows Type,
Source, `QLineEdit url` (read-only, `serveUrl`), Address (`entry.addr`), `QLineEdit le_StartFinishInfo`
(same format as MountWidget::updateStartFinishInfo). `QLabel message` (PlainText, word-wrap, hidden when
empty) shows the reason/error. Icons use the `_inv` suffix when `Settings/iconsColour` = white, 24×24.
copyUrl puts `serveUrl(entry)` on `QGuiApplication::clipboard()`; openBrowser emits
`openUrlRequested(QUrl(serveBrowserUrl(entry)))`.

## Sub-phases

### 2.1 ServeManager tokens, FIFO and browser URL helpers
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews the diff (signal order, FIFO, I-2).
- **Files:** WRITE `src/serve_manager.h`, `src/serve_manager.cpp`, `tests/tst_serve_manager.cpp`,
  `src/main_window.cpp` (only the `started` connection in `serveManager()`, to keep the build green:
  connect the new `started(quint64, ServeEntry)` to the existing `showServes()` via a lambda ignoring
  the args; removed in 2.3).
- **Change:**
  Preconditions: P1 DONE.
  Contract: D5, D9. Declare `extern const QLatin1String kWebuiFlavor;` in serve_manager.h, defined in
  serve_manager.cpp as `const QLatin1String kWebuiFlavor("webdav-webui");`.
  Steps:
  1. S1 — header: `quint64 start(const ServeParams &p);` signals `started(quint64 token, const ServeEntry &entry)`,
     `startFailed(quint64 token, const QString &message)`; private `struct PendingStart { quint64 token; ServeParams params; };`
     `QQueue<PendingStart> mStartQueue; bool mStartInFlight = false; quint64 mNextToken = 0;`
     `void processStartQueue();`. Free functions `QString serveBrowserUrl(const ServeEntry &e);`
     `bool serveOpensInBrowser(const QString &displayType);` with doc comments.
  2. S2 — cpp: `start` = `token = ++mNextToken; mStartQueue.enqueue({token, p}); processStartQueue(); return token;`.
     `processStartQueue`: return if in flight or queue empty; dequeue; in flight = true; `mCall("serve/start",
     serveStartParams(params), cb, kServeTimeoutMs)`. Callback (with `QPointer self`): ok with id+addr ⇒
     `emit started(token, e)`; ok but missing id/addr ⇒ `emit startFailed(token, "invalid serve/start reply")`
     then `emit error(same)`; not ok ⇒ `emit startFailed(token, failureText)` then `emit error(failureText)`.
     Then (if self) `mStartInFlight = false; refresh(); processStartQueue();`.
     `serveBrowserUrl`: if `!serveOpensInBrowser(e.type)` return `serveUrl(e)`; host = part before the last
     `:` (strip `[]`); if host ∈ {"", "0.0.0.0", "::"} ⇒ `"http://127.0.0.1:" + port + "/"`, else `serveUrl(e)`.
     `serveOpensInBrowser(t)` = t ∈ {"http","webdav", kWebuiFlavor}.
  3. S3 — tests: adapt `T_SRV_FLOW` to the new signals (spy `started` args at(0) token, at(1) entry);
     add `T_SRV_TOKENS`: two `start()` calls in a row ⇒ only one `serve/start` recorded before the event
     loop runs the first reply (`QCOMPARE(fake.paths().count("serve/start"), 1)` right after the calls),
     after `QTRY` both started, tokens 1 and 2 in order, call order `serve/start, serve/list, serve/start, serve/list`
     (refresh after each); a failing first start (`script["serve/start"] = errReply(500,"boom")`) emits
     `startFailed(1,"boom")` **before** `error("boom")` (record order with a shared QStringList in two lambdas)
     and the second start still runs. `T_SRV_BROWSER_URL` data rows: http `127.0.0.1:40855` ⇒ same as serveUrl;
     webdav `[::]:4321` ⇒ `http://127.0.0.1:4321/`; http `0.0.0.0:8080` ⇒ `http://127.0.0.1:8080/`;
     http `:8080` ⇒ `http://127.0.0.1:8080/`; sftp `[::]:22` ⇒ `sftp://[::]:22/` (unchanged);
     `serveOpensInBrowser`: http/webdav/webdav-webui true, ftp/sftp/nfs/dlna/s3/restic false.
  Checkpoint: after S2 and S3.
  Failure handling: none expected; signal-order doubts → supervisor.
- **Unit tests:** T-SRV-FLOW (updated), T-SRV-TOKENS, T-SRV-BROWSER-URL; existing T-SRV-PARAMS/LIST/URL/RULES
  unchanged. Gate G-U21: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_serve_manager$'` (1 test, 0 failed).
- **e2e tests:** existing `tst_e2e_serve` must still pass (G-U21e: `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh e2e qt64 -- -R '^tst_e2e_serve$'`);
  adapt only its use of `started` (signature) if it spies on it.
- **Done:** tests green; review recorded; unit closed; commit.

### 2.2 ServeJobWidget
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews after diff (state machine, finished() once, I-2).
- **Files:** READ `src/mount_widget.{h,cpp,ui}`, `src/job_widget.cpp` (restart icon name).
  NEW `src/serve_job_widget.h`, `src/serve_job_widget.cpp` (register in `src/CMakeLists.txt` header and
  source lists next to `serve_widget.*`), `tests/tst_serve_job_widget.cpp` (`rb_add_test(tst_serve_job_widget unit)`
  after `tst_serve_widget`).
- **Change:**
  Preconditions: 2.1 DONE.
  Contract: "ServeJobWidget contract" above, verbatim names. Transitions: Serving→Finished (markFinished),
  Starting→Serving (markRunning), Starting→Error (markStartFailed), Finished/Error→Starting (markStarting,
  only if canRestart), Serving→Serving (markRunning with same id: update addr only, no signal).
  `finished()` exactly once per Starting/Serving → Finished/Error transition. `isRunning` true in Starting/Serving.
  Steps:
  1. S1 — header/class skeleton + layout as specified; `updateStartFinishInfo` like MountWidget.
  2. S2 — state methods and buttons wiring; `cancel()` semantics; restart button visibility/enabled.
  3. S3 — tests `T_SJW_STATE`, `T_SJW_RESTART`, `T_SJW_ADOPTED`, `T_SJW_BUTTONS` (below).
  Checkpoint: after S2, S3.
  Failure handling: clipboard unavailable offscreen ⇒ QSKIP not allowed; use `QGuiApplication::clipboard()`
  (works in-process offscreen as in the former T_SRV_WINDOW) — if it fails, escalate.
- **Unit tests** (`tests/tst_serve_job_widget.cpp`, entry `{id "http-1", addr "127.0.0.1:4000", type "http", fs "loc:/w"}`, params type http):
  - T-SJW-STATE: initial `state()==Serving`, `isRunning`, `getStatus()=="0_zserve_serving"`, `url` text
    `http://127.0.0.1:4000/`, `info` text `http loc:/w`; click `cancel` ⇒ `stopRequested("http-1")` once and
    cancel disabled; `markFinished("Stopped")` ⇒ `finished` count 1, `getStatus()=="1_zserve_finished"`,
    `!isRunning`, cancel enabled with toolTip "Close", `message` text "Stopped"; second `markFinished` ⇒
    count still 1; click cancel ⇒ `closed` once.
  - T-SJW-RESTART: after finished, `restart` visible+enabled; click ⇒ `restartRequested` once;
    `markStarting()` ⇒ `getStatus()=="0_zserve_starting"`, `serveId()==""`, restart disabled, cancel disabled;
    `markRunning({id "http-2", addr "127.0.0.1:4001", …})` ⇒ Serving, url `http://127.0.0.1:4001/`,
    `serveId()=="http-2"`; `markFinished`, `markStarting`, `markStartFailed("port in use")` ⇒ finished
    emitted (total 3 over the test), status `1_zserve_error`, `message` "port in use" with `textFormat()==Qt::PlainText`.
  - T-SJW-ADOPTED: empty params ⇒ `restart` hidden; `markStarting()` is a no-op (state stays); after
    `adoptParams(p, "http")` ⇒ restart visible.
  - T-SJW-BUTTONS: `openBrowser` visible for displayType http/webdav/webdav-webui, hidden for sftp;
    entry webdav `[::]:4321` click ⇒ `openUrlRequested(QUrl("http://127.0.0.1:4321/"))`; `copyUrl` click ⇒
    clipboard text `http://[::]:4321/`; `filterFields()` == {"webdav", fs, "http://[::]:4321/"}.
  Gate G-U22: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_serve_job_widget$'` (1 test, 0 failed; 4 functions PASS).
- **e2e tests:** N/A — integration covered by 2.3.
- **Done:** class + tests green; review recorded; unit closed; commit.

### 2.3 MainWindow integration; remove the Serves window
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews the site list before S4 and the diff after (I-6, I-7, D12).
- **Files:** WRITE `src/main_window.h`, `src/main_window.cpp`, `src/main_window_tasks.cpp`,
  `src/main_window.ui` (remove `actionServes` action and its `<addaction>` in the File menu, plus the
  separator that followed it ≈L947–948), `src/serve_widget.h`, `src/serve_widget.cpp` (delete
  `ServesWindow` only), `tests/tst_serve_widget.cpp` (delete `T_SRV_WINDOW` and helpers it alone uses),
  `tests/fixtures/synthetic/mainwindow_snapshot.txt` (only line `A|actionServes|Serves…|1|0|0|`),
  `tests/CMakeLists.txt`. NEW `tests/tst_e2e_serve_jobs.cpp` (`rb_add_test(tst_e2e_serve_jobs e2e)` in
  the `RB_TEST_RCLONE AND UNIX AND NOT APPLE` block after `tst_e2e_serve`).
- **Change:**
  Preconditions: 2.1, 2.2 DONE.
  Contract: D4, D5, D9, D12, U1, U2 as above.
  Steps:
  1. S1 — Remove: `ServesWindow` (class, includes it alone needs), `MainWindow::showServes`, `servesWindow`,
     `mServesWindow`, the `actionServes` connect (≈L881) and the action/menu entry in the .ui. Literal
     search `ServesWindow`, `showServes`, `actionServes` in `RcloneBrowser/src` and `RcloneBrowser/tests`
     ⇒ 0 hits except the snapshot line, which is removed in S6.
  2. S2 — `main_window.h`: forward-declare `class ServeJobWidget;` and `#include "serve_manager.h"`
     (PendingServe holds a ServeParams by value). Public slot
     `void startServe(const ServeParams &p);`. Private: `struct PendingServe { ServeParams params; QPointer<ServeJobWidget> widget; };`
     `QHash<quint64, PendingServe> mPendingServes;` `ServeJobWidget *addServeJob(const ServeEntry &e, const QString &displayType, const ServeParams &p);`
     `ServeJobWidget *serveJobById(const QString &id) const;` `QList<ServeJobWidget *> serveJobs() const;`
     `void onServeStarted(quint64, const ServeEntry &); void onServeStartFailed(quint64, const QString &);`
     `void onServeListChanged(const QVector<ServeEntry> &);` `static QString serveDisplayType(const ServeParams &p);`
     (returns `p.flavor` when non-empty else `p.type`; `flavor` does not exist yet ⇒ in this phase return `p.type`;
     3.1 extends it).
  3. S3 — `main_window.cpp`:
     - `openServeDialog`: on Accepted call `startServe(dlg.params())`.
     - `startServe`: not `daemonRunning()` ⇒ `statusBar()->showMessage(tr("Serve requires the rclone backend"))`; else
       `const quint64 t = serveManager()->start(p); mPendingServes.insert(t, {p, nullptr});`.
     - `serveManager()`: replace the `started→showServes` connection by `started→onServeStarted`,
       add `startFailed→onServeStartFailed`, `listChanged→onServeListChanged`; inside the existing
       `global.daemon stateChanged` lambda add (capture `this`): if state != Running, for every
       `serveJobs()` widget with `isRunning`: `markFinished(tr("Backend stopped"))` (Starting widgets too:
       markFinished handles Starting). Keep `manager->refresh()`/`loadTypes()` there.
     - `onServeStarted(t, e)`: `PendingServe ps = mPendingServes.take(t);` if `ps.widget` ⇒ `ps.widget->markRunning(e)`;
       else if `serveJobById(e.id)` ⇒ that widget `adoptParams(ps.params, serveDisplayType(ps.params))`
       (and `markRunning(e)`); else `addServeJob(e, serveDisplayType(ps.params), ps.params)`.
       `statusBar()->showMessage(tr("Serve started: %1").arg(serveUrl(e)), 10000)`.
     - `onServeStartFailed(t, msg)`: `PendingServe ps = mPendingServes.take(t);` if `ps.widget` ⇒ `markStartFailed(msg)`.
     - `onServeListChanged(list)`: ids = set of `e.id`; every widget with `state()==Serving` and id ∉ ids ⇒
       `markFinished(tr("Stopped"))`; if `mPendingServes.isEmpty()`, every `e` whose id has no widget ⇒ `addServeJob(e, e.type, ServeParams())` (D4 guard).
     - `addServeJob`: mirror `addNewMount` (noJobsAvailable hide, `ui.tabs->setTabText(1, …++mJobCount)`,
       HLine, `insertWidget(0/1)`, `applyJobsFilter()`, sort buttons) and connect: `finished` ⇒ decrement
       `mJobCount`, tab text, `buttonStopAllJobs`/`buttonCleanNotRunning` enable, `sortJobs()` (as the
       Stream finished handler); `closed` ⇒ as the Stream closed handler; `stopRequested(id)` ⇒
       `serveManager()->stop(id)`; `restartRequested` ⇒ if `!daemonRunning()` status bar message, else
       `w->markStarting(); ui.tabs->setTabText(1, …++mJobCount); mPendingServes.insert(serveManager()->start(w->params()), {w->params(), w});`
       and update the two buttons; `openUrlRequested(url)` ⇒ `QDesktopServices::openUrl(url)`. End with
       button updates and `sortJobs()` like addNewMount.
  4. S4 — Jobs type switches (I-7). Add a `qobject_cast<ServeJobWidget *>` branch next to each
     `qobject_cast<StreamWidget *>` branch (8 sites; locate by literal search, lines are hints):
     a. `setupJobsTab` Clean not running (≈L1003): `if (!serve->isRunning) emit serve->closed();`
     b. `quitApp` (≈L1454): `if (serve->isRunning) serve->markFinished(tr("Rclone Browser quit")); emit serve->closed();` — never sets `processActive`.
     c. `canClose` (≈L2419): `if (serve->isRunning) serve->markFinished(tr("Rclone Browser quit")); else emit serve->closed();`
     d. `sortJobs` compare (≈L3163): `widgetStartDateTime = serve->getStartDateTime(); widgetStatus = serve->getStatus();`
     e. `sortJobs` reinsertion (≈L3224): remove/insert pair like the Stream branch.
     f. `applyJobsFilter` (≈L3253): `fields = serve->filterFields();`
     g. `main_window_tasks.cpp` oldest-job search (≈L2045): `if (!serve->isRunning) { widgetStartDateTime = serve->getStartDateTime(); foundOldest = true; }`
     h. `main_window_tasks.cpp` oldest-job removal (≈L2067): `emit serve->closed();`
     Then verify: literal-search counts of `qobject_cast<StreamWidget` and `qobject_cast<ServeJobWidget`
     in `RcloneBrowser/src` are equal (8). Read the other `ui.jobs` loops (main_window.cpp ≈L1058, ≈L2618,
     ≈L2770; main_window_tasks.cpp ≈L118, 299, 340, 404, 474, 857, 1229, 1802, 2139, 2250, 2534): they match
     JobWidget/MountWidget by task uniqueId; serves have none ⇒ no change; record the list in the checkpoint.
  5. S5 — NEW `tests/tst_e2e_serve_jobs.cpp` (T-E2E-SERVE-JOBS, below). Pattern: `tst_e2e_serve.cpp`
     (temp config with `[loc] type = local`, `SetRcloneConf`) + `tst_e2e_filter.cpp` (MainWindow, Jobs tab,
     `jobsLayout`, private HOME). Set `SetRclone(qgetenv("RB_TEST_RCLONE"))` and `Settings/rclone` to the same
     path before constructing `MainWindow`; `QTRY_VERIFY_WITH_TIMEOUT(global.daemon && global.daemon->state() == RcloneDaemon::State::Running, 20000)`.
  6. S6 — snapshot: run `tst_mainwindow_snapshot`; diff `mainwindow_snapshot.actual.txt` (next to the
     test binary in `build-docker-qt64/tests/`) against the fixture: the only difference must be the
     removed `A|actionServes…` line. Then delete that line from the fixture. Any other difference ⇒ stop,
     supervisor.
  Checkpoint: after S1, S3, S4 (site list), S5, S6.
  Failure handling: a modal box from `error()` in the e2e test would block — tests use only successful
  starts or set the expected failure path via a port already in use **only** in a unit test; if a box
  appears, escalate (do not add test-only switches to production code without supervisor approval).
- **Unit tests:** `tst_serve_widget` (without T_SRV_WINDOW), `tst_mainwindow_snapshot` (fixture updated),
  `tst_shortcuts`, `tst_startup` — gate G-U23: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64` all pass.
- **e2e tests:** T-E2E-SERVE-JOBS in `tests/tst_e2e_serve_jobs.cpp`:
  1. `w.startServe({type "http", fs "loc:<tmp>/w", addr "127.0.0.1:0", readOnly true})` ⇒ `QTRY` one
     `ServeJobWidget` child; tab text `Jobs (1)`; its `url` text starts `http://127.0.0.1:` and a
     `QNetworkAccessManager` GET on it returns 200 within 10 s.
  2. A second serve (type webdav) ⇒ 2 widgets, `Jobs (2)`; `jobsFilter` text `webdav` hides the http
     widget and its line (`hiddenOnPurpose` helper pattern of tst_e2e_filter), clearing shows both.
  3. Click the http widget's `cancel` ⇒ `QTRY` its `state()==Finished`, `Jobs (1)`, TCP connect to its
     port refused (helper `connectionRefused` from tst_e2e_serve).
  4. Click its `restart` ⇒ `QTRY` `state()==Serving`, `serveId()` differs from the first id, `Jobs (2)`, GET 200.
  5. Click the sort-by-status button (`buttonSortByStatus`) and sort-by-time: both serve widgets are
     still in `ui.jobs` (count of ServeJobWidget children whose parent layout is the jobs layout == 2).
  6. Stop the http serve again (`QTRY` Finished), then
     `QTimer::singleShot(0, [] { if (auto *b = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) b->button(QMessageBox::Yes)->click(); });`
     and trigger `w.findChild<QAction *>("actionCleanNotRunning")` ⇒ `QTRY` exactly 1 ServeJobWidget left.
  7. `global.daemon->stop()` ⇒ `QTRY` the remaining widget `state()==Finished`, `message` "Backend stopped", tab text `Jobs`.
  8. File menu has no action named `actionServes` (`w.findChild<QAction*>("actionServes") == nullptr`).
  Gate G-U23e: `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh e2e qt64 -- -R '^tst_e2e_serve'` — 2 tests (tst_e2e_serve, tst_e2e_serve_jobs), 0 failed.
- **Done:** behaviour as specified; site-count check recorded; all G-U23 gates pass; supervisor review
  recorded (I-6, I-7, D12, I-2); unit closed; commit.

### 2.4 Update README.md
- **Model:** agent-3:haiku
- **Assignment:** documentation; supervisor checks at P2.
- **Files:** `README.md`: the 2.1.0 "New in" bullet (mentions the "Serves" window of the File menu),
  `#faq-serve` (the paragraph describing OK → "Serves" window, File > "Serves…", Copy URL/Stop/Refresh,
  the "Closing the window…" and "When the backend is not running…" sentences, and the "A serve is not a
  task…" paragraph that says serves are "not listed in the Jobs tab"), the "Tasks, queue and jobs" section
  if it enumerates job kinds, and the `#backend` sentence "Serves started with Serve… run inside it".
- **Change:**
  Preconditions: 2.3 DONE.
  S1 — Replace the Serves-window description with the Jobs behaviour exactly as shipped: after OK the
  serve appears in Jobs ("Serving"), with Copy URL, Open in browser (http and webdav), Stop (then Close),
  Restart (same settings; not available for a serve the app did not start in this session), the details
  arrow (type, source, URL, address, started/finished), the status bar message "Serve started: <URL>";
  when rclone refuses, the message box as before. Serves count in "Jobs (N)", follow the Jobs filter, sort
  and "Clean not running"; "Stop all" does not stop them; quitting ends them; a backend restart/stop marks
  them finished ("Backend stopped") and Restart starts them again. Keep "A serve is not a task" but drop
  "not listed in the Jobs tab". Remove every mention of the Serves window/File > Serves….
  S2 — `grep -n "Serves" README.md` ⇒ no reference to a "Serves" window or menu item remains.
  Checkpoint: record sections touched.
- **Unit tests:** N/A — docs.
- **e2e tests:** N/A — docs; S2 grep recorded.
- **Done:** sections accurate; unit closed; commit.

## Phase gates and closure
- Every gate command of this phase runs with `export RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone` (STATE.md §3):
  a `dev.sh build` without it resets the CMake cache and drops the webui e2e test.
- Required gates: G-BUILD64, G-UNIT, G-E2E.
- README obligation 2.4 DONE.
- agent-1:opus reviews: I-6, I-7 (site count 8/8), D12 counters (Jobs (N) after stop/restart/clean),
  I-2 (no password outside the widget memory/POST body; nothing logged), removal completeness.
- Open P2, record review, mark DONE, commit closure.
