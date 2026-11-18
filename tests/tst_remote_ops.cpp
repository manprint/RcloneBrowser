#include <QtTest>

#include "remote_ops.h"

class TestRemoteOps : public QObject {
  Q_OBJECT

private slots:
  // T-OPS-MOVEARGS (#171): the argv of a folder move and of a file move, in
  // today's order (verb, common options, directory marker flags, source,
  // destination, dialog options), and the cleanup of the source folder.
  void T_OPS_MOVEARGS() {
    const QStringList common = {"--config", "/tmp/x y.conf", "--fast-list"};
    const QStringList extra = {"--create-empty-src-dirs",
                               "--delete-empty-src-dirs", "--verbose"};
    const QStringList markers = {"--s3-directory-markers",
                                 "--gcs-directory-markers",
                                 "--azureblob-directory-markers"};
    const auto build = [&](const QStringList &pathsAndDest) {
      return QStringList{"move"} + common + markers + pathsAndDest + extra;
    };

    // a folder goes to L/F, not into the contents of L
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "p/F", "L", common, extra),
             build({"loc:p/F", "loc:L/F"}));
    // spaces and brackets stay one argument each
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "My Docs/[2024] Photos (raw)",
                                       "Backup/a b", common, extra),
             build({"loc:My Docs/[2024] Photos (raw)",
                    "loc:Backup/a b/[2024] Photos (raw)"}));
    // the root of the remote is the empty path
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "F", "", common, extra),
             build({"loc:F", "loc:F"}));
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "p/F", "", common, extra),
             build({"loc:p/F", "loc:F"}));
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "p/F", "/", common, extra),
             build({"loc:p/F", "loc:/F"}));
    // a trailing '/' neither changes the name nor doubles a separator
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "p/F/", "L/", common, extra),
             build({"loc:p/F/", "loc:L/F"}));
    QCOMPARE(RemoteOps::folderMoveArgs("loc", "p/F/", "L", common, extra),
             build({"loc:p/F/", "loc:L/F"}));

    // a file, and the contents of a folder, keep the destination as given
    QCOMPARE(RemoteOps::moveArgs("loc", "p/f.txt", "L", common, extra),
             build({"loc:p/f.txt", "loc:L"}));
    QCOMPARE(RemoteOps::moveArgs("loc", "p/f.txt", "", common, extra),
             build({"loc:p/f.txt", "loc:"}));
    QCOMPARE(RemoteOps::moveArgs("loc", "p", "L", {}, {}),
             QStringList({"move", "loc:p", "loc:L"}));
    // the marker flags only come with an empty-folder option (they cost a
    // HEAD request per file on a bucket based remote)
    QCOMPARE(RemoteOps::moveArgs("loc", "p", "L", common, {"--verbose"}),
             (QStringList{"move"} + common +
              QStringList{"loc:p", "loc:L", "--verbose"}));
    QCOMPARE(RemoteOps::dirMarkerFlags(), markers);
    QCOMPARE(RemoteOps::emptyDirMarkerFlags({"--create-empty-src-dirs"}),
             markers);
    QCOMPARE(RemoteOps::emptyDirMarkerFlags({"--delete-empty-src-dirs"}),
             markers);
    QCOMPARE(RemoteOps::emptyDirMarkerFlags({"--verbose", "-n"}),
             QStringList());

    // the path pieces
    QCOMPARE(RemoteOps::joinPath("", "F"), QString("F"));
    QCOMPARE(RemoteOps::joinPath("L", "F"), QString("L/F"));
    QCOMPARE(RemoteOps::joinPath("L/", "F"), QString("L/F"));
    QCOMPARE(RemoteOps::joinPath("/", "F"), QString("/F"));
    QCOMPARE(RemoteOps::lastComponent("F"), QString("F"));
    QCOMPARE(RemoteOps::lastComponent("p/F"), QString("F"));
    QCOMPARE(RemoteOps::lastComponent("p/F//"), QString("F"));
    QCOMPARE(RemoteOps::lastComponent(""), QString());
    QCOMPARE(RemoteOps::lastComponent("/"), QString());

    // the destination is what the move action computed before 14.3, for
    // parents that are not the root
    const QStringList srcs = {"p/F", "My Docs/[2024] Photos (raw)", "a/b/c",
                              "top"};
    const QStringList parents = {"L", "Backup/a b", "x/y/z", "q"};
    for (int i = 0; i < srcs.size(); ++i) {
      QCOMPARE(RemoteOps::folderMoveDest(srcs.at(i), parents.at(i)),
               QDir(parents.at(i)).filePath(QDir(srcs.at(i)).dirName()));
    }

    // removing the empty source folder: rmdir, never rmdirs or purge; the
    // marker flags make it remove the marker of a bucket based remote
    QCOMPARE(RemoteOps::sourceCleanupArgs("loc", "p/F", common),
             QStringList({"rmdir", "--config", "/tmp/x y.conf", "--fast-list"}) +
                 markers + QStringList{"loc:p/F"});
    QVERIFY(RemoteOps::wantsSourceCleanup(extra));
    QVERIFY(RemoteOps::wantsSourceCleanup({"--delete-empty-src-dirs"}));
    // unchecked: the empty source stays
    QVERIFY(!RemoteOps::wantsSourceCleanup(
        {"--create-empty-src-dirs", "--verbose"}));
    QVERIFY(!RemoteOps::wantsSourceCleanup({}));
    // nothing is deleted by a dry run or an interactive run
    for (const QString flag :
         {"--dry-run", "-n", "--interactive", "-i", "--dry-run=true",
          "--dry-run=TRUE", "--dry-run=1", "--interactive=true", "-vn", "-nv",
          "-vi", "-iv", "-n=true"}) {
      QVERIFY2(!RemoteOps::wantsSourceCleanup(extra + QStringList{flag}),
               qPrintable(flag));
    }
    // spelled out as off, or other flags: the cleanup stays on
    for (const QString flag :
         {"--dry-run=false", "--interactive=false", "--dry-run=0", "-v", "-P",
          "--no-traverse", "--filter", "- stay.txt", "--max-depth=1"}) {
      QVERIFY2(RemoteOps::wantsSourceCleanup(extra + QStringList{flag}),
               qPrintable(flag));
    }

    // a move onto itself does nothing, so nothing may be cleaned up after it
    QVERIFY(RemoteOps::isSameLocation("F", "F"));
    QVERIFY(RemoteOps::isSameLocation(
        "F", RemoteOps::folderMoveDest("F", "")));
    QVERIFY(RemoteOps::isSameLocation(
        "p/F", RemoteOps::folderMoveDest("p/F", "p")));
    QVERIFY(RemoteOps::isSameLocation(
        "p/F/", RemoteOps::folderMoveDest("p/F/", "p/")));
    QVERIFY(RemoteOps::isSameLocation("/p/F", "p/F/"));
    QVERIFY(!RemoteOps::isSameLocation(
        "p/F", RemoteOps::folderMoveDest("p/F", "L")));
    QVERIFY(!RemoteOps::isSameLocation(
        "p/F", RemoteOps::folderMoveDest("p/F", "")));
    QVERIFY(!RemoteOps::isSameLocation(
        "p/F", RemoteOps::folderMoveDest("p/F", "p/F")));

    // the decision after the move process: only a finished, clean exit of a
    // folder move that went somewhere else, with the option on
    const auto clean = [&](bool folder, const QString &type, const QString &src,
                           const QString &dest, QProcess::ProcessState state,
                           QProcess::ProcessError error,
                           QProcess::ExitStatus status, int code,
                           const QStringList &opts) {
      return RemoteOps::shouldCleanSource(folder, type, src, dest, state, error,
                                          status, code, opts);
    };
    const auto ok = [&](bool folder, const QString &type, const QString &src,
                        const QString &dest, const QStringList &opts) {
      return clean(folder, type, src, dest, QProcess::NotRunning,
                   QProcess::UnknownError, QProcess::NormalExit, 0, opts);
    };
    QVERIFY(ok(true, "local", "p/F", "L/F", extra));
    QVERIFY(ok(true, "s3", "bkt/p/F", "bkt/L/F", extra));
    QVERIFY(ok(true, "drive", "F", "L/F", extra));
    // not a folder move (a file, several items)
    QVERIFY(!ok(false, "local", "p/F", "L/F", extra));
    // options: unchecked, dry run
    QVERIFY(!ok(true, "local", "p/F", "L/F", {"--verbose"}));
    QVERIFY(!ok(true, "local", "p/F", "L/F", extra + QStringList{"-vn"}));
    // onto itself
    QVERIFY(!ok(true, "local", "F", "F", extra));
    QVERIFY(!ok(true, "local", "p/F", "p/F", extra));
    QVERIFY(!ok(true, "local", "p/F/", "p/F", extra));
    // a top-level folder of a bucket-based remote is a bucket: kept
    for (const QString type :
         {"s3", "b2", "azureblob", "swift", "oracleobjectstorage", "oos",
          "qingstor", "storj", "tardigrade", "google cloud storage",
          "googlecloudstorage", "gcs"}) {
      QVERIFY2(!ok(true, type, "photos", "archive/photos", extra),
               qPrintable(type));
      QVERIFY2(!ok(true, type, "/photos/", "archive/photos", extra),
               qPrintable(type));
    }
    QVERIFY(ok(true, "drive", "photos", "archive/photos", extra));
    QVERIFY(ok(true, "local", "photos", "archive/photos", extra));
    // the process did not finish cleanly
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::Running,
                   QProcess::UnknownError, QProcess::NormalExit, 0, extra));
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::NotRunning,
                   QProcess::UnknownError, QProcess::NormalExit, 1, extra));
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::NotRunning,
                   QProcess::UnknownError, QProcess::NormalExit, 3, extra));
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::NotRunning,
                   QProcess::UnknownError, QProcess::CrashExit, 0, extra));
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::NotRunning,
                   QProcess::Crashed, QProcess::NormalExit, 0, extra));
    QVERIFY(!clean(true, "local", "p/F", "L/F", QProcess::NotRunning,
                   QProcess::FailedToStart, QProcess::NormalExit, 0, extra));
  }

  // T-OPS-MKDIRARGS (#220): the created path is <parent>/<name>, the root
  // parent included, and the names the dialog refuses.
  // The directory marker flags make a bucket based remote (S3, GCS, Azure
  // Blob) keep the new empty folder, which it would not create otherwise.
  void T_OPS_MKDIRARGS() {
    const QStringList common = {"--config", "/tmp/x y.conf", "--fast-list"};
    const QStringList markers = {"--s3-directory-markers",
                                 "--gcs-directory-markers",
                                 "--azureblob-directory-markers"};
    const auto build = [&](const QString &target) {
      return QStringList{"mkdir"} + common + markers + QStringList{target};
    };

    QCOMPARE(RemoteOps::mkdirArgs("loc", "", "new f", common),
             build("loc:new f"));
    QCOMPARE(RemoteOps::mkdirArgs("loc", "/", "new f", common),
             build("loc:/new f"));
    QCOMPARE(RemoteOps::mkdirArgs("loc", "a/b", "new f", common),
             build("loc:a/b/new f"));
    QCOMPARE(RemoteOps::mkdirArgs("loc", "a b", "[x] (y)", common),
             build("loc:a b/[x] (y)"));
    QCOMPARE(RemoteOps::mkdirArgs("loc", "a", "n", {}),
             QStringList{"mkdir"} + markers + QStringList{"loc:a/n"});

    for (const QString name :
         {"a", "new folder", "[x] (y)", " a ", "a\\b", "..a", "a.", "..."}) {
      QVERIFY2(RemoteOps::isValidFolderName(name), qPrintable(name));
    }
    for (const QString name :
         {"", "   ", "\t", "a/b", "/", "a/", "/a", ".", ".."}) {
      QVERIFY2(!RemoteOps::isValidFolderName(name), qPrintable(name));
    }
    QCOMPARE(RemoteOps::kMkdirTimeoutMs, 60000);
  }

  // T-OPS-DELETEARGS: purge with the directory marker flags for a folder (so
  // the marker of an empty folder of a bucket based remote goes too), delete
  // for a file.
  void T_OPS_DELETEARGS() {
    const QStringList common = {"--config", "/tmp/x y.conf"};
    const QStringList markers = RemoteOps::dirMarkerFlags();
    QCOMPARE(RemoteOps::deleteArgs("s3r", "bkt/new f", true, common),
             QStringList{"purge"} + common + markers +
                 QStringList{"s3r:bkt/new f"});
    QCOMPARE(RemoteOps::deleteArgs("s3r", "bkt/f.txt", false, common),
             QStringList{"delete"} + common + QStringList{"s3r:bkt/f.txt"});
    QCOMPARE(RemoteOps::deleteArgs("loc", "a", true, {}),
             QStringList{"purge"} + markers + QStringList{"loc:a"});
  }

  // T-OPS-RENAMEARGS: a file is renamed with moveto; a folder with move, the
  // marker flags and the empty-folder options, then mkdir of the new name and
  // rmdir of the old one (an empty folder of a bucket based remote is only a
  // marker, which move does not carry over).
  void T_OPS_RENAMEARGS() {
    const QStringList common = {"--config", "/tmp/x y.conf"};
    const QStringList markers = RemoteOps::dirMarkerFlags();
    QCOMPARE(
        RemoteOps::renameArgs("s3r", "bkt/a.txt", "bkt/b.txt", false, common),
        (QStringList{"moveto"} + common +
         QStringList{"s3r:bkt/a.txt", "s3r:bkt/b.txt"}));
    QCOMPARE(
        RemoteOps::renameArgs("s3r", "bkt/old f", "bkt/new f", true, common),
        (QStringList{"move"} + common + markers +
         QStringList{"s3r:bkt/old f", "s3r:bkt/new f",
                     "--create-empty-src-dirs", "--delete-empty-src-dirs"}));

    const auto followUps = [](const QString &type, const QString &src,
                              const QString &dest, QProcess::ProcessState state,
                              QProcess::ProcessError error,
                              QProcess::ExitStatus status, int exitCode,
                              const QStringList &opts) {
      return RemoteOps::folderRenameFollowUps("s3r", type, src, dest, state,
                                              error, status, exitCode, opts);
    };
    const auto done = [&](const QString &type, const QString &src,
                          const QString &dest, const QStringList &opts) {
      return followUps(type, src, dest, QProcess::NotRunning,
                       QProcess::UnknownError, QProcess::NormalExit, 0, opts);
    };
    const QStringList mkdirNew =
        QStringList{"mkdir"} + common + markers + QStringList{"s3r:bkt/new f"};
    const QStringList rmdirOld =
        QStringList{"rmdir"} + common + markers + QStringList{"s3r:bkt/old f"};
    QCOMPARE(done("s3", "bkt/old f", "bkt/new f", common),
             (QList<QStringList>{mkdirNew, rmdirOld}));
    QCOMPARE(done("local", "bkt/old f", "bkt/new f", common),
             (QList<QStringList>{mkdirNew, rmdirOld}));
    // a top-level folder of a bucket based remote is a bucket: the new one is
    // created, the old one is kept
    QCOMPARE(done("s3", "old", "new", common),
             (QList<QStringList>{QStringList{"mkdir"} + common + markers +
                                 QStringList{"s3r:new"}}));
    // onto itself, a dry run, an interactive run: nothing
    QVERIFY(done("s3", "bkt/old f", "bkt/old f", common).isEmpty());
    QVERIFY(done("s3", "bkt/old f", "bkt/new f",
                 common + QStringList{"--dry-run"})
                .isEmpty());
    QVERIFY(
        done("s3", "bkt/old f", "bkt/new f", common + QStringList{"-i"})
            .isEmpty());
    // a move that did not finish cleanly: nothing
    QVERIFY(followUps("s3", "bkt/old f", "bkt/new f", QProcess::NotRunning,
                      QProcess::UnknownError, QProcess::NormalExit, 1, common)
                .isEmpty());
    QVERIFY(followUps("s3", "bkt/old f", "bkt/new f", QProcess::Running,
                      QProcess::UnknownError, QProcess::NormalExit, 0, common)
                .isEmpty());
    QVERIFY(followUps("s3", "bkt/old f", "bkt/new f", QProcess::NotRunning,
                      QProcess::Crashed, QProcess::NormalExit, 0, common)
                .isEmpty());
    QVERIFY(followUps("s3", "bkt/old f", "bkt/new f", QProcess::NotRunning,
                      QProcess::UnknownError, QProcess::CrashExit, 0, common)
                .isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestRemoteOps)
#include "tst_remote_ops.moc"
