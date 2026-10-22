# Phase 9 — Serve (#129)

Intent: a folder of any remote can be served over http, webdav, ftp, sftp, dlna, nfs, restic or s3 through the app's rcd (`serve/start`). A modeless "Serves" window lists the running serves with their URL, Copy URL and Stop.
Prerequisites: P14 (revision 11; P14 follows P7).
Phase closure: P9; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); tests serve on 127.0.0.1 only.

## Local design context
Plan revision: 1; 9.1 amended by revision 23 (see "Revision 23" at the end of 9.1).
- R15 (local probe, rclone 1.75.2 rcd, 2026-10-21):
  - `serve/types` gives `{types:[dlna, ftp, http, nfs, restic, s3, sftp, webdav]}`;
  - `serve/start {type, fs, addr, [user, pass, read_only]}` gives `{addr:"127.0.0.1:40855", id:"http-8ae93585"}`;
  - `serve/list` gives `{list:[{id, addr, params:{…}}]}`;
  - `serve/stop {id}` and `serve/stopall` give `{}`;
  - an unknown type gives HTTP 500 `could not find serve type="nosuch"`; unknown params give `unknown parameters: <key>`;
  - with `read_only:true`, a webdav PUT fails (404) and no file is created;
  - an http GET of `a%20b.txt` returns the content.
- R15 additions (local probe, rclone 1.75.2 rcd in an isolated container, 2026-10-22, revision 23):
  - restic rejects `read_only` (`unknown parameters: read_only`) and starts without it;
  - nfs and dlna reject `user`/`pass` (`unknown parameters: pass, user`) and start with `read_only:true`;
  - ftp with `addr 127.0.0.1:0` listens on 2121 but reports `127.0.0.1:0` (a second one fails `bind: address already in use`); an explicit port starts and is reported; ftp without user/pass starts;
  - sftp without user and pass fails `… no authorization found, use --user/--pass or --authorized-keys or --no-auth or --auth-proxy`; it writes host keys under `$HOME/.cache/rclone/serve-sftp/`;
  - s3 starts with user/pass and with `read_only`;
  - `serve/list` echoes `params` including `user` and `pass` in clear text.
- Lifetime decision (supervisor, revision 1): serves live inside the app's rcd, so they stop when the app quits (`core/quit`). They are not tasks, are not persisted and do not appear in the Jobs tab, so the jobs bookkeeping and sorting are untouched. They appear in a modeless "Serves" window opened from the main window's File menu (revision 23: no toolbar button). This is the documented behavior.
- Serve credentials, when the user sets them, are protocol credentials for clients (not rc credentials). They are passed as rc params in the POST body, never in argv. I-5 is unaffected.
- `global.daemon` (5.2) must be Running; otherwise the actions are disabled with the tooltip "Requires the rclone backend".
- Current code (anchors): `remote_widget.ui` actions (`mount`, `stream`, …) and the tree context menu; `MainWindow` toolbar/menu actions (`actionStopAllTransfers` etc.).

### Contract (NEW `src/serve_manager.h/.cpp`, `src/serve_widget.h/.cpp`)
```cpp
struct ServeParams { QString type, fs, addr = "127.0.0.1:0", user, pass; bool readOnly = false; };
QJsonObject serveStartParams(const ServeParams &p);   // {type, fs, addr} + user/pass if non-empty + read_only:true if readOnly
struct ServeEntry { QString id, addr, type, fs; };
QVector<ServeEntry> parseServeList(const QJsonObject &reply);  // type/fs from params; entries without id skipped
QString serveUrl(const ServeEntry &e);                // http/webdav/restic/s3: "http://<addr>/"; ftp: "ftp://<addr>/"; sftp: "sftp://<addr>/"; nfs: "nfs://<addr>/"; dlna: "http://<addr>/"
class ServeManager : public QObject {
  Q_OBJECT
public:
  explicit ServeManager(RcCallFn call, QObject *parent = nullptr);   // RcCallFn from 6.1
  void start(const ServeParams &p);        // emits started(entry) or error(msg), then refresh()
  void stop(const QString &id);            // emits error on failure, then refresh()
  void refresh();                          // serve/list → listChanged
  void loadTypes();                        // serve/types → typesChanged
signals:
  void started(const ServeEntry &entry);
  void listChanged(const QVector<ServeEntry> &entries);
  void typesChanged(const QStringList &types);
  void error(const QString &message);
};
```
- `ServeDialog` (in `serve_widget.cpp`) has:
  - `QComboBox type` (from `typesChanged`; default `http`);
  - `QLineEdit addr` (default `127.0.0.1:0`, with the hint "use 0.0.0.0:PORT to expose on the network");
  - `QLineEdit user` and `QLineEdit pass` (password echo);
  - `QCheckBox readOnly` (checked by default).
- `ServesWindow` (modeless, single instance owned by MainWindow) has:
  - a `QTableWidget serves` with columns Type, Source, URL;
  - buttons `copyUrl`, `stop`, `refresh`;
  - it refreshes on show and after start/stop.
- RemoteWidget action `actionServe` ("Serve…") is enabled for a selected folder or the root when the daemon is Running.
  - It opens ServeDialog with `fs = <remote>:<path>`; OK calls `ServeManager::start`.
  - On `started`, ServesWindow is shown and raised. On `error`, a `QMessageBox::warning` shows it.
- MainWindow action `actionServes` ("Serves…") shows the window. Quitting needs no extra code, because `RcloneDaemon::stop` ends the serves.

## Sub-phases

### 9.1 ServeManager, dialogs, actions
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P9.
- **Files:**
  - NEW `src/serve_manager.h/.cpp` and `src/serve_widget.h/.cpp` (`Q_OBJECT` → `MOC`), registered.
  - WRITE `src/remote_widget.h/.cpp/.ui` (`actionServe`) and `src/main_window.h/.cpp/.ui` (`actionServes`, the ServesWindow member, wiring to `global.daemon`).
  - NEW `tests/tst_serve_manager.cpp` (unit, fake `RcCallFn`) and `tests/tst_e2e_serve.cpp` (e2e, real rcd).
- **Change:**
  - Preconditions: P7 DONE.
  - Steps:
    1. S1 — Pure functions and ServeManager, plus unit tests.
    2. S2 — Dialogs and actions. Expected: it builds; smoke passes; T-MW-SNAPSHOT is re-recorded under supervisor approval because a new action is added to the main window. Record the approval in STATE.md §8 as an expected fixture change listing the added lines only.
    3. S3 — e2e; full gates.
  - Failure handling: if `serve/start` for a given type is unavailable on a platform (for example nfs on Windows), the error from rclone is shown as is. There is no type filtering in the app.
- **Unit tests:** `tests/tst_serve_manager.cpp`:
  - T-SRV-PARAMS: `{http, "r:a", "127.0.0.1:0", "", "", false}` gives exactly `{type, fs, addr}`. With user `u`, pass `p` and readOnly true, it adds `user`, `pass` and `read_only:true`.
  - T-SRV-LIST: the R15 list JSON gives 2 entries with id, addr, type and fs. An entry without id is skipped.
  - T-SRV-URL: the per-type URLs per the contract.
  - T-SRV-FLOW: with a fake, `start()` sends `serve/start` then `serve/list`, and emits `started` then `listChanged`. An HTTP 500 reply emits `error` with the body error.
  - T-SRV-RULES (revision 23): `serveTypeRules` per type as in the Revision 23 note.
  - Gate G-U9.1: `scripts/dev.sh test qt64 -- -R tst_serve_manager` passes with 5 functions (revision 23).
- **e2e tests:** `tests/tst_e2e_serve.cpp`. Real `RcloneDaemon`, conf `[loc] type = local`, tree `w/a b.txt` = `hi`.
  - T-E2E-SERVE-HTTP: start http on `loc:<tmp>/w`. Within 5 s, `started` gives an addr. A QNAM GET of `serveUrl(e) + "a%20b.txt"` returns `hi`. `stop(id)` gives `listChanged` with 0 entries, and a GET then fails (connection refused) within 5 s.
  - T-E2E-SERVE-RO: webdav with readOnly. A PUT of `new.txt` fails, and `<tmp>/w/new.txt` does not exist.
  - T-E2E-SERVE-BADTYPE: type `nosuch` gives `error` containing `could not find serve type`.
  - T-E2E-SERVE-QUIT: start http, then `daemon.stop()`. The addr refuses connections within 5 s.
  - T-E2E-SERVE-TYPES (revision 23): every type of the rules table, started with rule-conformant params, starts with a non-zero port, is listed and stops; an environment failure (dlna, nfs) is tolerated with the rclone text logged, a parameter error (`unknown parameters`, `no authorization found`) never.
  - Gate G-U9.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_serve` passes with 5 functions (revision 23).
- **Done:** G-U9.1 and G-U9.1e pass; snapshot change approved and recorded; unit closed; completion commit.
- **Revision 23** (agent-1:opus, from the 9.1 prefetch on the P8 tree and the R15 additions; replaces the matching points above; full brief `scratchpad/draft91.md` + `scratchpad/decisions91.md`):
  - Contract additions in `serve_manager.h`: `parseServeTypes(reply)` (`types` strings, order kept); `ServeManager::types()` (last `typesChanged` payload); signal `listFailed(QString)` for `refresh()` failures only (shown inline in the window, never in a box); `refresh()` ignores a stale reply through a sequence counter; every callback is guarded with a `QPointer`; `serveUrl` uses the addr literally and gives the bare addr for an unknown type; `ServeEntry` never holds user or pass, and no params object or reply is logged (I-5).
  - `ServeTypeRules serveTypeRules(const QString &type)` (pure, in `serve_manager.h`): {readOnlyAllowed, loginAllowed, loginRequired, needsPort, defaultAddr}. restic: readOnlyAllowed false; nfs, dlna, s3: loginAllowed false; sftp: loginRequired (user and pass); ftp: needsPort, defaultAddr `127.0.0.1:2121`; every other type: all allowed, `127.0.0.1:0`. ServeDialog applies the rules on type change (disables and clears what is not allowed, replaces an untouched default addr) and disables OK for an empty addr, a missing or 0 ftp port, or an sftp login without both fields. `serveStartParams` stays literal. The type combo lists every type `serve/types` returns (fallback list dlna, ftp, http, nfs, restic, s3, sftp, webdav).
  - `actionServe` is disabled unless the daemon is Running (tooltip and status tip "Requires the rclone backend"), re-evaluated on selection and on `RcloneDaemon::stateChanged`; the Tools drop-down and the tree context menu show tooltips. MainWindow `actionServes` ("Serves…") is in the File menu before Preferences with a separator, always enabled; no toolbar button. ServeManager and ServesWindow are created lazily as children of MainWindow (the window is a `Qt::Window`, hidden on close); the RcCallFn is `daemonRcCall(QPointer<RcloneDaemon>(global.daemon), ctx)`.
  - T-MW-SNAPSHOT re-record: exactly one added line, `A|actionServes|Serves…|1|0|0|`.
  - Totals after 9.1: G-UNIT 24, G-E2E 18. The e2e file is in the `UNIX AND NOT APPLE` block, serves bind to 127.0.0.1 only, the QNAM uses no proxy, and the leftover-process check runs in `cleanupTestCase`.
- **Revision 24** (agent-1:opus at the 9.1 closure, from the 9.1 reviews and the P9 prefetch; replaces the matching points above):
  - ServeDialog also disables OK when a password is typed without a user name (hint "A password needs a user name"): rclone 1.75.2 turns the login on only with a user name (`lib/http/server.go` `if s.auth.BasicUser != ""`), so a password alone would start an open serve. `serveStartParams` stays literal.
  - NEW `tests/tst_serve_widget.cpp` (`QTEST_MAIN`): T-SRV-DIALOG (built-in list, defaults, the network warning per address form, every `serveTypeRules` effect on the fields, the default address and OK, `params()` trimming, `setTypes`) and T-SRV-WINDOW (the table from serve/list without the credentials, Copy URL, Stop sends `{id}`, the selection kept by id, an inline plain-text failure, "No serves are running", close only hides). Decision D3 deferred this file; it is added because the dialog carries the type rules. G-U9.1 runs `-R 'tst_serve_manager|tst_serve_widget'` with 7 functions; totals after 9.1: G-UNIT 25, G-E2E 18.
  - A serve error arriving while the backend is not Running (a call cut short by a quit or a restart) goes to the status bar, not to a modal box; the box shows rclone's text as plain text. `ServesWindow` holds the manager in a `QPointer`; the daemon `stateChanged` handler captures the manager, not the window.
  - T-E2E-SERVE-TYPES requires http, webdav, ftp, sftp, restic and s3 to start; only dlna and nfs may fail, for an environment reason, never with a parameter error. The e2e sets `RCLONE_CACHE_DIR` inside its temporary folder (sftp host keys).
- **Revision 26** (agent-1:opus at the P9 closure, from the P9 review; replaces "no other key is ever added" above):
  - `serveStartParams` adds `"authorized_keys": ""` for type sftp. Without it rclone 1.75.2 loads `~/.ssh/authorized_keys` of the backend's account: it accepts those keys under any user name, drops their options, and `loadAuthorizedKeys` loops forever on a line that is not a key (a trailing comment), holding the serve lock. With the empty value rclone skips the file; the dialog's user name and password are the only login.
  - T-SRV-PARAMS checks the sftp object. `tst_e2e_serve` sets HOME to a temporary folder whose `.ssh/authorized_keys` holds only a comment, so T-E2E-SERVE-TYPES fails if the key is ever dropped (mutation-checked). Function counts unchanged (G-U9.1 7, G-U9.1e 5).

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation (P9): a "Serve" section covering the supported types, the 127.0.0.1 default, exposing on the network (with a warning), read-only by default, and that serves stop when Rclone Browser quits.
- Supervisor agent-1:opus reviews. Close P9.
