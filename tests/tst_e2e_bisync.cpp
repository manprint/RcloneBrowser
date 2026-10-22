#include <QtTest>

#include "job_options.h"
#include "rb_test_support.h"
#include "rclone_process.h"
#include "transfer_monitor.h"
#include "utils.h"

namespace {

bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(content) == content.size();
}

// A bisync task over the relative directories p1 and p2 (paths with their
// directory names only keep the listing file names short, see R14).
JobOptions makeBisync(bool resync) {
  JobOptions jo(false);
  jo.description = QStringLiteral("e2e bisync");
  jo.operation = JobOptions::Bisync;
  jo.transfers = QStringLiteral("4");
  jo.checkers = QStringLiteral("8");
  jo.connectTimeout = QStringLiteral("60");
  jo.idleTimeout = QStringLiteral("300");
  jo.retries = QStringLiteral("3");
  jo.lowLevelRetries = QStringLiteral("10");
  jo.source = QStringLiteral("p1");
  jo.dest = QStringLiteral("p2");
  jo.bisyncWorkdir = QStringLiteral("wd");
  jo.bisyncResync = resync;
  return jo;
}

QStringList names(const QString &dir) {
  QStringList out = QDir(dir).entryList(QDir::Files | QDir::NoDotAndDotDot);
  out.sort();
  return out;
}

} // namespace

class TestE2eBisync : public QObject {
  Q_OBJECT

  QTemporaryDir mTmp;

  struct Outcome {
    QStringList argv;
    int exitCode = -1;
    bool running = true;
    QByteArray output;
    QStringList messages; // the message of every log event
    TransferMonitor::Result result = TransferMonitor::Result::Running;
    QString lastError;
    QStringList recentErrors;
  };

  // Runs the job's argv in the temp dir with the real rclone, the way a
  // transfer runs, and feeds every line to a TransferMonitor.
  Outcome run(const JobOptions &jo) {
    Outcome o;
    o.argv = jo.getOptions();
    const TransferArgs ta = PrepareTransferArgs(
        o.argv, RcloneEnvironment(RcloneEnvMode::Parsed));
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    p.setWorkingDirectory(mTmp.path());
    p.start(rbRclone(), ta.args);
    if (!p.waitForFinished(60000)) {
      p.kill();
      p.waitForFinished(5000);
    }
    o.running = p.state() != QProcess::NotRunning;
    o.output = p.readAll();

    TransferMonitor monitor;
    QObject::connect(&monitor, &TransferMonitor::logEvent, &monitor,
                     [&o](const RcloneLogEvent &e) { o.messages << e.msg; });
    for (const QByteArray &line : o.output.split('\n')) {
      monitor.consumeLine(line);
    }
    o.exitCode = p.exitCode();
    monitor.finish(p.exitCode(), p.exitStatus(), false);
    o.result = monitor.result();
    o.lastError = monitor.lastError();
    o.recentErrors = monitor.recentErrors();
    return o;
  }

  QString path(const QString &rel) const { return mTmp.filePath(rel); }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mTmp.isValid());
    QVERIFY(writeFile(path(QStringLiteral("p1/a.txt")), "a"));
    QVERIFY(writeFile(path(QStringLiteral("p2/b.txt")), "b"));
    QVERIFY(QDir().mkpath(path(QStringLiteral("wd"))));
  }

  // T-E2E-BISYNC-NEEDS-RESYNC: a first run without --resync is an error that
  // names the remedy (R14).
  void T_E2E_BISYNC_NEEDS_RESYNC() {
    const Outcome o = run(makeBisync(false));
    QVERIFY(!o.running);
    QCOMPARE(o.argv.mid(0, 3), (QStringList{QStringLiteral("bisync"),
                                            QStringLiteral("p1"),
                                            QStringLiteral("p2")}));
    QVERIFY(!o.argv.contains(QStringLiteral("--resync")));
    QCOMPARE(o.result, TransferMonitor::Result::Error);
    QCOMPARE(o.exitCode, 7);
    const QString errors = o.lastError + QLatin1Char('\n') +
                           o.recentErrors.join(QLatin1Char('\n'));
    QVERIFY2(errors.contains(QStringLiteral("Must run --resync")),
             qPrintable(errors));
    // nothing was copied
    QCOMPARE(names(path(QStringLiteral("p1"))), (QStringList{"a.txt"}));
    QCOMPARE(names(path(QStringLiteral("p2"))), (QStringList{"b.txt"}));
  }

  // T-E2E-BISYNC-RESYNC: with --resync both sides get the files of the other.
  void T_E2E_BISYNC_RESYNC() {
    const Outcome o = run(makeBisync(true));
    QVERIFY(!o.running);
    QVERIFY(o.argv.contains(QStringLiteral("--resync")));
    QVERIFY2(o.result == TransferMonitor::Result::Finished,
             o.output.constData());
    QCOMPARE(o.exitCode, 0);
    const QStringList both = {QStringLiteral("a.txt"), QStringLiteral("b.txt")};
    QCOMPARE(names(path(QStringLiteral("p1"))), both);
    QCOMPARE(names(path(QStringLiteral("p2"))), both);
    // --color NEVER keeps the escape sequences out of the messages (P1)
    QVERIFY(!o.output.contains('\x1b'));
    for (const QString &m : o.messages) {
      QVERIFY2(!m.contains(QChar(0x1b)), qPrintable(m));
    }
  }

  // T-E2E-BISYNC-UPDATE: after the resync a new and a changed file go across
  // without it.
  void T_E2E_BISYNC_UPDATE() {
    QVERIFY(writeFile(path(QStringLiteral("p1/c.txt")), "c"));
    QVERIFY(writeFile(path(QStringLiteral("p1/a.txt")), "a2"));
    // The flags the dialog keeps out of a Bisync task: with --delete-excluded
    // rclone 1.75.2 deletes p2/a.txt and p2/b.txt (outside the delta queue),
    // and with --ignore-existing the changed a.txt stays behind and the run
    // stops out of sync (exit 7).
    JobOptions jo = makeBisync(false);
    jo.deleteExcluded = true;
    jo.skipNewer = true;
    jo.noTraverse = true;
    jo.skipExisting = true;
    jo.dontUpdateModified = true;
    const Outcome o = run(jo);
    QVERIFY(!o.running);
    QVERIFY(!o.argv.contains(QStringLiteral("--resync")));
    for (const char *flag :
         {"--delete-excluded", "--update", "--no-traverse", "--ignore-existing",
          "--no-update-modtime"}) {
      QVERIFY2(!o.argv.contains(QLatin1String(flag)), flag);
    }
    QVERIFY2(o.result == TransferMonitor::Result::Finished,
             o.output.constData());
    QCOMPARE(o.exitCode, 0);
    QVERIFY(QFileInfo::exists(path(QStringLiteral("p2/c.txt"))));
    QCOMPARE(names(path(QStringLiteral("p2"))),
             (QStringList{"a.txt", "b.txt", "c.txt"}));
    QFile changed(path(QStringLiteral("p2/a.txt")));
    QVERIFY(changed.open(QIODevice::ReadOnly));
    QCOMPARE(changed.readAll(), QByteArray("a2"));
  }
};

QTEST_GUILESS_MAIN(TestE2eBisync)
#include "tst_e2e_bisync.moc"
