#include <QtTest>
#include <QtWidgets>

#include <memory>

#include "global.h"
#include "icon_cache.h"
#include "item_model.h"
#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "rclone_process.h"
#include "utils.h"

namespace {

// Non-ASCII names as UTF-8 escapes (the source encoding must not matter).
QString nameUmlaut() {
  return QString::fromUtf8("\xC3\x9C" "n" "\xC3\xAF" ".txt");
}
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

// "name|folder|size|modified" of every child, sorted.
QStringList childRecords(const QModelIndex &root) {
  QStringList records;
  for (const Item *c : itemOf(root)->childs) {
    records << QString("%1|%2|%3|%4")
                   .arg(c->name)
                   .arg(c->isFolder)
                   .arg(c->size)
                   .arg(c->modified);
  }
  records.sort();
  return records;
}

QProcessEnvironment baseEnv() {
  QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
  e.remove("RCLONE_CONFIG_PASS");
  e.remove("RCLONE_PASSWORD_COMMAND");
  return e;
}

} // namespace

// Folder listing through the rc daemon of phase 5 and its fallbacks.
class TestE2eRcListing : public QObject {
  Q_OBJECT

  QString mConf;
  QString mEncryptedConf;
  RcloneDaemon *mDaemon = nullptr;

  // Creates mDaemon, publishes it as global.daemon and waits until it serves.
  void startDaemon() {
    mDaemon = new RcloneDaemon;
    global.daemon = mDaemon;
    mDaemon->start();
    QTRY_VERIFY2_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                              qPrintable(mDaemon->lastError()), 10000);
  }

  // The calls RemoteWidget makes for a non-Windows remote. rowCount() on a
  // folder in state Unknown is the public call that starts ItemModel::load().
  // The root index is returned by value: QTRY_* may return from this void
  // helper.
  void loadModel(ItemModel &model, const QString &dir, QModelIndex &root) {
    root = model.addRoot("/", dir);
    model.rowCount(root);
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);
  }

  // One rc call through the daemon, waited for.
  void rcCall(const QString &path, const QJsonObject &params,
              RcReply &reply) {
    auto done = std::make_shared<bool>(false);
    auto result = std::make_shared<RcReply>();
    mDaemon->client()->call(path, params, this,
                            [done, result](const RcReply &r) {
                              *result = r;
                              *done = true;
                            },
                            20000);
    QTRY_VERIFY_WITH_TIMEOUT(*done, 25000);
    reply = *result;
  }

  // the tree of T-E2E-LIST-NAMES
  bool makeNamesTree(const QDir &d) {
    return QDir().mkpath(d.filePath("12345 ")) &&
           QDir().mkpath(d.filePath("dir [x] (y)")) &&
           writeFile(d.filePath("a:b.txt"), "a") &&
           writeFile(d.filePath("x \"q\".txt"), "q") &&
           writeFile(d.filePath(nameUmlaut()), "u") &&
           writeFile(d.filePath("t.txt"), "hello");
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<RcloneDaemon::State>();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    // I-9: temporary configs only, never the host one.
    mConf = QDir(rbTestDir().path()).filePath("t.conf");
    QVERIFY(writeFile(mConf, "[loc]\ntype = local\n"));
    SetRclone(rbRclone());
    SetRcloneConf(mConf);
    SetRclonePassword("");

    // the same config, encrypted with the password "pw"
    mEncryptedConf = QDir(rbTestDir().path()).filePath("enc.conf");
    QVERIFY(writeFile(mEncryptedConf, "[loc]\ntype = local\n"));
    QProcessEnvironment env = baseEnv();
    env.insert("RCLONE_PASSWORD_COMMAND", "echo pw");
    QProcess p;
    p.setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed, env));
    p.start(rbRclone(),
            {"config", "encryption", "set", "--config", mEncryptedConf});
    p.closeWriteChannel();
    QVERIFY(p.waitForFinished(20000));
    QVERIFY2(p.exitCode() == 0, p.readAllStandardError().constData());
    QFile c(mEncryptedConf);
    QVERIFY(c.open(QIODevice::ReadOnly));
    QVERIFY(c.readAll().startsWith("# Encrypted rclone configuration File"));
  }

  void cleanup() {
    auto settings = GetSettings();
    settings->remove("Settings/showHidden");
    settings->remove("Settings/defaultRcloneOptions");
    settings->remove("Settings/remoteMode");
    settings.reset();

    global.daemon = nullptr;
    if (mDaemon) {
      delete mDaemon; // terminate(), then kill()
      mDaemon = nullptr;
    }
    SetRclone(rbRclone());
    SetRcloneConf(mConf);
    SetRclonePassword("");
  }

  void cleanupTestCase() {
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "rcd --rc-addr 127.0.0.1:0"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
  }

  // T-E2E-RCLIST-NAMES: the tree and assertions of T-E2E-LIST-NAMES, listed
  // through the daemon.
  void T_E2E_RCLIST_NAMES() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(makeNamesTree(d));

    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(model.lastLoadUsedRc());

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
    const QString modified =
        model.data(model.index(tRow, 2, root), Qt::DisplayRole).toString();
    QVERIFY2(QRegularExpression(QRegularExpression::anchoredPattern(
                                    R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)"))
                 .match(modified)
                 .hasMatch(),
             qPrintable(modified));
    QCOMPARE(global.rcloneLsProcessCount, 0);
  }

  // T-E2E-RCLIST-PARITY: rc and lsjson give the same children (R17: also
  // with remoteMode "shared").
  void T_E2E_RCLIST_PARITY() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(makeNamesTree(d));
    QVERIFY(writeFile(d.filePath("big.bin"), QByteArray(4096, 'b')));
    QVERIFY(writeFile(d.filePath(".h.txt"), "h"));
    QVERIFY(writeFile(d.filePath("12345 /inner.txt"), "i"));

    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }

    QStringList viaRc;
    QStringList viaRcShared;
    {
      IconCache icons;
      ItemModel model(&icons, "loc", nullptr);
      QModelIndex root;
      loadModel(model, dir.path(), root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(model.lastLoadUsedRc());
      viaRc = childRecords(root);
    }
    QVERIFY(viaRc.size() >= 8);

    GetSettings()->setValue("Settings/remoteMode", "shared");
    {
      IconCache icons;
      ItemModel model(&icons, "loc", nullptr);
      QModelIndex root;
      loadModel(model, dir.path(), root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(model.lastLoadUsedRc());
      viaRcShared = childRecords(root);
    }
    GetSettings()->remove("Settings/remoteMode");
    QCOMPARE(viaRcShared, viaRc);

    mDaemon->stop();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Stopped,
                             5000);

    QStringList viaProcess;
    {
      IconCache icons;
      ItemModel model(&icons, "loc", nullptr);
      QModelIndex root;
      loadModel(model, dir.path(), root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(!model.lastLoadUsedRc());
      viaProcess = childRecords(root);
    }
    QCOMPARE(viaRc, viaProcess);
  }

  // T-E2E-RCLIST-ERROR: a missing dir is one warning row.
  void T_E2E_RCLIST_ERROR() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const int before = global.rcloneLsProcessCount;

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QSignalSpy lockedSpy(&model, &ItemModel::configLocked);
    const QString missing =
        "/nonexistent-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QModelIndex root;
    loadModel(model, missing, root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(model.lastLoadUsedRc());

    QCOMPARE(model.rowCount(root), 1);
    Item *row = itemOf(model.index(0, 0, root));
    QCOMPARE(row->state, Item::Special);
    QVERIFY(!row->isFolder);
    QVERIFY2(row->name.startsWith(warnPrefix()), qPrintable(row->name));
    QVERIFY2(row->name.contains("directory not found"), qPrintable(row->name));
    QVERIFY(model.isSpecial(model.index(0, 0, root)));
    QCOMPARE(lockedSpy.count(), 0);
    QCOMPARE(global.rcloneLsProcessCount, before);
  }

  // T-E2E-RCLIST-HIDDEN: showHidden=false hides the dot entries on rc too.
  void T_E2E_RCLIST_HIDDEN() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(writeFile(d.filePath(".h.txt"), "h"));
    QVERIFY(QDir().mkpath(d.filePath(".hid")));
    QVERIFY(writeFile(d.filePath(".hid/inner.txt"), "i"));
    QVERIFY(writeFile(d.filePath("vis.txt"), "v"));

    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }

    {
      IconCache icons;
      ItemModel model(&icons, "loc", nullptr);
      QModelIndex root;
      loadModel(model, dir.path(), root);
      if (QTest::currentTestFailed()) {
        return;
      }
      QVERIFY(model.lastLoadUsedRc());
      QCOMPARE(childNames(model, root),
               (QStringList{".h.txt", ".hid", "vis.txt"}));
    }

    GetSettings()->setValue("Settings/showHidden", false);
    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(model.lastLoadUsedRc());
    const QStringList names = childNames(model, root);
    QVERIFY(!names.contains(".h.txt"));
    QVERIFY(!names.contains(".hid"));
    QCOMPARE(names, (QStringList{"vis.txt"}));
  }

  // T-E2E-RCLIST-DEFAULTOPTS: user rclone flags force the lsjson process.
  void T_E2E_RCLIST_DEFAULTOPTS() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(writeFile(d.filePath("one.txt"), "1"));

    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    GetSettings()->setValue("Settings/defaultRcloneOptions", "--fast-list");

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(!model.lastLoadUsedRc());
    QCOMPARE(childNames(model, root), (QStringList{"one.txt"}));
  }

  // T-E2E-DAEMON-UNLOCK: a locked config gives configLocked and a warning
  // row; unlocking through the daemon makes the next load work (R11, R17).
  void T_E2E_DAEMON_UNLOCK() {
    qunsetenv("RCLONE_CONFIG_PASS");
    qunsetenv("RCLONE_PASSWORD_COMMAND");
    SetRclonePassword("");
    SetRcloneConf(mEncryptedConf);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDir d(dir.path());
    QVERIFY(writeFile(d.filePath("t.txt"), "hello"));
    QVERIFY(QDir().mkpath(d.filePath("sub")));

    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }

    IconCache icons;
    ItemModel model(&icons, "loc", nullptr);
    QSignalSpy lockedSpy(&model, &ItemModel::configLocked);
    QModelIndex root;
    loadModel(model, dir.path(), root);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(model.lastLoadUsedRc());
    QCOMPARE(lockedSpy.count(), 1);
    QCOMPARE(model.rowCount(root), 1);
    Item *row = itemOf(model.index(0, 0, root));
    QCOMPARE(row->state, Item::Special);
    QCOMPARE(row->name, warnPrefix() + "configuration is locked");

    // R17: unlock is ok for a wrong password, the next call still fails
    RcReply reply;
    rcCall("config/unlock", QJsonObject{{"configPassword", "wrong"}}, reply);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY2(reply.ok, qPrintable(reply.error));
    rcCall("config/listremotes", QJsonObject(), reply);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(!reply.ok);
    QCOMPARE(int(classifyConfigLoadError(1, reply.error.toUtf8())),
             int(ConfigLoadProblem::NeedsPassword));

    rcCall("config/unlock", QJsonObject{{"configPassword", "pw"}}, reply);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY2(reply.ok, qPrintable(reply.error));
    rcCall("config/listremotes", QJsonObject(), reply);
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY2(reply.ok, qPrintable(reply.error));
    QVERIFY(reply.result.value("remotes").toArray().contains(
        QJsonValue(QString("loc"))));

    // a re-load lists the entries and drops the warning row
    model.refresh(root);
    QVERIFY(itemOf(root)->isLoading());
    QTRY_VERIFY_WITH_TIMEOUT(isReady(root), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(iconsSettled(root), 20000);
    QVERIFY(model.lastLoadUsedRc());
    QCOMPARE(childNames(model, root), (QStringList{"sub", "t.txt"}));
    QCOMPARE(lockedSpy.count(), 1);
    QCOMPARE(global.rcloneLsProcessCount, 0);
  }
};

QTEST_MAIN(TestE2eRcListing)
#include "tst_e2e_rclisting.moc"
