#include <QtNetwork>
#include <QtTest>

#include "mount_launch.h"
#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_log.h"
#include "rclone_process.h"

// R10: `rclone serve webdav` has the same VFS layer and rc as `rclone mount`,
// and needs no FUSE, so it stands in for a mount. Every test stops what it
// starts; cleanup() kills leftovers and cleanupTestCase() checks that no
// server remains.
class TestE2eVfs : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QProcess *mServer = nullptr;

  static QByteArray readProc(qint64 pid, const QString &name) {
    QFile f(QString("/proc/%1/%2").arg(pid).arg(name));
    if (!f.open(QIODevice::ReadOnly)) {
      return QByteArray();
    }
    return f.readAll();
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
  }

  void cleanup() {
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

  void cleanupTestCase() {
    int exitCode = -1;
    for (int i = 0; i < 20 && exitCode != 1; ++i) {
      QProcess p;
      p.start("pgrep", {"-f", "serve webdav"});
      QVERIFY(p.waitForFinished(5000));
      exitCode = p.exitCode();
      if (exitCode != 1) {
        QTest::qWait(100);
      }
    }
    QCOMPARE(exitCode, 1);
  }

  // T-E2E-VFS-REFRESH (R10)
  void T_E2E_VFS_REFRESH() {
    QDir root(mDir.path());
    QVERIFY(root.mkpath("data"));
    QFile file(root.filePath("data/a.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    // I-9: an empty temporary config, never the host one.
    const QString conf = root.filePath("rclone.conf");
    QFile confFile(conf);
    QVERIFY(confFile.open(QIODevice::WriteOnly));
    confFile.close();

    const RcCredentials creds = generateRcCredentials();
    const MountLaunch ml = buildMountLaunch(
        {"serve", "webdav", root.filePath("data"), "--addr", "127.0.0.1:0",
         "--rc", "--rc-addr", "127.0.0.1:0", "--rc-user=stale",
         "--rc-pass", "stale", "--use-json-log", "--config", conf},
        RcloneEnvironment(RcloneEnvMode::Parsed), creds);
    QVERIFY(ml.rcEnabled);
    QVERIFY(!ml.args.join(' ').contains("stale"));

    mServer = new QProcess;
    mServer->setProcessEnvironment(ml.env);
    mServer->start(rbRclone(), ml.args, QIODevice::ReadOnly);
    QVERIFY(mServer->waitForStarted(5000));
    const qint64 pid = mServer->processId();
    QVERIFY(pid > 0);

    // the rc port comes from the "Serving remote control on" event
    int port = 0;
    QByteArray pending;
    QElapsedTimer timer;
    timer.start();
    while (port == 0 && timer.elapsed() < 15000) {
      mServer->waitForReadyRead(200);
      pending += mServer->readAllStandardError();
      int nl;
      while (port == 0 && (nl = pending.indexOf('\n')) >= 0) {
        const QByteArray line = pending.left(nl);
        pending.remove(0, nl + 1);
        port = parseRcServingPort(parseRcloneLogLine(line).msg).value_or(0);
      }
    }
    QVERIFY2(port > 0, "rc port not reported");

    RcClient client;
    client.setEndpoint(QUrl(QString("http://127.0.0.1:%1/").arg(port)), creds);

    // rclone's vfs/refresh rejects a JSON boolean for "recursive"
    bool done = false;
    RcReply reply;
    client.call("vfs/refresh", QJsonObject{{"recursive", "true"}}, this,
                [&](const RcReply &r) {
                  reply = r;
                  done = true;
                });
    QVERIFY(!done); // never synchronous
    QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
    QVERIFY2(reply.ok, qPrintable(reply.error));
    QVERIFY(reply.result.value("result").isObject());

    // I-5: no credential in the argv
    const QByteArray cmdline = readProc(pid, "cmdline");
    QVERIFY(!cmdline.isEmpty());
    QVERIFY(cmdline.contains("webdav"));
    QVERIFY(!cmdline.contains(creds.user.toUtf8()));
    QVERIFY(!cmdline.contains(creds.pass.toUtf8()));

    // the credentials reach the server through the environment
    const QList<QByteArray> environ = readProc(pid, "environ").split('\0');
    QVERIFY(environ.contains("RCLONE_RC_USER=" + creds.user.toUtf8()));
    QVERIFY(environ.contains("RCLONE_RC_PASS=" + creds.pass.toUtf8()));

    // the unmount of Windows: core/quit ends the process
    done = false;
    client.call("core/quit", QJsonObject(), this, [&](const RcReply &r) {
      reply = r;
      done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
    QVERIFY2(reply.ok, qPrintable(reply.error));
    QTRY_VERIFY_WITH_TIMEOUT(mServer->state() == QProcess::NotRunning, 5000);
  }
};

QTEST_GUILESS_MAIN(TestE2eVfs)
#include "tst_e2e_vfs.moc"
