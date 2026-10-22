#include <QtTest>
#include <QtWidgets>

#include "global.h"
#include "icon_cache.h"
#include "item_model.h"
#include "rb_test_support.h"
#include "remote_widget.h"
#include "utils.h"

namespace {

QString warnPrefix() { return QString::fromUtf8("\xE2\x9A\xA0 "); }

Item *itemOf(const QModelIndex &i) {
  return static_cast<Item *>(i.internalPointer());
}

// The top-level roots whose path is `dir`.
QModelIndexList rootsOf(ItemModel *model, const QString &dir) {
  QModelIndexList found;
  const QString wanted = QDir(dir).path();
  for (int r = 0; r < model->rowCount(QModelIndex()); ++r) {
    const QModelIndex top = model->index(r, 0, QModelIndex());
    if (model->path(top).path() == wanted) {
      found << top;
    }
  }
  return found;
}

// The root's listing is done and it has a child with that name.
bool hasChildNamed(ItemModel *model, const QModelIndex &root,
                   const QString &name) {
  if (!root.isValid() || itemOf(root)->state != Item::Ready) {
    return false;
  }
  for (int r = 0; r < model->rowCount(root); ++r) {
    if (model->data(model->index(r, 0, root), Qt::DisplayRole).toString() ==
        name) {
      return true;
    }
  }
  return false;
}

// IconCache works on Item pointers from its own thread: no item at any depth
// may still wait for its icon before the widget is destroyed.
bool subtreeSettled(const Item *item) {
  for (const Item *c : item->childs) {
    if (c->state == Item::LoadingIcon || !subtreeSettled(c)) {
      return false;
    }
  }
  return true;
}

bool iconsSettled(ItemModel *model) {
  for (int r = 0; r < model->rowCount(QModelIndex()); ++r) {
    if (!subtreeSettled(itemOf(model->index(r, 0, QModelIndex())))) {
      return false;
    }
  }
  return true;
}

// A RemoteWidget of the remote "loc" as MainWindow builds it. The destructor
// runs while `icons` (declared before it in the test) is still alive: it waits
// until no listing runs, then deletes the widget.
struct WidgetGuard {
  explicit WidgetGuard(IconCache *icons) {
    w = new RemoteWidget(icons, "loc", "local", nullptr);
    w->show();
    path = w->findChild<QLineEdit *>("path");
    go = w->findChild<QToolButton *>("goPath");
    tree = w->findChild<QTreeView *>("tree");
    model = tree ? qobject_cast<ItemModel *>(tree->model()) : nullptr;
  }
  ~WidgetGuard() {
    // the tests wait with QVERIFY; this covers their early returns
    if (model) {
      const bool settled = QTest::qWaitFor(
          [this]() {
            return global.rcloneLsProcessCount == 0 && iconsSettled(model);
          },
          20000);
      if (!settled) {
        qWarning("path entry e2e: a listing or an icon did not settle");
      }
    }
    QCoreApplication::processEvents();
    delete w;
  }
  bool ok() const { return w && path && go && tree && model; }

  // no listing runs and no icon is pending: the widget may go. A timeout
  // fails the test.
  bool settle() {
    return QTest::qWaitFor(
        [this]() {
          return global.rcloneLsProcessCount == 0 && iconsSettled(model);
        },
        20000);
  }

  // type `text` into the field and click Go
  void goTo(const QString &text) {
    path->setFocus();
    path->setText(text);
    go->click();
  }

  RemoteWidget *w = nullptr;
  QLineEdit *path = nullptr;
  QToolButton *go = nullptr;
  QTreeView *tree = nullptr;
  ItemModel *model = nullptr;
};

} // namespace

// The path field of a remote tab (D16) against a real rclone: a typed path
// becomes a root and is listed through `rclone lsjson`. The remote is a
// temporary config with one local remote (I-9: never the host config); on
// Linux the widget treats it like any remote, so the typed paths are absolute
// local paths.
class TestE2ePathEntry : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;

  QString sub() const { return mDir.filePath("sub"); }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    mConf = mDir.filePath("rclone.conf");
    QFile conf(mConf);
    QVERIFY(conf.open(QIODevice::WriteOnly));
    conf.write("[loc]\ntype = local\n");
    conf.close();
    SetRcloneConf(mConf);

    QVERIFY(QDir().mkpath(sub()));
    QFile file(QDir(sub()).filePath("inner.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hi");
    file.close();
    QFile second(QDir(sub()).filePath("second.txt"));
    QVERIFY(second.open(QIODevice::WriteOnly));
    second.close();
  }

  void cleanupTestCase() {
    // initTestCase failed before the config: `pgrep -f ""` would match all
    if (mConf.isEmpty()) {
      return;
    }
    // no rclone of this test (its config path is on every command line) is
    // left
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", mConf});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
  }

  // T-E2E-PATH-GO: a typed folder is opened as a root, listed and selected.
  void T_E2E_PATH_GO() {
    IconCache icons;
    WidgetGuard g(&icons);
    QVERIFY(g.ok());
    // the focus rules need an active window
    g.w->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(g.w));

    g.goTo(sub());
    QVERIFY(g.tree->hasFocus());

    QTRY_VERIFY_WITH_TIMEOUT(
        hasChildNamed(g.model, rootsOf(g.model, sub()).value(0), "inner.txt"),
        20000);
    const QModelIndex root = rootsOf(g.model, sub()).value(0);
    QCOMPARE(g.tree->currentIndex(), root);
    QCOMPARE(g.path->text(), "loc:" + sub());

    // while the field has the focus the selection does not overwrite the
    // typed text; Esc gives back the path of the selection
    const QModelIndex def = g.model->index(0, 0, QModelIndex());
    QVERIFY(def != root);
    const QString defText = "loc:" + g.model->path(def).path();
    g.path->setFocus();
    QVERIFY(g.path->hasFocus());
    g.path->setText("typed");
    g.tree->selectionModel()->setCurrentIndex(
        def, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCOMPARE(g.path->text(), QString("typed"));
    QTest::keyClick(g.path, Qt::Key_Escape);
    QCOMPARE(g.path->text(), defText);
    QVERIFY(g.tree->hasFocus());

    QVERIFY(g.settle());
  }

  // T-E2E-PATH-DUP: the same path again selects the root it already has.
  void T_E2E_PATH_DUP() {
    IconCache icons;
    WidgetGuard g(&icons);
    QVERIFY(g.ok());

    g.goTo(sub());
    QTRY_VERIFY_WITH_TIMEOUT(
        hasChildNamed(g.model, rootsOf(g.model, sub()).value(0), "inner.txt"),
        20000);

    // the selection moves away, so the second Go has to bring it back
    g.tree->selectionModel()->setCurrentIndex(
        g.model->index(0, 0, QModelIndex()),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QVERIFY(g.tree->currentIndex() != rootsOf(g.model, sub()).value(0));

    // Enter in the field is a trigger like Go
    g.path->setFocus();
    g.path->setText(sub() + "/");
    QTest::keyClick(g.path, Qt::Key_Return);
    QCOMPARE(rootsOf(g.model, sub()).size(), 1);
    // the default root and the typed one
    QCOMPARE(g.model->rowCount(QModelIndex()), 2);
    QCOMPARE(g.tree->currentIndex(), rootsOf(g.model, sub()).value(0));
    QCOMPARE(g.path->text(), "loc:" + sub());

    QVERIFY(g.settle());
  }

  // T-E2E-PATH-MULTI (B-T6): Delete, Copy and Move never act on a
  // top-level row (Delete would purge the whole root). processSelection keeps
  // a multi-selection to the siblings of the current row; with no current row
  // it can hold several top-level rows, and then the actions are off.
  void T_E2E_PATH_MULTI() {
    IconCache icons;
    WidgetGuard g(&icons);
    QVERIFY(g.ok());
    auto *purge = g.w->findChild<QAction *>("purge");
    auto *copy = g.w->findChild<QAction *>("copy");
    auto *move = g.w->findChild<QAction *>("move");
    QVERIFY(purge && copy && move);

    g.goTo(sub());
    QTRY_VERIFY_WITH_TIMEOUT(
        hasChildNamed(g.model, rootsOf(g.model, sub()).value(0), "second.txt"),
        20000);
    const QModelIndex def = g.model->index(0, 0, QModelIndex());
    const QModelIndex root = rootsOf(g.model, sub()).value(0);
    QVERIFY(def != root);
    QCOMPARE(g.model->rowCount(root), 2);
    const QModelIndex a = g.model->index(0, 0, root);
    const QModelIndex b = g.model->index(1, 0, root);
    QVERIFY(!g.model->isSpecial(a) && !g.model->isSpecial(b));

    QItemSelectionModel *sel = g.tree->selectionModel();
    const auto rows = QItemSelectionModel::Rows;

    // two files inside the typed root, as a click and a Ctrl+click give
    // them: the positive control
    sel->setCurrentIndex(a, QItemSelectionModel::ClearAndSelect | rows);
    sel->setCurrentIndex(b, QItemSelectionModel::Select | rows);
    QCOMPARE(sel->selectedRows().size(), 2);
    QVERIFY(purge->isEnabled());
    QVERIFY(copy->isEnabled());
    QVERIFY(move->isEnabled());

    // Ctrl+click on the root: the files are not its siblings and leave the
    // selection, the root alone has the actions off
    sel->setCurrentIndex(root, QItemSelectionModel::Select | rows);
    QCOMPARE(sel->selectedRows(), QModelIndexList{root});
    QVERIFY(!purge->isEnabled());

    // no current row: both roots stay selected, the actions are off
    sel->clearSelection();
    sel->clearCurrentIndex();
    sel->select(def, QItemSelectionModel::Select | rows);
    sel->select(root, QItemSelectionModel::Select | rows);
    QCOMPARE(sel->selectedRows().size(), 2);
    QVERIFY(!purge->isEnabled());
    QVERIFY(!copy->isEnabled());
    QVERIFY(!move->isEnabled());

    QVERIFY(g.settle());
  }

  // T-E2E-PATH-MISSING: a folder that does not exist gives the error row.
  void T_E2E_PATH_MISSING() {
    IconCache icons;
    WidgetGuard g(&icons);
    QVERIFY(g.ok());

    const QString missing = mDir.filePath("nope");
    g.goTo(missing);

    QTRY_VERIFY_WITH_TIMEOUT(
        [&]() {
          const QModelIndex root = rootsOf(g.model, missing).value(0);
          return root.isValid() && itemOf(root)->state == Item::Ready &&
                 g.model->rowCount(root) == 1 &&
                 g.model->isSpecial(g.model->index(0, 0, root));
        }(),
        20000);
    const QModelIndex root = rootsOf(g.model, missing).value(0);
    const QString text =
        g.model->data(g.model->index(0, 0, root), Qt::DisplayRole).toString();
    QVERIFY2(text.startsWith(warnPrefix()), qPrintable(text));
    QVERIFY2(text.contains("directory not found"), qPrintable(text));

    // a read-only field (a Drive mode switch is waiting) ignores Go
    QVERIFY(g.settle());
    g.path->setReadOnly(true);
    g.path->setText(mDir.filePath("other"));
    g.go->click();
    QVERIFY(rootsOf(g.model, mDir.filePath("other")).isEmpty());
    g.path->setReadOnly(false);

    // the Drive mode switch removes the typed roots and keeps the default one
    const QString defPath = g.model->path(g.model->index(0, 0, QModelIndex()))
                                .path();
    QVERIFY(QMetaObject::invokeMethod(g.w, "switchRemoteType",
                                      Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(g.model->rowCount(QModelIndex()), 1, 20000);
    QCOMPARE(g.model->path(g.model->index(0, 0, QModelIndex())).path(),
             defPath);
    QVERIFY(rootsOf(g.model, missing).isEmpty());
    QVERIFY(g.settle());

    // a rootless remote shows its own hint, and its "/" root is not listed
    const int listsBefore = global.rcloneLsProcessCount;
    RemoteWidget *ia = new RemoteWidget(&icons, "ia", "internetarchive", nullptr);
    ia->show();
    QCoreApplication::processEvents();
    auto *iaPath = ia->findChild<QLineEdit *>("path");
    auto *iaTree = ia->findChild<QTreeView *>("tree");
    QVERIFY(iaPath && iaTree);
    auto *iaModel = qobject_cast<ItemModel *>(iaTree->model());
    QVERIFY(iaModel);
    QCOMPARE(iaPath->placeholderText(),
             QString("Enter an Internet Archive item identifier and press "
                     "Go"));
    QCOMPARE(iaModel->rowCount(QModelIndex()), 1);
    QCOMPARE(itemOf(iaModel->index(0, 0, QModelIndex()))->state,
             Item::Unknown);
    QCOMPARE(global.rcloneLsProcessCount, listsBefore);
    delete ia;
  }
};

QTEST_MAIN(TestE2ePathEntry)
#include "tst_e2e_path_entry.moc"
