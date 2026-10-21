# Phase 6 — Native remote configuration (fixes #163, #172, #239 without a terminal)

Intent:
- "Config" opens a native dialog that creates or edits remotes through the rc config state machine, including OAuth providers, with cancel leaving no partial remote.
- The terminal flow stays as "Advanced (terminal)" and as the fallback when the daemon is unavailable.
- Revision 11: an isolated real-server harness (sftp, ftp, webdav, S3 on a per-run internal Docker network) checks config, listing, transfers and file operations against real protocol servers (6.3).
Prerequisites: P5.
Phase closure: P6; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); OAuth tests use only the local 127.0.0.1:53682 server and never complete a real authorization.

## Local design context
Plan revision: 11 (10: R18 supersedes the R13 decision and removes "Show advanced options"; 11: sub-phase 6.3 and G-SERVERS, R19).
- R5 (1.75.2):
  - `config/create {name, type, parameters, opt{nonInteractive, all, continue, state, result, obscure, noObscure}}` and `config/update {name, parameters, opt{…}}` both return `{State, Option, Error, Result}`; `State == ""` means done.
  - `Option` keys: `Name, FieldName, Help, Default, DefaultStr, Value, ValueStr, Required, IsPassword, Sensitive, Advanced, Exclusive, Hide, NoPrefix, Type, Examples[{Value, Help}]`.
  - drive asks `config_is_local`. Answering `true` blocks the call until the token arrives; `config/oauthstatus` then gives `{status:"running", authUrl:"http://127.0.0.1:53682/auth?state=…"}`.
  - `config/oauthstop` makes the blocked call fail with HTTP 500 `config failed to refresh token: oauth authentication was cancelled` and leaves `[name] type = drive` behind. `config/delete {name}` removes it.
  - `config/providers` gives `{providers:[{Name, Description, Prefix, Hide, Options[…]}]}`.
- R13 (local probe 2026-10-21, sftp with `all:true`):
  - the questions come in order `host` (string, Required), `user` (Default = OS user), `port` (int, Default 22), `pass` (IsPassword), `key_pem`, … for 14 steps;
  - answering only through `opt.result` stores `pass` in clear text, even with `opt.obscure`;
  - echoing every answer collected so far in `parameters` on each `continue` call stores `pass` obscured;
  - `config/update {name, opt{nonInteractive, all}}` asks the same questions with `Default` set to the current values (`host` Default `example.invalid`).
  - Decision (revision 2, superseded by R18 in revision 10): ConfigFlow always sends the accumulated answers as `parameters`.
- R18 (local probes 2026-10-21 in isolated containers, rclone 1.75.2, sftp and drive; supersedes the R13 decision):
  - Echo in `parameters` only the answers that differ from the option's `DefaultStr`. Echo a password only when it is non-empty, and never echo a `config_*` key. With these rules `pass` is still stored obscured (`rclone reveal` gives it back), and the section holds only the non-default keys (`type, host, user, port, pass`). Echoing every answer writes every option instead, including empty ones (`key_pem = `) and an obscured empty `key_file_pass`.
  - In `config/update`, the `DefaultStr` of every option is its current value; for a password it is the obscured stored value. Answering a password with its `DefaultStr` and not echoing it keeps the stored line byte-identical. Answering `""` replaces the line with an obscured empty string, so the password is lost. There is no way to delete a password through this flow.
  - With `all: true`, the non-advanced questions end with `config_fs_advanced` (bool, Exclusive, Default `false`). Answering `true` starts the advanced options, and the flow never ends: after the last advanced option the exact `State` strings (`*all-set,N,true`) repeat. This holds with either echo rule and with `all: true` also on the continue calls.
  - drive asks, in order: `client_id`, `client_secret`, `scope`, `service_account_file`, `config_fs_advanced`, `config_shared_client_id`, then `client_id` and `client_secret` a second time, then `config_is_local` (Default `true`). A repeated option name is therefore legitimate.
  - Decision: the echo rules above, `config_fs_advanced` always answered `false`, an empty Update password answered with `DefaultStr`, and a loop guard keyed on a repeated `State`. T-CFG-PARAMS, T-CFG-LOOP, T-E2E-CONFIG-SFTP and T-E2E-CONFIG-UPDATE guard these rules.
- D8: native config with nonInteractive and all, OAuth polling, cleanup on cancel, terminal fallback.
- Advanced options (revision 10, R18): `config_fs_advanced` is always answered `false` automatically, so rclone never asks the advanced options. If rclone still sends an option with `Advanced == true`, it is answered automatically with its `DefaultStr`. Advanced options are set through "Advanced (terminal)…". There is no "Show advanced options" checkbox.
- Current code (anchors):
  - `src/main_window.cpp`: `QObject::connect(ui.config, &QPushButton::clicked, this, &MainWindow::rcloneConfig)`; `MainWindow::rcloneConfig` (the 3.1 terminal launcher); `ui.refresh` triggers `rcloneListRemotes`.
  - `global.daemon` (5.2).

### Contract (NEW `src/config_flow.h/.cpp`)
```cpp
struct ConfigOption {
  QString name, help, type, defaultStr;
  bool required = false, isPassword = false, exclusive = false, advanced = false;
  QVector<QPair<QString, QString>> examples;   // (Value, Help)
};
ConfigOption configOptionFromJson(const QJsonObject &o);       // missing keys → defaults; Examples null → empty
struct ConfigStep { QString state; std::optional<ConfigOption> option; QString error; QString result; bool done = false; };
ConfigStep parseConfigStep(const QJsonObject &reply);           // done = (State == "")
struct ProviderInfo { QString name, description, prefix; bool hide = false; };
QVector<ProviderInfo> parseProviders(const QJsonObject &reply); // sorted by description, case-insensitive; Hide entries kept with hide=true
QString configAnswerText(const ConfigOption &o, const QVariant &v);  // bool option ("bool" type) → "true"/"false"; else v.toString()
bool isValidRemoteName(const QString &name);                    // ^[\w.+@](?:[\w.+@ -]*[\w.+@-])?$ with QRegularExpression::UseUnicodePropertiesOption (rclone rule: no leading '-' or space, no trailing space, no ':' '/' '\\')

using RcCallFn = std::function<void(const QString &path, const QJsonObject &params,
                                    std::function<void(const RcReply &)> done, int timeoutMs)>;

class ConfigFlow : public QObject {
  Q_OBJECT
public:
  enum class Mode { Create, Update };
  ConfigFlow(RcCallFn call, QObject *parent = nullptr);
  void startCreate(const QString &name, const QString &type);
  void startUpdate(const QString &name);
  void answer(const QVariant &value);           // only valid while waiting on a question
  void cancel();
  bool isOAuthRunning() const;
signals:
  void question(const ConfigOption &option, const QString &error);
  void oauthUrl(const QUrl &url);
  void finished(bool ok, const QString &message);
};
```
Request rules:
- Start:
  - Create: `config/create {name, type, parameters: {}, opt: {nonInteractive: true, all: true}}`.
  - Update: `config/update {name, parameters: {}, opt: {nonInteractive: true, all: true}}`.
- Each step reply goes through `parseConfigStep`:
  - done: `finished(true, "")`.
  - auto-answered with no signal: `config_fs_advanced` with `false`, and an option with `advanced == true` with its `defaultStr`.
  - otherwise: `question(option, step.error)`.
- Loop guard: the flow records the `State` of every auto-answered step. It stops when an auto-answered step's `State` was already auto-answered in this flow, or after 500 steps in total. It then emits `finished(false, "rclone config did not finish (state <State>)")` and runs the same cleanup as an error. The guard is not keyed on option names, because drive asks `client_id` twice (R18).
- `answer(v)`:
  - `text = configAnswerText(option, v)`. In Update mode an empty answer to an `isPassword` option becomes `option.defaultStr`, which keeps the stored value (R18).
  - `params[option.name] = text` only when the name does not start with `config_`, `text != option.defaultStr`, and, for an `isPassword` option, `text` is non-empty. Otherwise `params` has no entry for the name, and an earlier entry for it is removed. Auto-answers follow the same rule.
  - send `config/create {name, type, parameters: params, opt: {nonInteractive: true, continue: true, state: step.state, result: text}}` (Update: `config/update` without `type`).
  - Timeout is 30 s, except `option.name == "config_is_local" && text == "true"`, which gets 900000 ms and starts OAuth polling.
- OAuth polling: every 500 ms, `config/oauthstatus {}`. The first non-empty `authUrl` emits `oauthUrl` once. Polling stops when the blocked call returns, or on cancel.
- An HTTP error from any step gives `finished(false, error)`. A newly created remote is then deleted with `config/delete {name}` (Create mode only, only if it was absent at start; see `cancel`).
- Create with a name that the start `config/listremotes` already lists: `finished(false, "remote <name> already exists")`, with no `config/create` and no cleanup.
- In-flight calls: cleanup starts only when no step call is in flight. A `cancel()` during a step call (not the blocked OAuth call) waits for that reply, ignores its content, then cleans up. After `finished`, every late reply is ignored.
- `cancel()`:
  - if OAuth is running: `config/oauthstop {}`; the blocked call then fails, which leads to cleanup;
  - otherwise: Create mode deletes `name` if it now exists (`config/listremotes` contains it and it was absent at start; the start is preceded by one `config/listremotes` call to record this);
  - then `finished(false, "cancelled")`, emitted exactly once.

## Sub-phases

### 6.1 ConfigFlow state machine
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews before S1 (request rules and the cleanup rule) and after the diff.
- **Files:**
  - NEW `src/config_flow.h/.cpp` (`Q_OBJECT` → `MOC`), registered.
  - NEW `tests/tst_config_flow.cpp` (unit, scripted fake `RcCallFn`) and `tests/tst_e2e_config_flow.cpp` (e2e, real daemon).
- **Change:**
  - Preconditions: P5 DONE.
  - Contract: above.
  - Steps:
    1. S1 — Pure parsers plus their unit tests.
    2. S2 — The ConfigFlow machine plus fake-driven unit tests.
    3. S3 — e2e against the real rcd.
  - Failure handling: if the real daemon disagrees with R13 (for example the password is not obscured), stop and escalate. Do not add a `config/password` workaround without review.
- **Unit tests:** `tests/tst_config_flow.cpp`. The fake `RcCallFn` records `(path, params, timeoutMs)` and replies from a script, always asynchronously (queued with `QTimer::singleShot(0, …)`), like `RcClient::call`.
  - T-CFG-OPTION: the sftp `pass` option JSON (from R13) maps to isPassword true, type `string`, examples empty. An Exclusive option with 2 Examples maps exactly.
  - T-CFG-STEP: `{State:"", Error:"", Result:""}` is done. `{State:"*all,1,", Option:{Name:"host"}}` is not done, with option host.
  - T-CFG-PROVIDERS: a 3-provider reply is sorted by description, and the Hide flag is kept.
  - T-CFG-NAME: `a`, `my remote`, `x.y@z+1-2` are valid; ` a`, `a `, `-a`, `a:b`, `a/b` and empty are invalid.
  - T-CFG-ANSWER:
    - Script: create gives question host; `answer("h")` sends params `{host:"h"}`, `opt.result == "h"`, `opt.continue == true`; the reply is question pass; `answer("pw")` sends params `{host:"h", pass:"pw"}`; the reply is done, giving `finished(true)`.
    - The first recorded call is `config/listremotes`.
  - T-CFG-ADVANCED: a `config_fs_advanced` step is answered with `opt.result == "false"` and no `question` is emitted. An option with Advanced true is answered automatically with its DefaultStr, again with no `question`.
  - T-CFG-PARAMS (R18):
    - answering an option with its DefaultStr sends it as `opt.result` and leaves it out of `parameters`;
    - no recorded `parameters` ever contains a `config_*` key;
    - in Update mode, an empty answer to an isPassword option whose DefaultStr is `OBSC` sends `opt.result == "OBSC"` and no entry for it in `parameters`;
    - in Create mode, an empty password sends `opt.result == ""` and no entry in `parameters`.
  - T-CFG-LOOP: the fake replies to an auto-answered step with the same `State` again. The result is `finished(false, msg)` exactly once, with msg containing `did not finish`. In Create mode the cleanup follows: `config/listremotes`, then `config/delete` when the fake lists the name.
  - T-CFG-OAUTH:
    - answering `config_is_local` with true uses timeout 900000;
    - the fake answers `config/oauthstatus` with an authUrl on the second poll; `oauthUrl` is emitted once;
    - `cancel()` sends `config/oauthstop`; the fake then fails the blocked call with 500; `config/delete {name}` is sent; `finished(false, …)` is emitted once.
  - T-CFG-CANCEL-EXISTING: Update mode cancel never sends `config/delete`. A Create whose name the start `config/listremotes` already lists gives `finished(false, …)` containing `already exists`, with no `config/create` and no `config/delete`.
  - Gate G-U6.1: `scripts/dev.sh test qt64 -- -R tst_config_flow` passes with 10 functions.
- **e2e tests:** `tests/tst_e2e_config_flow.cpp`. A real `RcloneDaemon` on an empty temp conf; `RcCallFn` bound to `daemon.client()->call`.
  - T-E2E-CONFIG-SFTP:
    - create `s` of type sftp, answering host `example.invalid`, user `bob`, port `2222`, pass `s3cret`, and defaults otherwise (`config_fs_advanced` is auto-answered `false`);
    - `finished(true)`;
    - the conf has `[s]` with `host = example.invalid` and a `pass` value `v` that is not `s3cret`;
    - `rclone reveal v` prints `s3cret`;
    - the `[s]` section has exactly the keys `type`, `host`, `user`, `port` and `pass`, with no empty-value lines (R18).
  - T-E2E-CONFIG-UPDATE:
    - `startUpdate("s")` asks `host` first, with `defaultStr == "example.invalid"`;
    - answering `other.invalid`, leaving the `pass` answer empty and taking defaults otherwise changes the conf host. The `pass` line is byte-identical to before, and `user` and `port` are unchanged (R18);
    - a second update that answers `pass` with `n3w` gives a `pass` value that `rclone reveal` prints as `n3w`.
  - T-E2E-CONFIG-OAUTH-CANCEL:
    - create `g` of type drive, answering defaults until `config_is_local`, then `true`;
    - `oauthUrl` arrives within 10 s and starts with `http://127.0.0.1:53682/auth`;
    - `cancel()`; `finished(false)` within 10 s;
    - the conf contains no `[g]`;
    - no listener remains on 53682 (a connect to `127.0.0.1:53682` is refused within 5 s).
  - T-E2E-CONFIG-BADTYPE: create with type `nosuch` gives `finished(false, msg)` with a non-empty msg; the conf is unchanged.
  - Gate G-U6.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_config_flow` passes with 4 functions.
- **Done:** reviews recorded; G-U6.1 and G-U6.1e pass; unit closed; completion commit.

### 6.2 RemoteConfigDialog and MainWindow wiring
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P6.
- **Files:**
  - NEW `src/remote_config_dialog.h/.cpp` (`Q_OBJECT` → `MOC`; widgets in code, no .ui), registered.
  - WRITE `src/main_window.h/.cpp`: the `ui.config` handler, a remote context-menu "Edit remote…", and a menu or button entry "Advanced (terminal)…" calling `rcloneConfig`.
  - NEW `tests/tst_remote_config_dialog.cpp` (unit, `QTEST_MAIN`, fake `RcCallFn`).
- **Change:**
  - Preconditions: 6.1 DONE.
  - Contract (`RemoteConfigDialog(RcCallFn call, Mode mode, const QString &existingName, QWidget *parent)`):
    - Create mode, page 1:
      - `QLineEdit name` (validated by `isValidRemoteName`, plus "already exists" from `config/listremotes`);
      - `QLineEdit filter` plus `QListWidget providers` (from `config/providers`, hidden ones excluded, item text `"<Description> (<Name>)"`, data = Name);
      - Next is enabled when the name is valid and a provider is selected.
      - The objectNames are as written, and tests depend on them.
    - Question page (one widget set, rebuilt per question):
      - `QLabel title` (option name);
      - `QLabel help` (plain text, word-wrapped, the first 1200 chars);
      - `QLabel error` (red, hidden if empty);
      - editor `answerEditor` by rule:
        - type `bool` gives a QCheckBox, checked = defaultStr == "true";
        - exclusive with examples gives a non-editable QComboBox (items = example values, tooltips = example help);
        - non-exclusive with examples gives an editable QComboBox;
        - isPassword gives a QLineEdit with `Password` echo mode, empty initially (never prefilled). In Update mode with a non-empty defaultStr its placeholder text is `(unchanged)`: an empty answer keeps the stored password (R18);
        - otherwise a QLineEdit prefilled with defaultStr.
      - Required with empty text disables Next.
      - Next calls `flow.answer(value)` and disables the page until the next signal.
    - OAuth page: the text "Complete the sign-in in your browser. If it did not open, use this link:", a `QLabel oauthLink` with a clickable URL, `QDesktopServices::openUrl` called once, and a Cancel button that calls `flow.cancel()`.
    - Done: `finished(true)` gives `accept()`. `finished(false, msg)` gives a `QMessageBox::warning` with msg unless msg is "cancelled", then `reject()`. The dialog Cancel or close calls `flow.cancel()` and waits for `finished` (≤ 10 s, then reject).
    - MainWindow:
      - `ui.config` clicked: if `global.daemon` is Running, open `RemoteConfigDialog` (Create), and on accept call `rcloneListRemotes()`; otherwise call `rcloneConfig()` (terminal);
      - the remote item context menu gains "Edit remote…" (Update mode for the selected item's name; daemon only) and "Advanced (terminal)…" (always).
  - Steps:
    1. S1 — The dialog plus unit tests.
    2. S2 — MainWindow wiring; smoke.
- **Unit tests:** `tests/tst_remote_config_dialog.cpp` (offscreen, fake `RcCallFn` scripted with the provider list and a 3-question flow):
  - T-RCD-PAGE1: the `providers` list excludes the Hide entry. Next is disabled for the name `a:b` and enabled for `ok` plus a selection.
  - T-RCD-EDITORS: for the questions bool, exclusive-examples, password and string, `answerEditor` is respectively a QCheckBox; a QComboBox with `isEditable()` false; a QLineEdit with `echoMode() == Password` and empty text; a QLineEdit with the default text.
  - T-RCD-REQUIRED: a Required question with empty input disables Next.
  - T-RCD-FLOW: driving Next through 3 questions gives recorded params containing all 3 answers; the final done reply leads to `accept()` (`result() == QDialog::Accepted`).
  - T-RCD-CANCEL: cancel on a question page sends `config/delete` for Create (fake reports the name as created) and leads to `reject()`.
  - Gate G-U6.2: `scripts/dev.sh test qt64 -- -R tst_remote_config_dialog` passes with 5 functions.
- **e2e tests:** T-E2E-CONFIG-SFTP / -UPDATE / -OAUTH-CANCEL in 6.1 cover the real flow. The dialog-to-daemon integration is covered by G-SMOKE plus T-RCD-FLOW. A dialog e2e against real rcd is N/A because it would duplicate 6.1 through UI automation without new observable behavior.
- **Done:** G-U6.2 passes; smoke passes; unit closed; completion commit.

### 6.3 Isolated real-server harness and protocol e2e (revision 11)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff (I-9 and network isolation first).
- **Files:**
  - NEW `tests/servers/webdav/Dockerfile`, `tests/servers/webdav/httpd.conf`, `tests/servers/webdav/entry.sh` (contents below).
  - WRITE `scripts/dev.sh`: new commands `server-images` and `servers`; usage text.
  - NEW `tests/servers/tst_srv_protocols.cpp`, registered in `tests/CMakeLists.txt` inside `if(RB_TEST_RCLONE)` with the ctest label `servers` only (not `e2e`, not `unit`). The file lives outside the `tests/tst_*.cpp` glob, so the G-UNIT and G-E2E file counts do not change.
- **Change:**
  - Preconditions: 6.1 DONE (the tests create every remote through `ConfigFlow`).
  - Evidence (R19, local probe 2026-10-21, rclone 1.75.2, isolated `--internal` network, scratchpad `servers/`):
    - `atmoz/sftp:alpine` with the command `rb:rbpass:::upload` serves sftp; only `upload/` is writable.
    - `delfer/alpine-ftp-server:latest` with `USERS="rb|rbpass|/home/rb|1000"` and `ADDRESS=ftp` refuses writes with `553 Could not create file.` unless the user is chrooted: start it with `--entrypoint /sbin/tini … -- sh -c 'printf "chroot_local_user=YES\nallow_writeable_chroot=YES\n" >> /etc/vsftpd/vsftpd.conf && exec /bin/start_vsftpd.sh'`.
    - `httpd:2.4-alpine` with mod_dav needs `DavLockDB`; the image has no DBM driver (`DSO load failed`, every request 500). `RUN apk add --no-cache apr-util-dbm_gdbm` fixes it.
    - `minio/minio` is no longer pullable (pull access denied). `ghcr.io/coollabsio/minio:RELEASE.2025-10-15T17-29-55Z` with `-e MINIO_ROOT_USER=rbadmin -e MINIO_ROOT_PASSWORD=rbpass1234 … server /data` works with `provider = Minio`, `endpoint = http://s3:9000`.
    - With these four, `rclone mkdir`, `copy`, `lsjson -R`, `moveto` and `purge` all succeed (S3 `mkdir` of an empty directory is a no-op).
  - `tests/servers/webdav/Dockerfile`: `FROM httpd:2.4-alpine` + `RUN apk add --no-cache apr-util-dbm_gdbm`. `httpd.conf`: `Listen 80`; modules mpm_event, authn_file, authn_core, authz_user, authz_core, auth_basic, dav, dav_fs, dav_lock, unixd, dir, mime, log_config; `User daemon`/`Group daemon`; `DavLockDB /usr/local/apache2/var/DavLock`; `DocumentRoot /usr/local/apache2/dav` with `Dav On`, Basic auth from `/usr/local/apache2/conf/htpasswd`, `Require valid-user`; logs to `/proc/self/fd/1` and `/proc/self/fd/2`. `entry.sh`: create and chown the dav and var dirs to daemon, `htpasswd -bc … rb rbpass`, `exec httpd -f /usr/local/apache2/conf/rb-httpd.conf -DFOREGROUND`.
  - `scripts/dev.sh server-images` (needs network, like `image`): `docker pull atmoz/sftp:alpine delfer/alpine-ftp-server:latest ghcr.io/coollabsio/minio:RELEASE.2025-10-15T17-29-55Z` (one pull per image) and `docker build -t rb-srv-webdav:1 tests/servers/webdav`.
  - `scripts/dev.sh servers [flavor] [-- <ctest args>]`:
    1. Same rclone version check and build image check as the other commands. A missing server image exits 3 with `dev.sh: server image <img> missing; run scripts/dev.sh server-images (needs network)`.
    2. `NET=rb-srv-$$`; `docker network create --internal "$NET"`; abort (exit 5) unless `docker network inspect -f '{{.Internal}}' "$NET"` prints `true`.
    3. `trap` on EXIT removes the four containers and the network, whatever the outcome.
    4. Start the servers detached on `$NET` only, with `--network-alias sftp|ftp|webdav|s3` and names `$NET-sftp` etc. No `-p`/`--publish`: nothing is reachable from the host or any other host. Credentials are fixed test strings (`rb`/`rbpass`, `rbadmin`/`rbpass1234`), never real ones.
    5. Run the build image like `e2e`, with the same `--user`, `HOME`, `RCLONE_CONFIG` and mounts, but `--network "$NET"` instead of `--network none`, plus `-e RB_SRV=1`, and `ctest --test-dir $BDIR -L servers --output-on-failure` plus the passthrough args.
  - I-9 extension (revision 11): real-protocol tests use only servers started by `dev.sh servers` on a per-run `--internal` Docker network with no published ports. The client container joins only that network. No test reaches another host or a real account.
  - `tests/servers/tst_srv_protocols.cpp`:
    - Without `RB_SRV=1` every function `QSKIP`s with "run through scripts/dev.sh servers" (a plain `ctest` outside the harness stays green; G-SERVERS forbids SKIP).
    - `initTestCase`: `rbInitTestEnvironment()`; a real `RcloneDaemon` on an empty temp conf (I-9); for each backend wait up to 30 s for the server to answer (retry `rclone lsd <probe>` with a connection string, 1 s apart).
    - Data rows `sftp`, `ftp`, `webdav`, `s3`. Each row has the remote type, the answers by option name, and the writable prefix: sftp `{host: sftp, user: rb, pass: rbpass}` prefix `upload/`; ftp `{host: ftp, user: rb, pass: rbpass}` prefix ``; webdav `{url: http://webdav/, vendor: other, user: rb, pass: rbpass}` prefix ``; s3 `{provider: Minio, env_auth: false, access_key_id: rbadmin, secret_access_key: rbpass1234, endpoint: http://s3:9000}` prefix `rbt/` (bucket created with `rclone mkdir`). Questions not in the map take `defaultStr`.
- **Unit tests:** N/A (the parsers and the flow have their unit tests in 6.1; this unit adds protocol coverage).
- **e2e tests:** `tests/servers/tst_srv_protocols.cpp`, one row per backend in each function:
  - T-PROTO-CONFIG: `ConfigFlow::startCreate("rb-<row>", type)` answered from the map gives `finished(true)`; `config/listremotes` contains the name; for sftp, ftp and webdav the conf `pass` value is not `rbpass` and `rclone reveal` gives `rbpass`.
  - T-PROTO-LISTING: seed `<prefix>rb-list/` from a temp dir with `a b.txt` (6 bytes), `[x] (y).txt`, `ümlaut.txt` and `sub/c.txt`. `ItemModel` listings of `rb-<row>:<prefix>rb-list` through rc and through lsjson (the two paths of `tst_e2e_rclisting`) give the same (name, isDir, size) set, equal to the seeded set.
  - T-PROTO-TRANSFER: a 3 MiB random file is uploaded to `<prefix>rb-xfer/` through the job pipeline used by `tst_e2e_jobwidget` (`PrepareTransferArgs`, the Parsed environment, `TransferMonitor`): exit 0 and final stats bytes 3145728. A download into a new temp dir gives the same SHA-256.
  - T-PROTO-OPS: with the Parsed environment, `mkdir <prefix>rb-ops/d1` (skip the empty-dir check on s3), `copyto` a file into `d1`, `moveto` it to a new name, `deletefile`, `purge <prefix>rb-ops`. After each step `lsjson` shows the expected state.
  - Gate G-SERVERS (local only, never in CI): `scripts/dev.sh servers qt64 -- -V` exits 0 with `100% tests passed, 0 tests failed out of 1` (revision 12: `Total Tests:` is printed only by `ctest -N`), PASS for the 4 functions with all 4 rows and no SKIP. Afterwards `docker ps -a --filter name=rb-srv- -q` and `docker network ls --filter name=rb-srv- -q` print nothing.
- **Done:** G-SERVERS passes (twice in a row), plus G-UNIT and G-E2E counts unchanged; reviews recorded; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE, G-SERVERS (revision 11). G-BUILD610 if the image is present.
- README obligation (P6): replace the "Config opens a terminal" text with the native dialog (create/edit, OAuth sign-in in the browser, "Advanced (terminal)" fallback). Also state that advanced options are set only through "Advanced (terminal)…", and that a password left empty when editing stays unchanged. A stored password cannot be removed in the dialog (R18). Revision 12: Cancel while editing keeps the answers already confirmed with Next, because rclone saves each answer as it is given (probe 2026-10-22); Cancel while creating removes the new remote. In the developer section, document `scripts/dev.sh server-images` and `scripts/dev.sh servers` (isolated servers, internal network, no published ports).
- Supervisor agent-1:opus reviews. Close P6.
