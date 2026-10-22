#include <QtTest>

#include <memory>

#include "job_options.h"
#include "list_of_job_options.h"
#include "rb_test_support.h"
#include "transfer_dialog.h"
#include "utils.h"

namespace {

// The singleton's constructor is protected: the probe reloads the tasks file.
struct Probe : ListOfJobOptions {
  Probe() {}
};

// A task with a distinctive non-default value in every field that a version 8
// tasks file holds (everything up to deleteEmptySrcDirs). dryRun is not
// persisted. The fixture jobopts_v8.bin was recorded from this task with the
// code of classVersion 8: never change a value here without recording it again.
JobOptions makeV8() {
  JobOptions jo;
  jo.description = QStringLiteral("v8 distinctive task");
  jo.jobType = JobOptions::Download;
  jo.operation = JobOptions::Sync;
  jo.sync = true;
  jo.syncTiming = JobOptions::After;
  jo.skipNewer = true;
  jo.skipExisting = true;
  jo.compare = true;
  jo.compareOption = JobOptions::ChecksumIgnoreSize;
  jo.verbose = true;
  jo.sameFilesystem = true;
  jo.dontUpdateModified = true;
  jo.transfers = QStringLiteral("11");
  jo.checkers = QStringLiteral("12");
  jo.bandwidth = QStringLiteral("13M");
  jo.minSize = QStringLiteral("14k");
  jo.minAge = QStringLiteral("15d");
  jo.maxAge = QStringLiteral("16d");
  jo.maxDepth = 17;
  jo.connectTimeout = QStringLiteral("18");
  jo.idleTimeout = QStringLiteral("19");
  jo.retries = QStringLiteral("20");
  jo.lowLevelRetries = QStringLiteral("21");
  jo.deleteExcluded = true;
  jo.excluded = QStringLiteral("*.exc1\n*.exc2");
  jo.extra = QStringLiteral("--extra-a --extra-b \"x y\"");
  jo.source = QStringLiteral("remote-src:/some/source dir");
  jo.dest = QStringLiteral("/some/dest dir");
  jo.isFolder = true;
  jo.uniqueId = QUuid(QStringLiteral("{8e0c2f1a-5b3d-4c6e-9a7f-0123456789ab}"));
  jo.DriveSharedWithMe = true;
  jo.remoteMode = QStringLiteral("trash");
  jo.remoteType = QStringLiteral("drive");
  jo.mountReadOnly = true;
  jo.mountCacheLevel = JobOptions::Writes;
  jo.mountVolume = QStringLiteral("vol-22");
  jo.mountAutoStart = true;
  jo.mountRcPort = QStringLiteral("5572");
  jo.mountScript = QStringLiteral("/opt/mount-script.sh");
  jo.mountWinDriveMode = true;
  jo.included = QStringLiteral("*.inc1\n*.inc2");
  jo.noTraverse = true;
  jo.createEmptySrcDirs = true;
  jo.filtered = QStringLiteral("+ *.f1\n- *.f2");
  jo.deleteEmptySrcDirs = true;
  return jo;
}

// The version 8 fields, one by one (a failure names the field).
void compareV8(const JobOptions &got, const JobOptions &want) {
  QCOMPARE(got.description, want.description);
  QCOMPARE(got.jobType, want.jobType);
  QCOMPARE(got.operation, want.operation);
  QCOMPARE(got.dryRun, false);
  QCOMPARE(got.sync, want.sync);
  QCOMPARE(got.syncTiming, want.syncTiming);
  QCOMPARE(got.skipNewer, want.skipNewer);
  QCOMPARE(got.skipExisting, want.skipExisting);
  QCOMPARE(got.compare, want.compare);
  QCOMPARE(got.compareOption, want.compareOption);
  QCOMPARE(got.verbose, want.verbose);
  QCOMPARE(got.sameFilesystem, want.sameFilesystem);
  QCOMPARE(got.dontUpdateModified, want.dontUpdateModified);
  QCOMPARE(got.transfers, want.transfers);
  QCOMPARE(got.checkers, want.checkers);
  QCOMPARE(got.bandwidth, want.bandwidth);
  QCOMPARE(got.minSize, want.minSize);
  QCOMPARE(got.minAge, want.minAge);
  QCOMPARE(got.maxAge, want.maxAge);
  QCOMPARE(got.maxDepth, want.maxDepth);
  QCOMPARE(got.connectTimeout, want.connectTimeout);
  QCOMPARE(got.idleTimeout, want.idleTimeout);
  QCOMPARE(got.retries, want.retries);
  QCOMPARE(got.lowLevelRetries, want.lowLevelRetries);
  QCOMPARE(got.deleteExcluded, want.deleteExcluded);
  QCOMPARE(got.excluded, want.excluded);
  QCOMPARE(got.extra, want.extra);
  QCOMPARE(got.source, want.source);
  QCOMPARE(got.dest, want.dest);
  QCOMPARE(got.isFolder, want.isFolder);
  QCOMPARE(got.uniqueId, want.uniqueId);
  QCOMPARE(got.DriveSharedWithMe, want.DriveSharedWithMe);
  QCOMPARE(got.remoteMode, want.remoteMode);
  QCOMPARE(got.remoteType, want.remoteType);
  QCOMPARE(got.mountReadOnly, want.mountReadOnly);
  QCOMPARE(got.mountCacheLevel, want.mountCacheLevel);
  QCOMPARE(got.mountVolume, want.mountVolume);
  QCOMPARE(got.mountAutoStart, want.mountAutoStart);
  QCOMPARE(got.mountRcPort, want.mountRcPort);
  QCOMPARE(got.mountScript, want.mountScript);
  QCOMPARE(got.mountWinDriveMode, want.mountWinDriveMode);
  QCOMPARE(got.included, want.included);
  QCOMPARE(got.noTraverse, want.noTraverse);
  QCOMPARE(got.createEmptySrcDirs, want.createEmptySrcDirs);
  QCOMPARE(got.filtered, want.filtered);
  QCOMPARE(got.deleteEmptySrcDirs, want.deleteEmptySrcDirs);
}

// Where ListOfJobOptions keeps the tasks file (a private HOME, no portable mode)
QString tasksFile() {
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
         QStringLiteral("/tasks.bin");
}

bool replaceFile(const QString &from, const QString &to) {
  QFile::remove(to);
  return QFile::copy(from, to);
}

// The version 9 fields: the bisync options.
void compareBisync(const JobOptions &got, const JobOptions &want) {
  QCOMPARE(got.bisyncResync, want.bisyncResync);
  QCOMPARE(got.bisyncConflictResolve, want.bisyncConflictResolve);
  QCOMPARE(got.bisyncResilient, want.bisyncResilient);
  QCOMPARE(got.bisyncRecover, want.bisyncRecover);
  QCOMPARE(got.bisyncWorkdir, want.bisyncWorkdir);
}

// A task with every field that getOptions() reads, as the Copy and Move rows
// of T_JO_BISYNC_ARGS (and the Sync and Bisync rows, which add the sync flag)
// set it.
JobOptions makeCommon(JobOptions::Operation op, JobOptions::JobType type) {
  JobOptions jo;
  jo.jobType = type;
  jo.operation = op;
  jo.skipNewer = true;
  jo.skipExisting = true;
  jo.compare = true;
  jo.compareOption = JobOptions::ChecksumIgnoreSize;
  jo.sameFilesystem = true;
  jo.dontUpdateModified = true;
  jo.noTraverse = true;
  jo.createEmptySrcDirs = true;
  jo.deleteEmptySrcDirs = true;
  jo.transfers = QStringLiteral("4");
  jo.checkers = QStringLiteral("8");
  jo.bandwidth = QStringLiteral("1M");
  jo.minSize = QStringLiteral("1k");
  jo.minAge = QStringLiteral("1d");
  jo.maxAge = QStringLiteral("2d");
  jo.maxDepth = 3;
  jo.connectTimeout = QStringLiteral("60");
  jo.idleTimeout = QStringLiteral("300");
  jo.retries = QStringLiteral("3");
  jo.lowLevelRetries = QStringLiteral("10");
  jo.deleteExcluded = true;
  jo.extra = QStringLiteral("--extra-a \"x y\"");
  jo.source = QStringLiteral("s:/src");
  jo.dest = QStringLiteral("d:/dst");
  jo.included = QStringLiteral("*.inc");
  jo.excluded = QStringLiteral("*.exc");
  jo.filtered = QStringLiteral("+ f");
  jo.remoteType = QStringLiteral("drive");
  jo.remoteMode = QStringLiteral("shared");
  jo.dryRun = true;
  return jo;
}

// What getOptions() gives for makeCommon() after the default options and the
// sync timing flag, as the base code of classVersion 8 builds it (written by
// hand from that code, not computed).
QStringList commonTail() {
  return {QStringLiteral("--update"),
          QStringLiteral("--ignore-existing"),
          QStringLiteral("--checksum"),
          QStringLiteral("--ignore-size"),
          QStringLiteral("--one-file-system"),
          QStringLiteral("--no-update-modtime"),
          QStringLiteral("--no-traverse"),
          QStringLiteral("--create-empty-src-dirs"),
          QStringLiteral("--delete-empty-src-dirs"),
          QStringLiteral("--transfers"),
          QStringLiteral("4"),
          QStringLiteral("--checkers"),
          QStringLiteral("8"),
          QStringLiteral("--bwlimit"),
          QStringLiteral("1M"),
          QStringLiteral("--min-size"),
          QStringLiteral("1k"),
          QStringLiteral("--min-age"),
          QStringLiteral("1d"),
          QStringLiteral("--max-age"),
          QStringLiteral("2d"),
          QStringLiteral("--max-depth"),
          QStringLiteral("3"),
          QStringLiteral("--contimeout"),
          QStringLiteral("60s"),
          QStringLiteral("--timeout"),
          QStringLiteral("300s"),
          QStringLiteral("--retries"),
          QStringLiteral("3"),
          QStringLiteral("--low-level-retries"),
          QStringLiteral("10"),
          QStringLiteral("--delete-excluded"),
          QStringLiteral("--extra-a"),
          QStringLiteral("x y"),
          QStringLiteral("--filter"),
          QStringLiteral("- *.exc"),
          QStringLiteral("--filter"),
          QStringLiteral("+ *.inc"),
          QStringLiteral("--filter"),
          QStringLiteral("+ f"),
          QStringLiteral("--filter"),
          QStringLiteral("- **"),
          QStringLiteral("--drive-shared-with-me"),
          QStringLiteral("--dry-run")};
}

// The same without the flags a Bisync task never emits: rclone 1.75.2 bisync
// rejects --delete-empty-src-dirs ("unknown flag", exit 2), ignores --update and
// --no-traverse with a WARNING, and --delete-excluded makes it delete the
// destination files outside the delta queue (queue.go:254, sync.go:961).
QStringList bisyncTail() {
  QStringList tail = commonTail();
  for (const char *flag : {"--delete-empty-src-dirs", "--update",
                           "--no-traverse", "--delete-excluded"}) {
    tail.removeAll(QLatin1String(flag));
  }
  return tail;
}

// the head of the argv: the command, source, destination and default options
QStringList head(const QString &command, JobOptions::JobType type) {
  return {command,
          QStringLiteral("s:/src"),
          QStringLiteral("d:/dst"),
          QStringLiteral("--drc"),
          QStringLiteral("a"),
          type == JobOptions::Upload ? QStringLiteral("--up")
                                     : QStringLiteral("--down")};
}

QStringList join(const QStringList &a, const QStringList &b,
                 const QStringList &c = QStringList()) {
  return a + b + c;
}

} // namespace

class TestJobOptions : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    // A private HOME keeps the tasks of other tests out of the tasks file
    // (QStandardPaths test mode keeps it under $HOME/.qttest).
    QVERIFY(QDir().mkpath(rbTestDir().path() + QStringLiteral("/home")));
    qputenv("HOME", (rbTestDir().path() + QStringLiteral("/home")).toUtf8());
    QVERIFY(!IsPortableMode());
    QVERIFY(QDir().mkpath(QFileInfo(tasksFile()).absolutePath()));

    // The recorder of tests/fixtures/synthetic/jobopts_v8.bin. It only runs
    // with RB_RECORD_JO_V8=1, then skips the whole test, and it must run with
    // the code of classVersion 8: ListOfJobOptions writes the tasks file.
    if (qEnvironmentVariableIntValue("RB_RECORD_JO_V8") == 1) {
      QCOMPARE(JobOptions::classVersion, qint32(8));
      auto *jo = new JobOptions(makeV8());
      ListOfJobOptions *list = ListOfJobOptions::getInstance();
      QVERIFY(list->Persist(jo));
      QVERIFY(replaceFile(tasksFile(), rbFixture(QStringLiteral(
                                           "synthetic/jobopts_v8.bin"))));
      list->Forget(jo);
      delete jo;
      QSKIP("jobopts_v8.bin recorded (RB_RECORD_JO_V8=1)");
    }

    // default options that the argv rows below expect
    auto settings = GetSettings();
    settings->setValue(QStringLiteral("Settings/defaultRcloneOptions"),
                       QStringLiteral("--drc a"));
    settings->setValue(QStringLiteral("Settings/defaultUploadOptions"),
                       QStringLiteral("--up"));
    settings->setValue(QStringLiteral("Settings/defaultDownloadOptions"),
                       QStringLiteral("--down"));
  }

  // T-JO-COMPAT (I-7): a tasks file written by classVersion 8 loads with the
  // same value in every field, and the version 9 fields have their defaults.
  void T_JO_COMPAT() {
    // the stored values of the enums do not move
    QCOMPARE(int(JobOptions::Sync), 3);
    QCOMPARE(int(JobOptions::CryptCheck), 6);
    QCOMPARE(int(JobOptions::Bisync), 7);
    QCOMPARE(JobOptions::classVersion, qint32(9));

    const QString fixture =
        rbFixture(QStringLiteral("synthetic/jobopts_v8.bin"));
    QVERIFY2(QFileInfo::exists(fixture), qPrintable(fixture));
    QVERIFY(replaceFile(fixture, tasksFile()));
    Probe probe;
    QVERIFY(ListOfJobOptions::RestoreFromUserData(probe));
    const QList<JobOptions *> loaded = probe.getTasks();
    QCOMPARE(loaded.size(), 1);
    compareV8(*loaded.first(), makeV8());
    QVERIFY(!QTest::currentTestFailed());

    QCOMPARE(loaded.first()->bisyncResync, false);
    QCOMPARE(loaded.first()->bisyncConflictResolve, QStringLiteral("none"));
    QCOMPARE(loaded.first()->bisyncResilient, false);
    QCOMPARE(loaded.first()->bisyncRecover, false);
    QCOMPARE(loaded.first()->bisyncWorkdir, QString());
    qDeleteAll(loaded);
  }

  // T-JO-ROUNDTRIP: a version 9 Bisync task survives the stream, and so does
  // the tasks file written by ListOfJobOptions. Every pair of the three bisync
  // bools differs in at least one record, so a swapped read or write of two
  // fields shows.
  void T_JO_ROUNDTRIP() {
    JobOptions jo1 = makeV8();
    jo1.operation = JobOptions::Bisync;
    jo1.sync = false;
    jo1.bisyncResync = true;
    jo1.bisyncConflictResolve = QStringLiteral("newer");
    jo1.bisyncResilient = false;
    jo1.bisyncRecover = true;
    jo1.bisyncWorkdir = QStringLiteral("/some/work dir");

    JobOptions jo2 = jo1;
    jo2.uniqueId =
        QUuid(QStringLiteral("{1f2e3d4c-5b6a-4978-8695-a4b3c2d1e0f9}"));
    jo2.bisyncResync = false;
    jo2.bisyncConflictResolve = QStringLiteral("path2");
    jo2.bisyncResilient = true;
    jo2.bisyncRecover = true;
    jo2.bisyncWorkdir = QStringLiteral("/other/wd");
    const JobOptions *records[] = {&jo1, &jo2};

    QByteArray bytes;
    {
      QDataStream out(&bytes, QIODevice::WriteOnly);
      out.setVersion(QDataStream::Qt_5_2);
      out << jo1;
      out << jo2;
    }
    QDataStream in(bytes);
    in.setVersion(QDataStream::Qt_5_2);
    for (const JobOptions *want : records) {
      JobOptions back;
      in >> back;
      QCOMPARE(back.operation, JobOptions::Bisync);
      compareV8(back, *want);
      QVERIFY(!QTest::currentTestFailed());
      compareBisync(back, *want);
      QVERIFY(!QTest::currentTestFailed());
    }
    QVERIFY(in.atEnd());

    // the layout of the version 9 tail is frozen: bool, QString, bool, bool,
    // QString (the order phase 13.1 records as jobopts_v9.bin); both records
    // are pinned, since one record cannot hold three different bools
    struct Pinned {
      const JobOptions *jo;
      bool resync;
      QString conflict;
      bool resilient;
      bool recover;
      QString workdir;
    };
    const Pinned pinned[] = {
        {&jo1, true, QStringLiteral("newer"), false, true,
         QStringLiteral("/some/work dir")},
        {&jo2, false, QStringLiteral("path2"), true, true,
         QStringLiteral("/other/wd")}};
    for (const Pinned &p : pinned) {
      QByteArray tail;
      {
        QDataStream out(&tail, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        out << p.resync << p.conflict << p.resilient << p.recover << p.workdir;
      }
      QVERIFY(tail.size() > 5);
      QByteArray one;
      {
        JobOptions record = *p.jo; // operator<< takes a non-const JobOptions
        QDataStream out(&one, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_2);
        out << record;
      }
      QCOMPARE(one.right(tail.size()), tail);
    }

    // through the tasks file
    QFile::remove(tasksFile());
    auto *persisted1 = new JobOptions(jo1);
    auto *persisted2 = new JobOptions(jo2);
    ListOfJobOptions *list = ListOfJobOptions::getInstance();
    QVERIFY(list->Persist(persisted1));
    QVERIFY(list->Persist(persisted2));
    Probe probe;
    QVERIFY(ListOfJobOptions::RestoreFromUserData(probe));
    const QList<JobOptions *> loaded = probe.getTasks();
    QCOMPARE(loaded.size(), 2);
    for (JobOptions *got : loaded) {
      const JobOptions *want = got->uniqueId == jo1.uniqueId ? &jo1 : &jo2;
      QCOMPARE(got->uniqueId, want->uniqueId);
      compareV8(*got, *want);
      QVERIFY(!QTest::currentTestFailed());
      compareBisync(*got, *want);
      QVERIFY(!QTest::currentTestFailed());
    }
    qDeleteAll(loaded);
    list->Forget(persisted1);
    list->Forget(persisted2);
    delete persisted1;
    delete persisted2;
  }

  void T_JO_BISYNC_ARGS_data() {
    QTest::addColumn<int>("op");
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("syncFlag");
    QTest::addColumn<int>("timing");
    QTest::addColumn<bool>("resync");
    QTest::addColumn<QString>("conflict");
    QTest::addColumn<bool>("resilient");
    QTest::addColumn<bool>("recover");
    QTest::addColumn<QString>("workdir");
    QTest::addColumn<QStringList>("expected");

    const auto up = JobOptions::Upload;
    const auto down = JobOptions::Download;
    const auto none = QStringLiteral("none");
    const QStringList tail = commonTail();
    const QStringList btail = bisyncTail();
    const QString delAfter = QStringLiteral("--delete-after");

    // Copy, Move and Sync keep the argv of the base code (classVersion 8)
    QTest::newRow("base-copy")
        << int(JobOptions::Copy) << int(up) << false << int(JobOptions::After)
        << false << none << false << false << QString()
        << join(head("copy", up), tail);
    QTest::newRow("base-move")
        << int(JobOptions::Move) << int(up) << false << int(JobOptions::After)
        << false << none << false << false << QString()
        << join(head("move", up), tail);
    QTest::newRow("base-sync-upload")
        << int(JobOptions::Sync) << int(up) << true << int(JobOptions::After)
        << false << none << false << false << QString()
        << join(head("sync", up), {delAfter}, tail);
    QTest::newRow("base-sync-download-before")
        << int(JobOptions::Sync) << int(down) << true
        << int(JobOptions::Before) << false << none << false << false
        << QString()
        << join(head("sync", down), {QStringLiteral("--delete-before")}, tail);
    // a bisync-only field never reaches the other modes
    QTest::newRow("base-sync-ignores-bisync-fields")
        << int(JobOptions::Sync) << int(up) << true << int(JobOptions::During)
        << true << QStringLiteral("newer") << true << true
        << QStringLiteral("/w")
        << join(head("sync", up), {QStringLiteral("--delete-during")}, tail);

    // Bisync: the Sync flags without the sync timing, then the bisync flags
    QTest::newRow("bisync-all")
        << int(JobOptions::Bisync) << int(up) << true << int(JobOptions::After)
        << true << QStringLiteral("newer") << true << true
        << QStringLiteral("/w")
        << join(head("bisync", up), btail,
                {QStringLiteral("--resync"), QStringLiteral("--conflict-resolve"),
                 QStringLiteral("newer"), QStringLiteral("--resilient"),
                 QStringLiteral("--recover"), QStringLiteral("--workdir"),
                 QStringLiteral("/w")});
    QTest::newRow("bisync-defaults")
        << int(JobOptions::Bisync) << int(up) << false
        << int(JobOptions::UnknownTiming) << false << none << false << false
        << QString() << join(head("bisync", up), btail);
    QTest::newRow("bisync-download")
        << int(JobOptions::Bisync) << int(down) << false
        << int(JobOptions::UnknownTiming) << false << none << false << false
        << QString() << join(head("bisync", down), btail);
    QTest::newRow("bisync-timing-before")
        << int(JobOptions::Bisync) << int(up) << true << int(JobOptions::Before)
        << false << none << false << false << QString()
        << join(head("bisync", up), btail);
    QTest::newRow("bisync-timing-during")
        << int(JobOptions::Bisync) << int(up) << true << int(JobOptions::During)
        << false << none << false << false << QString()
        << join(head("bisync", up), btail);
    QTest::newRow("bisync-resync-only")
        << int(JobOptions::Bisync) << int(up) << false
        << int(JobOptions::UnknownTiming) << true << none << false << false
        << QString()
        << join(head("bisync", up), btail, {QStringLiteral("--resync")});
    QTest::newRow("bisync-conflict-path2")
        << int(JobOptions::Bisync) << int(up) << false
        << int(JobOptions::UnknownTiming) << false << QStringLiteral("path2")
        << false << false << QString()
        << join(head("bisync", up), btail,
                {QStringLiteral("--conflict-resolve"), QStringLiteral("path2")});
    QTest::newRow("bisync-workdir-only")
        << int(JobOptions::Bisync) << int(up) << false
        << int(JobOptions::UnknownTiming) << false << none << false << false
        << QStringLiteral("/w")
        << join(head("bisync", up), btail,
                {QStringLiteral("--workdir"), QStringLiteral("/w")});
  }

  // T-JO-BISYNC-ARGS: the argv of a Bisync task, and the unchanged argv of
  // Copy, Move and Sync (the expected lists are written by hand from the code
  // of classVersion 8).
  void T_JO_BISYNC_ARGS() {
    QFETCH(int, op);
    QFETCH(int, type);
    QFETCH(bool, syncFlag);
    QFETCH(int, timing);
    QFETCH(bool, resync);
    QFETCH(QString, conflict);
    QFETCH(bool, resilient);
    QFETCH(bool, recover);
    QFETCH(QString, workdir);
    QFETCH(QStringList, expected);

    JobOptions jo = makeCommon(JobOptions::Operation(op),
                               JobOptions::JobType(type));
    jo.sync = syncFlag;
    jo.syncTiming = JobOptions::SyncTiming(timing);
    jo.bisyncResync = resync;
    jo.bisyncConflictResolve = conflict;
    jo.bisyncResilient = resilient;
    jo.bisyncRecover = recover;
    jo.bisyncWorkdir = workdir;

    const QStringList argv = jo.getOptions();
    QCOMPARE(argv, expected);

    if (JobOptions::Operation(op) == JobOptions::Bisync) {
      QCOMPARE(argv.mid(0, 3), (QStringList{QStringLiteral("bisync"),
                                            QStringLiteral("s:/src"),
                                            QStringLiteral("d:/dst")}));
      for (const char *flag :
           {"--delete-before", "--delete-during", "--delete-after"}) {
        QVERIFY2(!argv.contains(QLatin1String(flag)), flag);
      }
      QCOMPARE(argv.contains(QStringLiteral("--resync")), resync);
      QCOMPARE(argv.contains(QStringLiteral("--resilient")), resilient);
      QCOMPARE(argv.contains(QStringLiteral("--recover")), recover);
      QCOMPARE(argv.contains(QStringLiteral("--conflict-resolve")),
               conflict != QStringLiteral("none"));
      QCOMPARE(argv.contains(QStringLiteral("--workdir")), !workdir.isEmpty());
      // makeCommon() sets all of these: bisync leaves them out
      for (const char *flag : {"--delete-empty-src-dirs", "--delete-excluded",
                               "--update", "--no-traverse"}) {
        QVERIFY2(!argv.contains(QLatin1String(flag)), flag);
      }
    } else {
      for (const char *flag : {"--delete-empty-src-dirs", "--delete-excluded",
                               "--update", "--no-traverse"}) {
        QVERIFY2(argv.contains(QLatin1String(flag)), flag);
      }
      QVERIFY(!argv.contains(QStringLiteral("--resync")));
      QVERIFY(!argv.contains(QStringLiteral("--workdir")));
      QVERIFY(!argv.contains(QStringLiteral("--conflict-resolve")));
    }
  }

  // T-JO-DIALOG: the TransferDialog shows and restores the Bisync options of a
  // saved task, hides them for the other modes and keeps its locked height.
  void T_JO_DIALOG() {
    auto dialogFor = [](JobOptions &task, bool download = false,
                        bool folder = true, bool edit = true,
                        bool multi = false) {
      return new TransferDialog(download, false, QStringLiteral("d:"),
                                QDir(QStringLiteral("/dst")), folder,
                                QStringLiteral("drive"),
                                QStringLiteral("shared"), multi,
                                multi ? QStringList{QStringLiteral("a.txt"),
                                                    QStringLiteral("photos")}
                                      : QStringList(),
                                nullptr, &task, edit);
    };

    // the height of the dialog of a Copy task is the base height
    JobOptions copyTask = makeCommon(JobOptions::Copy, JobOptions::Upload);
    copyTask.dryRun = false;
    std::unique_ptr<TransferDialog> copyDialog(dialogFor(copyTask));
    const int baseHeight = copyDialog->maximumHeight();
    QCOMPARE(copyDialog->minimumHeight(), baseHeight);
    QCOMPARE(copyDialog->getMode(), QStringLiteral("Copy"));
    QVERIFY(!copyDialog->findChild<QGroupBox *>("groupBisync")
                 ->isVisibleTo(copyDialog.get()));

    JobOptions task = makeCommon(JobOptions::Bisync, JobOptions::Upload);
    task.dryRun = false;
    task.sync = false;
    task.bisyncResync = true;
    task.bisyncConflictResolve = QStringLiteral("smaller");
    task.bisyncResilient = true;
    task.bisyncRecover = true;
    task.bisyncWorkdir = QStringLiteral("/some/work dir");
    const JobOptions want = task;
    const QStringList wantArgv = want.getOptions();
    QVERIFY(wantArgv.contains(QStringLiteral("--resync")));
    QVERIFY(wantArgv.contains(QStringLiteral("smaller")));

    std::unique_ptr<TransferDialog> d(dialogFor(task));
    auto *group = d->findChild<QGroupBox *>("groupBisync");
    auto *resync = d->findChild<QCheckBox *>("cbBisyncResync");
    auto *conflict = d->findChild<QComboBox *>("cbBisyncConflict");
    auto *resilient = d->findChild<QCheckBox *>("cbBisyncResilient");
    auto *recover = d->findChild<QCheckBox *>("cbBisyncRecover");
    auto *workdir = d->findChild<QLineEdit *>("leBisyncWorkdir");
    auto *rbCopy = d->findChild<QRadioButton *>("rbCopy");
    auto *rbBisync = d->findChild<QRadioButton *>("rbBisync");
    QVERIFY(group && resync && conflict && resilient && recover && workdir &&
            rbCopy && rbBisync);

    QVERIFY(rbBisync->isChecked());
    QVERIFY(group->isVisibleTo(d.get()));
    QVERIFY(resync->isChecked());
    QCOMPARE(conflict->currentIndex(), 4); // smaller
    QVERIFY(resilient->isChecked());
    QVERIFY(recover->isChecked());
    QCOMPARE(workdir->text(), QStringLiteral("/some/work dir"));
    QCOMPARE(d->getMode(), QStringLiteral("Bisync"));
    // the group is part of the locked height
    QVERIFY(d->maximumHeight() > baseHeight);
    QCOMPARE(d->minimumHeight(), d->maximumHeight());
    // what bisync ignores or rejects is disabled
    for (const char *name : {"deleteEmptySrcDirs", "checkDeleteExcluded",
                             "checkSkipNewer", "noTraverse"}) {
      QVERIFY2(!d->findChild<QCheckBox *>(name)->isEnabled(), name);
    }
    QCOMPARE(d->getOptions(), wantArgv);

    // another conflict value goes through the stored value, not the text
    conflict->setCurrentIndex(6);
    QVERIFY(d->getOptions().contains(QStringLiteral("path2")));
    conflict->setCurrentIndex(0);
    QVERIFY(!d->getOptions().contains(QStringLiteral("--conflict-resolve")));
    conflict->setCurrentIndex(4);

    // back to Copy: the group goes, the height returns, the controls too
    rbCopy->setChecked(true);
    QCOMPARE(d->getMode(), QStringLiteral("Copy"));
    QVERIFY(!group->isVisibleTo(d.get()));
    QCOMPARE(d->maximumHeight(), baseHeight);
    QCOMPARE(d->minimumHeight(), baseHeight);
    for (const char *name : {"deleteEmptySrcDirs", "checkDeleteExcluded",
                             "checkSkipNewer", "noTraverse"}) {
      QVERIFY2(d->findChild<QCheckBox *>(name)->isEnabled(), name);
    }
    QVERIFY(!d->getOptions().contains(QStringLiteral("--resync")));
    // and to Bisync again
    rbBisync->setChecked(true);
    QVERIFY(group->isVisibleTo(d.get()));
    QVERIFY(d->maximumHeight() > baseHeight);

    // a single downloaded file cannot be a Bisync task
    JobOptions fileTask = makeCommon(JobOptions::Copy, JobOptions::Download);
    std::unique_ptr<TransferDialog> fileDialog(
        dialogFor(fileTask, true, false, false));
    QVERIFY(!fileDialog->findChild<QRadioButton *>("rbBisync")->isEnabled());
    JobOptions folderTask = makeCommon(JobOptions::Copy, JobOptions::Download);
    std::unique_ptr<TransferDialog> folderDialog(
        dialogFor(folderTask, true, true, false));
    QVERIFY(folderDialog->findChild<QRadioButton *>("rbBisync")->isEnabled());
    // several rows: the source is their parent folder, whatever the first row is
    JobOptions multiTask = makeCommon(JobOptions::Copy, JobOptions::Download);
    std::unique_ptr<TransferDialog> multiDialog(
        dialogFor(multiTask, true, false, false, true));
    QVERIFY(multiDialog->findChild<QRadioButton *>("rbBisync")->isEnabled());
    // a saved multi-item task is reopened as a single item with its filter
    JobOptions savedMulti = makeCommon(JobOptions::Copy, JobOptions::Download);
    savedMulti.filtered = QStringLiteral("+ a.txt\n+ photos/**\n- *\n");
    std::unique_ptr<TransferDialog> savedMultiDialog(
        dialogFor(savedMulti, true, false, true));
    QVERIFY(
        savedMultiDialog->findChild<QRadioButton *>("rbBisync")->isEnabled());
    // a saved single-file Copy task stays without Bisync
    JobOptions savedFile = makeCommon(JobOptions::Copy, JobOptions::Download);
    savedFile.filtered.clear();
    std::unique_ptr<TransferDialog> savedFileDialog(
        dialogFor(savedFile, true, false, true));
    QVERIFY(
        !savedFileDialog->findChild<QRadioButton *>("rbBisync")->isEnabled());
  }

  // T-JO-NEWER: a tasks file from a newer version is refused.
  void T_JO_NEWER() {
    QByteArray bytes;
    {
      QDataStream out(&bytes, QIODevice::WriteOnly);
      out.setVersion(QDataStream::Qt_5_2);
      out << QStringLiteral("JobOptions") << qint32(99);
    }
    QDataStream in(bytes);
    in.setVersion(QDataStream::Qt_5_2);
    JobOptions jo;
    bool thrown = false;
    try {
      in >> jo;
    } catch (SerializationException &ex) {
      thrown = true;
      QCOMPARE(ex.Message, QStringLiteral("stored version is newer"));
    }
    QVERIFY(thrown);
  }
};

QTEST_MAIN(TestJobOptions)
#include "tst_job_options.moc"
