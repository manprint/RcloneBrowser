# Serve in Jobs, webdav-webui, remote icon grid — Plan overview

Authored 2026-10-23 by agent-1:opus (strong supervisor). Folder: `RcloneBrowser/docs/plans/002_plan-ServeJobsWebui/`.
Source request: `../Evolutiva01.md` (workspace root, items 1–4), user answers of 2026-10-23 (U1–U4 below).
Execution starts at [STATE.md](STATE.md); all live readiness and progress are there.

## Goal and reference scenario

Four changes to the Rclone Browser fork (`RcloneBrowser/`, Qt 6 / C++17), using what the rclone fork
(`rclone/`, `webui/` package, `--webui-*` flags) already ships. The rclone fork is **not modified**.

1. **Bug — remote icon size.** Preferences > remote icon scale ("multiplier") `1.0` makes the remote list
   items overlap so a remote cannot be selected precisely; `2.0` only mitigates it. Fix for every layout
   (`tiles`, `list`, `longlist`) and every positive scale.
2. **Serves in Jobs.** A started serve no longer opens the separate "Serves" window. Each serve appears in
   the Jobs tab like a mount, with every action the window offered (Copy URL, Stop, refresh by the
   daemon state) plus Restart, Open in browser and Close. The "Serves" window and File > "Serves…" are
   removed (U1). The "Serve…" configuration dialog stays modal.
3. **webdav-webui.** The Serve dialog Type list gets `webdav-webui` (right after `webdav`): rclone
   `serve webdav` with the fork's web interface, configurable with all 19 `--webui-*` options (basic +
   advanced section), plus a VFS cache mode selector, remembering the last values except the secret (U3).
   With an rclone that lacks the fork's `webdav_webui` option block (upstream rclone) the entry is shown
   but not usable, and the dialog says why. Classic `webdav` stays classic also with the fork binary.
4. **Toolbar.** The remote tab button row gets a "Serve" quick-action button next to Mount and Stream.

Reference scenario (G-FINAL):
- `tst_remote_list_layout` proves no overlap and exact hit-testing for 3 layouts × scales {0.5, 1, 1.5, 2, 4, 8}.
- `tst_e2e_serve_jobs` (MainWindow + real upstream rcd): start http serve → a `ServeJobWidget` in Jobs
  with URL, "Jobs (1)"; Stop → finished; Restart → running with a new id; filter/sort/clean handle it;
  daemon stop → every serve widget finished.
- `tst_e2e_serve_webui` (fork binary): webdav-webui serve answers a browser GET with the web UI page;
  options read back equal what was set; classic webdav on the fork has no web UI; the upstream binary
  reports the capability missing and never receives `serve/start` for webdav-webui.
- `remote_widget` has `buttonServe` bound to `actionServe` in all three button styles (`tst_shortcuts` `T_SC_SERVE_BUTTON`).

Exclusions: no change to `rclone/` sources; no serve persistence across app restarts (serves still end
with the backend); "Stop all jobs" keeps stopping transfers only; no TLS fields; no push/tag/release.

## User decisions (2026-10-23, AskUserQuestion)

| ID | Question | Answer |
|----|----------|--------|
| U1 | Serves window and File > Serves… | Remove both; serves only in Jobs; Serve… dialog stays modal |
| U2 | Ended serve in Jobs | Stays as finished (Close, Clean not running) **and** has Restart with the same params (password kept in memory only, for the session) |
| U3 | webdav-webui extras | Basic + Advanced sections; VFS cache mode selector; remember last values (never the secret); "Open in browser" button in the serve job |
| U4 | Authorizations | Network for docker image builds (`dev.sh image`, `server-images`); create branch `dev/002-serve-jobs-webui`; full-autonomous:true + WIP on; build the rclone fork binary (`go build`, offline) for e2e |

## Decisions

| ID | Decision and consequence | Authority/source | Supersedes |
|----|--------------------------|------------------|------------|
| D1 | Remote list geometry is computed once per listing by a NEW pure helper `remotesListGeometry(layout, iconPx, QFontMetrics, names)` (`src/remote_list_layout.{h,cpp}`) from the icon size **and the font metrics of the names**; `MainWindow::rcloneListRemotes` applies it after the item loop. Formula in phase_02.md §Local design. | supervisor, R1 | per-item grid `size+20/size+40`, `size+100/size+20`, `size+8` |
| D2 | `buttonServe` (QToolButton, `remote_widget.ui`) after `buttonStream`, `setDefaultAction(ui.actionServe)`, styled/sized in every branch that styles `buttonStream`. Enable/tooltip come from the action (`updateServeAction`). | supervisor; user item 4 | — |
| D3 | NEW `ServeJobWidget` (`src/serve_job_widget.{h,cpp}`, built in code) is the Jobs entry of one serve; `ServesWindow`, `MainWindow::showServes/servesWindow`, `actionServes` are deleted. | U1 | ServesWindow |
| D4 | The daemon is the source of truth: a serve widget is running exactly while its id is in the newest `serve/list` and the daemon is Running. A daemon leaving Running marks every running serve widget finished ("Backend stopped"). An id listed with no widget (started by an earlier timed-out request) is adopted as a widget without Restart. | supervisor; I-6 | — |
| D5 | `ServeManager::start` returns a `quint64` token; `started(token, entry)` and `startFailed(token, message)` replace `started(entry)`; `error(message)` keeps the message box. MainWindow maps token → pending (params, widget-or-null) to create a widget or to re-arm one on Restart. | supervisor | `started(entry)` |
| D6 | webdav-webui protocol: per start, `options/set {"webdav_webui": <all 19 keys>}` then `serve/start {type:"webdav", …}`; `Enabled` is true only for flavor webdav-webui. On a webui-capable rclone **every** webdav start (classic too) sends the full block, so classic gets `Enabled:false` and no option leaks from a previous start. Starts are serialized FIFO in ServeManager: one start (set+start pair) in flight. A failed `options/set` aborts that start (`startFailed` + `error`). | supervisor; R2, R3 | — |
| D7 | Capability: `options/blocks` reply `options` array contains `"webdav_webui"` ⇒ supported. Tri-state Unknown/Yes/No, reset to Unknown whenever the daemon state changes, reloaded on Running (with `loadTypes`). A webdav start while Unknown first resolves the capability. Never derive it from the version string. | supervisor; R3 | — |
| D8 | Serve dialog: pseudo-type `webdav-webui` inserted after `webdav`; "Web interface" group with Basic and collapsible Advanced sections, VFS cache mode combo; values remembered in QSettings `Serve/webui/<GoFieldName>` and `Serve/webui/VfsCacheMode`, never `Secret`. Unsupported/Unknown capability disables OK with an explanatory label. | U3; supervisor | — |
| D9 | "Open in browser" for types http, webdav and webdav-webui opens `serveBrowserUrl(entry)`: `serveUrl` with an unspecified host (`0.0.0.0`, `[::]`, `::`, empty) replaced by `127.0.0.1`. Copy URL keeps `serveUrl` unchanged. | U3; supervisor | — |
| D10 | No change to `rclone/`. Rejected: teaching the fork's webdav `AddRc` to parse a per-serve `webuiOpt` block (would change the fork and still need a fork release; the global block + serialization works with the shipped fork). | supervisor; R2 | — |
| D11 | Test infra: CMake cache `RB_TEST_RCLONE_WEBUI` (FILEPATH) passed to tests as env `RB_TEST_RCLONE_WEBUI`; `scripts/dev.sh` reads `RB_RCLONE_WEBUI`, bind-mounts it read-only at `/opt/rclone/rclone-webui` and passes `-DRB_TEST_RCLONE_WEBUI=…` (empty when unset). Fork binary built with `CGO_ENABLED=0 GOPROXY=off go build` into `RcloneBrowser/build-rclone-webui/rclone` (git-ignored by `build*`). `tst_e2e_serve_webui` is registered only when `RB_TEST_RCLONE_WEBUI` is non-empty (CI `build.yml` runs `ctest -L e2e` with an upstream rclone and no fork: an unconditional test would break CI); once registered it QFAILs (never QSKIPs) if the path is not executable; plan gates require its discovery (G-U33 `1 tests`, G-E2E total), and `dev.sh e2e` prints a notice when the variable is unset. | U4; supervisor (cold-read revision 1) | — |
| D12 | Serves count in `mJobCount`/"Jobs (N)", sort, filter, Clean not running and the >75 oldest-removal; "Stop all jobs" (`mTransferJobCount`) does not stop serves (same as mounts). canClose/quitApp mark running serve widgets finished without an rc call (the backend ends with the app). | supervisor | — |

## Open questions

None. All product questions answered (U1–U4). STATE.md readiness: READY.

## Architecture

```
RemoteWidget ──actionServe/buttonServe──► serveRequested(fs) ──► MainWindow::openServeDialog(fs)
                                                                   │ ServeDialog (modal; webdav-webui group)
                                                                   ▼
                                               MainWindow::startServe(ServeParams) ─► ServeManager::start → token
ServeManager (FIFO, one in flight):  [webdav && capability Yes] options/set webdav_webui ─► serve/start ─► started(token, entry) | startFailed
                                     refresh() serve/list ─► listChanged(entries)
                                     loadCapabilities() options/blocks ─► capabilitiesChanged(WebuiSupport)
MainWindow: token→pending; addServeJob() inserts ServeJobWidget+line in ui.jobs (like addNewMount);
            listChanged → widgets not listed → markFinished(); unknown ids → adopt
            daemon stateChanged(!Running) → all running serve widgets markFinished("Backend stopped")
ServeJobWidget: status button, info, URL, details (type, source, address, started/finished),
                Copy URL, Open in browser, Stop/Close (cancel button), Restart
```

## Interfaces and compatibility

| Surface | Exact name/type | Default | Errors/conflicts | Compatibility |
|---------|-----------------|---------|------------------|---------------|
| Helper | `RemotesListGeometry remotesListGeometry(const QString &layout, int iconPx, const QFontMetrics &fm, const QStringList &names)` | unknown layout ⇒ `tiles` | iconPx < 8 ⇒ 8 | settings keys unchanged |
| ServeParams | + `QString flavor` (`""` or `kWebuiFlavor` = `"webdav-webui"`), `WebuiOptions webui`, `QString vfsCacheMode` (`""` = not sent) | empty | — | `type` stays the rclone serve type (`webdav` for the webui flavor) |
| `serveStartParams` | adds `"vfs_cache_mode"` when non-empty; **never** a key starting with `webui` (I-4) | — | — | old keys unchanged |
| `webuiOptionsBlock(const WebuiOptions&, bool enabled)` | `{"webdav_webui":{19 keys}}`, Go field names (R2) | fork defaults | rclone 500 on bad duration/size → startFailed | — |
| ServeManager | `quint64 start(const ServeParams&)`; signals `started(quint64, ServeEntry)`, `startFailed(quint64, QString)`, `capabilitiesChanged(WebuiSupport)`; `loadCapabilities()`, `webuiSupport()` | Unknown | `error(QString)` kept | `started(ServeEntry)` removed (only MainWindow/tests use it) |
| MainWindow | public slot `void startServe(const ServeParams &p)` | — | not Running ⇒ status bar "Serve requires the rclone backend" | `showServes()` removed |
| UI | `actionServes` removed from `main_window.ui`; `buttonServe` added to `remote_widget.ui` | — | — | snapshot fixture updated (1 line) |
| QSettings | `Serve/webui/<GoFieldName>` (18 keys, no `Secret`), `Serve/webui/VfsCacheMode` | absent ⇒ fork default | — | new keys only |
| Test env | `RB_TEST_RCLONE_WEBUI`, dev.sh `RB_RCLONE_WEBUI` | empty | webui e2e QFAIL when empty | existing gates unchanged |

## Phase map

| Logical phase | File | Sub-phase IDs | Depends on | Assignment |
|---------------|------|---------------|------------|------------|
| 0 Environment and test infrastructure | [phase_01.md](phase_01.md) | 0.1, 0.2, 0.3 | none | agent-1:opus (0.1), agent-2:sonnet (0.2), agent-3:haiku (0.3) |
| 1 Remote list grid fix + Serve toolbar button | [phase_02.md](phase_02.md) | 1.1, 1.2, 1.3 | P0 | agent-2:sonnet, agent-3:haiku |
| 2 Serves in the Jobs tab | [phase_03.md](phase_03.md) | 2.1, 2.2, 2.3, 2.4 | P1 | agent-2:sonnet, agent-3:haiku |
| 3 webdav-webui | [phase_04.md](phase_04.md) | 3.1, 3.2, 3.3, 3.4 | P2 | agent-2:sonnet, agent-3:haiku |

No live status in this table.

## Reuse map

| Need | Path and symbol | Contract to preserve |
|------|-----------------|----------------------|
| Icon scale | `src/utils.cpp — RemotesIconScaleFromSetting` | number > 0 or letter values; unchanged |
| Remote listing | `src/main_window.cpp — MainWindow::rcloneListRemotes` | sort order, icons, tooltips unchanged |
| Jobs entry pattern | `src/main_window.cpp — MainWindow::addNewMount`, `addStream` | widget+HLine pair inserted at 0/1, counters, `closed` removes pair |
| Jobs loops | `src/main_window.cpp` `qobject_cast<StreamWidget` sites (setupJobsTab clean ≈L1003, quitApp ≈L1454, canClose ≈L2419, sortJobs ≈L3163/L3224, applyJobsFilter ≈L3253); `src/main_window_tasks.cpp` ≈L2045/L2067 (oldest removal) | add a ServeJobWidget branch to each |
| Status/visual pattern | `src/mount_widget.cpp — MountWidget` (status button text/colour, cancel→Close, `updateStartFinishInfo`, `filterFields`) | same look and status string scheme |
| Serve rc | `src/serve_manager.{h,cpp}` (`serveStartParams`, `parseServeList`, `serveUrl`, `serveTypeRules`, refresh seq) | I-5 of plan 001: credentials only in POST body |
| Serve dialog | `src/serve_widget.{h,cpp} — ServeDialog` | existing rules/validation unchanged for other types |
| rc fake | `tests/tst_serve_manager.cpp — struct Fake` | async replies |
| Real rcd e2e | `tests/tst_e2e_serve.cpp` (RcloneDaemon, temp config `loc`), `tests/tst_e2e_filter.cpp` (MainWindow on Jobs tab) | I-9: never host config |

## Research and evidence

Scope: rclone fork rc behaviour for the webui option block (local source + local probes; no web needed), and
Qt QListView grid semantics (official docs). Probes ran on 2026-10-23 with
`/tmp/claude-1000/…/scratchpad/probe.py` and `probe2.py` (scratchpad, not in the repo) against a fork binary
built offline (`GOPROXY=off go build`, fork HEAD `a527815ae`, reports `rclone v1.76.0-DEV`) and host
`/usr/bin/rclone v1.75.2` (upstream), each as `rcd --rc-no-auth --config /dev/null` on 127.0.0.1.

| ID | Question / required or optional | Evidence state | Affected decisions / units / tests |
|----|---------------------------------|----------------|------------------------------------|
| R1 | Why do remote items overlap at small scales; what does Qt do with gridSize/spacing? (required) | CONFIRMED (docs) + inference | D1; 1.1; T-ICON-GRID |
| R2 | How are webui options set for a serve started over rc? (required) | CONFIRMED (source + probe) | D6, D10, I-3, I-4, I-10; 3.1; T-WEBUI-BLOCK, T-E2E-WEBUI-OPTS |
| R3 | How to detect a webui-capable rclone; what does the fork do for plain webdav? (required) | CONFIRMED (probe) | D6, D7, I-5; 3.1, 3.2; T-WEBUI-CAPS, T-E2E-WEBUI-UPSTREAM |
| R4 | Value formats accepted by `options/set` and `serve/start vfs_cache_mode` (required) | CONFIRMED (probe) | D8; 3.1, 3.2; T-WEBUI-DIALOG |

### R1 — remote list overlap
- Sources: Qt 6 `QListView` reference, properties `gridSize`, `spacing`, `wordWrap` (https://doc.qt.io/qt-6/qlistview.html, page for Qt 6.12, accessed 2026-10-23); local `src/main_window.cpp — rcloneListRemotes`.
- Applicability: build images use Qt 6.4.2 and 6.10.3; these properties are unchanged across Qt 6 (inference).
- Documented fact: "When a grid layout is in force the spacing property is ignored"; `wordWrap` false by default and, even when on, "the cell will not be expanded… It will print ellipsis".
- Local fact: grid = f(icon size) only: tiles `(s+20, s+40)`, list `(s+100, s+20)`, longlist `(s+8, s+8)`, with `s = scale × PM_ListViewIconSize` (16 px on Fusion ⇒ 16 px at scale 1.0). `setSpacing(10)` is a no-op with a grid. No font metric is used.
- Supervisor inference: at 1.0 a tiles cell is 36×56 px while a label is wider and a list cell is 116 px wide for any name length, so item rects/painted labels exceed their cells and overlap neighbours; hit-testing then picks the wrong item. Not proven until T-ICON-GRID fails on the baseline (1.1 S1).
- Local check: not run (no Qt on host; images missing at planning time). 1.1 S1 runs it first.
- Decision impact: D1 formula sizes cells from font metrics; the oracle is geometric (no rect intersection, `indexAt(center)`, rect ⊆ cell). Rejected: only raising the minimum scale (keeps the bug for long names).
- Unresolved/replacement: none (1.1 S1 escalates if the baseline does not fail).

### R2 — webui options over rc
- Sources: `rclone/webui/options.go` (`OptionsInfo`, `Options`, `RegisterGlobalOptions{Name:"webdav_webui"}`), `rclone/cmd/serve/webdav/webdav_webui.go — webuiPreAuth` (reads global `webui.Opt`), `rclone/webui/ui.go — New(cfg Config, opt Options)` (by value), `rclone/cmd/serve/rc.go — startRc` (`rc.CheckParamsUsed`), `rclone/cmd/serve/webdav/webdav.go` `AddRc("webdav")` (parses only `vfsOpt`, `proxyOpt`, `opt`), `rclone/fs/rc/config.go — rcOptionsSet` (`Reshape` into the registered struct). Fork HEAD `a527815ae`, 2026-10-23.
- Documented/source fact: webui options are a process-global block, captured by value when a webdav server is built.
- Local result (probe): flat `webui_search` in `serve/start` ⇒ HTTP 500 `unknown parameters: webui_search` (fork and upstream). `options/set {"webdav_webui":{"Enabled":false,…}}` ⇒ 200; a serve started then has no web UI; after `Enabled:true` a second serve has the UI while the first stays classic (snapshot per serve). `options/get` returns Go field names: `Enabled, AllowDelete, Search, SearchMaxDepth, SearchMaxResults, SearchTimeout, PublinkPaths, Secret, TokenExpiry, Resume, ResumeDir, ResumeRetention, PlayerMaxRetry, PlayerReconnectInterval, DiskStats, UploadSettings, EditMaxSize, DirCount, MultiZipMax`. **Unknown keys are silently accepted (200)**. `serve/list` params contain only what `serve/start` got (`addr, fs, type`, plus e.g. `vfs_cache_mode`), never webui options.
- Decision impact: D6 (set full block, then start; serialized), I-10 read-back oracle (a typo would pass a 200-only test), D10 (no fork change), D4 (flavor tracked by RB, `serve/list` says `webdav` for both).

### R3 — capability and fork default
- Sources: probe; `rclone/fork-doc/uso.md` ("L'interfaccia è attiva per default", `--webui=false`).
- Local result: fork `options/blocks` lists `webdav_webui`; upstream 1.75.2 does not; upstream `options/set {"webdav_webui":…}` fails (`unknown option block`). Fork default `Enabled:true` ⇒ a plain `serve/start type=webdav` on the fork serves the web UI.
- Decision impact: D7 detection by block name (fork releases are `v1.75.2-webui.N`, a dev build says `v1.76.0-DEV`: version strings are unusable); D6 classic webdav sends `Enabled:false` on the fork; I-5.

### R4 — value formats
- Local result: durations accept `"45s"`, `"1h30m"`, `"3d"`, `"1M"`, `"0"`, `"off"`, `"-5s"` and integers (ns); `"abc"` ⇒ 500. Sizes accept `"512k"`, `"4Mi"`, `"1G"`, `"1.5M"`, `"0"`; `"bad"` ⇒ 500. Integers must be JSON numbers (`"7"` ⇒ 500). `PublinkPaths` must be a JSON array (`"/x,/y"` ⇒ 500; `[]` ok). `serve/start … "vfs_cache_mode":"writes"|"full"|"off"` ok, `"bogus"` ⇒ 500 listing `off, minimal, writes, full`. `Resume:true` with empty `ResumeDir` starts fine (default `<cache>/webui/resume`). Empty `Secret` ⇒ random keys per serve (`deriveKeys`).
- Decision impact: D8 widgets/validation regexes (phase_04.md), JSON types in `webuiOptionsBlock`.

- R4 addendum (probe 2026-10-23, fork binary): after `options/set` with string durations/sizes, `options/get {"blocks":"webdav_webui"}` returns durations as integer nanoseconds (`"45s"` ⇒ 45000000000, `"1h"` ⇒ 3600000000000, `"720h"` ⇒ 2592000000000000), sizes as integer bytes (`"8Mi"` ⇒ 8388608), ints/bools/arrays unchanged. T-E2E-WEBUI-OPTS relies on this.
- CI fact (`.github/workflows/build.yml`, read 2026-10-23): one Linux job downloads upstream rclone, configures with `-DRB_TEST_RCLONE=…` and runs `ctest -L e2e`; no job has the fork binary ⇒ D11 conditional registration.

## Invariants

| ID | Meaning | Guarding test |
|----|---------|---------------|
| I-1 | (revision 2) For layouts tiles/list/longlist × scales {0.5,1,1.5,2,4,8} with short and 40-char names, after the `.ui` properties (wordWrap true): no two item `visualRect`s intersect, `indexAt(visualRect(i).center())` is item i, in grid layouts each `visualRect(i)` fits inside `gridSize`, and each item leaves label room ≥ min(text width, 6 average chars) (phase_02.md). | T-ICON-GRID |
| I-2 | Serve password and webui `Secret` travel only in rc POST bodies; never logged, never in QSettings, never in `ServeEntry`; the Restart copy lives only in the widget's memory. | T-SRV-PARAMS (existing), T-WEBUI-REMEMBER, review |
| I-3 | On a webui-capable rclone every webdav start is `options/set` (all 19 keys, `Enabled` = flavor is webui) immediately followed by its `serve/start`, with no other rc start between. | T-WEBUI-SEQ, T-E2E-WEBUI-CLASSIC |
| I-4 | `serveStartParams` never emits a key starting with `webui`. | T-SRV-PARAMS (extended) |
| I-5 | webdav-webui is never started on an rclone without `webdav_webui`; the user sees the reason and OK is disabled. | T-WEBUI-DIALOG, T-WEBUI-SEQ (No case), T-E2E-WEBUI-UPSTREAM |
| I-6 | A ServeJobWidget is running iff its id is in the newest `serve/list` while the daemon is Running; daemon leaving Running ⇒ all serve widgets finished. | T-SJW-STATE, T-E2E-SERVE-JOBS |
| I-7 | Every Jobs loop that switches on widget type handles ServeJobWidget (clean, quit, canClose, sort compare, sort reinsert, filter, oldest removal). | T-E2E-SERVE-JOBS, P2 review checklist |
| I-8 | Zero compiler warnings (`-Werror`). | G-BUILD64, G-BUILD610 |
| I-9 | Tests never read the host rclone config; the webui e2e uses only `RB_TEST_RCLONE_WEBUI`, is registered only when it is set (CI stays green) and never QSKIPs; plan gates fail on its zero discovery. | G-U33 discovery, G-E2E total, review |
| I-10 | After a webui start, `options/get` `webdav_webui` equals the 19 values sent. | T-E2E-WEBUI-OPTS |

## Risks

| Risk | Design mitigation | Verification |
|------|-------------------|--------------|
| Grid formula still overlaps on some style | geometric oracle on real QListWidget offscreen, all layouts × scales | T-ICON-GRID (fails on baseline first) |
| Missed `qobject_cast` chain ⇒ serve widget lost in sort/filter | exact site list in phase_03.md 2.2; literal-search re-check step | T-E2E-SERVE-JOBS sort/filter/clean; P2 review |
| Concurrent webdav starts mix webui options | FIFO, one start in flight | T-WEBUI-SEQ (two queued starts) |
| Misspelled option key silently ignored | constants table; read-back test | T-WEBUI-BLOCK keys == list; T-E2E-WEBUI-OPTS |
| Fork binary unavailable/old in CI | gate needs `RB_TEST_RCLONE_WEBUI`; CI not in scope | G-E2E discovery count |
| Docker images missing on host | 0.1 rebuilds them (network authorized U4) | G-BASE |
| Start reply after 30 s timeout leaves orphan serve | adoption on `listChanged` (D4) | T-SJW-ADOPT (2.2) |

## Verification strategy

Baseline G-BASE (0.1) builds and runs unit+e2e on the unchanged tree. Each sub-phase runs its focused
`G-U*` gate (build + `ctest -R`). Each phase closure runs G-BUILD64, G-UNIT, G-E2E. The final closure P3
adds G-BUILD610, G-UNIT610, G-SMOKE and G-FINAL (named reference tests). STATE.md §3 is the command registry.

## Model-assignment summary

| Unit(s) | Implementer | Required supervisor review |
|---------|-------------|----------------------------|
| 0.1 | agent-1:opus (needs network/branch authority, environment judgement) | self (supervisor) — recorded as supervisor execution |
| 0.2 | agent-2:sonnet | agent-1:opus after diff (dev.sh/CMake) |
| 1.1 | agent-2:sonnet | agent-1:opus after S1 (baseline failure evidence) and after diff |
| 1.2 | agent-3:haiku | at P1 |
| 2.1 | agent-2:sonnet | agent-1:opus after diff (start tokens/FIFO, `startFailed` ordering) |
| 2.2 | agent-2:sonnet | agent-1:opus after diff (ServeJobWidget state machine, I-2, I-6) |
| 2.3 | agent-2:sonnet | agent-1:opus before S4 (qobject_cast site list a–h) and after diff (I-6, I-7) |
| 3.1 | agent-2:sonnet | agent-1:opus after diff (protocol/serialization I-3, I-4, error paths) |
| 3.2 | agent-2:sonnet | agent-1:opus after diff (I-2 persistence, I-5 UX) |
| 3.3 | agent-2:sonnet | agent-1:opus after diff (acceptance oracle, I-10) |
| 0.3, 1.3, 2.4, 3.4 (README) | agent-3:haiku | at phase closure |
| P0–P3 | agent-1:opus | every phase |
