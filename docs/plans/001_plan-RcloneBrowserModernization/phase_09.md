# Phase 8 — Bisync, copyto, copyurl (#243, #199)

Intent:
- Transfers support `rclone bisync` as a fourth mode with its key options, persisted in tasks.
- Users can copy a single file to an explicit destination name (`copyto`) and upload from a URL (`copyurl`).
- All of them run through `addTransfer`, so the JSON engine monitors them.
Prerequisites: P14 (revision 11; P14 follows P7).
Phase closure: P8; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); URL tests use a local `rclone serve http` on 127.0.0.1.

## Local design context
Plan revision: 1.
- R14 (local probe, rclone 1.75.2, 2026-10-21):
  - bisync flags: `--resync` (`-1`), `--resync-mode`, `--conflict-resolve none|path1|path2|newer|older|larger|smaller`, `--conflict-loser`, `--conflict-suffix`, `--resilient`, `--recover`, `--check-access`, `--max-lock`, `--compare`, `--create-empty-src-dirs`, `--workdir`, `--force`.
  - A first run without `--resync` exits 7 with error lines `Bisync critical error: cannot find prior Path1 or Path2 listings…` and `Bisync aborted. Must run --resync to recover.`. With `--resync` it exits 0 and logs `Copied (new)` per file.
  - Messages contain ANSI color codes unless `--color NEVER` is given (handled in P1).
  - Long absolute Path1+Path2 produce a listing/lock filename over 255 bytes, giving `Lock file exists, but contents are unreadable` and exit 1. This is an rclone limitation. The names are built from both full paths (`bilib.BasePath` = `filepath.Join(workDir, SessionName)`, `SessionName` = canonical(Path1) + ".." + canonical(Path2), no truncation or hashing), so `--workdir` only chooses their folder and cannot shorten them; only shorter folder paths help (revision 22).
  - `copyto a.txt "out/renamed a.txt"` logs `info` `object a.txt` `Copied (new) to: renamed a.txt`.
  - `copyurl <url> <dir>/ -a -p` downloads and prints the file name as a plain stdout line (a Text event). Flags: `-a/--auto-filename`, `-p/--print-filename`, `--no-clobber`, `--header-filename`, `--stdout`.
- I-7: task files with classVersion ≤ 8 load with identical field values. 8.1 bumps classVersion to 9. Phase 13.1 bumps it to 10.
- Current code (anchors):
  - `src/job_options.h`: `enum Operation { UnknownOp, Copy, Move, Sync, Mount, Check, CryptCheck }`; `classVersion = 8` in `job_options.cpp`.
  - `JobOptions::getOptions()` starts with `copy|move|sync`, then source and dest, then flags; sync-timing flags come from `syncTiming`.
  - `src/list_of_job_options.cpp`: `operator<<` writes every field; `operator>>` reads fields gated by `actualVersion >= N`.
  - `src/transfer_dialog.cpp/.ui`: `rbCopy`, `rbMove`, `rbSync`; `getMode()` returns `"Copy"|"Move"|"Sync"`; `getOptions()` and `setOptions()` map the radios to `operation`.
  - `MainWindow::runItem` dispatches saved tasks by operation.
  - `src/remote_widget.cpp` emits `addTransfer(info, src, dst, args, uuid, "", uuid)` (pattern near its upload/download handlers); RemoteWidget actions live in `remote_widget.ui` (`upload`, `download`, `copy`, `move`, …).

## Sub-phases

### 8.1 Bisync mode (#243)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P8, with serialization compatibility (I-7) in focus.
- **Files:**
  - WRITE `src/job_options.h/.cpp`, `src/list_of_job_options.cpp`, `src/transfer_dialog.h/.cpp/.ui` and `src/main_window_tasks.cpp` (`runItem` dispatch, task list labels).
  - NEW `tests/tst_job_options.cpp` (unit) and `tests/fixtures/synthetic/jobopts_v8.bin` (recorded in S1 with the unmodified code).
  - NEW `tests/tst_e2e_bisync.cpp` (e2e).
- **Change:**
  - Preconditions: P7 DONE.
  - Contract:
    - `enum Operation { UnknownOp, Copy, Move, Sync, Mount, Check, CryptCheck, Bisync }`, so Bisync is appended with value 7. `classVersion = 9`.
    - New fields, with their defaults:
      - `bool bisyncResync = false;`
      - `QString bisyncConflictResolve = "none";`
      - `bool bisyncResilient = false;`
      - `bool bisyncRecover = false;`
      - `QString bisyncWorkdir;` (empty means rclone's default).
    - `operator<<` appends them after `deleteEmptySrcDirs`. `operator>>` reads them under `if (actualVersion >= 9)`.
    - `getOptions()` for Bisync:
      - the list starts with `bisync`, source, dest;
      - then the same common flags as Sync, except that no `syncTiming` flag (`--delete-before|--delete-during|--delete-after`) is emitted;
      - then `--resync` if set, `--conflict-resolve <v>` if v ≠ "none", `--resilient` and `--recover` if set, and `--workdir <dir>` if non-empty.
      - `createEmptySrcDirs` keeps mapping to `--create-empty-src-dirs`, which bisync supports.
    - TransferDialog:
      - radio `rbBisync` ("Bisync") after `rbSync`, plus a `QGroupBox groupBisync` visible only while `rbBisync` is checked, holding:
        - `cbBisyncResync` ("First run / resync (--resync)");
        - `cbBisyncConflict` (QComboBox: none, newer, older, larger, smaller, path1, path2);
        - `cbBisyncResilient`, `cbBisyncRecover`;
        - `leBisyncWorkdir`;
      - `getMode()` returns `"Bisync"`; `getOptions()` and `setOptions()` map all of these.
      - `rbBisync` is enabled in both upload and download modes. Bisync needs only Path1 (source) and Path2 (dest), which both modes already compose.
    - `runItem` and the task list treat Bisync like Sync (same `addTransfer` path, label "Bisync").
  - Steps:
    1. S1 — Before any code change, write `tests/tst_job_options.cpp` with a recorder:
       - with `RB_RECORD_JO_V8=1`, serialize a JobOptions with distinctive non-default values for every v8 field to `jobopts_v8.bin` (QDataStream `Qt_5_2`, the same as ListOfJobOptions), then QSKIP;
       - record it with the unmodified code; record the fixture's sha256 in STATE.md §6.
    2. S2 — JobOptions changes and the serialization. Expected: T-JO-COMPAT and T-JO-ROUNDTRIP pass.
    3. S3 — TransferDialog UI and runItem wiring. Expected: it builds; T-MW-SNAPSHOT is re-recorded only if the main window snapshot changes. It should not, because the dialog is separate.
    4. S4 — e2e; full gates.
  - Failure handling: if bisync rejects a common flag emitted for Sync, add that flag to the Bisync exclusion list, record it, and add a T-JO-BISYNC-ARGS row.
- **Unit tests:** `tests/tst_job_options.cpp`:
  - T-JO-COMPAT (I-7): read `jobopts_v8.bin` with the new code. Every v8 field equals the recorded distinctive value, and the new fields have their defaults.
  - T-JO-ROUNDTRIP: a v9 JobOptions with Bisync and all bisync fields set survives a `<<`/`>>` round trip unchanged.
  - T-JO-BISYNC-ARGS:
    - Bisync with resync, conflict `newer`, resilient, recover and workdir `/w` gives `getOptions()` starting `bisync <src> <dst>`;
    - it contains `--resync`, `--conflict-resolve newer` (adjacent), `--resilient`, `--recover` and `--workdir /w`;
    - it contains none of `--delete-before|--delete-during|--delete-after`, even with `syncTiming` set.
  - T-JO-NEWER: a stream whose classVersion is 99 throws `SerializationException("stored version is newer")`.
  - Gate G-U8.1: `scripts/dev.sh test qt64 -- -R tst_job_options` passes with 4 functions, none skipped.
- **e2e tests:** `tests/tst_e2e_bisync.cpp`.
  - Setup: the working directory is a temp dir; `p1/a.txt`, `p2/b.txt`, `wd/`. Relative paths keep the listing names short (R14).
  - Each run uses `JobOptions{operation = Bisync, source = "p1", dest = "p2", bisyncWorkdir = "wd", …}.getOptions()`, then `PrepareTransferArgs`, then `RcloneEnvironment(Parsed)` and a QProcess in that cwd, with the lines fed to `TransferMonitor`.
  - T-E2E-BISYNC-NEEDS-RESYNC: no resync. Result Error, exit code 7, `lastError()` or recentErrors contains `Must run --resync`.
  - T-E2E-BISYNC-RESYNC: with resync, Finished. Both dirs contain `a.txt` and `b.txt`. There is no message with ESC (0x1B).
  - T-E2E-BISYNC-UPDATE: create `p1/c.txt`, then run without resync. Finished, and `p2/c.txt` exists.
  - Gate G-U8.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_bisync` passes with 3 functions.
- **Done:** G-U8.1 and G-U8.1e pass; unit closed; completion commit.
- **Revision 20** (agent-1:opus at the 8.1 closure, from the 8.1 reviews and probes against rclone 1.75.2; replaces the matching points above):
  - A Bisync task never emits `--delete-empty-src-dirs` (bisync rejects it: `unknown flag`, exit 2), `--update` and `--no-traverse` (bisync ignores both with a WARNING, `cmd/bisync/compare.go` `notSupported`), or `--delete-excluded` (bisync's delta copy keeps the filter options, so `sync.Sync` deletes every Path2 file outside the delta queue: `cmd/bisync/queue.go`, `fs/sync/sync.go` `DstIncludeAll`; agent-1 probe in an isolated container on 2026-10-22: after a resync, a run with `--delete-excluded` left p2 with only the new file). The TransferDialog disables those four checkboxes while Bisync is checked. Every other flag of the Sync path was probed one at a time and accepted (revision 23: two more are left out, see below).
  - `rbBisync` is disabled for a download of a single file (not a folder, not several items, not a saved Bisync task being edited). Before Run, Dry run or Save task the dialog refuses a relative `--workdir` (the listings must be found again by a run started from another directory, such as the scheduler), a file where bisync needs a folder, and offers to create a missing local destination folder (Run and Dry run: Yes or No; Save task: Yes, No to save without it, Cancel).
  - The conflict value comes from a fixed list by the combo index (the item texts are translatable). `--resync` and the workdir are not remembered as the next task's defaults.
  - `operator<<` and `operator>>` of JobOptions are declared in `job_options.h` (no longer `static`), so T-JO-NEWER calls them directly.
  - `tests/tst_job_options.cpp` uses `QTEST_MAIN` and has 5 functions: T-JO-DIALOG (the dialog shows, restores and hides the Bisync options, keeps its locked height, disables the four flags, maps the conflict index, and gates the single-file download) is added. T-JO-ROUNDTRIP writes two records in which every pair of the three bisync bools differs, and pins the v9 tail bytes of both. T-JO-BISYNC-ARGS has 12 data rows, 5 of them the Copy/Move/Sync argv of the base code. G-U8.1 counts 5 functions.
  - `tst_e2e_bisync` is in the plain `RB_TEST_RCLONE` block. T-E2E-BISYNC-UPDATE sets `deleteExcluded`, `skipNewer` and `noTraverse` and checks that none of their flags is in the argv and that p2 keeps `a.txt` and `b.txt`.
  - P8 README obligation also covers: the four flags a Bisync task leaves out and why, Bisync works on folders, and the workdir must be absolute.
- **Revision 23** (agent-1:opus, B-T3, from the P8 review and a probe against rclone 1.75.2 in an isolated container): `--ignore-existing` and `--no-update-modtime` are also left out of a Bisync task and their checkboxes ("Skip all files that exist", "Don't update mod-time if files are identical") are disabled while Bisync is checked: with either one bisync copies nothing for an updated file and the next run aborts with "path1 and path2 are out of sync" (exit 7). Six flags in all; T-JO-DIALOG checks the six checkboxes and T-E2E-BISYNC-UPDATE also sets `skipExisting` and `dontUpdateModified` and checks that an updated Path1 file reaches Path2. Fixed in d6ba55e; the README (P8) lists the six.

### 8.2 copyto and copyurl actions (#199)
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P8.
- **Files:**
  - NEW `src/remote_actions.h/.cpp`, registered.
  - WRITE `src/remote_widget.h/.cpp/.ui`: actions `actionCopyTo` and `actionUploadFromUrl`, the context menu entries and the handlers.
  - NEW `tests/tst_remote_actions.cpp` (unit) and `tests/tst_e2e_remote_actions.cpp` (e2e).
- **Change:**
  - Preconditions: 8.1 DONE.
  - Contract:
    ```cpp
    QStringList buildCopytoArgs(const QString &src, const QString &dst);                  // {"copyto", src, dst}
    bool isHttpUrl(const QString &s);                                                      // QUrl valid, scheme http|https, non-empty host
    QStringList buildCopyurlArgs(const QString &url, const QString &destDir,
                                 const QString &fileName, bool noClobber);
    // fileName empty → {"copyurl", url, dirWithSlash(destDir), "--auto-filename", "--print-filename"}
    // else           → {"copyurl", url, joinRclonePath(destDir, fileName)}
    // noClobber      → append "--no-clobber"
    QString joinRclonePath(const QString &dir, const QString &name);                        // "r:" + "a" → "r:a"; "r:a" + "b" → "r:a/b"; "r:a/" + "b" → "r:a/b"; "/x" + "y" → "/x/y"
    ```
    - `actionCopyTo` ("Copy to…") is enabled when exactly one non-folder, non-Special item is selected.
      - The dialog is a `QInputDialog::getText` titled "Copy to", with the label "Destination (remote:path or local path)" and the default `joinRclonePath(<remote>:<current dir>, <name>)`.
      - On OK with non-empty text and text ≠ source: `emit addTransfer("Copy to " + dst, src, dst, buildCopytoArgs(src, dst) + GetDefaultOptionsList("defaultRcloneOptions"), uuid, "", uuid)`.
    - `actionUploadFromUrl` ("Upload from URL…") is enabled when a folder (or the root) is selected.
      - Dialog NEW `src/url_upload_dialog.h/.cpp` (code-built), with:
        - `QLineEdit url`, validated by `isHttpUrl`, so OK is disabled while invalid;
        - `QCheckBox autoName` (checked by default), and `QLineEdit fileName`, enabled when not autoName;
        - `QCheckBox noClobber`.
      - On OK: `emit addTransfer("Upload from URL " + url, url, destDir, buildCopyurlArgs(...) + defaults, uuid, "", uuid)`.
    - Both actions are added to the tree context menu next to `upload`/`download`, and to the tools frame if that is where the other single-item actions live (the implementer follows the existing placement and records it).
  - Steps:
    1. S1 — `remote_actions` plus unit tests.
    2. S2 — Actions, dialog and wiring; smoke.
    3. S3 — e2e; full gates.
- **Unit tests:** `tests/tst_remote_actions.cpp`:
  - T-ACT-COPYTO: `buildCopytoArgs("r:a b.txt", "/x/y z.txt")` returns exactly the three items.
  - T-ACT-URL: `https://a.b/c` and `http://127.0.0.1:8/x` are valid; `ftp://a`, `a.b/c` and empty are invalid.
  - T-ACT-COPYURL: the auto form ends with `--auto-filename --print-filename`, and the dest ends with `/`. The named form has dest `r:d/n.txt`. noClobber appends `--no-clobber`.
  - T-ACT-JOIN: the 4 examples.
  - Gate G-U8.2: `scripts/dev.sh test qt64 -- -R tst_remote_actions` passes with 4 functions.
- **e2e tests:** `tests/tst_e2e_remote_actions.cpp`. Every run goes through `PrepareTransferArgs`, `RcloneEnvironment(Parsed)` and TransferMonitor.
  - T-E2E-COPYTO: `copyto <tmp>/p/a.txt "<tmp>/out/renamed a.txt"` gives Finished, the file exists with equal content, and one Log `Copied (new) to: renamed a.txt`.
  - T-E2E-COPYURL:
    - start `rbRclone() serve http <tmp>/web --addr 127.0.0.1:0 --use-json-log --config <empty>` with `web/f 1.txt`, and read its URL from the first `http://127.0.0.1:` match in its log;
    - `buildCopyurlArgs(url + "f%201.txt", "<tmp>/dl", "", false)` gives Finished, `<tmp>/dl/f 1.txt` exists with equal content, and one Text event `f 1.txt`;
    - stop the server.
  - T-E2E-COPYURL-NOCLOBBER: repeating with `noClobber` after changing the local file to `local` leaves its content `local`. The exit code is recorded in the evidence, not asserted.
  - Gate G-U8.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_remote_actions` passes with 3 functions.
- **Done:** G-U8.2 and G-U8.2e pass; smoke passes; unit closed; completion commit.
- **Revision 21** (agent-1:opus, from the 8.2 prefetch read on the 8.1 tree and rclone 1.75.2; replaces the matching points above):
  - Files: also NEW `src/url_upload_dialog.h/.cpp`, `src/CMakeLists.txt` and `tests/CMakeLists.txt`; `src/remote_widget.h` only if a member is needed.
  - `joinRclonePath(dir, name)` = `dir.endsWith(':') ? dir + name : RemoteOps::joinPath(dir, name)` (reuses 14.3; `joinPath("r:", "a")` alone gives `r:/a`). `dirWithSlash(dir)` appends `/` only when `dir` ends with neither `/` nor `:`, so the root of a remote stays `r:` (`r:/` is the filesystem root on sftp, ftp and local). Long flags only, `--no-clobber` last; no `--config` in the args (MainWindow::addTransfer appends it).
  - Enabling: Copy to needs one selected item that is not a folder, not the top level and not Special; Upload from URL one selected folder or the root; both are off in the Google Drive shared-with-me and trashed views (the argv has no mode flag), and both are disabled explicitly in the loading/Special branch of the selection handler.
  - Placement: the tree context menu (`upload`, Upload from URL…, `download`, Copy to…) and the Tools drop-down menu after `link`; no new toolbar button (the 14.4 button row stays as it is); the existing copy and upload icons.
  - UrlUploadDialog: OK also needs a valid file name (`RemoteOps::isValidFolderName`) while autoName is unchecked; with autoName checked the empty name (auto form) is passed; the URL is passed as typed (trimmed).
  - Copy to: a destination ending in `/` or `:` is completed with the file name; both handlers add only `GetDefaultOptionsList("defaultRcloneOptions")` and use two separate `QUuid::createUuid()` values, like upload and download.
  - Tests: extra cases are rows or asserts inside the 4 + 3 functions (the #199 URL with literal brackets in T-ACT-URL, `r:` and `r:d/` in T-ACT-COPYURL, the named form against real rclone in T-E2E-COPYURL); the e2e file is in the `RB_TEST_RCLONE AND UNIX AND NOT APPLE` block (pgrep leftover check); the serve URL regex is `http://127\.0\.0\.1:\d+/`; the copyurl file name is a stdout line, so the e2e merges the channels. New totals: G-UNIT 23, G-E2E 17.
  - The #199 triage row becomes `fixed in 8.2 (…)` at the 8.2 closure; the README (Copy to…, Upload from URL…, the URL shows in the job and the copied command, the name comes from the URL path) and CHANGELOG at the P8 closure.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E (with the `rclone rcd|serve` leftover check inside the container), G-SMOKE, G-SERVERS (with the `rb-srv-` container and network check), check-triage. G-BUILD610 if the image is present.
- README obligation (P8): document Bisync (first run needs "resync", the conflict options, the long-path note: shorter folder paths, `--workdir` does not shorten the names), "Copy to…" and "Upload from URL…".
- Supervisor agent-1:opus reviews. Close P8.
- **Revision 22** (agent-1:opus, from the P8 prefetch and rclone 1.75.2 `cmd/bisync/bilib`; replaces the matching points above):
  - R14 was wrong about `--workdir` (see R14). The `leBisyncWorkdir` tooltip in `src/transfer_dialog.ui` drops "Use a short path if rclone reports a file name that is too long." and says that very long folder paths can make the listing file names too long and only shorter folder paths help; the README says the same in Known limitations.
  - Files of the closure: `README.md` (Fork changes bullet; FAQ `faq-bisync`, `faq-bisync-recover` after `faq-include-exclude-order`, `faq-copy-to`, `faq-upload-from-url` after `faq-drive-duplicates`; one sentence each in `faq-copy-action` and `faq-dry-run`; Known limitations `faq-limit-bisync`, `faq-limit-bisync-path`, `faq-limit-url-name`), `CHANGELOG.md` (`## [2.1.0] - unreleased` above 2.0.0: three NEW lines and a CHANGED line for the tasks.bin version, which 2.0.0 cannot read and replaces at its next save; VERSION unchanged, P13 appends to this section), `src/transfer_dialog.ui` (the tooltip), `docs/triage/upstream-issues.md` (README anchors in the #243 and #199 notes).
  - Every rclone behaviour the README states is checked against rclone 1.75.2 (source or a probe in an isolated container) by the P8 review; every UI string it quotes is checked against `src/`.
  - The dev branch carries the `release/2.0.0` merge (1c813ac), so CHANGELOG already reads `## [2.0.0] - 2026-10-22`.
