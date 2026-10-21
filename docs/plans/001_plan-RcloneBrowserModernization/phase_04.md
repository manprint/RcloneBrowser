# Phase 3 — Tier-1 fixes and version policy (fixes #79, #162, #163, #172, #239; verifies #107/#117/#130/#140/#146/#250)

Intent:
- The instance lock is per user.
- "Config" opens a working terminal on Linux and macOS.
- The rclone version is parsed robustly, and rclone < 1.75.0 gets a blocking dialog.
- Update checks are asynchronous, point at manprint/RcloneBrowser, and accept a custom rclone release repo.
- Preferences can test the selected rclone.
- Config-password handling respects `RCLONE_PASSWORD_COMMAND`.
Prerequisites: P2.
Phase closure: P3; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6). Tests may use 127.0.0.1 servers only.

## Local design context
Plan revision: 6 (3.2 update-check clarifications).
- U1: the minimum rclone is 1.75.0. The support check compares only the numeric triple, so `1.75.0-beta…` and `1.75.2-DEV` count as supported.
- U4: the update check for Rclone Browser uses `manprint/RcloneBrowser`.
- D9: `RcloneVersion` replaces `compareVersion` (deleted in 3.2).
- D12: units marked CI-compiled touch `Q_OS_WIN`/`Q_OS_MACOS` code that the local images cannot compile. Keep those edits minimal; agent-1:opus reviews them; G-CI is recorded as debt.
- R8 — config password (local probe, rclone 1.75.2, 2026-10-21). For an encrypted config:
  - with no password, stderr has `CRITICAL: Failed to load config file "…": unable to decrypt configuration and not allowed to ask for password - set RCLONE_CONFIG_PASS to your configuration password`, exit 1;
  - a wrong `RCLONE_CONFIG_PASS` gives `ERROR : Couldn't decrypt configuration, most likely wrong password.` then `CRITICAL: … using RCLONE_CONFIG_PASS env password, unable to decrypt configuration`;
  - `RCLONE_PASSWORD_COMMAND="echo pw"` gives exit 0 with no prompt;
  - a failing command gives `ERROR : Using --password-command returned: exit status 1` then `CRITICAL: … password command failed: exit status 1`;
  - `rclone version` never loads the config.
  - Today's `MainWindow::getConfigPassword` prompts whenever stderr contains `RCLONE_CONFIG_PASS` (including the wrong-password case) and is silent for a failed command.
- R9 — terminals (local, host gnome-terminal 3.52.0): `gnome-terminal [--wait] -- COMMAND…` is supported; `--wait` waits for the child to exit.
  - konsole `--nofork -e`, xfce4-terminal `--disable-server -x` and xterm `-e` are established CLI conventions, not verified locally. Tests use fake terminal scripts.
  - ptyxis/kgx are unverified and are not candidates. A user can set `$TERMINAL`; an unknown basename uses the generic `-e`.
- Current code (anchors):
  - `src/main.cpp`: `QLockFile lockFile(tmpDir + "/.RcloneBrowser_4q6RgLs2RpbJA.lock")`, with `tmpDir = QDir::tempPath()` when not portable. That is `/tmp`, shared by all users on Linux (#162).
  - `src/main_window.cpp — MainWindow::rcloneConfig`:
    - Linux uses `$TERMINAL`, then gnome-terminal, xfce4-terminal, xterm, x-terminal-emulator, konsole, always with `-e "<cmd>"` (gnome-terminal ignores `-e` with a string → #163/#172/#239);
    - macOS writes `/tmp/rclone_config.command` (shared path);
    - the finished handler refreshes remotes only when `code == 0`.
  - `MainWindow::rcloneGetVersion` parses with string replace; Windows-only `compareVersion(…, "1.50")` mount warnings exist there and in `src/remote_widget.cpp` (near `Settings/rcloneVersion`).
  - Update checks are in the same function. They block on `QEventLoop` with `QNetworkAccessManager` against `rclone/rclone` and `kapitainsky/RcloneBrowser`, storing `QDate::currentDate().toString()` in `Settings/lastRcloneUpdateCheck` / `Settings/lastRcloneBrowserUpdateCheck`.
  - `src/preferences_dialog.cpp/.ui` holds `ui.rclone`, `ui.rcloneBrowse`, `ui.checkRcloneUpdates` and `ui.checkRcloneBrowserUpdates`, saved in `MainWindow` (the `Settings/checkRcloneUpdates` writes).
  - `assets/rclone-browser.appdata.xml`: summary typo `platfrom`.

## Sub-phases

### 3.1 Per-user lock, terminal launcher, appdata typo
- **Model:** agent-3:haiku
- **Assignment:** implementer. CI-compiled (macOS branch of `rcloneConfig`). agent-1:opus reviews at P3, with the macOS hunk explicitly in focus.
- **Files:**
  - NEW `src/instance_lock.h/.cpp` and `src/terminal_command.h/.cpp`, registered.
  - WRITE `src/main.cpp` (lock path) and `src/main_window.cpp` (`rcloneConfig`, Linux and macOS branches only).
  - WRITE `assets/rclone-browser.appdata.xml`.
  - NEW `tests/tst_platform_helpers.cpp` (unit) and `tests/tst_e2e_terminal.cpp` (e2e, Linux only: `if(RB_TEST_RCLONE AND UNIX AND NOT APPLE)`).
- **Change:**
  - Preconditions: P2 DONE.
  - Contract:
    ```cpp
    // instance_lock.h
    QString instanceLockFileName(bool perUser, quint64 uid);  // ".RcloneBrowser_4q6RgLs2RpbJA.lock" or ".RcloneBrowser_4q6RgLs2RpbJA_<uid>.lock"
    // terminal_command.h
    QString shellQuote(const QString &s);                      // POSIX: '…' with ' → '\''; "" → ''
    struct TerminalLaunch { QString program; QStringList args; };
    QStringList terminalCandidates(const QProcessEnvironment &env);  // [$TERMINAL if set], gnome-terminal, konsole, xfce4-terminal, xterm, x-terminal-emulator (names, not paths)
    TerminalLaunch buildTerminalLaunch(const QString &terminalPath, const QString &shellCommand);
    QString rcloneConfigShellCommand(const QString &rclone, const QStringList &confArgs); // shellQuote(rclone) + " config" + (confArgs.size()==2 ? " --config " + shellQuote(confArgs[1]) : "") + "; printf '\\n%s' 'Press Enter to close'; read _"
    ```
    - `buildTerminalLaunch` chooses by `QFileInfo(terminalPath).fileName()`:
      - `gnome-terminal`: `--wait -- sh -c <cmd>`;
      - `konsole`: `--nofork -e sh -c <cmd>`;
      - `xfce4-terminal`: `--disable-server -x sh -c <cmd>`;
      - anything else (xterm, x-terminal-emulator, $TERMINAL values): `-e sh -c <cmd>`.
      - `program = terminalPath`.
    - main.cpp: on `#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)` use `perUser = !IsPortableMode()` and `uid = getuid()` (`<unistd.h>`). Elsewhere `perUser = false`, because the macOS and Windows temp dirs are already per user. The lock is `QLockFile lockFile(tmpDir + "/" + instanceLockFileName(perUser, uid))`.
    - rcloneConfig, Linux:
      - for each candidate, `QStandardPaths::findExecutable(name)` (for `$TERMINAL`: the value itself if it is an absolute executable path, else findExecutable);
      - the first hit gives `buildTerminalLaunch(path, rcloneConfigShellCommand(GetRclone(), GetRcloneConf()))`;
      - no hit gives the existing critical message box;
      - `ApplyRcloneEnvironment(p, RcloneEnvMode::Interactive)` (already set in 1.2).
    - rcloneConfig, macOS (CI-compiled):
      - `QTemporaryFile tmp(QDir::tempPath() + "/rclone_config_XXXXXX.command")` with `setAutoRemove(false)`;
      - write `#!/bin/sh\n` plus the shell command;
      - set the existing permissions; `open <file>`.
    - All platforms: the finished handler calls `rcloneListRemotes()` regardless of the exit code, and still calls `p->deleteLater()`. The Windows branch is otherwise unchanged.
    - appdata: `platfrom` becomes `platform`.
  - Steps:
    1. S1 — Implement the helpers and the unit tests. Run `scripts/dev.sh test`.
    2. S2 — Wire main.cpp and rcloneConfig; fix appdata. Build, run the e2e and smoke.
- **Unit tests:** `tests/tst_platform_helpers.cpp`:
  - T-LOCK-NAME: `(false, 0)` gives `.RcloneBrowser_4q6RgLs2RpbJA.lock`; `(true, 1000)` gives `.RcloneBrowser_4q6RgLs2RpbJA_1000.lock`.
  - T-LOCK-CONFLICT: in a temp dir, two `QLockFile`s on the same name make the second `tryLock(100)` fail. Names for uid 1000 and 1001 both lock.
  - T-TERM-QUOTE: `a b` gives `'a b'`; `it's` gives `'it'\''s'`; `""` gives `''`.
  - T-TERM-BUILD: data-driven over `/usr/bin/gnome-terminal`, `/usr/bin/konsole`, `/usr/bin/xfce4-terminal`, `/usr/bin/xterm` and `/opt/foo/myterm`, checking the exact args from the contract.
  - T-TERM-CANDIDATES: env `TERMINAL=/x/t` puts `/x/t` first; without it, the list starts with `gnome-terminal`.
  - T-TERM-CONFIGCMD: rclone `/a b/rclone` with conf args `{"--config", "/c d/r.conf"}` gives `'/a b/rclone' config --config '/c d/r.conf'; printf …`.
  - Gate G-U3.1: `scripts/dev.sh test qt64 -- -R tst_platform_helpers` passes with 6 functions.
- **e2e tests:** `tests/tst_e2e_terminal.cpp`, T-E2E-TERMINAL-FAKE:
  - write an executable fake `<tmp>/bin/xterm`: `#!/bin/sh\nprintf '%s\n' "$@" > "$RB_ARGV_OUT"\nshift\nexec "$@"`;
  - build the launch for it with the command `printf '%s|%s' 'a b' "it's" > <tmp>/out </dev/null`;
  - run it with `QProcess` (env `RB_ARGV_OUT`) and wait ≤ 10 s;
  - expect: the argv file's first line is `-e`; `<tmp>/out` contains exactly `a b|it's`.
  - Gate G-U3.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_terminal` passes.
- **Done:** G-U3.1 and G-U3.1e pass; the macOS hunk is listed as CI debt in STATE.md §7; unit closed; completion commit.

### 3.2 Version parsing and policy, async update checks, rclone test in Preferences
- **Model:** agent-2:sonnet
- **Assignment:** implementer. CI-compiled (Windows `compareVersion` removals). agent-1:opus reviews at P3.
- **Files:**
  - NEW `src/rclone_version.h/.cpp` and `src/update_check.h/.cpp` (`Q_OBJECT` → `MOC`), registered.
  - WRITE `src/main_window.cpp`: `rcloneGetVersion`, the update-check blocks, and saving the new preference.
  - WRITE `src/remote_widget.cpp`: remove the `compareVersion` "1.50" block and keep the mount availability that path chose for supported versions.
  - WRITE `src/utils.h/.cpp`: delete `compareVersion`.
  - WRITE `src/preferences_dialog.h/.cpp/.ui`: add `rcloneUpdateRepo` (QLineEdit), `rcloneUpdateRepoError` (QLabel, hidden by default), `testRclone` (QPushButton "Test") and `rcloneTestResult` (QLabel).
  - NEW `tests/tst_rclone_version.cpp` (unit) and `tests/tst_e2e_update_check.cpp` (e2e: local QTcpServer, no external network).
- **Change:**
  - Preconditions: 3.1 DONE.
  - Contract:
    ```cpp
    struct RcloneVersion {
      int major = 0, minor = 0, patch = 0; QString suffix; bool valid = false;
      static RcloneVersion fromVersionOutput(QStringView output); // first line must match ^rclone v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.+-]+))?\s*$
      static RcloneVersion fromTag(QStringView tag);              // ^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.+-]+))?$
      QString toString() const;                                   // "1.75.2" or "1.76.0-beta.1"; "" when invalid
      int compare(const RcloneVersion &o) const;                  // triple first; equal triple: no suffix > suffix; two suffixes: QString::compare
    };
    RcloneVersion minimumRcloneVersion();                         // 1.75.0
    bool isSupportedRclone(const RcloneVersion &v);               // valid && triple >= 1.75.0 (suffix ignored)
    QString rcloneVersionProblem(const QString &versionOutput);   // "" if supported, else the dialog body text below
    ```
    - `rcloneVersionProblem` body:
      - invalid: `"Rclone Browser 2 requires rclone 1.75.0 or newer.\nCould not recognize the output of 'rclone version':\n<first line, max 200 chars>"`;
      - too old: `"Rclone Browser 2 requires rclone 1.75.0 or newer.\nFound: rclone v<toString()>"`.
    ```cpp
    struct ReleaseInfo { QString tag; RcloneVersion version; QUrl htmlUrl; };
    std::optional<ReleaseInfo> parseLatestRelease(const QByteArray &json); // needs string tag_name parseable by fromTag; html_url optional
    bool isValidRepoSlug(const QString &slug);                              // ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$
    bool updateCheckDue(const QString &storedDate, const QDate &today);    // true unless storedDate == today.toString(Qt::ISODate)
    class UpdateChecker : public QObject {
      Q_OBJECT
    public:
      explicit UpdateChecker(QObject *parent = nullptr);
      void setApiBase(const QUrl &base);     // default https://api.github.com ; tests use http://127.0.0.1:<port>
      void check(const QString &slug, const RcloneVersion &current); // GET <base>/repos/<slug>/releases/latest, header Accept: application/vnd.github+json, timeout 15 s
    signals:
      void newerAvailable(const ReleaseInfo &release);
      void upToDate();
      void failed(const QString &reason);     // HTTP error, timeout, bad JSON, invalid slug
    };
    ```
    - Version policy in `rcloneGetVersion`, on exit 0:
      - `v = RcloneVersion::fromVersionOutput(stdout)` and store `Settings/rcloneVersion = v.toString()`;
      - if `rcloneVersionProblem(stdout)` is non-empty, show a modal `QMessageBox` (Critical) with that text plus `"\n\nrclone path: " + GetRclone()`, and the buttons "Preferences…", "Download rclone" and "Quit":
        - "Preferences…" emits `ui.preferences->trigger()`;
        - "Download rclone" opens `https://rclone.org/downloads/` via QDesktopServices and shows the dialog again;
        - "Quit" calls `close()`.
      - Do not continue to `rcloneListRemotes()`, update checks or other startup work while the version is unsupported.
      - The implementer confirms (and records) that accepting Preferences re-runs `rcloneGetVersion`. If it does not, wire that call.
    - Remove both Windows `compareVersion(…, "1.50")` blocks; every supported rclone satisfies them. Delete `compareVersion`. `git grep -n compareVersion src` must be empty.
    - Update checks:
      - one `UpdateChecker` member created in the MainWindow constructor;
      - the rclone check uses slug `Settings/rcloneUpdateRepo` (default `rclone/rclone`; if `!isValidRepoSlug`, skip and log to stderr) with `current = v`;
      - the browser check uses the constant `"manprint/RcloneBrowser"` with `current = RcloneVersion::fromTag(RCLONE_BROWSER_VERSION)`;
      - both are gated by the existing check-enable settings and `updateCheckDue`. On dispatch, store `QDate::currentDate().toString(Qt::ISODate)`; a legacy locale-format value simply counts as due.
      - Messages keep today's HTML text, with links to `release.htmlUrl` (fallback: `https://github.com/<slug>/releases/latest`).
      - Distinguish signals by using two checker instances, or a `check` call that includes a context string. The implementer picks one and records it.
      - (revision 6) Update checks run only from the exit-0 path after the version is supported; the non-zero-exit branch (password retry, Preferences) starts none. If `RcloneVersion::fromTag(RCLONE_BROWSER_VERSION)` is invalid (VERSION is `2.0` until 4.2 sets `2.0.0`), the browser check is skipped with one `qWarning` line and the stored date is not written. `upToDate` shows nothing; `failed(reason)` shows nothing and logs `qWarning` with the slug and reason (today's code is silent on errors; the smoke container has no network). The blocking rule covers the `rcloneGetVersion` handler only; `autoStartMounts` (scheduled by the constructor) is unchanged.
    - Preferences:
      - `rcloneUpdateRepo` loads and saves `Settings/rcloneUpdateRepo`; on edit, an invalid slug shows `rcloneUpdateRepoError` ("Expected owner/repo") and OK is disabled;
      - `testRclone` runs `<ui.rclone text> version` in a `QProcess` with `ApplyRcloneEnvironment` and a 10 s kill timer;
      - on finish, `rcloneTestResult` shows `"<first line> — OK"` (green) if supported, `"<first line> — requires 1.75.0 or newer"` (red) if not, or `"Cannot run: <QProcess::errorString()>"` (red).
  - Steps:
    1. S1 — `rclone_version` and its unit tests. Expected: they pass.
    2. S2 — Version policy wiring and the `compareVersion` removal. Expected: it builds; the grep is empty.
    3. S3 — `update_check` and the MainWindow wiring; Preferences widgets.
    4. S4 — e2e tests, then the full `scripts/dev.sh test e2e smoke`.
  - Failure handling: the smoke test runs with host rclone 1.75.2, so the dialog must not appear. If smoke hangs on a modal dialog, the version parse is wrong; fix the parse, not the smoke.
- **Unit tests:** `tests/tst_rclone_version.cpp`:
  - T-VER-PARSE, data-driven:
    - `rclone v1.75.2\n- os/version: …` gives 1.75.2 valid;
    - `rclone v1.76.0-beta.8901.0e4a5b6c7.master` gives the suffix `beta.8901.0e4a5b6c7.master`;
    - `rclone v1.75.2-DEV` gives the suffix `DEV`;
    - `rclone 1.75.0` is valid;
    - `rclone v1.60.1-DEV` is valid;
    - `foo`, `rclone vX.Y`, `` and `rclone v1.75` are invalid.
  - T-VER-COMPARE: 1.75.2 > 1.75.0; 1.75.0 > 1.75.0-beta.1; 1.76.0-beta.1 > 1.75.2; 2.0.0 == 2.0.0.
  - T-VER-SUPPORTED: 1.75.0, 1.75.0-beta.x, 1.75.2-DEV and 1.80.0 are supported; 1.74.9, 1.60.1 and invalid are not.
  - T-VER-PROBLEM: `rcloneVersionProblem("rclone v1.75.2")` is empty. `"rclone v1.60.1"` contains `Found: rclone v1.60.1`. `"garbage"` contains `Could not recognize`.
  - T-UPD-PARSE:
    - `{"tag_name":"v2.0.0-beta.1","html_url":"https://x"}` gives version 2.0.0-beta.1 and the htmlUrl;
    - `{"tag_name":"latest"}` gives nullopt;
    - `[]` gives nullopt.
  - T-UPD-SLUG: `rclone/rclone` and `a-b/c.d_e` are valid; `rclone`, `a/b/c`, `a b/c` and empty are invalid.
  - T-UPD-DUE: stored `2026-10-21` with today 2026-10-21 is not due; `2026-10-20` is due; `Wed Oct 21 2026` (legacy) is due; empty is due.
  - Gate G-U3.2: `scripts/dev.sh test qt64 -- -R tst_rclone_version` passes with 7 functions.
- **e2e tests:** `tests/tst_e2e_update_check.cpp`:
  - A local `QTcpServer` on 127.0.0.1:0 answers one request with a canned `HTTP/1.1 <code>` response (Content-Length set, `Connection: close`). `setApiBase("http://127.0.0.1:<port>")`.
  - T-E2E-UPD-NEWER: 200 with tag `v9.9.9`, current 1.75.2. `newerAvailable` fires within 5 s with tag `v9.9.9`. The server saw the path `/repos/rclone/rclone/releases/latest`.
  - T-E2E-UPD-SAME: tag `v1.75.2` gives `upToDate`.
  - T-E2E-UPD-404: gives `failed`, with a reason containing `404`.
  - T-E2E-VERSION-REAL: run `rbRclone() version` through `ApplyRcloneEnvironment`; `isSupportedRclone(fromVersionOutput(stdout))` is true.
  - Gate G-U3.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_update_check` passes with 4 functions.
- **Done:** greps empty; G-U3.2 and G-U3.2e pass; the CI debt is noted; unit closed; completion commit.

### 3.3 Config password handling (#79) and Tier-1 verification record
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P3.
- **Files:**
  - WRITE `src/rclone_process.h/.cpp`: add the classifier.
  - WRITE `src/main_window.cpp`: `getConfigPassword`.
  - NEW `tests/fixtures/synthetic/config_errors/{needs.txt,wrong.txt,cmdfail.txt,other.txt}` holding the exact R8 stderr lines with the path replaced by `/FIXTURE`, plus a README entry. `other.txt` is the `config_fail` critical text line.
  - WRITE `tests/tst_rclone_process.cpp`: new test functions.
  - NEW `tests/tst_e2e_config_password.cpp`.
  - WRITE `docs/triage/upstream-issues.md`: the "Fork status" column for the verified issues.
- **Change:**
  - Preconditions: 3.2 DONE.
  - Contract:
    ```cpp
    enum class ConfigLoadProblem { None, NeedsPassword, WrongPassword, PasswordCommandFailed, Other };
    ConfigLoadProblem classifyConfigLoadError(int exitCode, const QByteArray &stderrBytes);
    ```
    - Rules in order:
      1. exit 0 gives None;
      2. stderr containing `password command failed` gives PasswordCommandFailed;
      3. `most likely wrong password`, or `using RCLONE_CONFIG_PASS env password`, gives WrongPassword;
      4. `not allowed to ask for password` gives NeedsPassword;
      5. anything else gives Other.
    - The match is case-sensitive and works on JSON-log or text stderr alike, because it is a substring match.
    - `getConfigPassword(p)` keeps its signature and return semantics (true means retry):
      - read `stderr = p->readAllStandardError()` and call the classifier with `p->exitCode()`;
      - NeedsPassword: the existing prompt; true if accepted.
      - WrongPassword: `SetRclonePassword("")`, then the same prompt with the label prefixed by `"Wrong password. "`; true if accepted.
      - PasswordCommandFailed: `QMessageBox::warning` with `"RCLONE_PASSWORD_COMMAND failed:\n" + lastRcloneStderrMessage(stderr)`; false (no prompt, #79).
      - Other: keep the existing `unknown command` branch for parity, otherwise false.
  - Verification record (no code): in the triage doc, set "Fork status" for:
    - #107, #130, #140 → `fixed in P2 (T-E2E-LIST-NAMES/UNKNOWNSIZE)`;
    - #117 → `fixed in P1 (T-E2E-JOB-CANCEL)`;
    - #141, #142, #160 → `fixed in P1`;
    - #162 → `fixed in 3.1`;
    - #163, #172, #239 → `fixed in 3.1 (terminal); native config in P6`;
    - #79 → `fixed in 3.3`;
    - #146, #250 → `already implemented: <setting/UI name found in autoStartMounts>`, using evidence from reading `MainWindow::autoStartMounts` and its preference key.
  - Steps:
    1. S1 — Classifier, fixtures and unit tests.
    2. S2 — `getConfigPassword` wiring.
    3. S3 — e2e tests; triage update.
- **Unit tests:** in `tests/tst_rclone_process.cpp`, T-CFGPASS-CLASSIFY, data-driven over the 4 fixtures plus `(0, "")`. Expected: NeedsPassword, WrongPassword, PasswordCommandFailed, Other, None. Gate G-U3.3: `scripts/dev.sh test qt64 -- -R tst_rclone_process` passes, with 10 functions (9 from 1.2 plus 1).
- **e2e tests:** `tests/tst_e2e_config_password.cpp`.
  - Setup: a temp `c.conf` with `[mem] type = memory`, encrypted by running `rbRclone() config encryption set --config c.conf` with `RCLONE_PASSWORD_COMMAND="echo pw"` and stdin closed. Assert the file starts with `# Encrypted rclone configuration File`.
  - T-E2E-CFGPASS-COMMAND (#79): base env plus `RCLONE_PASSWORD_COMMAND="echo pw"`, through `RcloneEnvironment(Parsed, base)`. `listremotes --long --json --ask-password=false --config c.conf` exits 0, and `parseListRemotesJson` gives `mem`. This proves the Parsed env keeps the variable.
  - T-E2E-CFGPASS-NEEDS: without a password, the classifier gives NeedsPassword.
  - T-E2E-CFGPASS-WRONG: `SetRclonePassword("bad")` gives WrongPassword. Reset with `SetRclonePassword("")`.
  - T-E2E-CFGPASS-CMDFAIL: `RCLONE_PASSWORD_COMMAND=false` gives PasswordCommandFailed.
  - Gate G-U3.3e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_config_password` passes with 4 functions.
- **Done:** G-U3.3 and G-U3.3e pass; triage column updated; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present. G-CI is debt for the 3.1/3.2 CI-compiled hunks.
- README obligation (P3): add "Requirements: rclone 1.75.0 or newer" near the top, and document the Preferences "Test" button, the custom rclone release repository and `RCLONE_PASSWORD_COMMAND` support.
- Supervisor agent-1:opus reviews, including the CI-compiled hunks. Close P3.
