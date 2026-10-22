# Phase 1 — Remote list grid fix and Serve toolbar button

Intent: remote icons never overlap and are selectable at any scale/layout (bug, request item 1); the
remote tab button row has a "Serve" button (request item 4).
Prerequisites: P0 DONE (branch, images, harness).
Phase closure: P1; review by agent-1:opus.

## State and ownership contract
Read STATE.md first. In a fresh session reconcile the active unit, actual diff, step checkpoint, scope,
and commit reference before selecting work. In a delegated worker session, follow the coordinator's
assigned OPEN unit; do not take over state/commits or revert active work. Open before edits; checkpoint
after meaningful batches and before yielding. Close after tests/review/coherence, commit when configured
(full-autonomous:true), then continue only within scope. Missing design goes to agent-1:opus.
Code lookups: the workspace index also covers worktrees `wt-beta1`, `wt-rel200`, `wt-alkl58`; always
restrict searches to `RcloneBrowser/` (tokensave `path_include: ["RcloneBrowser/"]`).

## Local design context
Plan revision: 1.
- R1 (Qt 6 QListView docs, accessed 2026-10-23): with a grid (`gridSize` non-empty) `spacing` is
  ignored; `wordWrap` never enlarges a cell. Today's grid depends on the icon size only (tiles
  `(s+20, s+40)`, list `(s+100, s+20)`, longlist `(s+8, s+8)`, `s = scale × PM_ListViewIconSize`,
  16 px on Fusion), so at scale 1.0 labels/items are wider than their cells and overlap
  (supervisor inference, proven by T-ICON-GRID-LEGACY in 1.1 S1).
- D1: geometry from icon size **and font metrics**, applied once per listing, with an explicit item size
  hint on every item so each item rect is known and fits its grid cell.
- I-1: layouts tiles/list/longlist × scales {0.5,1,1.5,2,4,8}, short and 40-char names ⇒ no two
  `visualItemRect`s intersect; `indexAt(rect.center())` returns that item; with a grid every item rect
  fits in `gridSize`.
- D2: `buttonServe` after `buttonStream`, default action `actionServe`, styled exactly like `buttonStream`.
- I-8: no warnings (`-Werror`).

### D1 algorithm (exact)
```
constexpr int kPad = 6;
struct RemotesListGeometry {
  QListView::ViewMode viewMode; bool wrapping; QSize iconSize;
  QSize gridSize;            // QSize() = no grid
  QSize itemSize;            // set as Qt::SizeHintRole on every item
  int spacing; Qt::TextElideMode elideMode;
};
RemotesListGeometry remotesListGeometry(const QString &layout, int iconPx,
                                        const QFontMetrics &fm, const QStringList &names);
void applyRemotesListGeometry(QListWidget *list, const RemotesListGeometry &g);
```
- `iconPx = qMax(iconPx, 8)`; `ch = qMax(1, fm.averageCharWidth())`;
  `maxText = max(fm.horizontalAdvance(name))` over `names` (0 when empty);
  `label(lo, hi) = qBound(lo*ch, maxText, hi*ch)`; `textH = fm.height()`.
- `tiles` (also any unknown layout): IconMode, wrapping true, elide ElideMiddle,
  `itemSize = (qMax(iconPx, label(6,16)) + 2*kPad, iconPx + fm.lineSpacing() + 2*kPad)`,
  `gridSize = itemSize + (2*kPad, 2*kPad)`, spacing 0.
- `list`: ListMode, wrapping true, ElideRight,
  `itemSize = (iconPx + kPad + label(6,32) + 2*kPad, qMax(iconPx, textH) + kPad)`,
  `gridSize = itemSize + (2*kPad, kPad)`, spacing 0.
- `longlist`: ListMode, wrapping false, ElideRight,
  `itemSize = (iconPx + kPad + label(6,64) + 2*kPad, qMax(iconPx, textH) + kPad)`,
  `gridSize = QSize()`, spacing `kPad/2`.
- `applyRemotesListGeometry`: `setViewMode`, `setMovement(QListView::Static)`,
  `setResizeMode(QListView::Adjust)`, `setWrapping`, `setIconSize(iconSize)`, `setGridSize(gridSize)`
  (QSize() clears it), `setSpacing`, `setTextElideMode`, `setWordWrap(false)`,
  `setUniformItemSizes(true)`, then `item(i)->setSizeHint(itemSize)` for every row.

## Sub-phases

### 1.1 Remote list geometry helper and its use
- **Model:** agent-2:sonnet
- **Assignment:** implement test-first; agent-1:opus reviews S1 evidence (legacy repro must fail I-1)
  and the final diff.
- **Files:** READ `src/main_window.cpp — MainWindow::rcloneListRemotes` (≈L2030–2200), `src/utils.cpp —
  RemotesIconScaleFromSetting`, `tests/CMakeLists.txt`, `tests/support/rb_test_support.h`.
  WRITE `src/main_window.cpp — rcloneListRemotes`, `src/CMakeLists.txt` (header list next to
  `remote_icons.h` ≈L89, source list next to `remote_icons.cpp` ≈L149), `tests/CMakeLists.txt`.
  NEW `src/remote_list_layout.h`, `src/remote_list_layout.cpp` (core library, `#include "pch.h"` like
  other sources), `tests/tst_remote_list_layout.cpp` (registered `rb_add_test(tst_remote_list_layout unit)`
  after `tst_remote_icons`, outside the Linux-only block).
- **Change:**
  Preconditions: P0 DONE.
  Contract: D1 algorithm above; listing order, icons, tooltips, dark/light `_inv` choice, macOS 1.5×
  factor and the `RemotesIconScaleFromSetting` semantics unchanged.
  Scope: only the geometry; no change to preferences UI/keys.
  Steps:
  1. S1 — NEW `tests/tst_remote_list_layout.cpp` with a helper `static int violations(QListWidget&)`
     returning the number of I-1 violations (pairs whose `visualItemRect` intersect + items whose
     `indexAt(center)` differs + rects not inside `gridSize` when the grid is valid), and
     `T_ICON_GRID_LEGACY`: builds a 800×600 `QListWidget` with the 12 names below and a 64×64 filled
     pixmap icon, applies the **legacy** settings copied from today's code for `tiles` and `list` with
     `size = 16` (scale 1.0), shows it, `QVERIFY(QTest::qWaitForWindowExposed(&w))`, `w.doItemsLayout()`,
     and `QVERIFY(violations(w) > 0)` for tiles **and** for list.
     Names: `a`, `gd`, `drive`, `s3`, `photos`, `backup-2026`, `a_remote_with_a_rather_long_name_xyz_1234`,
     `ÄÖÜ-umlauts`, `onedrive_business`, `x`, `pcloud`, `mega`.
     Register the test, build, run it on the unchanged main_window. Expected: PASS (legacy overlaps).
     If it fails (no overlap reproduced), STOP and escalate to agent-1:opus with the numbers: the
     oracle does not capture the bug.
  2. S2 — NEW `src/remote_list_layout.{h,cpp}` with D1 exactly. Register in `src/CMakeLists.txt`.
  3. S3 — add `T_ICON_GRID` (data-driven, `_data()` rows `layout × scale` = 18 rows): same names/icon,
     `iconPx = qRound(scale * 16)`, `g = remotesListGeometry(layout, iconPx, w.fontMetrics(), names)`,
     `applyRemotesListGeometry(&w, g)`, show, `doItemsLayout`, `QCOMPARE(violations(w), 0)`,
     `QCOMPARE(w.iconSize(), QSize(qMax(iconPx,8), qMax(iconPx,8)))`, and for tiles
     `QVERIFY(g.itemSize.height() >= g.iconSize.height() + w.fontMetrics().height())`.
     Add `T_ICON_GRID_EMPTY`: empty names ⇒ helper returns valid itemSize with width ≥ 6*averageCharWidth.
  4. S4 — `rcloneListRemotes`: hoist the loop-invariant block that computes `size` and `img_add`
     (the `#if !defined(Q_OS_MACOS)` … `#endif` block, it reads only `darkModeIni`, `iconsColour`,
     `lightModeiconScale/darkModeIconScale`, `style`, `QSysInfo`) before `for (const RemoteInfo &ri …)`;
     delete the per-item `setIconSize`/`setViewMode`/`setGridSize`/`setSpacing`/… block (the three
     `if (mIconsLayout == …)` blocks); after the loop call
     `applyRemotesListGeometry(ui.remotes, remotesListGeometry(mIconsLayout, size, ui.remotes->fontMetrics(), names))`
     where `names` collects `ri.name` in the loop. Keep the "variable not used" workaround only if
     still needed to avoid warnings. Expected: build 0 warnings.
  Checkpoint: after S1 (evidence: legacy violation counts), S3, S4.
  Failure handling: if T_ICON_GRID fails for a row, print the offending rects (QVERIFY2 message) and
  escalate after two fix attempts; do not change I-1 or the scale list.
- **Unit tests:** T-ICON-GRID-LEGACY, T-ICON-GRID (18 rows), T-ICON-GRID-EMPTY in
  `tests/tst_remote_list_layout.cpp`; gate G-U11: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_remote_list_layout$'`
  — ctest reports `1 tests` and QtTest prints `Totals: N passed, 0 failed` with N ≥ 21 (3 functions + 18 data rows + init/cleanup).
  Also `tst_mainwindow_snapshot`, `tst_startup` pass (G-UNIT).
- **e2e tests:** N/A for automation beyond G-E2E regression — the behaviour is pure layout; visual
  confirmation by the user is optional and recorded if given.
- **Done:** I-1 tests green; legacy repro evidence recorded; 0 warnings; review by agent-1:opus
  recorded; unit closed; commit.

### 1.2 Serve button in the remote tab button row
- **Model:** agent-3:haiku
- **Assignment:** implement mechanical UI wiring; agent-1:opus reviews at P1.
- **Files:** WRITE `src/remote_widget.ui` (new `<item>` after the `buttonStream` item ≈L160),
  `src/remote_widget.cpp — RemoteWidget::RemoteWidget` (default actions ≈L229–244; button style branches
  ≈L246–370), `tests/tst_shortcuts.cpp` (new test function).
- **Change:**
  Preconditions: P0 DONE (independent of 1.1).
  Contract: D2. The button shows/enables exactly like `actionServe` (tooltip "Requires the rclone
  backend" when disabled is inherited through the default action).
  Steps:
  1. S1 — `remote_widget.ui`: copy the `buttonStream` `<item>` block, rename to `buttonServe`, text
     `Serve`, same `toolButtonStyle`. Place it immediately after the `buttonStream` item.
  2. S2 — `remote_widget.cpp`: add `ui.buttonServe->setDefaultAction(ui.actionServe);` after the
     `buttonStream` line. Then run a literal search for `buttonStream` in `RcloneBrowser/src/` and, for
     every statement that styles/sizes `buttonStream` (`setIconSize`, `setMinimumWidth`,
     `setToolButtonStyle`, `setVisible`, …), add the same statement for `buttonServe` right after it.
     Expected: number of `buttonServe` statements = number of `buttonStream` statements.
  3. S3 — NEW test `T_SC_SERVE_BUTTON` in `tests/tst_shortcuts.cpp` (pattern of `T_SC_REMOTE_CONTEXT`):
     for each `Settings/buttonStyle` in {`textandicon`, `textonly`, `icononly`} set the setting, construct
     `RemoteWidget(&icons, "ia", "internetarchive")`, find `buttonServe` and `buttonStream`
     (`QToolButton`), assert `buttonServe->defaultAction() == rw->findChild<QAction*>("actionServe")`,
     same parent layout and `layout->indexOf(serve) == layout->indexOf(stream) + 1`,
     `serve->toolButtonStyle() == stream->toolButtonStyle()`, `serve->iconSize() == stream->iconSize()`,
     `serve->minimumWidth() == stream->minimumWidth()`, `!serve->isEnabled()` (no daemon in the test).
  Checkpoint: after S2.
  Failure handling: if the layout check fails because the row uses nested layouts, assert the same
  parent widget and that `serve->x() > stream->x()` instead, and record the change for review.
- **Unit tests:** T-SC-SERVE-BUTTON; gate G-U12: `scripts/dev.sh build qt64 && scripts/dev.sh test qt64 -- -R '^tst_shortcuts$' -V` — 1 test, 0 failed; the `-V` output (ctest shows the QtTest output only with `-V` or on failure) contains `PASS   : TestShortcuts::T_SC_SERVE_BUTTON()` (class name as declared in the file).
- **e2e tests:** N/A — wiring only; actionServe behaviour is covered by existing e2e tests.
- **Done:** button present in all styles; tests green; unit closed; commit.

### 1.3 Update README.md
- **Model:** agent-3:haiku
- **Assignment:** documentation; supervisor checks at P1.
- **Files:** `README.md`: the `#faq-serve` answer (first sentence lists where "Serve…" is found), the
  Preferences FAQ entry that mentions the remote icon scale (search "icon" in the "Preferences and
  options" section), the "Remote tab" shortcut table only if it lists buttons (it lists keys: no change).
- **Change:**
  Preconditions: 1.1, 1.2 DONE.
  S1 — `#faq-serve`: add the "Serve" button of the remote tab's button row to the places it can be
  started from. S2 — icon scale: if the text promises a behaviour about overlaps/sizes, make it accurate
  (any positive scale works; labels longer than the cell are shortened with "…"). Leave unchanged if no
  such text exists and record that. Preserve language and structure.
  Checkpoint: record sections touched.
- **Unit tests:** N/A — docs.
- **e2e tests:** N/A — docs.
- **Done:** sections accurate; unit closed; commit.

## Phase gates and closure
- Every gate command of this phase runs with `export RB_RCLONE_WEBUI=$PWD/build-rclone-webui/rclone` (STATE.md §3):
  a `dev.sh build` without it resets the CMake cache and drops the webui e2e test.
- Required gates: G-BUILD64, G-UNIT, G-E2E (with `RB_RCLONE_WEBUI`).
- README obligation 1.3 DONE.
- agent-1:opus reviews: D1 formula as implemented, I-1 evidence (legacy repro + 18 rows), D2 parity.
- Open P1, record review, mark DONE, commit closure.
