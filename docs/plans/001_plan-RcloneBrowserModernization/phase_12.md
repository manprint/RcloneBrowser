# Phase 11 — Dual pane (#71)

**SKIPPED by revision 25.** Phase 11 (dual pane, drag between remote views: #28, #71) is not done in plan 001. User, 2026-10-22: "per me puoi non tradurre mi sta bene l'inglese!" and, on the dual pane, "a me va bene l'interafccia come è adesso, possiamo anche skippare questa fase." Nothing below is implemented; the text stays as the design record. The issues are `deferred` in the triage.

Intent:
- A "Dual pane" tab shows two remote views side by side.
- An item can be dragged from one pane to a folder in the other, or sent with "Copy to other pane". Either way, the existing TransferDialog opens prefilled, and the transfer runs through `addTransfer`.
Prerequisites: P10, which provides the shortcut contexts that make two visible RemoteWidgets safe.
Phase closure: P11; review by agent-1:opus.

## State and ownership contract
Read STATE.md §0–1 first for scope, ownership, recovery, checks and commit rules. Open a unit before editing it. Checkpoint at recovery boundaries. Close with evidence. A delegated worker follows its assigned OPEN unit. Missing design decisions go to agent-1:opus. No network access (U6).

## Local design context
Plan revision: 1.
- D16: the dual pane is a tab holding two RemoteWidgets in a QSplitter, with drag-drop and "Copy to other pane".
- Scope decision (supervisor, revision 1): one item per drag or copy. The TransferDialog takes a single source; multi-selection shows `"Drag or copy one item at a time"` in the status bar. A multi-item transfer is out of scope and recorded as a follow-up idea in the triage doc.
- Current code (anchors):
  - `src/item_model.cpp`: `flags()` returns `ItemIsDropEnabled` only; `canDropMimeData` accepts a single local file URL; `dropMimeData` emits `drop(const QDir &path, const QModelIndex &parent)`.
  - `src/remote_widget.cpp`: the drop handler (connected to `ItemModel::drop`) opens a `TransferDialog` in upload mode with the dropped local path as source and emits `addTransfer` (the `emit addTransfer(QString("%1 from %2").arg(t.getMode()).arg(src), src, …)` site).
  - `MainWindow` `ui.open` handler: creates `new RemoteWidget(&mIcons, name, remoteType, ui.tabs)`, connects 4 signals, and adds a tab.
- MIME contract: type `application/x-rclonebrowser-item`, payload UTF-8 JSON `{"v":1,"remote":"<name>","type":"<remoteType>","path":"<full rclone path>","isDir":bool}`. The full rclone path is `<remote>:<item path>`, or the plain local path for the local remote on Windows drive roots (the implementer reuses the path composition the existing download handler uses for its source).

## Sub-phases

### 11.1 Drag between remote views
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff, focusing on the ItemModel drag/drop flags, not breaking the existing local-file drop, and the Special-row exclusion.
- **Files:**
  - WRITE `src/item_model.h/.cpp`: `flags`, `mimeTypes`, `mimeData`, `canDropMimeData`, `dropMimeData`, and the new signal `dropItem(const QString &sourcePath, bool isDir, const QModelIndex &target)`.
  - WRITE `src/remote_widget.h/.cpp`: `tree->setDragEnabled(true)`, `setDragDropMode(QAbstractItemView::DragDrop)`, and a handler that opens TransferDialog with the remote source.
  - NEW `src/item_mime.h/.cpp`, registered.
  - NEW `tests/tst_item_mime.cpp` (unit) and `tests/tst_e2e_drag.cpp` (e2e).
- **Change:**
  - Preconditions: P10 DONE.
  - Contract:
    ```cpp
    struct DraggedItem { QString remote, type, path; bool isDir = false; };
    QByteArray encodeDraggedItem(const DraggedItem &d);
    std::optional<DraggedItem> decodeDraggedItem(const QByteArray &bytes);   // nullopt unless v==1 and all keys typed correctly
    bool canDropDragged(const DraggedItem &d, const QString &targetFullPath); // false if target == d.path or target starts with d.path + "/" (dropping a folder into itself)
    ```
    - `ItemModel::flags`: non-Special, non-loading items additionally get `ItemIsDragEnabled`. Folders and roots keep `ItemIsDropEnabled`.
    - `mimeTypes()` returns the custom type plus the types the existing drop already accepts.
    - `mimeData(indexes)`: exactly one valid non-Special index gives the encoded custom MIME; anything else returns nullptr.
    - `canDropMimeData`: the custom type with `canDropDragged` and a target folder accepts. The existing local-URL rule is unchanged.
    - `dropMimeData`: the custom type emits `dropItem(path, isDir, parent)`; the existing branch is unchanged.
    - RemoteWidget `dropItem` handler:
      - open the same TransferDialog construction the local drop uses (upload mode, `isDownload = false`) with `setSource(sourcePath)` and the destination folder of `target`;
      - on accept, emit `addTransfer` with the same arguments pattern as the local-drop site.
      - A remote source in upload mode works because `TransferDialog::getSource` returns the text as is in upload mode. The implementer confirms and records this.
  - Steps:
    1. S1 — `item_mime` plus unit tests.
    2. S2 — ItemModel and RemoteWidget changes.
    3. S3 — e2e; full gates. Send the diff to agent-1:opus.
- **Unit tests:** `tests/tst_item_mime.cpp`:
  - T-MIME-ROUNDTRIP: encode then decode is identity for the path `r:dir [x]/ñ "q".txt`.
  - T-MIME-BAD: `{}`, `{"v":2,…}`, invalid JSON and a non-string path give nullopt.
  - T-MIME-CANDROP: `("r:a", "r:a")` is false; `("r:a", "r:a/b")` is false; `("r:a", "r:ab")` is true; `("r:a", "s:a")` is true.
  - Gate G-U11.1: `scripts/dev.sh test qt64 -- -R tst_item_mime` passes with 3 functions.
- **e2e tests:** `tests/tst_e2e_drag.cpp`. Uses ItemModel directly, with two models on `loc` over temp dirs A and B, loaded via the 2.2 helper.
  - T-E2E-DRAG-MIME: `modelA.mimeData({index of a.txt})` decodes to path `loc:<A>/a.txt`, isDir false. `modelA.mimeData({special row})` is nullptr.
  - T-E2E-DRAG-DROP: `modelB.canDropMimeData(mime, CopyAction, -1, 0, rootB)` is true. `dropMimeData` emits `dropItem` with `("loc:<A>/a.txt", false, rootB)`.
  - T-E2E-DRAG-LOCALURL: the existing local-URL drop on modelB still emits `drop` (regression).
  - Gate G-U11.1e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_drag` passes with 3 functions.
- **Done:** review recorded; gates pass; unit closed; completion commit.

### 11.2 DualPaneWidget tab and "Copy to other pane"
- **Model:** agent-2:sonnet
- **Assignment:** implementer. agent-1:opus reviews after the diff (tab lifecycle and signal wiring).
- **Files:**
  - NEW `src/dual_pane_widget.h/.cpp` (`Q_OBJECT` → `MOC`), registered.
  - WRITE `src/remote_widget.h/.cpp`: the public `QString selectedItemFullPath(bool *isDir) const`, `QString currentFolderFullPath() const`, `void openTransferFrom(const QString &sourcePath, bool isDir, const QString &destFolder)` (factored out of the 11.1 handler), and `actionCopyToOtherPane` (Ctrl+Shift+C, WidgetWithChildrenShortcut, hidden unless in a dual pane).
  - WRITE `src/main_window.h/.cpp/.ui`: an "Open in dual pane" button `openDual` on the Remotes tab, plus the tab creation and wiring.
  - NEW `tests/tst_e2e_dual_pane.cpp` (e2e, `QTEST_MAIN`).
- **Change:**
  - Preconditions: 11.1 DONE.
  - Contract:
    - `DualPaneWidget(IconCache *icons, const QVector<QPair<QString, QString>> &remotes /*name,type*/, const QString &left, const QString &right, QWidget *parent)`:
      - `QSplitter splitter` (horizontal) holding two panes;
      - each pane is a `QVBoxLayout` with a `QComboBox remotePicker` (all remotes), then a RemoteWidget;
      - changing a picker replaces that pane's RemoteWidget (`deleteLater` on the old one) and re-wires its signals;
      - signal forwarding: it re-emits the 4 RemoteWidget signals (`addTransfer`, `addStream`, `addNewMount`, `addSavedTransfer`) for MainWindow to connect once;
      - `RemoteWidget *pane(int i)` (0/1) for tests;
      - `copyToOtherPane(int from)` takes `pane(from)->selectedItemFullPath(&isDir)`; if non-empty, it calls `pane(1-from)->openTransferFrom(path, isDir, pane(1-from)->currentFolderFullPath())`.
      - Each pane's `actionCopyToOtherPane` is shown and connected to `copyToOtherPane(i)`.
      - For tests, `openTransferFrom` takes an optional hook `std::function<void(TransferDialog&)> beforeExec`. When set (tests only), it is called instead of `exec()`, and the dialog is treated as accepted.
    - MainWindow `openDual` clicked: left = the selected remote (or the first one), right = `local` if a remote of type local exists, else the same as left. Add the tab "Dual pane" with the 4 forwarded signals connected, then make it current. Several dual-pane tabs are allowed.
  - Steps:
    1. S1 — RemoteWidget accessors and `openTransferFrom` (refactor of the 11.1 handler with no behavior change).
    2. S2 — DualPaneWidget and MainWindow wiring; smoke; re-record T-MW-SNAPSHOT under supervisor approval (one new button; record the diff).
    3. S3 — e2e; full gates.
- **Unit tests:** N/A — the logic is wiring over units already tested (`item_mime`, TransferDialog); the e2e below exercises it with real panes.
- **e2e tests:** `tests/tst_e2e_dual_pane.cpp`.
  - Setup: conf `[a] type = local` and `[b] type = local`; temp dirs A (with `f 1.txt`) and B; DualPaneWidget with left `a`, right `b`; each pane's root opened on its dir through `path` + Go (10.1).
  - T-E2E-DUAL-PANES: `pane(0)` and `pane(1)` exist, with remote names `a` and `b`. Their trees are visible in the splitter.
  - T-E2E-DUAL-COPY:
    - select `f 1.txt` in pane 0, set the hook to capture the dialog's `getOptions()`, then call `copyToOtherPane(0)`;
    - the captured JobOptions has source `a:<A>/f 1.txt` and dest `b:<B>`;
    - a QSignalSpy on the forwarded `addTransfer` gets one emission;
    - running its args through `PrepareTransferArgs` and real rclone (the same helper as the 1.4 e2e) gives `<B>/f 1.txt`.
  - T-E2E-DUAL-PICKER: changing pane 1's picker to `a` replaces the RemoteWidget (a different pointer), and the old one is destroyed (QPointer null after event processing).
  - Gate G-U11.2e: `scripts/dev.sh e2e qt64 -- -R tst_e2e_dual_pane` passes with 3 functions.
- **Done:** review recorded; gates pass; snapshot change recorded; unit closed; completion commit.

## Phase gates and closure
- Required gates: G-BUILD64, G-UNIT, G-E2E, G-SMOKE. G-BUILD610 if the image is present.
- README obligation (P11): a "Dual pane" section covering opening, drag-and-drop, "Copy to other pane" (Ctrl+Shift+C) and the one-item-at-a-time note.
- Supervisor agent-1:opus reviews. Close P11.
