#include <QtTest>
#include <QtWidgets>

#include "export_dialog.h"
#include "export_runner.h"
#include "global.h"
#include "icon_cache.h"
#include "item_model.h"
#include "lsjson_parser.h"
#include "progress_dialog.h"
#include "rb_test_support.h"
#include "rclone_format.h"
#include "rclone_process.h"
#include "utils.h"

namespace {

// Non-ASCII names as UTF-8 escapes (the source encoding must not matter).
QString nameUmlaut() {
  return QString::fromUtf8("\xC3\x9C" "n" "\xC3\xAF" ".txt");
}
QString nameTilde() { return QString::fromUtf8("\xC3\xB1" "-file.dat"); }
QString warnPrefix() { return QString::fromUtf8("\xE2\x9A\xA0 "); }

bool writeFile(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  return f.write(data) == data.size();
}

Item *itemOf(const QModelIndex &i) {
  return static_cast<Item *>(i.internalPointer());
}

// The folder of `root` finished loading (Loading2 -> Ready).
bool isReady(const QModelIndex &root) {
  return itemOf(root)->state == Item::Ready;
}

// IconCache works on Item pointers from its own thread: no child may still
// wait for its icon before the tree is changed or the model is destroyed.
bool iconsSettled(const QModelIndex &root) {
  for (const Item *c : itemOf(root)->childs) {
    if (c->state == Item::LoadingIcon) {
      return false;
    }
  }
  return true;
}

QStringList childNames(ItemModel &model, const QModelIndex &root) {
  QStringList names;
  for (int r = 0; r < model.rowCount(root); ++r) {
    names << model.data(model.index(r, 0, root), Qt::DisplayRole).toString();
  }
  names.sort();
  return names;
}

QStringList readLines(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    return QStringList();
  }
  QStringList lines =
      QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
  return lines;
}

} // namespace

class TestE2eListing : public QObject {
  Q_OBJECT

  QString mConf;

  // The calls RemoteWidget makes for a non-Windows remote. rowCount() on a
  // folder in state Unknown is the public call that starts ItemModel::load()
  // (ItemModel has no canFetchMore()/fetchMore()). The root index is
  // returned by value: QTRY_* may return from this void helper.
  void loadModel(ItemModel &model, const QString &dir, QModelIndex &root) {
    root = model.addRoot("/", dir);
    model.rowCount(root);
    // Note: ItemModel::isLoading(root) looks at the *parent* of the index
    // (it is meant for children), so it is always false for a root; the
    // folder item's own state is what tells that the load is over.
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    mConf = QDir(rbTestDir().path()).filePath("t.conf");
    QVERIFY(writeFile(mConf, "[loc]\ntype = local\n"));
    SetRclone(rbRclone());
    SetRcloneConf(mConf);
  }

  // T-E2E-LIST-NAMES (#107, #130, #160)
  void T_E2E_LIST_NAMES() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(QDir().mkpath(d.filePath("12345 ")));
    QVERIFY(QDir().mkpath(d.filePath("dir [x] (y)")));
    QVERIFY(writeFile(d.filePath("a:b.txt"), "a"));
    QVERIFY(writeFile(d.filePath("x \"q\".txt"), "q"));
    QVERIFY(writeFile(d.filePath(nameUmlaut()), "u"));
    QVERIFY(writeFile(d.filePath("t.txt"), "hello"));

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }

    QStringList expected = {"12345 ", "dir [x] (y)", "a:b.txt",
                            "x \"q\".txt", nameUmlaut(), "t.txt"};
    expected.sort();
    QCOMPARE(childNames(model, root), expected);
    QVERIFY(!model.isLoading(model.index(0, 0, root)));

    int tRow = -1;
    for (int r = 0; r < model.rowCount(root); ++r) {
      const QModelIndex i = model.index(r, 0, root);
      const QString name = model.data(i, Qt::DisplayRole).toString();
      if (name == "12345 ") {
        QVERIFY(model.isFolder(i));
        QCOMPARE(name.size(), 6); // the trailing space is kept
      }
      if (name == "dir [x] (y)") {
        QVERIFY(model.isFolder(i));
      }
      if (name == "a:b.txt") {
        QVERIFY(!model.isFolder(i));
      }
      if (name == "t.txt") {
        tRow = r;
      }
    }
    QVERIFY(tRow >= 0);
    const QModelIndex t0 = model.index(tRow, 0, root);
    QVERIFY(!model.isFolder(t0));
    QCOMPARE(itemOf(t0)->size, qint64(5));
    QCOMPARE(model.data(model.index(tRow, 1, root), Qt::DisplayRole).toString(),
             formatNiceSize(5));
    const QString modified =
        model.data(model.index(tRow, 2, root), Qt::DisplayRole).toString();
    QVERIFY2(QRegularExpression(QRegularExpression::anchoredPattern(
                                    R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)"))
                 .match(modified)
                 .hasMatch(),
             qPrintable(modified));
  }

  // T-E2E-LIST-UNKNOWNSIZE (#140): a local remote never reports Size -1, so
  // the recorded entry is converted by the same function load() uses.
  void T_E2E_LIST_UNKNOWNSIZE() {
    QFile f(rbFixture("synthetic/lsjson_variants.jsonl"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray lineA = f.readLine();
    const auto entry = parseLsjsonLine(lineA);
    QVERIFY(entry.has_value());
    QCOMPARE(entry->size, qint64(-1));
    QVERIFY(!entry->isDir);

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    const QModelIndex root = model.addRoot("/", QDir::tempPath());
    Item *rootItem = itemOf(root);
    rootItem->state = Item::Ready; // no load: the child is added by hand
    Item *item = ItemModel::itemFromEntry(*entry, rootItem);
    rootItem->childs.append(item);

    QCOMPARE(item->size, qint64(-1));
    QCOMPARE(item->name, QString("Doc"));
    const QModelIndex i1 = model.index(0, 1, root);
    QVERIFY(i1.isValid());
    QCOMPARE(model.data(i1, Qt::DisplayRole).toString(), QString());
    QVERIFY(!model.data(model.index(0, 2, root), Qt::DisplayRole)
                 .toString()
                 .isEmpty());
  }

  // T-E2E-LIST-ERROR: a failed listing is one visible warning row.
  void T_E2E_LIST_ERROR() {
    const int before = global.rcloneLsProcessCount;

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    const QString missing =
        "/nonexistent-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QModelIndex root;
    loadModel(model, missing, root);
    if (QTest::currentTestFailed()) {
      return;
    }

    QCOMPARE(model.rowCount(root), 1);
    Item *row = itemOf(model.index(0, 0, root));
    QCOMPARE(row->state, Item::Special);
    QVERIFY(!row->isFolder);
    QVERIFY2(row->name.startsWith(warnPrefix()), qPrintable(row->name));
    QVERIFY2(row->name.contains("directory not found"), qPrintable(row->name));
    // the warning row has no size and no icon
    QCOMPARE(model.data(model.index(0, 1, root), Qt::DisplayRole).toString(),
             QString());
    QVERIFY(!model.isLoading(model.index(0, 0, root)));
    QVERIFY(model.isSpecial(model.index(0, 0, root)));
    QCOMPARE(global.rcloneLsProcessCount, before);
  }

  // T-E2E-LIST-REFRESH: a reload merges without duplicates.
  void T_E2E_LIST_REFRESH() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(QDir().mkpath(d.filePath("sub")));
    QVERIFY(writeFile(d.filePath("keep.txt"), "k"));
    QVERIFY(writeFile(d.filePath("t.txt"), "hello"));

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QCOMPARE(childNames(model, root),
             (QStringList{"keep.txt", "sub", "t.txt"}));

    QVERIFY(writeFile(d.filePath("new.txt"), "n"));
    QVERIFY(QFile::remove(d.filePath("t.txt")));
    model.refresh(root);
    QVERIFY(itemOf(root)->isLoading());
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);

    const QStringList names = childNames(model, root);
    QCOMPARE(names, (QStringList{"keep.txt", "new.txt", "sub"}));
    QStringList unique = names;
    QCOMPARE(unique.removeDuplicates(), 0);
    QCOMPARE(global.rcloneLsProcessCount, 0);
  }

  // T-E2E-LIST-REFRESH-BUSY: a refresh drops the rows that are gone, but a
  // folder still being listed or a file still waiting for its icon has its
  // pointer held elsewhere (the listing, IconCache): it is only marked, the
  // end of that work frees it. The same when its parent row is freed.
  void T_E2E_LIST_REFRESH_BUSY() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(QDir().mkpath(d.filePath("busy")));
    QVERIFY(writeFile(d.filePath("icon.txt"), "i"));
    QVERIFY(writeFile(d.filePath("keep.txt"), "k"));

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QCOMPARE(childNames(model, root),
             (QStringList{"busy", "icon.txt", "keep.txt"}));

    // the states a running listing and a pending icon lookup leave
    Item *busy = nullptr;
    Item *icon = nullptr;
    for (Item *c : itemOf(root)->childs) {
      if (c->name == "busy") {
        busy = c;
      } else if (c->name == "icon.txt") {
        icon = c;
      }
    }
    QVERIFY(busy && icon);
    busy->state = Item::Loading2;
    icon->state = Item::LoadingIcon;

    QVERIFY(QDir(d.filePath("busy")).removeRecursively());
    QVERIFY(QFile::remove(d.filePath("icon.txt")));
    model.refresh(root);
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);
    QCOMPARE(childNames(model, root), QStringList{"keep.txt"});
    // out of the tree but not freed
    QVERIFY(busy->isDeleted);
    QVERIFY(icon->isDeleted);
    busy->state = Item::Ready;
    delete busy;
    icon->state = Item::Ready;
    delete icon;

    Item *parent = new Item();
    Item *waiting = new Item();
    waiting->parent = parent;
    waiting->state = Item::LoadingIcon;
    parent->childs.append(waiting);
    delete parent;
    QVERIFY(waiting->isDeleted);
    waiting->state = Item::Ready;
    delete waiting;
  }

  // T-E2E-LIST-RENAME: a renamed folder takes the items already listed under
  // it along (their paths follow the new name, else an action on one of them
  // would reach the old path); the model root (an invalid index) is never
  // renamed; a drive label changes only the shown name of a root.
  void T_E2E_LIST_RENAME() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(writeFile(d.filePath("old/in.txt"), "i"));

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QCOMPARE(childNames(model, root), QStringList{"old"});
    const QModelIndex folder = model.index(0, 0, root);
    model.rowCount(folder);
    QTRY_VERIFY_WITH_TIMEOUT(isReady(folder), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(folder), 20000);
    QCOMPARE(childNames(model, folder), QStringList{"in.txt"});

    // the rename on the remote, then the model's
    QVERIFY(QDir(d).rename("old", "new"));
    model.rename(folder, "new");
    QCOMPARE(model.path(folder).path(), d.filePath("new"));
    QCOMPARE(model.path(model.index(0, 0, folder)).path(),
             d.filePath("new/in.txt"));

    // the model root: no crash, nothing changes
    model.rename(QModelIndex(), "x");
    QCOMPARE(model.path(root).path(), d.path());

    model.setRootLabel(root, "/ (label)");
    QCOMPARE(model.data(root, Qt::DisplayRole).toString(), QString("/ (label)"));
    QCOMPARE(model.path(root).path(), d.path());
    // not a root: nothing changes
    model.setRootLabel(folder, "y");
    QCOMPARE(model.data(folder, Qt::DisplayRole).toString(), QString("new"));
    model.setRootLabel(QModelIndex(), "z");
    QCOMPARE(model.path(root).path(), d.path());
  }

  // T-E2E-EXPORT: lsjson -R streamed into a CSV (and a names-only) file.
  void T_E2E_EXPORT() {
    QTemporaryDir src, out;
    QVERIFY(src.isValid() && out.isValid());
    const QDir d(src.path());
    const QStringList files = {
        "a.txt", "b.csv", "dir [x] (y)/inner.txt", "12345 /trail.txt",
        nameUmlaut(), "deep/er/" + nameTilde(), "x \"q\".txt"};
    for (const QString &f : files) {
      QVERIFY(writeFile(d.filePath(f), QByteArray(7, 'x')));
    }

    ExportDialog dlg("loc", QDir(src.path()), nullptr);
    const QStringList options = dlg.getOptions();
    QVERIFY(options.mid(0, 4) ==
            (QStringList{"lsjson", "-R", "--files-only", "--no-mimetype"}));

    // the extra options split as in the other dialogs: quotes keep a value
    // with spaces in one argument, a double space gives no empty argument
    // (rclone would take it for a second path)
    auto *extra = dlg.findChild<QLineEdit *>("textExtra");
    QVERIFY(extra);
    extra->setText("--exclude \"dir [x] (y)/**\"  --max-depth 9");
    QCOMPARE(dlg.getOptions().mid(4),
             (QStringList{"--exclude", "dir [x] (y)/**", "--max-depth", "9",
                          "loc:" + QDir(src.path()).path()}));
    extra->clear();

    // CSV: "path","yyyy-MM-dd HH:mm:ss",size; a quote in a path is doubled
    const QString csv = QDir(out.path()).filePath("list.csv");
    ProgressDialog *p1 =
        startExport("loc", src.path(), options, csv, false, nullptr);
    QVERIFY(p1);
    QProcess *proc1 = p1->findChild<QProcess *>();
    QVERIFY(proc1);
    QTRY_VERIFY_WITH_TIMEOUT(proc1->state() == QProcess::NotRunning, 30000);
    QCOMPARE(proc1->exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc1->exitCode(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(readLines(csv).size(), files.size(), 10000);
    const QRegularExpression csvRe(QRegularExpression::anchoredPattern(
        QStringLiteral("\"((?:[^\"]|\"\")+)\",\"\\d{4}-\\d\\d-\\d\\d "
                       "\\d\\d:\\d\\d:\\d\\d\",7")));
    QStringList paths;
    for (const QString &line : readLines(csv)) {
      const auto m = csvRe.match(line);
      QVERIFY2(m.hasMatch(), qPrintable(line));
      paths << m.captured(1).replace("\"\"", "\"");
    }
    paths.sort();
    QStringList expected = files;
    expected.sort();
    QCOMPARE(paths, expected);
    QVERIFY(paths.contains("dir [x] (y)/inner.txt"));
    delete p1;

    // names only: "path"
    const QString txt = QDir(out.path()).filePath("list.txt");
    ProgressDialog *p2 =
        startExport("loc", src.path(), options, txt, true, nullptr);
    QVERIFY(p2);
    QProcess *proc2 = p2->findChild<QProcess *>();
    QVERIFY(proc2);
    QTRY_VERIFY_WITH_TIMEOUT(proc2->state() == QProcess::NotRunning, 30000);
    QTRY_COMPARE_WITH_TIMEOUT(readLines(txt).size(), files.size(), 10000);
    QStringList quoted;
    for (const QString &f : files) {
      quoted << "\"" + QString(f).replace("\"", "\"\"") + "\"";
    }
    quoted.sort();
    QStringList got = readLines(txt);
    got.sort();
    QCOMPARE(got, quoted);
    delete p2;
  }

  // T-E2E-REMOTES: the call rcloneListRemotes makes, with a temporary config.
  void T_E2E_REMOTES() {
    const QString conf = QDir(rbTestDir().path()).filePath("remotes.conf");
    QVERIFY(writeFile(conf, "[mem]\ntype = memory\n\n"
                            "[loc]\ntype = local\ndescription = d\n"));

    QProcess p;
    ApplyRcloneEnvironment(&p);
    p.start(rbRclone(), QStringList()
                            << "listremotes"
                            << "--long"
                            << "--json"
                            << "--ask-password=false"
                            << "--config" << conf,
            QIODevice::ReadOnly);
    QVERIFY(p.waitForFinished(30000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    const QByteArray err = p.readAllStandardError();
    const QByteArray outBytes = p.readAllStandardOutput();
    QVERIFY2(p.exitCode() == 0, err.constData());

    const auto remotes = parseListRemotesJson(outBytes);
    QVERIFY2(remotes.has_value(), outBytes.constData());
    QCOMPARE(remotes->size(), 2);
    bool seenLoc = false;
    bool seenMem = false;
    for (const RemoteInfo &r : *remotes) {
      if (r.name == "loc") {
        seenLoc = true;
        QCOMPARE(r.type, QString("local"));
        QCOMPARE(r.description, QString("d"));
      }
      if (r.name == "mem") {
        seenMem = true;
        QCOMPARE(r.type, QString("memory"));
      }
    }
    QVERIFY(seenLoc && seenMem);
  }
};

QTEST_MAIN(TestE2eListing)
#include "tst_e2e_listing.moc"
