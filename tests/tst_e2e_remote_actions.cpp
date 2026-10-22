#include <QtTest>

#include "rb_test_support.h"
#include "rclone_log.h"
#include "rclone_process.h"
#include "remote_actions.h"
#include "transfer_monitor.h"

namespace {

bool writeFile(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

QByteArray readFile(const QString &path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

// #199: Copy to (copyto) and Upload from URL (copyurl) as the app runs them:
// the argv goes through PrepareTransferArgs, the output (stdout and stderr
// merged, as addTransfer does) through a TransferMonitor. The URL is served by
// `rclone serve http` on 127.0.0.1. Every test seeds its own files and starts
// its own server; cleanup() kills a leftover server and cleanupTestCase()
// checks that none remains.
class TestE2eRemoteActions : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;
  QProcess *mServer = nullptr;
  QByteArray mServerLog; // stderr of the server until it reported its URL

  struct Outcome {
    int exitCode = -1;
    bool running = true;
    TransferMonitor::Result result = TransferMonitor::Result::Running;
    QString lastError;
    QList<RcloneLogEvent> logs;
    QList<RcloneLogEvent> texts;
  };

  // One rclone run on the empty temporary config (I-9).
  Outcome run(const QStringList &userArgs) const {
    Outcome o;
    const TransferArgs ta = PrepareTransferArgs(
        userArgs + QStringList{"--config", mConf},
        RcloneEnvironment(RcloneEnvMode::Parsed));
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    p.start(rbRclone(), ta.args);
    p.closeWriteChannel();
    if (!p.waitForFinished(60000)) {
      p.kill();
      p.waitForFinished(5000);
    }
    o.running = p.state() != QProcess::NotRunning;
    const QByteArray output = p.readAll();

    TransferMonitor monitor;
    QObject::connect(&monitor, &TransferMonitor::logEvent, &monitor,
                     [&o](const RcloneLogEvent &e) {
                       if (e.kind == RcloneLogEvent::Kind::Text) {
                         o.texts << e;
                       } else if (e.kind == RcloneLogEvent::Kind::Log) {
                         o.logs << e;
                       }
                     });
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &line : lines) {
      monitor.consumeLine(line);
    }
    o.exitCode = p.exitCode();
    monitor.finish(p.exitCode(), p.exitStatus(), false);
    o.result = monitor.result();
    o.lastError = monitor.lastError();
    return o;
  }

  // Starts `rclone serve http` on <tmp>/web and returns its URL (with the
  // final '/'), read from the "HTTP Server started on [...]" event.
  QString startServer() {
    const QString web = QDir(mDir.path()).filePath("web");
    mServerLog.clear();
    mServer = new QProcess;
    mServer->setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    mServer->start(rbRclone(),
                   {"serve", "http", web, "--addr", "127.0.0.1:0",
                    "--use-json-log", "--config", mConf},
                   QIODevice::ReadOnly);
    if (!mServer->waitForStarted(5000)) {
      return QString();
    }
    const QRegularExpression re(R"(http://127\.0\.0\.1:\d+/)");
    QByteArray pending;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 15000) {
      mServer->waitForReadyRead(200);
      const QByteArray chunk = mServer->readAllStandardError();
      mServerLog += chunk;
      pending += chunk;
      int nl;
      while ((nl = pending.indexOf('\n')) >= 0) {
        const QByteArray line = pending.left(nl);
        pending.remove(0, nl + 1);
        const QRegularExpressionMatch m =
            re.match(parseRcloneLogLine(line).msg);
        if (m.hasMatch()) {
          return m.captured(0);
        }
      }
      // a server that exited will never report a URL
      if (mServer->state() == QProcess::NotRunning) {
        break;
      }
    }
    return QString();
  }

  void stopServer() {
    if (!mServer) {
      return;
    }
    if (mServer->state() != QProcess::NotRunning) {
      mServer->kill();
      mServer->waitForFinished(5000);
    }
    delete mServer;
    mServer = nullptr;
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    // I-9: an empty temporary config, never the host one
    mConf = QDir(mDir.path()).filePath("empty.conf");
    QVERIFY(writeFile(mConf, QByteArray()));
  }

  void cleanup() { stopServer(); }

  void cleanupTestCase() {
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "serve http"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
  }

  // T-E2E-COPYTO: a file copied to another name in another folder.
  void T_E2E_COPYTO() {
    const QString root = mDir.path();
    const QString src = root + "/p/a.txt";
    const QString dst = root + "/out/renamed a.txt";
    QVERIFY(writeFile(src, "copyto content"));

    const Outcome o = run(buildCopytoArgs(src, dst));
    QVERIFY(!o.running);
    QCOMPARE(o.result, TransferMonitor::Result::Finished);
    QVERIFY(QFileInfo::exists(dst));
    QCOMPARE(readFile(dst), QByteArray("copyto content"));
    QCOMPARE(readFile(src), QByteArray("copyto content"));

    QStringList copied;
    for (const RcloneLogEvent &e : o.logs) {
      if (e.msg.startsWith("Copied")) {
        QCOMPARE(e.object, QString("a.txt"));
        copied << e.msg;
      }
    }
    QCOMPARE(copied, QStringList{"Copied (new) to: renamed a.txt"});
  }

  // T-E2E-COPYURL: the auto form names the file from the URL and prints the
  // name (one Text event); the named form uses the given name.
  void T_E2E_COPYURL() {
    const QString root = mDir.path();
    QVERIFY(writeFile(root + "/web/f 1.txt", "hello"));
    const QString url = startServer();
    QVERIFY2(!url.isEmpty(),
             qPrintable("serve http did not report its URL:\n" + mServerLog));
    const QString dl = root + "/dl";

    const Outcome o = run(buildCopyurlArgs(url + "f%201.txt", dl, "", false));
    QVERIFY(!o.running);
    QCOMPARE(o.result, TransferMonitor::Result::Finished);
    QVERIFY(QFileInfo::exists(dl + "/f 1.txt"));
    QCOMPARE(readFile(dl + "/f 1.txt"), QByteArray("hello"));
    QCOMPARE(o.texts.size(), 1);
    QCOMPARE(o.texts.front().msg, QString("f 1.txt"));

    const Outcome named =
        run(buildCopyurlArgs(url + "f%201.txt", dl, "named.txt", false));
    QVERIFY(!named.running);
    QCOMPARE(named.result, TransferMonitor::Result::Finished);
    QVERIFY(QFileInfo::exists(dl + "/named.txt"));
    QCOMPARE(readFile(dl + "/named.txt"), QByteArray("hello"));

    stopServer();
  }

  // T-E2E-COPYURL-NOCLOBBER: --no-clobber leaves an existing file alone. The
  // exit code is recorded, not asserted; the last error must name the existing
  // file as the reason, so a run that failed for another reason (an unknown
  // flag, a dead server) does not pass.
  void T_E2E_COPYURL_NOCLOBBER() {
    const QString root = mDir.path();
    QVERIFY(writeFile(root + "/web/f 1.txt", "hello"));
    const QString url = startServer();
    QVERIFY2(!url.isEmpty(),
             qPrintable("serve http did not report its URL:\n" + mServerLog));
    const QString dl = root + "/dlnc";

    const Outcome first =
        run(buildCopyurlArgs(url + "f%201.txt", dl, "", false));
    QCOMPARE(first.result, TransferMonitor::Result::Finished);
    QCOMPARE(readFile(dl + "/f 1.txt"), QByteArray("hello"));

    QVERIFY(writeFile(dl + "/f 1.txt", "local"));
    const Outcome o = run(buildCopyurlArgs(url + "f%201.txt", dl, "", true));
    QVERIFY(!o.running);
    QCOMPARE(readFile(dl + "/f 1.txt"), QByteArray("local"));
    qInfo().noquote() << "no-clobber exit code:" << o.exitCode;
    qInfo().noquote() << "no-clobber lastError:" << o.lastError;
    // rclone 1.75.2 operations.CopyURL: "CopyURL failed: file already exist"
    QVERIFY2(o.lastError.contains("file already exist"),
             qPrintable(o.lastError));

    stopServer();
  }
};

QTEST_GUILESS_MAIN(TestE2eRemoteActions)
#include "tst_e2e_remote_actions.moc"
