# Phase 4 — Fork integration, documentation, beta readiness

Intent:
- Nicolaius's UI customizations are integrated with their defaults preserving today's look.
- The README and CHANGELOG describe the manprint fork, and VERSION is numeric `2.0.0`.
- The tree is ready for the user to run G-REL and decide on the `v2.0.0-beta.1` tag.
Prerequisites: P3. P-1 done (`git cat-file -t cc483cce5` prints `commit`).
Phase closure: P4; review by agent-1:opus. G-REL for the beta tag is a separate, user-requested gate (STATE.md §3).

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6). The executor never creates a tag (U4, I-6).

## Local design context
Plan revision: 1.
- D13: the Nicolaius work comes in by cherry-pick from the local ref `nicolaius/my-customizations`. totza2010's useful pieces were already reimplemented (3.2 repo/validation/date check, 0.3 VERSION/tag check); its progress regexes are superseded by D3; its teldrive icon goes to 13.2.
- R7: `cc483cce5` contains the icon size multiplier, alphabetically sorted remotes, options to hide tabs and remote icons, tab colors and a stream player chooser. It is 1 commit ahead of and 0 behind the upstream test2 base.
- Expected conflicts:
  - `MainWindow::rcloneListRemotes`: sorting versus the JSON parse from 2.2;
  - `MainWindow` preference save block and the `preferences_dialog.ui` additions from 3.2;
  - possibly `addStream`.
- Conflict rule: keep this plan's behavior (JSON parsing, `ApplyRcloneEnvironment`, the 3.2 widgets) and add the fork's feature on top of it. Never reintroduce text parsing, `UseRclonePassword` or `compareVersion`.
- Default rule: every new setting the commit adds defaults to today's behavior: tabs and remote icons visible, multiplier 1, no tab colors, the existing stream player. If the commit's default differs, change it in the resolution and record that. A behavior change with no setting (for example alphabetical remote sorting) is accepted, and 4.2 lists it in the CHANGELOG.
- U4: VERSION is `2.0.0`, which `CFBundleShortVersionString` needs. The beta tag is `v2.0.0-beta.1`, accepted by the 0.3 check through the `VERSION-*` rule.

## Sub-phases

### 4.1 Cherry-pick Nicolaius customizations
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews before S2 (the conflict list and the planned resolutions) and after the diff.
- **Files:**
  - READ `git show --stat cc483cce5` and `git show cc483cce5`.
  - WRITE the files touched by the commit. The expected set is `src/main_window.cpp/.h`, `src/preferences_dialog.cpp/.h/.ui` and `src/stream_widget.*`, plus whatever `--stat` shows.
  - NEW `tests/tst_settings_defaults.cpp` (unit, `QTEST_MAIN`).
- **Change:**
  - Preconditions: P3 DONE; P-1.
  - Steps:
    1. S1 — `git show --stat cc483cce5`. List every new `Settings/...` key the commit reads, with its default as written in the commit. Record both lists in STATE.md §6.
    2. S2 — After agent-1:opus approves the planned resolutions, run `git cherry-pick -x --no-commit cc483cce5` and resolve conflicts by the conflict rule. Apply the default rule. Build. Expected: it builds with `-Werror`.
    3. S3 — Tests, then `scripts/dev.sh test e2e smoke`. Send the diff to agent-1:opus.
  - Recovery boundary: after S2. The working tree holds an uncommitted cherry-pick. Checkpoint it and do not abort without recording why.
  - Failure handling: if a fork feature cannot work with the 2.2 JSON remote list (for example, sorting needs a different data path), adapt the feature, not the parser. If the commit needs a newer Qt API than 6.4, guard it with `QT_VERSION_CHECK` or escalate.
  - The completion commit message keeps the `(cherry picked from commit cc483cce5…)` line produced by `-x`, plus the PEV trailers.
- **Unit tests:** `tests/tst_settings_defaults.cpp`, T-SET-DEFAULTS:
  - with an empty test QSettings (`rbInitTestEnvironment`), construct `PreferencesDialog`;
  - for each new widget the commit adds, the default state equals the recorded today-behavior default;
  - for example, the hide-tabs checkbox is unchecked and the multiplier spinbox is 1. The exact widget list comes from S1.
  - Gate G-U4.1: `scripts/dev.sh test qt64 -- -R tst_settings_defaults` passes; the function count is ≥ 1, with one QCOMPARE per new key.
- **e2e tests:** G-SMOKE with the default settings, plus the full G-UNIT and G-E2E suites, which must not regress. A feature-specific e2e is N/A: these are view preferences with no rclone interaction, and the default-state unit test guards the compatibility promise.
- **Done:** resolution plan and diff reviewed; G-U4.1, G-UNIT, G-E2E and smoke pass; unit closed; completion commit.

### 4.2 README, CHANGELOG and VERSION for the fork
- **Model:** agent-3:haiku
- **Assignment:** documentation implementer. agent-1:opus reviews at P4.
- **Files:** WRITE `README.md`, `CHANGELOG.md` and `VERSION`.
- **Change:**
  - Preconditions: 4.1 DONE.
  - `VERSION`: exactly `2.0.0` with no trailing text (a final newline is fine; CMake reads it with `file(READ)`). Confirm that `RCLONE_BROWSER_VERSION` handles the newline: if `file(READ)` keeps it, add `string(STRIP …)` in the root `CMakeLists.txt` and record it.
  - README:
    - Title block: "Rclone Browser (manprint fork)", a one-paragraph note that it is a maintained fork of kapitainsky/RcloneBrowser, and a link to the manprint releases page.
    - New "Requirements" section: rclone ≥ 1.75.0 and Qt ≥ 6.4.
    - "How to get it": the manprint releases (Windows x64/ARM64, Linux AppImage, macOS arm64 DMG, FreeBSD). Distribution packages may still ship the upstream 1.8.
    - macOS:
      - after downloading the DMG, `xattr -dr com.apple.quarantine "/Applications/Rclone Browser.app"`, or right click → Open, because the build is ad-hoc signed and not notarized;
      - rclone via Homebrew: `brew install rclone` (the app finds `/opt/homebrew/bin/rclone` on first launch). This covers the content of PRs #232/#164.
    - "Compatibility": replace the 10.9 / CentOS 7 / i386 statements with the actual CI targets in `.github/workflows/build.yml` (read them from the workflow file).
    - Add a "Fork changes" list linking the CHANGELOG.
    - Keep the "Development (Docker)" section (0.1), "Transfers and logging" (P1) and the P3 notes.
    - The upstream PGP-signing paragraph becomes "Releases of this fork are built by GitHub Actions; checksums are attached to each release" only if the release job attaches checksums. Otherwise remove the PGP claim.
  - CHANGELOG: a new top section `## [2.0.0-beta.1] - unreleased` with `NEW`/`FIXED`/`CHANGED` bullets. Each bullet names its issue number, taking only items marked fixed in the triage doc.
    - FIXED: #141, #142, #160, #107, #130, #140, #117, #162, #163, #172, #239, #79.
    - NEW: JSON engine, Docker dev env, tests, macOS arm64 DMG, Test rclone button, custom update repo, Nicolaius customizations.
    - CHANGED: minimum rclone 1.75.0; the update check targets manprint.
  - Steps:
    1. S1 — VERSION and the CMake strip check. Expected: `scripts/dev.sh build` passes, and the About dialog string is `v2.0.0`, checked with `strings build-docker-qt64/build/rclone-browser | grep -c 'GUI for rclone, v2.0.0'` ≥ 1.
    2. S2 — README and CHANGELOG.
- **Unit tests:** N/A — documentation and a version string. Check G-U4.2:
  - `test "$(tr -d '\n' < VERSION)" = 2.0.0`;
  - `grep -c 'rclone 1.75.0' README.md` ≥ 1;
  - `grep -c '#141' CHANGELOG.md` ≥ 1;
  - a link check: every relative link in README resolves to an existing file (`python3 -I` script in scratchpad).
- **e2e tests:** N/A — no runtime behavior change beyond the version string asserted in S1.
- **Done:** G-U4.2 passes; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation: satisfied by 4.2. P4 rereads the README sections against behavior.
- Close P4. Afterwards the supervisor tells the user, in the handoff summary, that the tree is beta-ready and lists the G-REL steps: P-4 push, then CI green, then the user decides on the tag. Tagging is never part of P4.
