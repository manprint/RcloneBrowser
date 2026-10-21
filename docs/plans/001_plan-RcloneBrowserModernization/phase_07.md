# Phase 6 — Native remote configuration (fixes #163, #172, #239 without a terminal)

Intent:
- "Config" opens a native dialog that creates or edits remotes through the rc config state machine, including OAuth providers, with cancel leaving no partial remote.
- The terminal flow stays as "Advanced (terminal)" and as the fallback when the daemon is unavailable.
Prerequisites: P5.
Phase closure: P6; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); OAuth tests use only the local 127.0.0.1:53682 server and never complete a real authorization.

## Local design context
Plan revision: 1.
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
  - Decision: ConfigFlow always sends the accumulated answers as `parameters` (as `rclone config create --help` says: "Any default config values should be passed in with each invocation of --continue"). T-E2E-CONFIG-SFTP guards this with `rclone reveal`.
- D8: native config with nonInteractive and all, OAuth polling, cleanup on cancel, terminal fallback.
- Advanced options: with `all:true`, options with `Advanced == true` are answered automatically with `DefaultStr` unless the user checked "Show advanced options". This keeps the first screen short.
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
  void setShowAdvanced(bool on);
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
  - option advanced and not showAdvanced: answer automatically with `defaultStr`, with no signal.
  - otherwise: `question(option, step.error)`.
- `answer(v)`:
  - `text = configAnswerText(option, v)`; `params[option.name] = text`;
  - send `config/create {name, type, parameters: params, opt: {nonInteractive: true, continue: true, state: step.state, result: text}}` (Update: `config/update` without `type`).
  - Timeout is 30 s, except `option.name == "config_is_local" && text == "true"`, which gets 900000 ms and starts OAuth polling.
- OAuth polling: every 500 ms, `config/oauthstatus {}`. The first non-empty `authUrl` emits `oauthUrl` once. Polling stops when the blocked call returns, or on cancel.
- An HTTP error from any step gives `finished(false, error)`. A newly created remote is then deleted with `config/delete {name}` (Create mode only, only if it was absent at start; see `cancel`).
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
- **Unit tests:** `tests/tst_config_flow.cpp`. The fake `RcCallFn` records `(path, params)` and replies from a script.
  - T-CFG-OPTION: the sftp `pass` option JSON (from R13) maps to isPassword true, type `string`, examples empty. An Exclusive option with 2 Examples maps exactly.
  - T-CFG-STEP: `{State:"", Error:"", Result:""}` is done. `{State:"*all,1,", Option:{Name:"host"}}` is not done, with option host.
  - T-CFG-PROVIDERS: a 3-provider reply is sorted by description, and the Hide flag is kept.
  - T-CFG-NAME: `a`, `my remote`, `x.y@z+1-2` are valid; ` a`, `a `, `-a`, `a:b`, `a/b` and empty are invalid.
  - T-CFG-ANSWER:
    - Script: create gives question host; `answer("h")` sends params `{host:"h"}`, `opt.result == "h"`, `opt.continue == true`; the reply is question pass; `answer("pw")` sends params `{host:"h", pass:"pw"}`; the reply is done, giving `finished(true)`.
    - The first recorded call is `config/listremotes`.
  - T-CFG-ADVANCED: an option with Advanced true is answered automatically with its DefaultStr, and no `question` is emitted. With `setShowAdvanced(true)`, it is emitted.
  - T-CFG-OAUTH:
    - answering `config_is_local` with true uses timeout 900000;
    - the fake answers `config/oauthstatus` with an authUrl on the second poll; `oauthUrl` is emitted once;
    - `cancel()` sends `config/oauthstop`; the fake then fails the blocked call with 500; `config/delete {name}` is sent; `finished(false, …)` is emitted once.
  - T-CFG-CANCEL-EXISTING: Update mode cancel never sends `config/delete`.
  - Gate G-U6.1: `scripts/dev.sh test qt64 -- -R tst_config_flow` passes with 8 functions.
- **e2e tests:** `tests/tst_e2e_config_flow.cpp`. A real `RcloneDaemon` on an empty temp conf; `RcCallFn` bound to `daemon.client()->call`.
  - T-E2E-CONFIG-SFTP:
    - create `s` of type sftp, answering host `example.invalid`, user `bob`, port `2222`, pass `s3cret`, and defaults otherwise (showAdvanced false);
    - `finished(true)`;
    - the conf has `[s]` with `host = example.invalid` and a `pass` value `v` that is not `s3cret`;
    - `rclone reveal v` prints `s3cret`.
  - T-E2E-CONFIG-UPDATE: `startUpdate("s")` asks `host` first, with `defaultStr == "example.invalid"`. Answering `other.invalid`, then defaults, changes the conf host.
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
      - `QCheckBox showAdvanced`;
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
        - isPassword gives a QLineEdit with `Password` echo mode, empty initially (never prefilled);
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

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation (P6): replace the "Config opens a terminal" text with the native dialog (create/edit, OAuth sign-in in the browser, "Advanced (terminal)" fallback).
- Supervisor agent-1:opus reviews. Close P6.
