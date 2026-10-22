#include <QtTest>

#include "global.h"
#include "rb_test_support.h"
#include "rclone_process.h"
#include "remote_ops.h"
#include "utils.h"

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  return f.write(data) == data.size();
}

// The dialog defaults of a move (RemoteFolderDialog): the options the move
// action passes after the two paths.
QStringList dialogExtra() {
  return {"--create-empty-src-dirs", "--delete-empty-src-dirs", "--verbose"};
}

// Every file and folder below `root` as sorted relative paths, folders with a
// trailing '/'. Empty when `root` does not exist.
QStringList tree(const QString &root) {
  QStringList names;
  const QDir base(root);
  QDirIterator it(root,
                  QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    names << base.relativeFilePath(it.filePath()) +
                 (it.fileInfo().isDir() ? "/" : "");
  }
  names.sort();
  return names;
}

} // namespace

class TestE2eRemoteOps : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;

  // One rclone call on the temporary config (I-9), the environment the app
  // gives its rclone processes. Output (stdout and stderr) goes to `out`.
  int run(const QStringList &args, QString *out = nullptr) const {
    QProcess p;
    QProcessEnvironment env = RcloneEnvironment(RcloneEnvMode::Parsed);
    env.insert(QStringLiteral("RCLONE_CONFIG"), mConf);
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(GetRclone(), args);
    p.closeWriteChannel();
    if (!p.waitForFinished(60000)) {
      p.kill();
      p.waitForFinished(5000);
      return -1;
    }
    if (out) {
      *out = QString::fromUtf8(p.readAll());
    }
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -2;
  }

  QStringList common() const { return {"--config", mConf}; }

  // The folder tree every row moves.
  static bool seed(const QString &dir) {
    return writeFile(dir + "/a b.txt", "hello\n") &&
           writeFile(dir + "/[x] (y).txt", "xy") &&
           writeFile(dir + "/sub/c.txt", "cc") &&
           QDir().mkpath(dir + "/empty");
  }

  static QStringList seeded() {
    return {"[x] (y).txt", "a b.txt", "empty/", "sub/", "sub/c.txt"};
  }

  // R-REPRO of #171 on the argv the move action ran before 14.3: records what
  // is left of the source folder, never asserts it.
  void recordLegacy(const QString &name, bool destExists) {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString p = tmp.filePath("p/F");
    const QString l = tmp.filePath("L/F");
    QVERIFY(seed(p));
    if (destExists) {
      QVERIFY(writeFile(l + "/old.txt", "old"));
    }
    // literally the base argv: move <conf> <src> <dest> <dialog options>
    const QStringList legacy =
        QStringList{"move"} + common() +
        QStringList{"loc:" + p, "loc:" + l} + dialogExtra();
    QString out;
    const int rc = run(legacy, &out);
    qInfo("legacy move (%s): exit %d, p/F exists %d, p/F tree [%s], L/F tree "
          "[%s]",
          qPrintable(name), rc, QDir(p).exists() ? 1 : 0,
          qPrintable(tree(p).join(", ")), qPrintable(tree(l).join(", ")));
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(mDir.isValid());
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    SetRclone(rbRclone());
    // I-9: a temporary config with one local remote, never the host one
    mConf = mDir.filePath("rclone.conf");
    QVERIFY(writeFile(mConf, "[loc]\ntype = local\n"));
    SetRcloneConf(mConf);

    recordLegacy("L/F absent", false);
    recordLegacy("L/F present", true);

    // why the folder move keeps `move`: moveto has neither dialog option
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(seed(tmp.filePath("p/F")));
    QString out;
    const int rc = run(QStringList{"moveto"} + common() +
                           QStringList{"loc:" + tmp.filePath("p/F"),
                                       "loc:" + tmp.filePath("L/F"),
                                       "--delete-empty-src-dirs"},
                       &out);
    qInfo("moveto with --delete-empty-src-dirs: exit %d, %s", rc,
          qPrintable(out.trimmed().split('\n').last()));
  }

  void cleanupTestCase() { SetRcloneConf(QString()); }

  // T-E2E-OPS-MOVEFOLDER (#171): moving folder F of p into L gives L/F with
  // the whole tree, also when L/F exists, and p/F is gone, not even empty. The
  // rows `filtered` and `self` run the cleanup on a source that must survive:
  // a filter keeps stay.txt in p/F, and an empty p/F moved into p stays.
  void T_E2E_OPS_MOVEFOLDER_data() {
    QTest::addColumn<QString>("srcName");
    QTest::addColumn<QString>("destParent");
    QTest::addColumn<bool>("destExists");
    QTest::addColumn<bool>("isFile");
    QTest::addColumn<bool>("filtered");
    QTest::addColumn<bool>("self");
    QTest::newRow("fresh") << QString("F") << QString("L") << false << false
                           << false << false;
    QTest::newRow("merge") << QString("F") << QString("L") << true << false
                           << false << false;
    QTest::newRow("special") << QString("My [src] dir")
                             << QString("Dest dir (1)") << false << false
                             << false << false;
    QTest::newRow("file") << QString("f.txt") << QString("L") << false << true
                          << false << false;
    QTest::newRow("filtered") << QString("F") << QString("L") << false << false
                              << true << false;
    QTest::newRow("self") << QString("F") << QString("p") << false << false
                          << false << true;
  }

  void T_E2E_OPS_MOVEFOLDER() {
    QFETCH(QString, srcName);
    QFETCH(QString, destParent);
    QFETCH(bool, destExists);
    QFETCH(bool, isFile);
    QFETCH(bool, filtered);
    QFETCH(bool, self);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString p = tmp.filePath("p");
    const QString src = p + "/" + srcName;
    const QString dest = tmp.filePath(destParent);
    QVERIFY(writeFile(p + "/keep.txt", "keep"));
    QStringList expected;
    if (isFile) {
      QVERIFY(writeFile(src, "file"));
    } else if (self) {
      QVERIFY(QDir().mkpath(src)); // an empty folder, the one rmdir removes
    } else {
      QVERIFY(seed(src));
      expected = seeded();
      if (destExists) {
        QVERIFY(writeFile(dest + "/" + srcName + "/old.txt", "old"));
        expected << "old.txt";
        expected.sort();
      }
      if (filtered) {
        QVERIFY(writeFile(src + "/stay.txt", "stay"));
      }
    }

    QStringList extra = dialogExtra();
    if (filtered) {
      extra << "--filter" << "- stay.txt";
    }
    // the argv the move action starts, and for a folder the cleanup after it
    const QStringList argv =
        isFile ? RemoteOps::moveArgs("loc", src, dest, common(), extra)
               : RemoteOps::folderMoveArgs("loc", src, dest, common(), extra);
    // the action does not start the move of a folder onto itself; running it
    // shows that rclone does nothing and exits 0, which is what the cleanup
    // gate must not take for a move
    const QString target =
        isFile ? dest : RemoteOps::folderMoveDest(src, dest);
    QVERIFY(self == RemoteOps::isSameLocation(src, target));
    QString out;
    const int rc = run(argv, &out);
    if (!self) {
      QVERIFY2(rc == 0, qPrintable(QString::number(rc) + ": " + out));
    } else {
      qInfo("move onto itself: exit %d", rc);
    }

    // the decision of the move action, on what the process reported
    const bool clean = RemoteOps::shouldCleanSource(
        !isFile, "local", src, target, QProcess::NotRunning,
        QProcess::UnknownError,
        rc == -2 ? QProcess::CrashExit : QProcess::NormalExit, rc, extra);
    QCOMPARE(clean, !isFile && !self);
    if (clean) {
      QString cleanOut;
      const int crc =
          run(RemoteOps::sourceCleanupArgs("loc", src, common()), &cleanOut);
      // the exit code is not part of the oracle: rmdir fails harmlessly when
      // the folder is already gone or not empty
      qInfo("cleanup (%s): exit %d", QTest::currentDataTag(), crc);
    }

    if (isFile) {
      QVERIFY(QFileInfo(dest + "/" + srcName).isFile());
      QVERIFY(!QFileInfo::exists(src));
    } else if (self) {
      QVERIFY2(QDir(src).exists(), "the empty folder was removed");
      QVERIFY(tree(src).isEmpty());
    } else if (filtered) {
      QCOMPARE(tree(dest + "/" + srcName), expected);
      QVERIFY(!QFileInfo::exists(dest + "/" + srcName + "/stay.txt"));
      // rmdir refused the folder that still holds the filtered file
      QVERIFY2(QFileInfo(src + "/stay.txt").isFile(),
               qPrintable(tree(src).join(", ")));
    } else {
      QCOMPARE(tree(dest + "/" + srcName), expected);
      QVERIFY2(!QFileInfo::exists(src), qPrintable(tree(src).join(", ")));
    }
    QVERIFY(QDir(p).exists());
    QVERIFY(QFileInfo(p + "/keep.txt").isFile());
  }
};

QTEST_MAIN(TestE2eRemoteOps)
#include "tst_e2e_remote_ops.moc"
