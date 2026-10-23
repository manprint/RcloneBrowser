# Phase 3 — webdav-webui

Intent: the Serve dialog offers `webdav-webui` (rclone `serve webdav` with the fork's web interface)
configurable with all 19 `--webui-*` options and a VFS cache mode; it works with the fork binary,
is refused with a clear message on an rclone without the web interface, and classic `webdav` stays
classic on both binaries.
Prerequisites: P2 DONE.
Phase closure: P3 (final phase); review by agent-1:opus.

## State and ownership contract
Read STATE.md first. In a fresh session reconcile the active unit, actual diff, step checkpoint, scope,
and commit reference before selecting work. In a delegated worker session, follow the coordinator's
assigned OPEN unit; do not take over state/commits or revert active work. Open before edits; checkpoint
after meaningful batches and before yielding. Close after tests/review/coherence, commit when configured
(full-autonomous:true), then continue only within scope. Missing design goes to agent-1:opus.
Restrict code searches to `RcloneBrowser/` (the index also covers worktrees `wt-*`). Never edit `../rclone`.

## Local design context
Plan revision: 1.
- R2 (fork source + probe, fork `a527815ae`): the webui options are one **process-global** option block
  `webdav_webui` of the rcd, read when a webdav server is built and copied into it (`webui.New(cfg, opt)` by
  value). `serve/start` rejects flat `webui_*` keys (`unknown parameters`). `options/set` with JSON keys =
  Go field names sets them; **unknown keys are silently accepted**.
- R3: the fork lists `webdav_webui` in `options/blocks`; upstream 1.75.2 does not and fails `options/set`
  on it. The fork's default is `Enabled: true`: a plain webdav start on the fork serves the web UI.
- R4: durations as strings (`"30s"`, `"1h30m"`, `"3d"`, `"off"`) or ns integers; sizes as strings
  (`"4Mi"`, `"512k"`, `"1G"`); integers as JSON numbers; `PublinkPaths` as a JSON array; invalid values ⇒
  HTTP 500 with rclone's text. `vfs_cache_mode` in `serve/start` accepts `off|minimal|writes|full`.
  Empty `ResumeDir` ⇒ `<rclone cache>/webui/resume`; empty `Secret` ⇒ random keys per serve.
- D6: per webdav start on a capable rclone: `options/set {"webdav_webui": <19 keys>}` (Enabled = flavor is
  webui) then `serve/start {type:"webdav",…}`, FIFO with one start in flight (2.1). Failed `options/set`
  ⇒ that start fails (`startFailed`, then `error`) and no `serve/start` is sent.
- D7: capability tri-state from `options/blocks`, reset on any daemon state change, reloaded on Running;
  a webdav start while Unknown resolves it first.
- D8: dialog design below; remembered values via `GetSettings()` (`src/utils.h`, the app's settings store, portable-mode aware) under `Serve/webui/<GoFieldName>` (18 keys) and
  `Serve/webui/VfsCacheMode`; **never** `Secret`.
- D10: no change to `../rclone`.
- I-2: `Secret` and the serve password only in rc POST bodies and widget memory; never logged/persisted.
- I-3, I-4, I-5, I-10 (overview.md): sequence, no `webui*` key in `serve/start`, never start webui on an
  incapable rclone, read-back equality.

### Option table (exact; order = `webuiOptionKeys()`)
| Go key (JSON) | C++ field (`WebuiOptions`) | JSON type | Default | Flag | Dialog widget (objectName) | Section |
|---|---|---|---|---|---|---|
| Enabled | — (flavor) | bool | — | `--webui` | — | — |
| AllowDelete | `allowDelete` | bool | true | `--webui-allow-delete` | QCheckBox `webuiAllowDelete` "Allow deleting, moving and overwriting" | Basic |
| Search | `search` | bool | true | `--webui-search` | QCheckBox `webuiSearch` "Search" | Basic |
| SearchMaxDepth | `searchMaxDepth` | number | 8 | `--webui-search-max-depth` | QSpinBox `webuiSearchMaxDepth` 1..1000 | Advanced |
| SearchMaxResults | `searchMaxResults` | number | 1000 | `--webui-search-max-results` | QSpinBox `webuiSearchMaxResults` 1..1000000 | Advanced |
| SearchTimeout | `searchTimeout` | string (duration) | "30s" | `--webui-search-timeout` | QLineEdit `webuiSearchTimeout` | Advanced |
| PublinkPaths | `publinkPaths` (QStringList) | array of string | [] | `--webui-publink-paths` | QLineEdit `webuiPublinkPaths` "Public link paths", comma-separated | Basic |
| Secret | `secret` | string | "" | `--webui-secret` | QLineEdit `webuiSecret` (Password echo) | Advanced |
| TokenExpiry | `tokenExpiry` | string (duration) | "72h" | `--webui-token-expiry` | QLineEdit `webuiTokenExpiry` | Advanced |
| Resume | `resume` | bool | false | `--webui-resume` | QCheckBox `webuiResume` "Remember playback positions" | Basic |
| ResumeDir | `resumeDir` | string | "" | `--webui-resume-dir` | QLineEdit `webuiResumeDir` (placeholder "rclone cache folder") | Advanced |
| ResumeRetention | `resumeRetention` | string (duration) | "720h" | `--webui-resume-retention` | QLineEdit `webuiResumeRetention` | Advanced |
| PlayerMaxRetry | `playerMaxRetry` | number | 30 | `--webui-player-max-retry` | QSpinBox `webuiPlayerMaxRetry` 0..10000 | Advanced |
| PlayerReconnectInterval | `playerReconnectInterval` | number | 10 | `--webui-player-reconnect-interval` | QSpinBox `webuiPlayerReconnectInterval` 1..3600 | Advanced |
| DiskStats | `diskStats` | bool | false | `--webui-disk-stats` | QCheckBox `webuiDiskStats` "Disk usage statistics" | Basic |
| UploadSettings | `uploadSettings` | bool | false | `--webui-upload-settings` | QCheckBox `webuiUploadSettings` "Concurrent upload settings" | Basic |
| EditMaxSize | `editMaxSize` | string (size) | "4Mi" | `--webui-edit-max-size` | QLineEdit `webuiEditMaxSize` | Advanced |
| DirCount | `dirCount` | bool | false | `--webui-dir-count` | QCheckBox `webuiDirCount` "Show the number of entries of folders" | Basic |
| MultiZipMax | `multiZipMax` | number | 200 | `--webui-multi-zip-max` | QSpinBox `webuiMultiZipMax` 1..1000000 | Advanced |

VFS cache mode: QComboBox `vfsCacheMode` items `off`, `minimal`, `writes`, `full` (default `off`), Basic
section, hint QLabel `vfsHint` "writes or full is needed to resume interrupted uploads". `off` is sent as
nothing (`vfsCacheMode` empty); others as `"vfs_cache_mode"` in `serve/start`.
Validation regexes (anchored, Qt `QRegularExpression`): duration
`^(off|([0-9]+(\.[0-9]+)?(ns|us|µs|ms|s|m|h|d|w|M|y))+)$`; size `^(off|[0-9]+(\.[0-9]+)?([bkMGTPE]i?)?)$`.
PublinkPaths parse: split on `,`, trim each, drop empty.

## Sub-phases

### 3.1 webui option block, capability and start sequence in ServeManager
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews after diff (I-3 ordering, error paths, I-2, I-4).
- **Files:** WRITE `src/serve_manager.h`, `src/serve_manager.cpp`, `tests/tst_serve_manager.cpp`,
  `src/main_window.cpp` (`serveManager()` daemon lambda; `serveDisplayType`).
- **Change:**
  Preconditions: P2 DONE.
  Contract: option table; D6, D7; messages:
  `kWebuiUnsupported` = "This rclone does not support webdav-webui: it needs rclone built from the webui fork. Choose webdav for a WebDAV serve without the web interface."
  options/set failure ⇒ "webdav-webui options: " + rclone text.
  Steps:
  1. S1 — header: `struct WebuiOptions { … fields/defaults of the table … };`; `ServeParams` += `QString flavor; WebuiOptions webui; QString vfsCacheMode;`
     free functions `const QStringList &webuiOptionKeys();` (19 Go keys in table order) and
     `QJsonObject webuiOptionsBlock(const WebuiOptions &o, bool enabled);`; `ServeManager` gets
     `enum class WebuiSupport { Unknown, Yes, No }; Q_ENUM(WebuiSupport)`, `WebuiSupport webuiSupport() const;`
     `void loadCapabilities(); void resetCapabilities();` signal `capabilitiesChanged(ServeManager::WebuiSupport)`,
     private `WebuiSupport mWebui = WebuiSupport::Unknown; bool mCapsInFlight = false; bool mCapsTriedForFront = false;`.
  2. S2 — `serveStartParams`: add `"vfs_cache_mode"` when `p.vfsCacheMode` non-empty; nothing else changes
     (`type` is already `"webdav"` for the flavor).
  3. S3 — `loadCapabilities()`: if in flight return; `mCall("options/blocks", {}, …)`: ok ⇒ Yes when
     `reply.result["options"]` is an array containing the string `webdav_webui`, else No; not ok ⇒ unchanged
     (qWarning "options/blocks failed:" + failureText). Emit `capabilitiesChanged` when the value changes.
     Then `mCapsInFlight=false; mCapsTriedForFront = true; processStartQueue();`.
     `resetCapabilities()`: set Unknown (emit when changed), `mCapsTriedForFront=false`.
  4. S4 — `processStartQueue()` decision for the queue front `f` (peek before dequeue):
     - `f.params.type != "webdav"` ⇒ plain `serve/start` (2.1 path).
     - webdav and `mWebui == Unknown` and `!mCapsTriedForFront` ⇒ `loadCapabilities()` and return (the
       reply re-enters `processStartQueue`).
     - dequeue; `mCapsTriedForFront = false`; in flight = true.
     - flavor webui and `mWebui != Yes` ⇒ `startFailed(token, kWebuiUnsupported)`, `error(same)`, in flight false, `processStartQueue()` (no rc call).
     - `mWebui == Yes` ⇒ `options/set webuiOptionsBlock(f.params.webui, flavor == kWebuiFlavor)`; ok ⇒
       `serve/start` (same callback as 2.1); not ok ⇒ `startFailed`, `error("webdav-webui options: "+text)`, in flight false, `refresh()`, `processStartQueue()`.
     - otherwise (classic, `No`, or `Unknown` after a failed check) ⇒ plain `serve/start`.
  5. S5 — `main_window.cpp`: in the `global.daemon` `stateChanged` lambda call `manager->resetCapabilities()`
     first and `manager->loadCapabilities()` next to `loadTypes()` when Running; in the initial
     `if (daemonRunning())` block call `loadCapabilities()` too. `serveDisplayType(p)` returns `p.flavor`
     when non-empty.
  6. S6 — tests (below).
  Checkpoint: after S4, S6.
  Failure handling: re-entrancy bugs (double serve/start) ⇒ add a QVERIFY on recorded paths, fix; after
  two failed attempts escalate.
- **Unit tests** (`tests/tst_serve_manager.cpp`, `Fake` with async replies):
  - T-WEBUI-BLOCK: `webuiOptionKeys()` equals the 19 table keys in order; `webuiOptionsBlock(WebuiOptions(), true)`
    has exactly one top key `webdav_webui` whose object keys == those 19 keys; `Enabled` true; defaults equal
    the table (bools `isBool`, numbers `isDouble`, `PublinkPaths` `isArray` empty, durations/sizes strings);
    with `publinkPaths = {"/a","/b"}` the array is `["/a","/b"]`; `enabled=false` ⇒ `Enabled` false.
  - T-SRV-PARAMS (extend): webui flavor params with `vfsCacheMode "writes"` ⇒ body `type == "webdav"`,
    `vfs_cache_mode == "writes"`, no key with prefix `webui` (iterate keys; I-4); empty `vfsCacheMode` ⇒ no `vfs_cache_mode` key.
  - T-WEBUI-CAPS: blocks `{"options":["main","webdav_webui"]}` ⇒ Yes + one signal; `{"options":["main"]}` ⇒ No;
    `errReply(500,"x")` ⇒ stays Unknown; `resetCapabilities()` from Yes ⇒ Unknown + signal.
  - T-WEBUI-SEQ: (a) Yes script; `start(webui p1)`, `start(classic webdav p2)`, `start(http p3)` back-to-back ⇒
    paths exactly `options/blocks, options/set, serve/start, serve/list, options/set, serve/start, serve/list, serve/start, serve/list`;
    first `options/set` `Enabled` true and `SearchMaxDepth` = p1 value; second `Enabled` false; `started` tokens 1,2,3.
    (b) No script ⇒ `start(webui)` emits `startFailed(1, kWebuiUnsupported)` and records no `options/set`/`serve/start`;
    `start(classic)` ⇒ `serve/start` only. (c) Yes script with `options/set` ⇒ `errReply(500,"bad suffix")` ⇒
    `startFailed` message starts "webdav-webui options: " and no `serve/start` for it; a following start still runs.
  Gate G-U31: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_serve_manager$'`.
- **e2e tests:** covered by 3.3; `tst_e2e_serve` (upstream) must still pass: G-U31e
  `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh e2e qt64 -- -R '^tst_e2e_serve'`.
- **Done:** tests green; review recorded; unit closed; commit.

### 3.2 Serve dialog: webdav-webui type, options, remembered values
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews after diff (I-2 persistence, I-5 UX).
- **Files:** WRITE `src/serve_widget.h`, `src/serve_widget.cpp` (`ServeDialog`), `src/main_window.cpp`
  (`openServeDialog`), `tests/tst_serve_widget.cpp`.
- **Change:**
  Preconditions: 3.1 DONE.
  Contract: option table, regexes, D8. Existing behaviour for other types unchanged (T_SRV_DIALOG stays green).
  Steps:
  1. S1 — type list: private helper `static QStringList withWebuiFlavor(QStringList types)` inserts
     `kWebuiFlavor` right after `"webdav"` when `"webdav"` is present and the flavor is absent; applied to
     the built-in list and in `setTypes` (compare after applying).
  2. S2 — widgets: `QGroupBox *mWebuiGroup` (objectName `webuiGroup`, title "Web interface") added to the
     main layout after the form, visible only when `type() == kWebuiFlavor`; inside: Basic `QFormLayout`
     (table "Basic" rows + `vfsCacheMode` + `vfsHint`), a checkable `QToolButton webuiAdvancedToggle`
     ("Advanced", arrow `Qt::RightArrow`/`Qt::DownArrow`), `QWidget webuiAdvanced` (hidden by default) with
     the "Advanced" rows; `QLabel webuiUnsupported` (PlainText, warning colour via `setWarningColor`).
     Defaults from `WebuiOptions()`. `webuiSecret` tooltip: "Signs download links and public links. Empty:
     a new random secret at every start, so earlier links stop working."
  3. S3 — `public slots: void setWebuiSupport(ServeManager::WebuiSupport s);` stores it and calls
     `updateOk()`. In `updateOk()` when flavor: support Unknown ⇒ `webuiUnsupported` "Checking whether this
     rclone supports webdav-webui…", OK disabled; No ⇒ `kWebuiUnsupported`, OK disabled; Yes ⇒ label hidden;
     then each duration field must match the duration regex (problem `Invalid duration: <row label>`) and
     `webuiEditMaxSize` the size regex (`Invalid size: Edit max size`) ⇒ OK disabled with `mProblem`.
     Connect every webui text field's `textChanged` to `updateOk`.
  4. S4 — `params()`: when `type() == kWebuiFlavor`: `p.type = "webdav"`, `p.flavor = kWebuiFlavor`,
     `p.webui` from widgets (PublinkPaths parse rule), `p.vfsCacheMode = combo == "off" ? "" : combo`;
     rules/address/user/pass/readOnly as for webdav. Non-flavor: flavor, vfsCacheMode empty, webui default.
  5. S5 — remember (always through `GetSettings()` from `utils.h`, never a bare `QSettings`): `loadWebuiSettings()` in the constructor (only keys present override defaults; types:
     bool via `toBool`, int via `toInt`, strings; PublinkPaths stored as the field text) and
     `saveWebuiSettings()` from an override of `accept()` (calls `QDialog::accept()` after saving) when
     the flavor is selected. Keys: `Serve/webui/<GoKey>` for the 17 option keys (19 − `Enabled` − `Secret`)
     plus `Serve/webui/VfsCacheMode` = 18 settings keys. Never read or write `Serve/webui/Secret`.
  6. S6 — `main_window.cpp openServeDialog`: after constructing `dlg`: `dlg.setWebuiSupport(serveManager()->webuiSupport());`
     connect `capabilitiesChanged → &ServeDialog::setWebuiSupport`; call `serveManager()->loadCapabilities()`
     next to `loadTypes()`.
  7. S7 — tests (below).
  Checkpoint: after S3, S5, S7.
  Failure handling: layout/height issues of the dialog are cosmetic — record, do not change the contract.
- **Unit tests** (`tests/tst_serve_widget.cpp`):
  - T-WEBUI-DIALOG: built-in list has `webdav-webui` at `indexOf("webdav")+1`; `setTypes({"http","webdav"})` ⇒
    `{"http","webdav","webdav-webui"}`; `setTypes({"http"})` ⇒ no flavor. Select webdav ⇒ `webuiGroup` hidden;
    select flavor ⇒ visible, `webuiAdvanced` hidden until the toggle is clicked. Support Unknown ⇒ OK disabled
    and label text starts "Checking"; `setWebuiSupport(No)` ⇒ OK disabled, label == `kWebuiUnsupported`;
    `Yes` ⇒ OK enabled. Set `webuiAllowDelete` off, `webuiPublinkPaths` " /a , ,/b ", `webuiSearchMaxDepth` 3,
    `webuiEditMaxSize` "8Mi", `webuiTokenExpiry` "1h", `webuiSecret` "s3cr3t", `vfsCacheMode` "writes" ⇒
    `params()` type "webdav", flavor "webdav-webui", `webui.allowDelete` false, `publinkPaths` {"/a","/b"},
    `searchMaxDepth` 3, `editMaxSize` "8Mi", `tokenExpiry` "1h", `secret` "s3cr3t", `vfsCacheMode` "writes".
    `webuiSearchTimeout` "abc" ⇒ OK disabled, problem "Invalid duration: …"; back to "30s" ⇒ enabled.
    `vfsCacheMode` "off" ⇒ `params().vfsCacheMode` empty. Classic webdav ⇒ flavor empty.
  - T-WEBUI-REMEMBER: `initTestCase` must call `rbInitTestEnvironment()` (`tests/support/rb_test_support.h`; add it,
    `tst_serve_widget` does not call it yet) so settings are hermetic; clear with `GetSettings()->remove("Serve/webui")`;
    inspect with `GetSettings()->beginGroup("Serve/webui")` + `childKeys()`; dialog A (flavor, Yes) with the values above, `accept()`; the
    `Serve/webui` group has exactly 18 child keys, none containing "Secret"; values stored (e.g.
    `Serve/webui/SearchMaxDepth` == 3, `Serve/webui/VfsCacheMode` == "writes"); dialog B shows the stored
    values and `webuiSecret` empty. A dialog accepted with classic webdav writes nothing new.
  Gate G-U32: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_serve_widget$'`.
- **e2e tests:** N/A — 3.3.
- **Done:** tests green; review recorded; unit closed; commit.

### 3.3 End-to-end webdav-webui with the fork and upstream binaries
- **Model:** agent-2:sonnet
- **Assignment:** implement the acceptance tests; agent-1:opus reviews the oracle and evidence.
- **Files:** NEW `tests/tst_e2e_serve_webui.cpp`; WRITE `tests/CMakeLists.txt`: inside the
  `RB_TEST_RCLONE AND UNIX AND NOT APPLE` block, `if(RB_TEST_RCLONE_WEBUI) rb_add_test(tst_e2e_serve_webui e2e) endif()`
  (D11: CI has no fork binary). Production code only if a test exposes a defect
  in 3.1/3.2 contracts (then fix within those contracts and record it).
- **Change:**
  Preconditions: 3.1, 3.2 DONE; fork binary from 0.1 (`build-rclone-webui/rclone`).
  Contract: I-3, I-5, I-10; harness pattern of `tests/tst_e2e_serve.cpp` (RcloneDaemon, `RcCallFn`, temp
  config `[loc] type = local`, every serve on 127.0.0.1, pgrep cleanup check).
  Steps:
  1. S1 — `initTestCase`: `const QString fork = qEnvironmentVariable("RB_TEST_RCLONE_WEBUI");`
     `QVERIFY2(!fork.isEmpty() && QFileInfo(fork).isExecutable(), "RB_TEST_RCLONE_WEBUI must point to the rclone webui fork")` (I-9: once registered, fail, never skip).
     Helpers: `startDaemon(path)` (SetRclone(path) then the tst_e2e_serve pattern), `get(url, ua)` returning
     `{status, body}` via `QNetworkAccessManager` with `Accept: text/html` and the given `User-Agent`,
     `rcGet(path, params)` using the daemon client, `webuiParams(…)` building a flavor `ServeParams`.
  2. S2 — test functions below, in this order; each stops its serves (or the daemon) in `cleanup()`.
  Checkpoint: after S2.
  Failure handling: a failing oracle ⇒ capture rc replies (without printing Secret) and escalate.
- **Unit tests:** N/A — e2e unit.
- **e2e tests** (`tests/tst_e2e_serve_webui.cpp`, fork daemon unless stated):
  - T-E2E-WEBUI-CAPS: `loadCapabilities()` ⇒ `webuiSupport() == Yes`.
  - T-E2E-WEBUI-PAGE: start flavor (fs `loc:<tmp>/w`, `readOnly` false, `searchMaxDepth` 3, `searchTimeout`
    "45s", `publinkPaths` {"/"}, `editMaxSize` "8Mi", `tokenExpiry` "1h", `secret` "e2e-secret",
    `vfsCacheMode` "writes") ⇒ `started`; GET `/` with UA `Mozilla/5.0` ⇒ 200 and body contains `__webui_`;
    GET with UA `curl/8.0` ⇒ 200 and body lacks `__webui_`. `serve/list` entry params `vfs_cache_mode` == "writes".
  - T-E2E-WEBUI-OPTS (I-10): right after that start, `options/get {"blocks":"webdav_webui"}` ⇒ all 19 keys equal
    the sent values with conversions: `SearchTimeout` 45000000000, `TokenExpiry` 3600000000000,
    `ResumeRetention` 2592000000000000, `EditMaxSize` 8388608, `PublinkPaths` ["/"], `Enabled` true,
    `Secret` "e2e-secret" (compared, never printed: use QVERIFY, not QCOMPARE, for Secret).
  - T-E2E-WEBUI-CLASSIC (I-3): start flavor A, then classic webdav B, then flavor C ⇒ Mozilla GET: A has UI,
    B has no UI, C has UI.
  - T-E2E-WEBUI-SEQ: call `start(flavor D)` and `start(classic E)` back-to-back without waiting ⇒ after both
    `started`: D has UI, E has none.
  - T-E2E-WEBUI-UPSTREAM (I-5): restart the daemon with `RB_TEST_RCLONE` (upstream) ⇒ `loadCapabilities()` ⇒ No;
    `start(flavor)` ⇒ `startFailed` with `kWebuiUnsupported` and `serve/list` empty; `start(classic webdav)` ⇒
    started, Mozilla GET body lacks `__webui_`.
  Gate G-U33: `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh build qt64 && RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh e2e qt64 -- -R '^tst_e2e_serve_webui$'`
  — ctest `1 tests`, 0 failed; QtTest output lists the 6 functions PASS. Negative check: without
  `RB_RCLONE_WEBUI` (fresh configure) the same ctest filter reports `No tests were found` (not registered,
  as in CI) and `dev.sh e2e` prints the notice; then rebuild with the variable.
- **Done:** 6 functions pass with the fork; negative check recorded; supervisor review of oracle recorded;
  unit closed; commit.

### 3.4 Update README.md
- **Model:** agent-3:haiku
- **Assignment:** documentation; supervisor checks at P3.
- **Files:** `README.md`: `#faq-serve` (type list sentence, per-type bullets, "Read only" paragraph if it
  lists types), the 2.1.0 "New in" bullet only if it lists serve types (add webdav-webui there only if
  that bullet is the changelog of the next release; otherwise leave it), `CHANGELOG.md` top unreleased
  section if one exists (add entries for items 1–4; do not invent a version/date).
- **Change:**
  Preconditions: 3.1–3.3 DONE.
  S1 — Add a `webdav-webui` bullet after `webdav`: what it is (rclone's `serve webdav` with the web
  interface of the rclone webui fork: browse, upload/download files and folders, edit, delete/move,
  multiple selection, download and public links, media player, mobile layout); requires an rclone built
  from that fork — with another rclone the type is listed but the dialog says "This rclone does not support
  webdav-webui…" and OK stays disabled; the "Web interface" options (Basic/Advanced) with a table
  option → rclone flag → default, from the option table of plan phase 3; VFS cache mode and the hint;
  that the values are remembered for the next time except the secret, which is never saved; that an
  empty secret makes links stop working after a restart; that with the fork rclone the plain `webdav` type
  is served without the web interface; that the "Open in browser" button opens it.
  S2 — Verify every flag name/default against `../rclone/webui/options.go` (read-only).
  Checkpoint: record sections and the verification.
- **Unit tests:** N/A — docs.
- **e2e tests:** N/A — docs; S2 verification recorded.
- **Done:** sections accurate; unit closed; commit.

## Phase gates and closure
- Every gate command of this phase runs with `export RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone` (STATE.md §3):
  a `dev.sh build` without it resets the CMake cache and drops the webui e2e test.
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-BUILD610, G-UNIT610, G-SMOKE, G-FINAL (STATE.md §3).
- README obligation 3.4 DONE.
- agent-1:opus reviews: I-2…I-5, I-10 evidence, the reference scenario (overview.md), all four request
  items against `Evolutiva01.md`, and README accuracy.
- Open P3, record review, mark DONE, set Scope result COMPLETE, commit closure. No push/tag/release.
