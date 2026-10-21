# Phase 13 — Ecosystem extras and final integration

Intent:
- Uploads can carry custom HTTP headers, and uploads to S3 can carry object tags (`X-Amz-Tagging`).
- OpenBSD and NetBSD CI jobs join the workflow.
- New remote icons and the macFUSE link text are updated.
- The final integration gates prove the reference scenario.
Prerequisites: P12. P-1 (iamnhx and totza2010 refs).
Phase closure: P13 = final closure; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6); the S3 test uses a local `rclone serve s3` on 127.0.0.1.

## Local design context
Plan revision: 1.
- D13: xrp001 contributes only the idea (`036882904` upload headers, `74fdeb738` S3 tags as an `X-Amz-Tagging` header). iamnhx contributes only the OpenBSD/NetBSD CI jobs from `a2e96c64a`. totza2010 contributes its teldrive icon.
- R16 (local probe, rclone 1.75.2, 2026-10-21):
  - `rclone copy src s3t:bkt --header-upload "X-Amz-Tagging: proj=rb&env=test" --header-upload "Cache-Control: no-cache" --dump headers` against an anonymous local `rclone serve s3 <dir> --addr 127.0.0.1:0` (remote `type = s3, provider = Rclone, endpoint = <url>, force_path_style = true`) exits 0;
  - the debug dump of `PUT /bkt/t%201.txt?x-id=PutObject` contains `X-Amz-Tagging: proj=rb&env=test` and `Cache-Control: no-cache`;
  - `--dump headers` forces `-vv`.
  - Whether a real S3 server stores the tags is not verified locally. An optional MinIO check (P-3) is listed in STATE.md §3 as G-S3-MINIO and is not required.
  - A `serve s3 --auth-key AK,SK` run returned `InvalidAccessKeyId`. The test uses anonymous access (STATE.md §10).
- I-7: classVersion 9 (from 8.1) becomes 10. Files with version ≤ 9 load unchanged.
- Icons: `MainWindow::rcloneListRemotes` resolves `:media/images/remotes_icons/<type with ' '→'_'><suffix>.png`, else `unknown`.
  - The local comparison of `rclone help backends` against `src/images/remotes_icons` (plan time) found no icon for: archive, azurefiles, cloudinary, combine, compress, doi, drime, filefabric, filelu, filen, filescom, gcs, gofile, gphotos, hasher, hdfs, hidrive, huaweidrive, iclouddrive, imagekit, internetarchive, internxt, linkbox, netstorage, oos, pikpak, pixeldrain, protondrive, quatrix, seafile, shade, sia, smb, storj, ulozto, zoho.
  - Some of these are naming mismatches: listremotes reports the backend name (for example `google cloud storage`), not the short prefix.
- Current code (anchors): JobOptions (8.1 state); TransferDialog upload mode; `.github/workflows/build.yml` FreeBSD job (pattern for the BSD VM jobs); `src/remote_widget.cpp` macFUSE text `<a href="https://osxfuse.github.io">FUSE for macOS</a>`.

## Sub-phases

### 13.1 Upload headers and S3 tags
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P13, with I-7 in focus.
- **Files:**
  - WRITE `src/job_options.h/.cpp`, `src/list_of_job_options.cpp` and `src/transfer_dialog.h/.cpp/.ui`.
  - NEW `src/upload_headers.h/.cpp`, registered.
  - WRITE `tests/tst_job_options.cpp` (new functions plus a `jobopts_v9.bin` fixture recorded in S1 with the 8.1 code).
  - NEW `tests/tst_upload_headers.cpp` (unit) and `tests/tst_e2e_s3_headers.cpp` (e2e).
- **Change:**
  - Preconditions: P12 DONE.
  - Contract:
    ```cpp
    struct S3Tag { QString key, value; };
    std::optional<QVector<S3Tag>> parseS3Tags(const QString &text);   // lines "key=value" (trimmed, blank lines skipped); nullopt if a line lacks '=' or key empty; >10 tags → nullopt (S3 limit)
    QString s3TaggingHeader(const QVector<S3Tag> &tags);              // "X-Amz-Tagging: " + join(percentEncode(k) + "=" + percentEncode(v), "&")
    std::optional<QStringList> parseUploadHeaders(const QString &text); // lines "Name: value"; nullopt if a line lacks ':' or the name is not a token [!#$%&'*+.^_`|~0-9A-Za-z-]+
    QStringList headerUploadArgs(const QStringList &headers);        // each → "--header-upload", header
    ```
    - JobOptions v10: `QString uploadHeaders;` (raw text) and `QString s3Tags;` (raw text), appended to `operator<<` and read under `actualVersion >= 10`.
    - `getOptions()`, for jobType Upload:
      - append `headerUploadArgs(parseUploadHeaders(uploadHeaders).value_or(QStringList{}))`;
      - if the destination remote type is s3 and tags parse non-empty (the existing `remoteType` field, serialized since v4; RemoteWidget sets it to its own remote's type, which is the destination of an Upload job), also append `"--header-upload", s3TaggingHeader(tags)`.
      - Invalid text never reaches getOptions, because the dialog validation blocks OK.
    - TransferDialog, upload mode only:
      - a collapsible "Upload headers" `QPlainTextEdit uploadHeaders`;
      - for an s3 remote, "S3 object tags" `QPlainTextEdit s3Tags`;
      - an inline red `QLabel uploadHeadersError` disables OK while either field fails to parse.
  - Steps:
    1. S1 — Record `jobopts_v9.bin` with the unmodified 8.1 code (`RB_RECORD_JO_V9=1`, same mechanism as 8.1) and record its sha256.
    2. S2 — `upload_headers` plus unit tests.
    3. S3 — JobOptions v10 plus dialog.
    4. S4 — e2e; full gates.
- **Unit tests:**
  - `tests/tst_upload_headers.cpp`:
    - T-HDR-PARSE: `"Cache-Control: no-cache\n\nX-Foo: a:b"` gives 2 headers, the value keeping the colon. `"bad header"` and `"Bad Name: x"` give nullopt.
    - T-TAG-PARSE: `"proj=rb\nenv=test"` gives 2 tags; `"novalue"` and `"=x"` give nullopt; 11 lines give nullopt.
    - T-TAG-HEADER: `[{proj, rb}, {"a b", "c&d"}]` gives `X-Amz-Tagging: proj=rb&a%20b=c%26d`.
  - `tests/tst_job_options.cpp`:
    - T-JO-COMPAT-V9: the v9 fixture loads with identical v9 fields and empty v10 fields. T-JO-COMPAT (v8) still passes.
    - T-JO-UPLOAD-HEADERS: an Upload job to remoteType `s3` with both fields set gives `getOptions()` containing the adjacent pairs `--header-upload` `Cache-Control: no-cache` and `--header-upload` `X-Amz-Tagging: proj=rb&env=test`. The same job with remoteType `drive` has no X-Amz-Tagging. A Download job has neither.
  - Gate G-U13.1: `scripts/dev.sh test qt64 -- -R 'tst_upload_headers|tst_job_options'` passes. The function counts are 3 and 6 (4 from 8.1 plus 2).
- **e2e tests:** `tests/tst_e2e_s3_headers.cpp`, T-E2E-S3-TAGS (R16):
  - start `rbRclone() serve s3 <tmp>/store --addr 127.0.0.1:0 --use-json-log --config <empty>` (anonymous) with `store/bkt/`;
  - write a conf with remote `s3t` per R16;
  - run `rclone` with `JobOptions{Upload, source <tmp>/src, dest s3t:bkt, remoteType s3, uploadHeaders "Cache-Control: no-cache", s3Tags "proj=rb\nenv=test"}.getOptions()` plus `--dump headers` and `RcloneEnvironment(Parsed)`. This is direct, because the dump needs debug output and `PrepareTransferArgs` would otherwise only pass `-v`.
  - Expected: exit 0; `store/bkt/t 1.txt` exists; the process's debug output (stderr text; no `--use-json-log` in this direct run) contains a `PUT /bkt/t%201.txt` request whose dumped header block contains `X-Amz-Tagging: proj=rb&env=test`.
  - Gate G-U13.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_s3_headers` passes.
- **Done:** gates pass; unit closed; completion commit.

### 13.2 BSD CI jobs, remote icons, macFUSE text
- **Model:** agent-3:haiku
- **Assignment:** implementer. CI-only workflow change (G-CI debt). agent-1:opus reviews at P13, with the workflow diff in focus.
- **Files:**
  - WRITE `.github/workflows/build.yml` (OpenBSD and NetBSD jobs from `git show a2e96c64a -- .github/workflows`, adapted to this workflow's structure and `ctest -L unit`).
  - NEW `src/images/remotes_icons/teldrive*.png` from the totza2010 ref (`git show <totza ref>:<path>`; locate the path with `git ls-tree -r --name-only <ref> | grep -i teldrive`).
  - WRITE the resource `.qrc` that lists remote icons.
  - WRITE `src/main_window.cpp`: an icon alias map in a NEW `src/remote_icons.h/.cpp`, registered.
  - WRITE `src/remote_widget.cpp` (macFUSE text).
  - NEW `tests/tst_remote_icons.cpp` (unit).
- **Change:**
  - Preconditions: 13.1 DONE; P-1.
  - Contract:
    ```cpp
    QString remoteIconBaseName(const QString &remoteType);
    // lowercase, ' '→'_'; aliases: "google cloud storage"→"google_cloud_storage" if that icon exists, "gcs"→same, "smb"→"smb" (no icon → unknown) …;
    // returns the base name whose ":media/images/remotes_icons/<base>.png" exists, else "unknown"
    ```
    - The alias table holds only entries whose target icon exists at implementation time. The implementer lists the icons directory and records the table.
    - `rcloneListRemotes` uses `remoteIconBaseName(type)` and keeps the existing suffix logic (`img_add`).
    - macFUSE: the link becomes `<a href="https://macfuse.github.io">macFUSE</a>`; the surrounding text says "macFUSE".
    - BSD jobs:
      - same triggers as the other jobs;
      - VM action versions pinned exactly as in iamnhx's commit (no bumps);
      - the build runs with `-DRB_BUILD_TESTS=ON`, then `ctest -L unit`;
      - jobs are not added to `create_release` needs, so they never block releases.
  - Steps:
    1. S1 — Icons and alias map plus tests.
    2. S2 — macFUSE text.
    3. S3 — Workflow jobs; YAML parse check G-YAML (STATE.md §3; host PyYAML, checked 2026-10-21; the image has no PyYAML).
- **Unit tests:** `tests/tst_remote_icons.cpp`:
  - T-ICON-ALIAS: `remoteIconBaseName("drive")` is `drive`; `"teldrive"` is `teldrive`; `"nosuch"` is `unknown`; `"google cloud storage"` is the existing GCS icon base name (recorded).
  - T-ICON-EXISTS: every base name produced by the alias table exists as a resource.
  - Gate G-U13.2: `scripts/dev.sh test qt64 -- -R tst_remote_icons` passes with 2 functions.
- **e2e tests:** N/A — the icon lookup is pure and resource-backed; the workflow jobs are verified only by G-CI (user push).
- **Done:** gates pass; CI debt recorded; unit closed; completion commit.

## Phase gates and closure (final)
- Required gates: G-BUILD64, G-BUILD610 (required at final; BLOCKED until P-2 if the image is absent), G-UNIT, G-E2E, G-SMOKE, G-I18N, G-FINAL. G-CI is required for COMPLETE: the user pushes (P-4) and reports the run; the supervisor records the run ID and job conclusions.
- G-FINAL (STATE.md §3) checks the reference scenario 1–7 from overview.md:
  1. both builds;
  2. both suites, with the discovered test count equal to STATE.md §11;
  3. T-E2E-JOB-STATS, -ERRORS and -LOGFILE with the polluted env;
  4. T-E2E-JOB-CANCEL;
  5. T-E2E-LIST-NAMES and -UNKNOWNSIZE;
  6. T-E2E-DAEMON-*, T-E2E-RCLIST-*, T-E2E-CONFIG-*;
  7. each F3 e2e.
  Scenario item 8 is G-CI.
- README obligation (P13): "Uploads: headers and S3 tags"; supported platforms including OpenBSD/NetBSD (from CI); a final full README read-through against behavior. Version rule: if `git tag -l v2.0.0` is non-empty or STATE.md §7 records a user-reported v2.0.0 tag, add CHANGELOG `## [2.1.0] - unreleased` for the P8–P13 changes and set VERSION to `2.1.0` (same strip check as 4.2); otherwise add the P8–P13 changes to the `## [2.0.0] - unreleased` section and keep VERSION `2.0.0`.
- The supervisor reviews the whole feature and invariants I-1 … I-8. Close P13. The plan is COMPLETE only when G-CI has passed. Otherwise the final state is `BLOCKED (G-CI user push)` with all local gates green.
