# Phase 0 — Foundation: dev environment, test harness, CI hygiene, triage

Intent: anyone can build, unit-test and e2e-test the app locally in Docker with no network access. CI runs the same tests. Obsolete CI is removed. The upstream issue backlog has a dispositions document awaiting user sign-off.
Prerequisites: G-BASE recorded in STATE.md §7; prefetch P-1 done (fork refs, needed by 0.3 for hrpv).
Phase closure: P0; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6): never run `scripts/dev.sh image`, `git fetch`, `gh` or `curl`.

## Local design context
Plan revision: 2.
- I-9 (revision 2, user 2026-10-21: "non devi assolutamente usare i remote configurati sull host. mai"; "se hai bisogno di test, crea dei remote isolati e temporanei"): no test, gate, fixture recorder or probe uses the host's rclone configuration. Every rclone run uses a temporary config holding isolated temporary remotes (local dirs under a temp dir, `memory`, or servers on 127.0.0.1). Containers never mount the host `$HOME` or `~/.config/rclone`. On the host, the rclone binary runs only `version`, which never loads a config (R8).
- D10: there is one static library, `rclone-browser-core`.
  - It holds every current source except `main.cpp`, plus all MOC/UI outputs.
  - The executable target contains only `main.cpp`, the QRC output, and (per platform) `resources.rc` or `icon.icns`.
  - Tests link `rclone-browser-core`.
  - Warnings come from an INTERFACE target, `rb_warnings`: `/W4 /WX /wd4100 /wd4189` on MSVC, `-pedantic -Wall -Wextra -Werror` elsewhere. It is linked by core, the exe and every test.
  - Compile definitions `RCLONE_BROWSER_VERSION="…"`, `QT_IMPLICIT_QFILEINFO_CONSTRUCTION` and (if Multimedia is found) `RB_HAVE_MULTIMEDIA` become `target_compile_definitions(rclone-browser-core PUBLIC …)`, so tests see the same ones.
  - Root option `RB_BUILD_TESTS` (default ON) adds `enable_testing()` and `add_subdirectory(tests)`.
  - Cache path `RB_TEST_RCLONE` (default empty): e2e tests are registered only when it is non-empty.
  - CTest labels are exactly `unit` and `e2e`.
- D11: images and gates.
  - `rb-build:24.04` already exists locally (Qt 6.4.2, GCC 13, CMake 3.28) and is the required image.
  - `rb-build:qt610` (Ubuntu 22.04, aqt Qt 6.10.3 at `/opt/Qt/6.10.3/gcc_64`) is built only by the user (prefetch P-2).
  - Containers run with `--network none`, as the host uid:gid, with `HOME=/tmp/home`.
  - The repo is mounted at `/src`. The host rclone is mounted read-only at `/opt/rclone/rclone`.
- D12: CI changes are verified only by G-CI after a user push. Locally, only the YAML syntax check G-YAML (host PyYAML, present 2026-10-21) and `git diff` review are possible.
- I-6: no network, push or tag. I-8: `-Werror` everywhere.
- Test support contract (`tests/support/rb_test_support.h`, header-only, NEW):
  ```cpp
  #pragma once
  #include <QtCore>
  // Call first in initTestCase(): makes QSettings/QStandardPaths hermetic.
  inline QTemporaryDir &rbTestDir() { static QTemporaryDir d; return d; }
  inline void rbInitTestEnvironment() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("rclone-browser-tests");
    QCoreApplication::setApplicationName("rclone-browser-tests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, rbTestDir().path());
    qputenv("XDG_CONFIG_HOME", (rbTestDir().path() + "/xdg").toUtf8());
    // I-9: any rclone child started without --config uses an empty temp config, never the host one.
    // The file must exist: a missing RCLONE_CONFIG file makes rclone print a NOTICE line (local probe 2026-10-21).
    const QString emptyConf = QDir(rbTestDir().path()).filePath("rclone-empty.conf");
    QFile f(emptyConf); f.open(QIODevice::WriteOnly); f.close();
    qputenv("RCLONE_CONFIG", emptyConf.toUtf8());
  }
  inline QString rbFixture(const QString &rel) {   // RB_FIXTURES is set by CTest
    return QDir(qEnvironmentVariable("RB_FIXTURES")).filePath(rel);
  }
  inline QString rbRclone() { return qEnvironmentVariable("RB_TEST_RCLONE"); }
  ```
  With a fresh `XDG_CONFIG_HOME`, `IsPortableMode()` is false. `GetSettings()` then returns `QSettings()`, which uses the INI default format under `rbTestDir()`.
- Test naming rule (binding for every phase):
  - each test ID `T-FOO-BAR` is the private slot `T_FOO_BAR()` (hyphens become underscores) in the file the unit names, so QtTest prints `PASS   : <Class>::T_FOO_BAR()`;
  - data-driven slots keep the same name and add `T_FOO_BAR_data()`;
  - files with e2e tests are named `tests/tst_e2e_*.cpp` and get the CTest label `e2e`; every other `tests/tst_*.cpp` gets the label `unit`;
  - "passes with N functions" in a G-U gate means: no `FAIL!` line, and the number of distinct `::T_[A-Z0-9_]+\(` names in `PASS` lines of that executable's `ctest -V` output equals N. `initTestCase`/`cleanupTestCase` and data rows are not counted.

## Sub-phases

### 0.1 Docker dev environment and `scripts/dev.sh`
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews at P0.
- **Files:**
  - NEW `docker/qt64.Dockerfile`: the content below. It builds the tag `rb-build:24.04` that already exists locally.
  - NEW `docker/qt610.Dockerfile`.
  - NEW `scripts/dev.sh` (executable, `#!/usr/bin/env bash`, `set -euo pipefail`).
  - WRITE `README.md`: a new section `## Development (Docker)` before the license section.
  - READ the scratch history in STATE.md §3 (G-BASE command).
- **Change:**
  - Preconditions: G-BASE recorded.
  - Contract (`scripts/dev.sh <command> [qt64|qt610]`, flavor default `qt64`, run from anywhere):
    - Resolve the repo root with `git rev-parse --show-toplevel`.
    - Image map: `qt64→rb-build:24.04`; `qt610→rb-build:qt610`, with `-DCMAKE_PREFIX_PATH=/opt/Qt/6.10.3/gcc_64`.
    - `RB_RCLONE` env, default `$(command -v rclone)`. It must exist. Its first `version` line must match `^rclone v1\.(7[5-9]|[89][0-9])\.`. On a mismatch print `dev.sh: rclone >= 1.75 required, found: <line>` and exit 2.
    - Missing image: print `dev.sh: image <tag> missing; build it with: scripts/dev.sh image <flavor> (needs network)` and exit 3.
    - Commands:
      - `image`: runs `docker build -t <tag> -f docker/<flavor>.Dockerfile docker/`. Documented as needing network; the executor never runs it.
      - `build`: runs `cmake -S /src -B /src/build-docker-<flavor> -G Ninja -DCMAKE_BUILD_TYPE=Release -DRB_BUILD_TESTS=ON -DRB_TEST_RCLONE=/opt/rclone/rclone [prefix] && cmake --build /src/build-docker-<flavor> -j"$(nproc)"`.
      - `test`: runs `ctest --test-dir /src/build-docker-<flavor> -L unit --output-on-failure` plus any arguments given after `--` (for example `scripts/dev.sh test qt64 -- -R 'tst_rclone_(log|format)'`). Focused gates G-U<id> use this form.
      - `e2e`: same as `test` with `-L e2e`.
      - `smoke`: in a fresh temp `XDG_CONFIG_HOME`, runs `QT_QPA_PLATFORM=offscreen /src/build-docker-<flavor>/build/rclone-browser > /tmp/smoke.log 2>&1 &`, then `sleep 10`. If the process is alive, print `smoke: alive after 10s`, kill it and exit 0. Otherwise print the log and exit 1.
      - `fixtures`: runs `bash /src/scripts/record-fixtures.sh /opt/rclone/rclone /src/tests/fixtures/rclone-1.75`. If the script is missing, print `dev.sh: scripts/record-fixtures.sh not present yet` and exit 4.
      - `shell`: interactive bash.
    - Every container command uses `docker run --rm --network none --user "$(id -u):$(id -g)" -e HOME=/tmp/home -e RCLONE_CONFIG=/tmp/home/rclone-empty.conf -v "$ROOT:/src" -v "$RB_RCLONE:/opt/rclone/rclone:ro" -w /src <image> bash -c 'mkdir -p /tmp/home && : > /tmp/home/rclone-empty.conf && <cmd>'`, plus `-it` for `shell` only. The empty file must exist: with a missing `RCLONE_CONFIG` file rclone prints `NOTICE: Config file "…" not found - using defaults`, while an existing empty file is silent (local probe with a temp config, 2026-10-21).
  - Steps:
    1. S1 — Write `docker/qt64.Dockerfile`:
       ```dockerfile
       # Local dev/test image: distro Qt (oldest supported Qt 6 here). Tag: rb-build:24.04
       FROM ubuntu:24.04
       ENV DEBIAN_FRONTEND=noninteractive
       RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
             build-essential cmake ninja-build git \
             qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev libgl-dev \
             qt6-tools-dev qt6-l10n-tools \
           && rm -rf /var/lib/apt/lists/*
       ```
       Expected: the file is the recipe that built the local `rb-build:24.04`, plus `qt6-tools-dev qt6-l10n-tools` (Qt Linguist, needed from phase 12, R6). The existing image keeps working without them, because LinguistTools is an optional CMake component. Rebuilding the image is prefetch P-5 (user, network).
    2. S2 — Write `docker/qt610.Dockerfile`:
       ```dockerfile
       # Local image mirroring CI (Qt 6.10.3 via aqt on Ubuntu 22.04). Tag: rb-build:qt610
       FROM ubuntu:22.04
       ENV DEBIAN_FRONTEND=noninteractive
       RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
             build-essential cmake ninja-build git python3-pip ca-certificates \
             libgl1-mesa-dev libegl1 libxkbcommon0 libxkbcommon-x11-0 libfontconfig1 \
             libfreetype6 libglib2.0-0 libdbus-1-3 libxcb-cursor0 \
           && rm -rf /var/lib/apt/lists/*
       RUN pip3 install --no-cache-dir aqtinstall==3.3.0 \
           && aqt install-qt linux desktop 6.10.3 linux_gcc_64 -m qtmultimedia -O /opt/Qt
       ```
       Expected: the file exists and is unbuilt (the user builds it in P-2).
    3. S3 — Write `scripts/dev.sh` per the contract. Then run `bash -n scripts/dev.sh` and `chmod +x`. Expected: syntax OK. `scripts/dev.sh nosuch` prints usage and exits 64.
    4. S4 — Run `scripts/dev.sh build qt64` and `scripts/dev.sh smoke qt64`. Expected: the build succeeds (unknown `-DRB_*` variables only warn), and smoke prints `smoke: alive after 10s`.
    5. S5 — README `## Development (Docker)`: the commands, the flavors, the need for network for `image`, and that the host rclone ≥ 1.75 is mounted. Expected: the section exists and the other sections are unchanged.
  - Recovery boundary: after S4 (long build).
  - Failure handling: if the build fails at baseline code, compare with G-BASE. A new failure goes to agent-1:opus.
- **Unit tests:** N/A — there is no test framework yet (0.2 creates it). The script contract is checked by G-U0.1 assertions.
- **e2e tests:** G-U0.1. `scripts/dev.sh build qt64` exits 0 and `build-docker-qt64/build/rclone-browser` exists. `scripts/dev.sh smoke qt64` exits 0 with `smoke: alive after 10s`. `RB_RCLONE=/bin/true scripts/dev.sh build` exits 2.
- **Done:** files exist; G-U0.1 passes; README section added; unit closed in STATE.md; completion commit `sub-phase(0.1): …` with PEV trailers.

### 0.2 Core library split and QtTest harness
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews the CMake design before S2 and the diff after S5 (build-system contract, MSVC PCH, macOS bundle).
- **Files:**
  - WRITE `CMakeLists.txt` (root): option, `enable_testing`, subdirectory.
  - WRITE `src/CMakeLists.txt`: core library, exe, `rb_warnings`, definitions.
  - NEW `tests/CMakeLists.txt`, `tests/support/rb_test_support.h` (content in Local design context), `tests/tst_utils.cpp` (unit), `tests/tst_e2e_rclone_smoke.cpp` (e2e).
  - NEW `tests/fixtures/.gitkeep`.
  - READ `src/utils.cpp` (`GetDefaultOptionsList`, `GetSettings`, `IsPortableMode`).
- **Change:**
  - Preconditions: 0.1 DONE.
  - Contract:
    - `src/CMakeLists.txt` keeps the lists `UI`, `MOC`, `OTHER`, `SOURCE`. Define `set(CORE_SOURCE ${SOURCE})` and `list(REMOVE_ITEM CORE_SOURCE main.cpp)`.
    - `add_library(rclone-browser-core STATIC ${CORE_SOURCE} ${OTHER} ${MOC} ${MOC_OUT} ${UI_OUT})`.
    - `target_include_directories(rclone-browser-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})`.
    - `target_link_libraries(rclone-browser-core PUBLIC Qt6::Widgets Qt6::Network ${OPTIONAL_QT_LIBS} rb_warnings)`. On APPLE also add `${COCOA_LIB} ${IOKKIT_LIB} ${USER_NOTIFICATIONS_LIB} ${UNIFORM_TYPE_IDENTIFIERS_LIB}`.
    - The `.mm` files and `osx_helper.h` / `mac_os_*` stay in the core lists, as today.
    - Exe targets keep their current names and properties (WIN32 `RcloneBrowser` with the WIN32 flag; APPLE `rclone-browser` MACOSX_BUNDLE with the plist properties; other `rclone-browser` plus the install rules). Each is built from `main.cpp ${QRC_OUT}` plus `resources.rc` (WIN32) or `icon.icns` (APPLE) and links `rclone-browser-core`.
    - `ADD_MSVC_PRECOMPILED_HEADER(pch.h pch.cpp CORE_SOURCE)` and `(… MOC_OUT)`, called where the `SOURCE` calls are today. `CORE_SOURCE` inherits `pch.cpp` from `SOURCE`, exactly as `SOURCE` does today. main.cpp gets no PCH flags: it includes headers that include `pch.h` normally.
    - Replace `add_definitions("-pedantic …")` / `CMAKE_CXX_FLAGS /W4 /WX…` with `add_library(rb_warnings INTERFACE)` plus `target_compile_options(rb_warnings INTERFACE $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX /wd4100 /wd4189,-pedantic -Wall -Wextra -Werror>)`. Use the generator expression; if that is awkward, use the `if(MSVC)` form. Define it in the root CMakeLists before `add_subdirectory(src)` so tests can link it.
    - Root: after `project()`, add `option(RB_BUILD_TESTS "Build tests" ON)` and `set(RB_TEST_RCLONE "" CACHE FILEPATH "rclone used by e2e tests")`. After `add_subdirectory(src)`: `if(RB_BUILD_TESTS) enable_testing() add_subdirectory(tests) endif()`.
    - `tests/CMakeLists.txt`:
      ```cmake
      find_package(Qt6 REQUIRED COMPONENTS Test)
      function(rb_add_test name label)
        add_executable(${name} ${name}.cpp ${ARGN})
        target_link_libraries(${name} PRIVATE rclone-browser-core Qt6::Test rb_warnings)
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES LABELS ${label} TIMEOUT 300
          ENVIRONMENT "QT_QPA_PLATFORM=offscreen;RB_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures;RB_TEST_RCLONE=${RB_TEST_RCLONE}")
      endfunction()
      rb_add_test(tst_utils unit)
      if(RB_TEST_RCLONE)
        rb_add_test(tst_e2e_rclone_smoke e2e)
      endif()
      ```
      Later units append `rb_add_test` lines here (the registration convention).
  - Scope: no source behavior change, no file moves.
  - Steps:
    1. S1 — Draft the CMake changes. Send the diff to agent-1:opus for design review before building. Expected: approval is recorded in STATE.md §7.
    2. S2 — Apply them. Run `scripts/dev.sh build qt64`. Expected: the build succeeds and the exe exists at the same path as before (`build-docker-qt64/build/rclone-browser`, because `CMAKE_RUNTIME_OUTPUT_DIRECTORY` is unchanged).
    3. S3 — `tests/tst_utils.cpp` (QTEST_GUILESS_MAIN, class `TestUtils`):
       - `initTestCase()` calls `rbInitTestEnvironment()`.
       - `T_UTIL_SPLIT()` (T-UTIL-SPLIT): write `GetSettings()->setValue("Settings/defaultRcloneOptions", R"(--a --b="x y" --c z)")` and destroy the settings object (scope). Then `QCOMPARE(GetDefaultOptionsList("defaultRcloneOptions"), QStringList({"--a","--b=x y","--c","z"}))`.
       - `T_UTIL_EMPTY()` (T-UTIL-EMPTY): an unset key gives an empty list.
       - `T_UTIL_PORTABLE()` (T-UTIL-PORTABLE): `QVERIFY(!IsPortableMode())`.
       Expected: 3 test functions plus init/cleanup pass.
    4. S4 — `tests/tst_e2e_rclone_smoke.cpp` (QTEST_GUILESS_MAIN):
       - `initTestCase()` calls `rbInitTestEnvironment()`.
       - `T_E2E_SMOKE_VERSION()` (T-E2E-SMOKE-VERSION) runs `QProcess` `rbRclone() version`. It asserts exit 0 and that the first line matches `^rclone v1\.(7[5-9]|[89]\d)\.`.
       - `T_E2E_SMOKE_LSJSON()` (T-E2E-SMOKE-LSJSON): create a `QTemporaryDir` with file `a.txt` ("x"). `lsjson <dir>` exits 0 and the stdout JSON array has 1 element with `Name=="a.txt"` and `Size==1`.
       - Both functions set the child environment with all `RCLONE_*` removed and then `RCLONE_CONFIG` set back to the temp empty config from `rbInitTestEnvironment()` (I-9; local lambda, because the D2 factory does not exist yet).
       Expected: pass.
    5. S5 — Run `scripts/dev.sh test` and `scripts/dev.sh e2e`. Run `scripts/dev.sh smoke`. Send the diff to agent-1:opus. Expected: ctest reports `tst_utils` passed (label unit, 1 test) and `tst_e2e_rclone_smoke` passed (label e2e, 1 test).
  - Recovery boundary: after S2.
  - Failure handling: a link error from a missing symbol means a file is in the wrong target; fix the lists. A static-init or MOC duplicate means `MOC_OUT` was listed twice (today's code lists `${MOC_OUT}` twice; keep it once in core). The MSVC/macOS outcome is unknown locally → mark the unit CI-compiled (D12) and record a G-CI debt row in STATE.md §9.
- **Unit tests:** T-UTIL-SPLIT, T-UTIL-EMPTY, T-UTIL-PORTABLE in `tests/tst_utils.cpp`. Gate G-U0.2: `scripts/dev.sh test` output contains `tst_utils .* Passed` and `100% tests passed, 0 tests failed out of 1`.
- **e2e tests:** T-E2E-SMOKE-VERSION, T-E2E-SMOKE-LSJSON in `tests/tst_e2e_rclone_smoke.cpp`. Gate G-U0.2: `scripts/dev.sh e2e` shows `out of 1` and passed. The QtTest output shows `Totals: 4 passed` (init + 2 + cleanup).
- **Done:** build, tests and smoke are green. Reviews (before/after) are recorded. A G-CI debt row exists for the Windows/macOS link check. Unit closed; completion commit.

### 0.3 CI: tests in every job, Linux e2e, obsolete CI removal, CodeQL, dependabot, VERSION/tag check
- **Model:** agent-2:sonnet
- **Assignment:** implementer, CI-compiled (D12). agent-1:opus reviews at P0, with G-CI evidence when the user pushes.
- **Files:**
  - WRITE `.github/workflows/build.yml`.
  - DELETE `.travis.yml`, `.appveyor.yml`.
  - WRITE `README.md`: remove the Travis/AppVeyor badge references `[1]`, `[2]`, `[img1]`, `[img2]` and their uses on line 3. Keep the other badges.
  - NEW `.github/workflows/codeql.yml` and `.github/dependabot.yml`, from `git show hrpv/master:<path>` (prefetch P-1).
- **Change:**
  - Preconditions: 0.2 DONE. P-1 done (`git rev-parse hrpv/master` succeeds); otherwise the unit is BLOCKED on P-1.
  - Contract:
    - Every build job configures with `-DRB_BUILD_TESTS=ON`. After the build it runs `ctest --test-dir build -L unit --output-on-failure -C Release`.
      - Windows: before ctest, `$env:PATH = "$env:QT_ROOT_DIR\bin;$env:PATH"`.
      - FreeBSD: inside the VM run script.
      - macOS: before bundling.
      - Linux: before `make install`.
    - The Linux job additionally runs `ctest -L e2e` with an official rclone v1.75.2 binary, downloaded in CI with a SHA256 check:
      - URL `https://downloads.rclone.org/v1.75.2/rclone-v1.75.2-linux-amd64.zip`;
      - the expected hash is taken from `https://downloads.rclone.org/v1.75.2/SHA256SUMS` in the same step (CI may use the network; the local executor never does);
      - pass `-DRB_TEST_RCLONE=$RUNNER_TEMP/rclone/rclone`.
    - `create_release` gets a first step that fails when the tag does not match VERSION: `test "${GITHUB_REF_NAME#v}" = "$(cat VERSION)" || [[ "${GITHUB_REF_NAME#v}" == "$(cat VERSION)"-* ]]`. This is the idea from totza2010 `5d5656008`.
    - codeql.yml: keep hrpv's file. Set triggers to `workflow_dispatch` plus `schedule: cron '0 3 * * 1'`, with C++ build mode `manual` using the Linux build commands.
    - dependabot.yml: `package-ecosystem: github-actions`, `directory: /`, `schedule.interval: monthly`. Do not take hrpv's action-version bumps.
  - Steps:
    1. S1 — Edit build.yml per the contract. Expected: G-YAML passes.
    2. S2 — Delete the Travis/AppVeyor files and their README badges. Expected: `git grep -n -i 'travis\|appveyor'` returns no results outside CHANGELOG.md.
    3. S3 — Add codeql.yml and dependabot.yml. Expected: the YAML parses.
  - Failure handling: no local CI. Any job failure in G-CI reopens the unit with a new attempt.
- **Unit tests:** N/A — workflow files. Local check G-U0.3: YAML parse and the grep assertion.
- **e2e tests:** G-CI (user push). All 5 build jobs green (CodeQL is not push-triggered: the user also starts one `workflow_dispatch` run of it and reports its result), ctest unit output present in each job log, and the Linux job shows `tst_e2e_rclone_smoke … Passed`. Until it runs, record `not-run (needs push)` and a G-CI debt row.
- **Done:** local checks pass; G-CI debt recorded; unit closed; completion commit.

### 0.4 Upstream triage document (user sign-off)
- **Model:** agent-3:haiku
- **Assignment:** author. agent-1:opus reviews the dispositions at P0. The user signs off per issue; that sign-off is not a prerequisite of any unit.
- **Files:**
  - READ `docs/plans/001_plan-RcloneBrowserModernization/inputs/upstream-open-issues.json` (129 issues) and `inputs/upstream-open-prs.json` (10 PRs).
  - NEW `docs/triage/upstream-issues.md`.
- **Change:**
  - Preconditions: 0.1 DONE (ID order only; the unit reads plan inputs and needs no build).
  - Contract: one Markdown table per category: Bug-fixed-by-plan, Bug-verify, Feature-in-plan, Feature-deferred, Packaging/platform, Question/support, Obsolete.
    - Columns: `# | Title | Created | Disposition | Plan unit | User OK`.
    - Disposition is one of `fixed-by <unit>`, `verify-in <unit>`, `in-plan <unit>`, `deferred`, `wontfix-obsolete`, `needs-user`.
    - `User OK` starts empty.
  - Known mappings (supervisor):
    - #141 #142 #160 → 1.4
    - #107 #130 #140 → 2.2
    - #162 → 3.1 (PR #173 reimplemented)
    - #163 #172 #239 → 3.1 (terminal) and 6.2 (native)
    - #79 → 3.3 and 5.2
    - #117 → 3.3
    - #146 #250 → already implemented (`MainWindow::autoStartMounts`), disposition `verify-in P3`
    - #161 → PR #1 (macOS arm64 DMG)
    - #243 → 8.1; #199 → 8.2; #129 → 9.1; #16 → 10.1; #21 → 10.2; #94 → 10.3; #71 → 11.x; #47 → 12.x
    - PRs #246 #225 #212 → `superseded-by 1.1`; #173 → 3.1; #205 → 3.1; #232 #164 → 4.2; #152 → obsolete (Travis); #126 → obsolete (Qt6 port); #166 → deferred
  - Every other issue gets a disposition from its title. Unclear ones get `needs-user`.
  - Steps:
    1. S1 — Generate the tables from the JSON (a local python3 one-off, not committed). Expected: 129 issue rows and 10 PR rows; the counts are printed under each heading.
    2. S2 — Write an intro paragraph: the source date is 2026-10-21, nothing is posted upstream, and the user fills `User OK`. Expected: the file renders.
- **Unit tests:** N/A — documentation. Check G-U0.4: `python3 -I -c` counts table rows starting with `| #`; the sum is 139.
- **e2e tests:** N/A — no behavior.
- **Done:** document present and counts verified; unit closed; completion commit. The user sign-off is tracked in STATE.md §9 as a non-blocking item.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE (qt64). G-BUILD610 runs if the image `rb-build:qt610` exists; otherwise record `blocked (P-2)`. G-CI is recorded as debt.
- Assertions: unit 1/1, e2e 1/1, smoke alive, no compiler warnings (`grep -c 'warning:' build log == 0`).
- README obligation: 0.1 (Development section) and 0.3 (badges). At closure, verify the README build instructions still match `scripts/dev.sh`.
- Every sub-phase must be DONE or SKIPPED with a justification. Run the full phase gates now.
- Supervisor agent-1:opus reviews integration, I-6, I-8, the tests and the documentation.
- Open closure unit P0, record the review and the verified phase result, mark the phase DONE, and close with commit `phase(0): …`. Never fabricate an empty commit.
