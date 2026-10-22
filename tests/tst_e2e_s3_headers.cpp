#include <QtTest>

#include "job_options.h"
#include "rb_test_support.h"
#include "rclone_log.h"
#include "rclone_process.h"

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

// The headers of the request that starts with `prefix` in a `--dump headers`
// log (up to and including its last header line), or nothing. The same tag
// string also appears in other places of the log (the "starting with
// parameters" line, the HEAD response), so the check is scoped to this block.
QByteArray requestBlock(const QByteArray &err, const QByteArray &prefix) {
  const qsizetype i = err.indexOf(prefix);
  if (i < 0) {
    return {};
  }
  const qsizetype end = err.indexOf("\r\n\r\n", i);
  return end < 0 ? QByteArray() : err.mid(i, end + 2 - i);
}

} // namespace

// 13.1: --header-upload as the app builds it. `rclone serve s3` on 127.0.0.1
// (anonymous, no keys) takes the upload, and `--dump headers` on the client
// shows the request that the S3 backend sent. Every run uses a temporary
// config (I-9); cleanup() kills a leftover server and cleanupTestCase() checks
// that none remains.
class TestE2eS3Headers : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mEmptyConf, mConf;
  QProcess *mServer = nullptr;
  QByteArray mServerLog; // stderr of the server until it reported its URL

  // Starts `rclone serve s3` on <tmp>/store and returns its port, or 0.
  int startServer() {
    const QString store = QDir(mDir.path()).filePath("store");
    mServerLog.clear();
    mServer = new QProcess;
    mServer->setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    mServer->start(rbRclone(),
                   {"serve", "s3", store, "--addr", "127.0.0.1:0",
                    "--use-json-log", "--config", mEmptyConf},
                   QIODevice::ReadOnly);
    if (!mServer->waitForStarted(5000)) {
      return 0;
    }
    const QRegularExpression re(R"(http://127\.0\.0\.1:(\d+)/)");
    QByteArray pending;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 15000) {
      mServer->waitForReadyRead(200);
      const QByteArray chunk = mServer->readAllStandardError();
      mServerLog += chunk;
      pending += chunk;
      qsizetype nl;
      while ((nl = pending.indexOf('\n')) >= 0) {
        const QByteArray line = pending.left(nl);
        pending.remove(0, nl + 1);
        const QRegularExpressionMatch m =
            re.match(parseRcloneLogLine(line).msg);
        if (m.hasMatch()) {
          return m.captured(1).toInt();
        }
      }
      // a server that exited will never report a port
      if (mServer->state() == QProcess::NotRunning) {
        break;
      }
    }
    return 0;
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

  // An upload task as the dialog saves it, with the values of the dialog
  // defaults (empty ones make rclone fail).
  JobOptions makeUpload(const QString &type, const QString &dest) const {
    JobOptions jo(false);
    jo.description = QStringLiteral("e2e s3 headers");
    jo.operation = JobOptions::Copy;
    jo.transfers = QStringLiteral("4");
    jo.checkers = QStringLiteral("8");
    jo.connectTimeout = QStringLiteral("60");
    jo.idleTimeout = QStringLiteral("300");
    jo.retries = QStringLiteral("3");
    jo.lowLevelRetries = QStringLiteral("10");
    jo.source = QDir(mDir.path()).filePath("src");
    jo.dest = dest;
    jo.remoteType = type;
    jo.remoteMode = QStringLiteral("main");
    jo.uploadHeaders = QStringLiteral("Cache-Control: no-cache");
    jo.s3Tags = QStringLiteral("proj=rb\nenv=test");
    return jo;
  }

  // Runs the argv of the task as it is, with --dump headers; true if rclone
  // exited normally with code 0. `err` is its stderr.
  bool upload(const JobOptions &jo, QByteArray *err) const {
    QProcess p;
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.setProcessEnvironment(RcloneEnvironment(RcloneEnvMode::Parsed));
    p.start(rbRclone(), jo.getOptions() + QStringList{"--dump", "headers",
                                                       "--config", mConf});
    p.closeWriteChannel();
    if (!p.waitForFinished(60000)) {
      p.kill();
      p.waitForFinished(5000);
    }
    *err = p.readAllStandardError();
    return p.state() == QProcess::NotRunning &&
           p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    // I-9: temporary configs, never the host one
    mEmptyConf = QDir(mDir.path()).filePath("empty.conf");
    mConf = QDir(mDir.path()).filePath("s3.conf");
    QVERIFY(writeFile(mEmptyConf, QByteArray()));
    QVERIFY(QDir().mkpath(mDir.path() + "/store/bkt"));
    QVERIFY(QDir().mkpath(mDir.path() + "/store/bkt2"));
    QVERIFY(writeFile(mDir.path() + "/src/t 1.txt", "hello"));
  }

  void cleanup() { stopServer(); }

  void cleanupTestCase() {
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "serve s3"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
  }

  // T-E2E-S3-TAGS: an upload to an S3 remote sends the headers and the
  // X-Amz-Tagging tags; the same task to another remote type, which only
  // differs in its type, sends the headers and no tags.
  void T_E2E_S3_TAGS() {
    const int port = startServer();
    QVERIFY2(port > 0, mServerLog.constData());
    QVERIFY(writeFile(
        mConf,
        QStringLiteral("[s3t]\ntype = s3\nprovider = Rclone\n"
                       "endpoint = http://127.0.0.1:%1\n"
                       "force_path_style = true\n")
            .arg(port)
            .toUtf8()));

    QByteArray err;
    const bool ok = upload(makeUpload("s3", "s3t:bkt"), &err);
    QVERIFY2(ok, err.constData());
    QCOMPARE(readFile(mDir.path() + "/store/bkt/t 1.txt"), QByteArray("hello"));
    // the full request line has a query in rclone 1.75.2 (?x-id=PutObject)
    const QByteArray block = requestBlock(err, "PUT /bkt/t%201.txt");
    QVERIFY2(!block.isEmpty(), err.constData());
    QVERIFY2(block.contains("\r\nX-Amz-Tagging: proj=rb&env=test\r\n"),
             block.constData());
    QVERIFY2(block.contains("\r\nCache-Control: no-cache\r\n"),
             block.constData());

    // negative control: not an S3 remote type, so no tags (the headers stay)
    QByteArray err2;
    const bool ok2 = upload(makeUpload("drive", "s3t:bkt2"), &err2);
    QVERIFY2(ok2, err2.constData());
    QVERIFY(QFileInfo::exists(mDir.path() + "/store/bkt2/t 1.txt"));
    const QByteArray block2 = requestBlock(err2, "PUT /bkt2/t%201.txt");
    QVERIFY2(!block2.isEmpty(), err2.constData());
    QVERIFY2(block2.contains("\r\nCache-Control: no-cache\r\n"),
             block2.constData());
    QVERIFY2(!block2.contains("X-Amz-Tagging"), block2.constData());

    stopServer();
  }
};

QTEST_GUILESS_MAIN(TestE2eS3Headers)
#include "tst_e2e_s3_headers.moc"
