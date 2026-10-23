# Bugs — plan 002

## B-A001 — webdav-webui start refused as "not supported" when the capability check fails

- Date / actual agent: 2026-10-23 / agent-1:opus (coordinator), implementation and review via workflow subagents
- Request: user (2026-10-23, after plan closure): "Fixa!" the known edge reported at P3 — a webdav-webui
  start queued while the backend restarts is refused with the misleading "This rclone does not support
  webdav-webui…" text.
- State owner: STATE.md §1 (bug B-A001 / 1)
- Plan impact: D7 (capability tri-state) and I-5 (never start webui on an incapable rclone) unchanged;
  refines the message when the capability is Unknown after a failed check. Units 3.1/3.2 code paths.
- Related findings: none
- Settings: full-autonomous:true; WIP commits:on (STATE.md §3)
- Identity: plan 002_plan-ServeJobsWebui; unit B-A001; attempt 1
- Baseline / branch: `751779a`, `dev/002-serve-jobs-webui`; untracked `.serena/` (never stage)
- Assignment/reviewer: implementation by a workflow worker; adversarial review by independent workflow
  reviewers; agent-1:opus supervisor approval
- Files: WRITE `src/serve_manager.{h,cpp}`, `src/serve_widget.{h,cpp}`, `src/main_window.cpp`
  (openServeDialog wiring only), `tests/tst_serve_manager.cpp`, `tests/tst_serve_widget.cpp`,
  `tests/tst_e2e_serve_webui.cpp`, `README.md` (#faq-serve webdav-webui bullet)
- Symptom / reproduction: queue `start(webdav-webui)`; the backend stops (Preferences change) ⇒
  `stateChanged` ⇒ `resetCapabilities()` drains the queue ⇒ `options/blocks` fails with
  "rclone daemon is not running" ⇒ capability stays Unknown, tried ⇒ the start fails with
  `kWebuiUnsupported` although the rclone may support it. Expected: an honest message naming the failed
  check and rclone's text. Same family: a Serve dialog open during a failed check keeps "Checking whether
  this rclone supports webdav-webui..." forever.
- Root cause: `ServeManager::processStartQueue` (src/serve_manager.cpp) refuses a webui flavor whenever
  `mWebui != Yes`, without distinguishing No from Unknown-after-failed-check; the failure text of
  `loadCapabilities` is only logged; the dialog has no failed-check state.
- Contract:
  - `ServeManager` keeps the last `options/blocks` failure text (cleared by a successful check and by
    `resetCapabilities()`); new signal `capabilitiesCheckFailed(const QString &text)` emitted on a failed,
    non-stale check; accessor `webuiCheckError()`.
  - webui flavor start with `mWebui == No` ⇒ `kWebuiUnsupported` (unchanged). With `mWebui == Unknown`
    after a failed check ⇒ `startFailed`/`error` with
    `kWebuiCheckFailed + rclone's text`, where `kWebuiCheckFailed` =
    "Could not check whether this rclone supports webdav-webui: " (shared `extern` like
    `kWebuiUnsupported`). No `options/set`/`serve/start` sent (I-5 kept). Classic webdav unchanged.
  - Serve dialog: `setWebuiCheckFailed(const QString &text)` slot; while support is Unknown and a check
    failed, `webuiUnsupported` shows `kWebuiCheckFailed + text` (PlainText) and OK stays disabled; a later
    `setWebuiSupport(Yes|No)` or a new check clears it. `openServeDialog` passes the current
    `webuiCheckError()` and connects `capabilitiesCheckFailed`.
  - Nothing else changes; the secret is never part of any message.
- Steps: S1 regression tests first (fail before the fix); S2 implement; S3 gates; S4 adversarial review;
  S5 README; S6 commit.
- Tests: `tst_serve_manager` T_WEBUI_CHECK_FAILED (failed check ⇒ honest message, no options/set or
  serve/start; reset-drain during a stopped backend ⇒ same; No still gives kWebuiUnsupported);
  `tst_serve_widget` T_WEBUI_DIALOG_CHECK_FAILED; `tst_e2e_serve_webui` T_E2E_WEBUI_RESTART (fork:
  backend stopped ⇒ webui start fails with the check-failed message, not kWebuiUnsupported; backend
  started again ⇒ the same start succeeds with the web UI).
- Done: regression tests fail before / pass after; full unit, e2e (with RB_RCLONE_WEBUI) green, build
  0 warnings; review approved; README updated; commit `unit:B-A001:1`.

### Checkpoint
See STATE.md §1 and §6.

### Outcome and evidence
- Changes: `ServeManager` keeps the failed-check text (`webuiCheckError()`, signal
  `capabilitiesCheckFailed`), refuses a webui start on Unknown with `kWebuiCheckFailed` + rclone's text
  (No still gives `kWebuiUnsupported`), waits for a check already in flight instead of a stale failed one,
  and `resetCapabilities()` also announces Unknown when it cleared a shown failure. `ServeDialog::setWebuiCheckFailed`
  shows the failure (PlainText, OK disabled); `openServeDialog` passes the current failure and connects the signal.
- Regression evidence: before the fix (API stubs only) `FAIL! TestServeManager::T_WEBUI_CHECK_FAILED()`
  (got the kWebuiUnsupported refusal) and `FAIL! TestServeWidget::T_WEBUI_DIALOG_CHECK_FAILED()` (got
  "Checking…"); after: both PASS, plus `T_E2E_WEBUI_RESTART` PASS (fork rcd stopped ⇒ check-failed message;
  started again ⇒ the same start serves the web UI). Mutation check: moving the generation guard into the
  success branch makes T_WEBUI_CHECK_FAILED fail.
- Gates: clean qt64 build 0 warnings; unit 32/32 (`-V`); e2e 23/23 (`-V`, RB_RCLONE_WEBUI set); test
  secrets absent from logs (`/tmp/rb-gates/ba001-final-*.log`).
- Review: workflow — 3 independent lens reviewers (state machine, UI/wiring, tests/security), each finding
  attacked by a skeptic; 5 confirmed (2 low behaviour: stale dialog text after reset, start refused on an
  old failed check while a newer one was in flight; 3 test-coverage) and fixed with tests; agent-1:opus
  read the final production diff: approved.
- README: #faq-serve webdav-webui bullet — failed-check message and what happens to a waiting start.
- Plan reconciliation: D7/I-5 unchanged; message refinement only; STATE.md §4/§7.
- Commit: unit:B-A001:1
