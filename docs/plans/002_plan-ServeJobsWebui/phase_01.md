# Phase 0 — Environment and test infrastructure

Intent: an execution branch, the docker build images, a fork rclone binary, a recorded baseline, and a
test harness that can run end-to-end tests against the fork binary.
Prerequisites: none (plan READY).
Phase closure: P0; review by agent-1:opus.

## State and ownership contract
Read STATE.md first. In a fresh session reconcile the active unit, actual diff, step checkpoint, scope,
and commit reference before selecting work. In a delegated worker session, follow the coordinator's
assigned OPEN unit; do not take over state/commits or revert active work. Open before edits; checkpoint
after meaningful batches and before yielding. Close after tests/review/coherence, commit when configured
(full-autonomous:true ⇒ commit every closed unit), then continue only within scope. Missing design goes
to agent-1:opus.

## Local design context
Plan revision: 1.
- U4 (user, 2026-10-23): network is authorized for `scripts/dev.sh image qt64`, `scripts/dev.sh image qt610`
  and `scripts/dev.sh server-images` only; branch `dev/002-serve-jobs-webui` may be created once from
  `dev/001-modernization` @ `19e93fa`; building the rclone fork with `go build` (offline) is authorized.
  No push, tag, release, or change to `../rclone`.
- D11: CMake cache `RB_TEST_RCLONE_WEBUI` (FILEPATH, default empty) → test env `RB_TEST_RCLONE_WEBUI`;
  `scripts/dev.sh` reads env `RB_RCLONE_WEBUI`, bind-mounts it read-only at `/opt/rclone/rclone-webui`
  and configures with `-DRB_TEST_RCLONE_WEBUI=/opt/rclone/rclone-webui` (or `-DRB_TEST_RCLONE_WEBUI=`
  when unset, so a stale cache value never survives). Fork binary: `RcloneBrowser/build-rclone-webui/rclone`
  (ignored by `.gitignore` `build*`), built with `CGO_ENABLED=0` because the qt610 image is Ubuntu 22.04
  (older glibc than the host). `tst_e2e_serve_webui` (phase 3) is registered only when the cache value is
  non-empty, because CI runs `ctest -L e2e` with upstream rclone only.
- I-9: containers never see the host HOME or rclone config (existing dev.sh rule); the fork binary is
  mounted read-only like `RB_RCLONE`.
- R3: the fork's `version` says `v1.76.0-DEV` (dev build) or `v1.75.2-webui.N` (release): dev.sh checks
  only that the file exists and `version` starts with `rclone v1.`; it does not judge webui support.

## Sub-phases

### 0.1 Branch, images, fork binary, baseline
- **Model:** agent-1:opus
- **Assignment:** environment setup needing user-granted authority; supervisor executes and records evidence.
- **Files:** READ `scripts/dev.sh`, `docker/*.Dockerfile`, `../rclone` (HEAD only). WRITE none in the repo
  (generated dirs `build-docker-*`, `build-rclone-webui/` are ignored). STATE.md §3/§7 evidence.
- **Change:**
  Preconditions: repo `RcloneBrowser` on `dev/001-modernization` at `19e93fa`, only untracked `.serena/`
  and this plan folder.
  Contract: afterwards `git branch --show-current` = `dev/002-serve-jobs-webui`; images `rb-build:24.04`,
  `rb-build:qt610` exist; `build-rclone-webui/rclone version` prints `rclone v1.`; G-BASE recorded.
  Scope: no source edit.
  Steps:
  1. S1 — `git switch -c dev/002-serve-jobs-webui` (one-off U4 authorization). Expected: branch at `19e93fa`.
  2. S2 — `scripts/dev.sh image qt64` then `scripts/dev.sh image qt610` (network). Expected: both images
     inspectable. If a build fails for network reasons, retry once; then BLOCKED (B-NET) with the log.
  3. S3 — `scripts/dev.sh server-images` (network). Expected: 4 server images present (needed only by
     the optional `servers` label; failure is recorded, not blocking).
  4. S4 — `(cd ../rclone && git rev-parse HEAD && CGO_ENABLED=0 GOPROXY=off GOFLAGS=-mod=readonly go build -o ../RcloneBrowser/build-rclone-webui/rclone .)`
     then `build-rclone-webui/rclone version | head -1` and `file build-rclone-webui/rclone` (must say
     "statically linked"). Record the fork HEAD (`a527815ae` at planning). If offline build fails for a
     missing module, rerun with network only after asking the user (not covered by U4).
  5. S5 — Run G-BASE (STATE.md §3). Expected: build exit 0, 0 warnings; unit and e2e counts recorded.
     Any failing test is a known baseline failure: list names in §7 and §9; supervisor decides.
  Checkpoint: after S2, S4, S5.
  Failure handling: missing docker permission → BLOCKED, ask user. Baseline test failures do not block
  unless they are in serve/remote-list tests touched by this plan (then supervisor analysis first).
- **Unit tests:** N/A — no code change.
- **e2e tests:** G-BASE e2e run on baseline (evidence only).
- **Done:** branch, images, binary present; G-BASE evidence in §7; unit closed; commit: none possible
  without code changes (record `uncommitted`, no empty commit), plan folder is committed with 0.2.

### 0.2 Fork binary in the test harness
- **Model:** agent-2:sonnet
- **Assignment:** implement; agent-1:opus reviews the diff (dev.sh quoting, I-9).
- **Files:** WRITE `CMakeLists.txt` (top level, next to `set(RB_TEST_RCLONE "" CACHE FILEPATH …)`),
  `tests/CMakeLists.txt — rb_add_test` ENVIRONMENT, `scripts/dev.sh`. NEW none.
- **Change:**
  Preconditions: 0.1 DONE.
  Contract: D11 exactly. Without `RB_RCLONE_WEBUI` every existing command behaves as before.
  Steps:
  1. S1 — `CMakeLists.txt`: add `set(RB_TEST_RCLONE_WEBUI "" CACHE FILEPATH "rclone webui fork used by e2e tests")`
     right after the `RB_TEST_RCLONE` line. Expected: configure unchanged otherwise.
  2. S2 — `tests/CMakeLists.txt` `rb_add_test`: ENVIRONMENT string becomes
     `"QT_QPA_PLATFORM=offscreen;RB_FIXTURES=…;RB_TEST_RCLONE=${RB_TEST_RCLONE};RB_TEST_RCLONE_WEBUI=${RB_TEST_RCLONE_WEBUI}"`.
  3. S3 — `scripts/dev.sh`: after the `RB_RCLONE` validation block add:
     `RB_RCLONE_WEBUI=${RB_RCLONE_WEBUI:-}`; when non-empty: `[ -e ]` else exit 2 with
     `dev.sh: RB_RCLONE_WEBUI not found: <path>`; `readlink -f`; version line must match `^rclone v1\.` else
     exit 2; set `WEBUI_MOUNT=(-v "$RB_RCLONE_WEBUI:/opt/rclone/rclone-webui:ro")` and
     `WEBUI_DEF=-DRB_TEST_RCLONE_WEBUI=/opt/rclone/rclone-webui`; when empty `WEBUI_MOUNT=()` and
     `WEBUI_DEF=-DRB_TEST_RCLONE_WEBUI=`. Append `$WEBUI_DEF` to the `build` INNER cmake configure line,
     and `${WEBUI_MOUNT[@]+"${WEBUI_MOUNT[@]}"}` to both `docker run` invocations (servers client and the
     final exec). Usage text: one line `Environment: RB_RCLONE_WEBUI = rclone webui fork binary for the webui e2e tests (optional)`.
     In the `e2e` case, when `RB_RCLONE_WEBUI` is empty print to stderr
     `dev.sh: RB_RCLONE_WEBUI not set: the webdav-webui e2e test is not registered` (no exit change).
     Expected: `bash -n scripts/dev.sh` ok; `shellcheck scripts/dev.sh` (if installed) no new findings.
  4. S4 — Build with and without the var: `scripts/dev.sh build qt64` then
     `grep RB_TEST_RCLONE_WEBUI build-docker-qt64/CMakeCache.txt` shows empty value;
     `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh build qt64` shows
     `/opt/rclone/rclone-webui`.
  Checkpoint: after S3 and S4.
  Failure handling: quoting issues with `set -u` and empty arrays → use the `${arr[@]+"${arr[@]}"}` form
  already used for `TTY_ARGS`.
- **Unit tests:** G-U02: `bash -n scripts/dev.sh`; S4 cache checks; `RB_RCLONE_WEBUI=/nonexistent scripts/dev.sh test qt64; echo $?` prints 2.
- **e2e tests:** `RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone scripts/dev.sh e2e qt64` passes with the
  same count as G-BASE (no new tests yet).
- **Done:** diff limited to the 3 files; G-U02 pass; review recorded; unit closed; commit (includes this
  plan folder, provenance: authored by the planning session 2026-10-23).

### 0.3 Update README.md (developer section)
- **Model:** agent-3:haiku
- **Assignment:** documentation; supervisor checks at P0.
- **Files:** `README.md` — the `scripts/dev.sh` paragraphs (search "`scripts/dev.sh` builds, tests") only.
- **Change:**
  Preconditions: 0.2 DONE.
  S1 — Add one sentence after the `RB_RCLONE` sentence: the optional `RB_RCLONE_WEBUI=/path/to/rclone`
  (an rclone built from the webui fork) is mounted read-only at `/opt/rclone/rclone-webui` and enables
  the webdav-webui end-to-end test; without it that test is not registered (dev.sh prints a notice), so set it
  for `build` and `e2e` alike. Preserve style.
  S2 — Verify the commands quoted exist in `scripts/dev.sh usage`.
  Checkpoint: record edits.
- **Unit tests:** N/A — docs.
- **e2e tests:** N/A — docs.
- **Done:** sentence present and accurate; unit closed; commit.

## Phase gates and closure
- From 0.2 on, every gate command runs with `export RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone` (STATE.md §3):
  a `dev.sh build` without it resets the CMake cache and drops the webui e2e test.
- Required gates: G-BUILD64, G-UNIT, G-E2E (with `RB_RCLONE_WEBUI` set) — counts equal G-BASE.
- README obligation 0.3 DONE.
- agent-1:opus reviews dev.sh/CMake diff for I-9 and backwards compatibility.
- Open P0, record review and gate results, mark phase DONE, commit closure (full autonomy).
