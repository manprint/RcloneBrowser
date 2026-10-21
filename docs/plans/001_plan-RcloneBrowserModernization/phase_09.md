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
  - Long absolute Path1+Path2 produce a listing/lock filename over 255 bytes, giving `Lock file exists, but contents are unreadable` and exit 1. This is an rclone limitation; the README advises `--workdir` or shorter paths.
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

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation (P8): document Bisync (first run needs "resync", the conflict options, the long-path `--workdir` note), "Copy to…" and "Upload from URL…".
- Supervisor agent-1:opus reviews. Close P8.
