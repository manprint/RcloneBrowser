# Tasks — plan 002

## T-A001 — CHANGELOG entries for plan 002

- Date / actual agent: 2026-10-23 / agent-1:opus
- Request: user "continua" after the final report, whose open question was whether to add an
  `## [Unreleased]` section to CHANGELOG.md (the 3.4 contract allowed edits only to an existing one).
- State owner: STATE.md §1 (task T-A001 / 1)
- Plan impact: 3.4 contract note "CHANGELOG untouched (no unreleased section)" superseded by this user-driven
  task; no code or decision change.
- Settings: full-autonomous:true; WIP commits:on
- Identity: plan 002_plan-ServeJobsWebui; unit T-A001; attempt 1
- Baseline / branch: `55b67bd`, `dev/002-serve-jobs-webui`
- Files: WRITE `CHANGELOG.md` (new top `## [Unreleased]` section; no version or date invented)
- Contract: entries for Evolutiva01 items 1–4, the Serves window removal and the RB_RCLONE_WEBUI test
  variable, worded from the shipped behaviour (README #faq-serve); existing sections untouched.
- Tests: N/A — docs; checked against README and the B-A001/plan evidence.
- Done: section added; commit.

### Outcome and evidence
- Changes: `CHANGELOG.md` `## [Unreleased]` with 3 NEW, 1 FIXED, 2 CHANGED entries.
- Review: agent-1:opus, wording checked against README #faq-serve and the shipped strings.
- Commit: unit:T-A001:1
