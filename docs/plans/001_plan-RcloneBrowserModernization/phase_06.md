# Phase 5 — rc backend core

Intent:
- The app runs one private `rclone rcd` on 127.0.0.1 with environment-only credentials.
- Folder listing uses `operations/list` when the daemon runs, falls back to the lsjson process otherwise, and an encrypted config is unlocked through `config/unlock`.
- Mount rc credentials leave argv, and mounts gain "Refresh cache" (`vfs/refresh`).
Prerequisites: P4.
Phase closure: P5; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); only 127.0.0.1 is used. Tests must stop every rcd they start (`cleanup()` kills leftovers).

## Local design context
Plan revision: 1.
- R4 (1.75.2):
  - `rclone rcd --rc-addr 127.0.0.1:0 --use-json-log` logs `{"level":"notice","msg":"Serving remote control on http://127.0.0.1:PORT/",…}`;
  - `RCLONE_RC_USER`/`RCLONE_RC_PASS` from the env enable Basic auth; a request without auth gets 401;
  - `core/quit` exits in under 2 s;
  - `core/version` returns `{version, decomposed:[1,75,2], …}`;
  - error replies are HTTP ≥ 400 with body `{error, input, path, status}`.
- R11 (local probe 2026-10-21): with an encrypted config and no password, rcd still starts. The first config-using call returns HTTP 500 with an `error` containing `not allowed to ask for password - set RCLONE_CONFIG_PASS`. `config/unlock {"configPassword":"pw"}` returns `{}`, and later calls succeed.
- R12 (local probe):
  - `operations/list {"fs":"<remote>:<path>","remote":"","opt":{"noMimeType":true}}` returns `{list:[lsjson objects]}`;
  - `"_filter":{"ExcludeRule":[".*/**",".*"]}` hides dot entries exactly like `GetShowHidden()`'s CLI flags;
  - connection-string overrides `remote,key=value:path` are accepted (checked with `loc,links=true:`);
  - errors: a missing dir gives `error in ListJSON: directory not found`; an unknown remote gives `didn't find section in config file ("nope")`.
  - Drive `remoteMode` maps to `<remote>,shared_with_me=true:` and `<remote>,trashed_only=true:`. These are the documented option names of `--drive-shared-with-me`/`--drive-trashed-only`; they are not locally testable, so a unit test checks the string only.
- R10 (local probe): with `rclone serve webdav <dir> --rc --rc-addr 127.0.0.1:0` (the same VFS layer as mount), `vfs/list` gives `{"vfses":[…]}` and `vfs/refresh {"recursive":true}` gives `{"result":{"":"OK"}}`. Tests use serve webdav as the VFS host because Docker has no FUSE.
- D6: transport and lifecycle (port from the log line, `core/quit`, then kill after 3 s).
- D7: mounts stay separate processes, with rc credentials in the env.
  - Refinement (supervisor, revision 1): the user mount script keeps its documented positional args `rclone port user password folder` for compatibility. That is the single I-5 exception. The script also gets `RCLONE_RC_USER`/`RCLONE_RC_PASS`/`RB_RC_PORT` in its env. The README marks the positional credentials as deprecated.
- I-5: rc credentials never appear in the argv of rclone or rcd (guarded by T-DAEMON-ARGV and T-MOUNT-ARGV).
- Listing rule: rc listing is used only when the daemon is Running and `GetDefaultOptionsList("defaultRcloneOptions")` is empty. Arbitrary user flags cannot map to rc parameters, so otherwise the lsjson process from 2.2 is used. This rule is documented in the README.
- Current code (anchors):
  - `src/global.h — class Global` (process counters, `usedRcPorts`);
  - `MainWindow::rcloneGetVersion` success path (3.2 policy), then `rcloneListRemotes()`;
  - `MainWindow::canClose` / close handling;
  - `src/mount_dialog.cpp` getOptions rc block (random 10/22-char `--rc-user=`/`--rc-pass=`);
  - `src/main_window.cpp` runItem mount branch (the same generation);
  - `MainWindow::addNewMount` (creates the mount QProcess and `new MountWidget(mount, remote, folder, argsFinal, script, uniqueId, info)`);
  - `src/mount_widget.cpp`: the readyRead port capture regex `Serving\sremote\scontrol\son`; script launch using `mArgs.indexOf(QRegularExpression("^--rc-user\\S+"))`; Windows unmount using `rclone rc core/quit --rc-addr localhost:<port> <user> <pass>`.

### Contract (NEW `src/rc_client.h/.cpp`, `src/rclone_daemon.h/.cpp`)
```cpp
struct RcCredentials { QString user, pass; };
RcCredentials generateRcCredentials();   // user = "rb-" + base64url(9 bytes), pass = base64url(24 bytes); QRandomGenerator::system(); Base64UrlEncoding|OmitTrailingEquals
void applyRcCredentials(QProcessEnvironment &env, const RcCredentials &c);  // RCLONE_RC_USER / RCLONE_RC_PASS
std::optional<int> parseRcServingPort(QStringView logMsgOrLine);            // finds "Serving remote control on http://127.0.0.1:" / "localhost:" / "[::1]:" then digits; nullopt otherwise
QString rcListFs(const QString &remote, const QString &path, const QString &remoteMode); // "main" → "r:path"; "shared" → "r,shared_with_me=true:path"; "trash" → "r,trashed_only=true:path"

struct RcReply { bool ok = false; int httpStatus = 0; QJsonObject result; QString error; };
RcReply parseRcReply(int httpStatus, const QByteArray &body, const QString &networkError);
// 200 + object → ok; 200 + non-object → error "invalid JSON reply"; ≥400 → body.error if string else networkError; 0 → networkError

class RcClient : public QObject {
  Q_OBJECT
public:
  explicit RcClient(QObject *parent = nullptr);
  void setEndpoint(const QUrl &base, const RcCredentials &c);   // base "http://127.0.0.1:<port>/"
  bool hasEndpoint() const;
  using Callback = std::function<void(const RcReply &)>;
  void call(const QString &path, const QJsonObject &params, QObject *context, Callback cb, int timeoutMs = 30000);
  // POST base+path; Content-Type application/json; Authorization "Basic " + base64(user:pass);
  // QNetworkRequest::setTransferTimeout(timeoutMs). cb runs once, unless `context` was destroyed first.
};

class RcloneDaemon : public QObject {
  Q_OBJECT
public:
  enum class State { Stopped, Starting, Running, Failed };
  Q_ENUM(State)
  explicit RcloneDaemon(QObject *parent = nullptr);
  ~RcloneDaemon() override;                   // stop(); waits ≤ 3 s, then kill
  void start();                               // no-op unless Stopped/Failed
  void stop();                                // Running: core/quit (2 s timeout), kill at 3 s; Starting: kill
  State state() const; int port() const; QString lastError() const;
  RcClient *client();                         // endpoint set while Running
  qint64 processId() const;                   // tests
signals:
  void stateChanged(RcloneDaemon::State state);
  void failed(const QString &message);
};
```
- `start()`:
  - QProcess, SeparateChannels;
  - env = `RcloneEnvironment(Parsed)` plus `applyRcCredentials`;
  - program `GetRclone()`; args `rcd --rc-addr 127.0.0.1:0 --use-json-log --ask-password=false` plus `GetRcloneConf()`;
  - stderr lines go through `parseRcloneLogLine`. The first port found sets `port`, the client endpoint and state Running.
  - If no port arrives within 10 s: kill, state Failed, `lastError = "rcd did not report its port"`.
  - Process exit while Starting or Running (not by `stop`): state Failed, with `lastError` the last Error/Critical message or `"rcd exited (code N)"`.
  - stdout is drained and ignored.

## Sub-phases

### 5.1 RcClient, credentials, RcloneDaemon
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews before S1 (contract walk-through: timeout and lifetime semantics) and after the diff (I-5 and process lifetime).
- **Files:**
  - NEW `src/rc_client.h/.cpp` and `src/rclone_daemon.h/.cpp` (both `Q_OBJECT` → `MOC`), registered.
  - WRITE the root and `src/CMakeLists.txt`: `Qt6::Network` is already linked (update checks use QNAM); confirm, and add it only if missing.
  - NEW `tests/tst_rc_client.cpp` (unit) and `tests/tst_e2e_daemon.cpp` (e2e, `QTEST_GUILESS_MAIN` with a QCoreApplication event loop).
- **Change:**
  - Preconditions: P4 DONE.
  - Contract: above.
  - Steps:
    1. S1 — `rc_client` pure functions plus the unit tests. Expected: they pass.
    2. S2 — `RcClient::call` and `RcloneDaemon`. Expected: it builds.
    3. S3 — e2e tests. Run `scripts/dev.sh test e2e`.
  - Failure handling: if `setTransferTimeout` misbehaves on Qt 6.4, implement the timeout with a single-shot QTimer that aborts the reply. Record which one was used.
- **Unit tests:** `tests/tst_rc_client.cpp`:
  - T-RC-CREDS: 100 generations are pairwise distinct. The user matches `^rb-[A-Za-z0-9_-]{12}$` and the pass matches `^[A-Za-z0-9_-]{32}$`.
  - T-RC-PORT:
    - `Serving remote control on http://127.0.0.1:36637/` gives 36637;
    - the JSON line from `tests/fixtures/rclone-1.75/rcd_start` (msg field) gives its port, which is > 0;
    - `Serving remote control on http://localhost:5572/` gives 5572;
    - `nothing` gives nullopt.
  - T-RC-REPLY:
    - `(200, {"a":1})` is ok with result a=1;
    - `(200, [1])` gives `invalid JSON reply`;
    - `(500, {"error":"x"})` gives error x with status 500;
    - `(401, "", "Host requires authentication")` gives that error with status 401;
    - `(0, "", "Connection refused")` is not ok.
  - T-RC-FS: `("gd", "a/b", "main")` gives `gd:a/b`; `"shared"` gives `gd,shared_with_me=true:a/b`; `"trash"` gives `gd,trashed_only=true:a/b`.
  - Gate G-U5.1: `scripts/dev.sh test qt64 -- -R tst_rc_client` passes with 4 functions.
- **e2e tests:** `tests/tst_e2e_daemon.cpp`. `SetRclone(rbRclone())`; temp conf `[loc] type = local`.
  - T-E2E-DAEMON-START: Running within 10 s and `port() > 0`. `core/version` returns ok, with `decomposed[0] == 1` and `decomposed[1] >= 75`.
  - T-DAEMON-ARGV (I-5): read `/proc/<pid>/cmdline`. It contains neither the user nor the pass, and does contain `rcd`. `/proc/<pid>/environ` (same uid, readable) contains `RCLONE_RC_USER=<user>`.
  - T-E2E-DAEMON-AUTH: a raw QNAM POST to `core/version` with a wrong password gives `parseRcReply` httpStatus 401 and not ok.
  - T-E2E-DAEMON-QUIT: `stop()`; the process exits within 3 s; the state goes to Stopped; a test hook `RcloneDaemon::lastStopWasKill()` is false.
  - T-E2E-DAEMON-KILL: after Running, `kill(pid, SIGSTOP)`, then `stop()`. Stopped within 5 s; `lastStopWasKill()` is true.
  - T-E2E-DAEMON-FAIL: `SetRclone("/bin/false")`, then `start()`. Within 5 s `failed` is emitted and the state is Failed with a non-empty `lastError`.
  - Gate G-U5.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_daemon` passes with 6 functions, and no `rclone rcd` process from the test remains (`pgrep -f "rcd --rc-addr 127.0.0.1:0"` checked in `cleanupTestCase`).
- **Done:** both reviews recorded; G-U5.1 and G-U5.1e pass; unit closed; completion commit.

### 5.2 Daemon lifecycle in MainWindow; rc listing with fallback; config unlock
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff, focusing on startup ordering, quit/close paths, ItemModel dual-path parity and the password prompt loop.
- **Files:**
  - WRITE `src/global.h`: add `class RcloneDaemon; … RcloneDaemon *daemon = nullptr;`.
  - WRITE `src/main_window.h/.cpp`:
    - add a `RcloneDaemon *mDaemon` member;
    - start it after the supported-version check passes;
    - stop it in the quit and close paths, after jobs are handled;
    - restart it when Preferences change the rclone path or config;
    - add `ensureRcUnlocked()`.
  - WRITE `src/item_model.h/.cpp`: the rc listing path, sharing `itemFromEntry` and the merge.
  - WRITE `src/lsjson_parser.h/.cpp`: use `lsjsonEntryFromObject` for rc list objects (already in 2.1, no change expected).
  - NEW `tests/tst_e2e_rclisting.cpp`.
- **Change:**
  - Preconditions: 5.1 DONE.
  - Contract:
    - Lifecycle:
      - `global.daemon = mDaemon` (owned by MainWindow);
      - `mDaemon->start()` runs right before `rcloneListRemotes()` on the supported path;
      - on `failed(msg)`, a non-modal status-bar message: `"rclone backend unavailable, using direct commands: " + msg`. Listing falls back automatically. There is no retry loop: the next Preferences accept or app start retries.
      - Quit: after the existing job handling in `canClose`/quit, call `mDaemon->stop()` and wait for Stopped with `QEventLoop` and a 4 s cap.
    - `ItemModel::load` rc path. Condition: `global.daemon && global.daemon->state() == Running && GetDefaultOptionsList("defaultRcloneOptions").isEmpty()`.
      - Build params: `fs = rcListFs(mRemote, parent->path.path(), Settings/remoteMode)`, `remote = ""`, `opt = {noMimeType: true}`. When `Settings/showHidden` is false, add `_filter = {ExcludeRule: [".*/**", ".*"]}`.
      - `global.daemon->client()->call("operations/list", params, this, cb)`, with the same process-counter bookkeeping (+1, then −1 in the callback) and the same loading spinner.
      - Callback, ok: each element of `result["list"]` (array of objects) goes through `lsjsonEntryFromObject`, then `itemFromEntry`, then into the cache, then the existing merge.
      - Callback, not ok:
        - if `classifyConfigLoadError(1, error.toUtf8())` is NeedsPassword or WrongPassword, call `emit configLocked()` (new ItemModel signal → MainWindow `ensureRcUnlocked()`) and show the Special row `⚠ configuration is locked`;
        - otherwise show the Special row `"⚠ " + error`.
      - Add a test hook `bool lastLoadUsedRc() const`.
    - `MainWindow::ensureRcUnlocked()` (re-entrancy guarded by a bool):
      - the 3.3 prompt flow (NeedsPassword/WrongPassword dialog);
      - on accept: `SetRclonePassword(pw)` and `client()->call("config/unlock", {configPassword: pw})`;
      - on ok, refresh the current remote views (the existing refresh API); on error, repeat the prompt with the Wrong prefix;
      - cancel stops the loop.
    - Out of scope (unchanged): `rcloneListRemotes` and other operations stay CLI in this phase (U3; the config flow moves to rc in P6).
  - Steps:
    1. S1 — Daemon lifecycle wiring. Expected: it builds; smoke passes (daemon starts in smoke, and the app exits cleanly when killed by smoke).
    2. S2 — ItemModel rc path and the configLocked flow. Expected: it builds.
    3. S3 — e2e tests; full `scripts/dev.sh test e2e smoke`.
  - Recovery boundary: after S1.
  - Failure handling: if a result differs between the rc and lsjson paths (names, sizes, modified text) for the same dir, it is a bug in the conversion. Fix the conversion; never fork the merge.
- **Unit tests:** N/A — the rc path needs a live daemon; the conversion functions are already unit-tested in 2.1.
- **e2e tests:** `tests/tst_e2e_rclisting.cpp`. Uses a real `RcloneDaemon` assigned to `global.daemon` and the 2.2 `loadModel` helper.
  - T-E2E-RCLIST-NAMES: the same tree and assertions as T-E2E-LIST-NAMES, plus `lastLoadUsedRc() == true`.
  - T-E2E-RCLIST-PARITY: for the same dir, the children (name, isFolder, size, modified) from the rc path equal those from the lsjson path (daemon stopped, `lastLoadUsedRc() == false`).
  - T-E2E-RCLIST-ERROR: a missing dir gives one Special row, whose name starts `⚠ ` and contains `directory not found`.
  - T-E2E-RCLIST-HIDDEN: with `Settings/showHidden=false`, `.h.txt` and `.hid` are absent.
  - T-E2E-RCLIST-DEFAULTOPTS: with `Settings/defaultRcloneOptions="--fast-list"`, `lastLoadUsedRc() == false`.
  - T-E2E-DAEMON-UNLOCK:
    - encrypted conf as in 3.3, `SetRclonePassword("")`, daemon started;
    - the load emits `configLocked` and the Special row `⚠ configuration is locked`;
    - then `client()->call("config/unlock", {configPassword: "pw"})` is ok;
    - a re-load lists entries.
  - Gate G-U5.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_rclisting` passes with 6 functions.
- **Done:** review recorded; G-U5.2e passes; smoke passes; unit closed; completion commit.

### 5.3 Mount credentials via environment; Refresh cache
- **Model:** agent-2:sonnet
- **Assignment:** implementer. CI-compiled (Windows unmount). agent-1:opus reviews at P5, with the Windows hunk and the script exception in focus.
- **Files:**
  - NEW `src/mount_launch.h/.cpp`, registered.
  - WRITE `src/mount_dialog.cpp`: remove the credential generation, keep `--rc --rc-addr`.
  - WRITE `src/main_window.cpp`: the runItem mount branch (same removal) and `addNewMount`.
  - WRITE `src/mount_widget.h/.cpp/.ui`: credentials member, script env, a `refreshCache` QToolButton, the Windows unmount through RcClient.
  - NEW `tests/tst_mount_launch.cpp` (unit) and `tests/tst_e2e_vfs.cpp` (e2e).
- **Change:**
  - Preconditions: 5.2 DONE.
  - Contract:
    ```cpp
    struct MountLaunch { QStringList args; QProcessEnvironment env; RcCredentials creds; bool rcEnabled = false; };
    MountLaunch buildMountLaunch(const QStringList &args, const QProcessEnvironment &base, const RcCredentials &creds);
    // removes every token matching ^--rc-(user|pass)(=.*)?$ and, for the non-'=' form, its following value token;
    // rcEnabled = args contains "--rc"; when rcEnabled: env = base + applyRcCredentials(creds); else env = base
    ```
    - `addNewMount`:
      - `auto ml = buildMountLaunch(argsFinal, RcloneEnvironment(Parsed), generateRcCredentials());`
      - `mount->setProcessEnvironment(ml.env)`; start with `ml.args`;
      - `widget->setRcCredentials(ml.creds, ml.rcEnabled)`.
      - Remove the existing `UseRclonePassword`/`ApplyRcloneEnvironment` call there, since the env is now set explicitly.
    - `MountWidget`:
      - the script launch keeps positional `rclone port user password folder`, with user/password from the stored credentials (D7 exception), and its env = `RcloneEnvironment(Parsed)` + `applyRcCredentials` + `RB_RC_PORT=<port>`;
      - `refreshCache` (text "Refresh cache") is enabled once the port is known and `rcEnabled`. Click: `RcClient` (a member, endpoint `http://127.0.0.1:<port>/` with the credentials) calls `vfs/refresh {"recursive": true}`. ok appends `Refresh cache: OK`, otherwise `Refresh cache failed: <error>`, to `ui.output`.
      - Windows unmount (CI-compiled): replace the `rclone rc core/quit …` process with `RcClient::call("core/quit", {})`. On a callback error, set `mUnmountingError` the way the old process's non-zero exit did.
  - Steps:
    1. S1 — `mount_launch` plus the unit tests.
    2. S2 — Dialog, runItem and addNewMount wiring. Expected: `git grep -n "rc-user=\|rc-pass=" src` shows no generation code (only the strip regex in `mount_launch.cpp`).
    3. S3 — MountWidget changes; e2e; full gates.
- **Unit tests:** `tests/tst_mount_launch.cpp`:
  - T-MOUNT-ARGV (I-5): input `mount r: /m --rc --rc-addr localhost:5600 --rc-user=old --rc-pass=old2 --rc-user x --vfs-cache-mode full` gives:
    - args exactly `mount r: /m --rc --rc-addr localhost:5600 --vfs-cache-mode full`;
    - env `RCLONE_RC_USER` = creds.user; `rcEnabled` true.
  - T-MOUNT-NORC: without `--rc`, `rcEnabled` is false and the env has no `RCLONE_RC_USER`.
  - Gate G-U5.3: `scripts/dev.sh test qt64 -- -R tst_mount_launch` passes with 2 functions.
- **e2e tests:** `tests/tst_e2e_vfs.cpp`, T-E2E-VFS-REFRESH (R10):
  - start `rbRclone() serve webdav <tmp> --addr 127.0.0.1:0 --rc --rc-addr 127.0.0.1:0 --use-json-log --config <empty>` with env from `buildMountLaunch` (the mount stand-in);
  - read the rc port with `parseRcServingPort` on stderr events;
  - `RcClient` `vfs/refresh {"recursive":true}` is ok with `result["result"]` an object;
  - `/proc/<pid>/cmdline` contains no credentials;
  - stop the server with `core/quit`.
  - Gate G-U5.3e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_vfs` passes.
- **Done:** greps clean; G-U5.3 and G-U5.3e pass; CI debt noted; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present. G-CI is debt for the 5.3 Windows hunk.
- Assertion: after the whole e2e suite, no `rclone rcd` or `rclone serve` process started by the tests remains.
- README obligation (P5): a "Backend" subsection covering:
  - the private rcd (localhost, random credentials, env only);
  - the listing fallback rule (default rclone options → direct commands);
  - mount "Refresh cache";
  - the mount script contract: positional args kept but credentials deprecated; env variables `RCLONE_RC_USER`, `RCLONE_RC_PASS`, `RB_RC_PORT`.
- Supervisor agent-1:opus reviews. Close P5.
