#include <QtNetwork>
#include <QtTest>

#include <signal.h>

#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "utils.h"

// R4: the private rcd of phase 5. Every test stops what it starts; cleanup()
// kills leftovers and cleanupTestCase() checks that no rcd remains.
class TestE2eDaemon : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  RcloneDaemon *mDaemon = nullptr;

  // Creates mDaemon and waits until rcd serves.
  void startDaemon() {
    mDaemon = new RcloneDaemon;
    mDaemon->start();
    QTRY_VERIFY2_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                              qPrintable(mDaemon->lastError()), 10000);
  }

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
    qRegisterMetaType<RcloneDaemon::State>();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    // I-9: rcd uses this temporary config, never the host one.
    const QString conf = mDir.filePath("rclone.conf");
    QFile f(conf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[loc]\ntype = local\n");
    f.close();
    SetRcloneConf(conf);
  }

  void cleanup() {
    if (!mDaemon) {
      return;
    }
    const qint64 pid = mDaemon->processId();
    if (pid > 0) {
      ::kill(static_cast<pid_t>(pid), SIGCONT); // a stopped rcd ignores SIGTERM
    }
    delete mDaemon; // terminate(), then kill()
    mDaemon = nullptr;
    SetRclone(rbRclone());
    qunsetenv("RCLONE_RC_NO_AUTH");
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

  // T-E2E-DAEMON-START
  void T_E2E_DAEMON_START() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    QVERIFY(mDaemon->port() > 0);
    QVERIFY(mDaemon->client()->hasEndpoint());
    QVERIFY(mDaemon->lastError().isEmpty());

    bool done = false;
    RcReply reply;
    mDaemon->client()->call("core/version", QJsonObject(), this,
                            [&](const RcReply &r) {
                              reply = r;
                              done = true;
                            });
    QVERIFY(!done); // never synchronous
    QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
    QVERIFY2(reply.ok, qPrintable(reply.error));
    QCOMPARE(reply.httpStatus, 200);
    const QJsonArray decomposed = reply.result.value("decomposed").toArray();
    QVERIFY(decomposed.size() >= 2);
    QCOMPARE(decomposed.at(0).toInt(), 1);
    QVERIFY(decomposed.at(1).toInt() >= 75);

    // an rc error reaches the callback as an error reply
    done = false;
    mDaemon->client()->call("nope/nothing", QJsonObject(), this,
                            [&](const RcReply &r) {
                              reply = r;
                              done = true;
                            });
    QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
    QVERIFY(!reply.ok);
    QVERIFY(reply.httpStatus >= 400);
    QVERIFY(!reply.error.isEmpty());
  }

  // T-DAEMON-ARGV (I-5)
  void T_DAEMON_ARGV() {
    // not inherited by rcd
    qputenv("RCLONE_RC_NO_AUTH", "true");
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const qint64 pid = mDaemon->processId();
    QVERIFY(pid > 0);
    const RcCredentials c = mDaemon->credentials();
    QVERIFY(!c.user.isEmpty());
    QVERIFY(!c.pass.isEmpty());

    const QByteArray cmdline = readProc(pid, "cmdline");
    QVERIFY(!cmdline.isEmpty());
    const QList<QByteArray> argv = cmdline.split('\0');
    QVERIFY(argv.contains("rcd"));
    QVERIFY(argv.contains("--rc-addr"));
    QVERIFY(argv.contains("127.0.0.1:0"));
    QVERIFY(!cmdline.contains(c.user.toUtf8()));
    QVERIFY(!cmdline.contains(c.pass.toUtf8()));

    const QByteArray environ = readProc(pid, "environ");
    QVERIFY(!environ.isEmpty());
    const QList<QByteArray> vars = environ.split('\0');
    QVERIFY(vars.contains("RCLONE_RC_USER=" + c.user.toUtf8()));
    QVERIFY(vars.contains("RCLONE_RC_PASS=" + c.pass.toUtf8()));
    for (const QByteArray &v : vars) {
      QVERIFY2(!v.startsWith("RCLONE_RC_NO_AUTH"), v.constData());
    }
  }

  // T-E2E-DAEMON-AUTH
  void T_E2E_DAEMON_AUTH() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const RcCredentials c = mDaemon->credentials();

    QNetworkAccessManager nam;
    nam.setProxy(QNetworkProxy::NoProxy);
    const QUrl url(QString("http://127.0.0.1:%1/core/version").arg(mDaemon->port()));

    auto post = [&](const QByteArray &authorization) {
      QNetworkRequest request(url);
      request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
      request.setTransferTimeout(10000);
      if (!authorization.isEmpty()) {
        request.setRawHeader("Authorization", authorization);
      }
      QNetworkReply *reply = nam.post(request, QByteArray("{}"));
      QSignalSpy spy(reply, &QNetworkReply::finished);
      const bool finished = spy.wait(10000) || spy.count() > 0;
      RcReply r;
      if (finished) {
        r = parseRcReply(
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
            reply->readAll(),
            reply->error() != QNetworkReply::NoError ? reply->errorString()
                                                     : QString());
      }
      delete reply;
      return qMakePair(finished, r);
    };

    const auto wrong =
        post("Basic " + (c.user + ":wrong-password").toUtf8().toBase64());
    QVERIFY(wrong.first);
    QCOMPARE(wrong.second.httpStatus, 401);
    QVERIFY(!wrong.second.ok);

    const auto none = post(QByteArray());
    QVERIFY(none.first);
    QCOMPARE(none.second.httpStatus, 401);
    QVERIFY(!none.second.ok);

    const auto good =
        post("Basic " + (c.user + ":" + c.pass).toUtf8().toBase64());
    QVERIFY(good.first);
    QVERIFY2(good.second.ok, qPrintable(good.second.error));
  }

  // T-E2E-DAEMON-QUIT
  void T_E2E_DAEMON_QUIT() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const qint64 pid = mDaemon->processId();
    QVERIFY(pid > 0);
    QSignalSpy failedSpy(mDaemon, &RcloneDaemon::failed);
    QSignalSpy stateSpy(mDaemon, &RcloneDaemon::stateChanged);

    mDaemon->stop();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Stopped,
                             3000);
    QVERIFY(!QFileInfo::exists(QString("/proc/%1").arg(pid)));
    QVERIFY(!mDaemon->lastStopWasKill());
    QCOMPARE(mDaemon->port(), 0);
    QVERIFY(!mDaemon->client()->hasEndpoint());
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RcloneDaemon::State>(),
             RcloneDaemon::State::Stopped);

    // stop() on a stopped daemon is a no-op, start() works again
    mDaemon->stop();
    QCOMPARE(stateSpy.count(), 1);
    mDaemon->start();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                             10000);
  }

  // T-E2E-DAEMON-KILL
  void T_E2E_DAEMON_KILL() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const qint64 pid = mDaemon->processId();
    QVERIFY(pid > 0);
    QCOMPARE(::kill(static_cast<pid_t>(pid), SIGSTOP), 0);

    // a stopped rcd never answers: the transfer timeout ends the call
    bool done = false;
    RcReply reply;
    mDaemon->client()->call("core/version", QJsonObject(), this,
                            [&](const RcReply &r) {
                              reply = r;
                              done = true;
                            },
                            1000);
    QTRY_VERIFY_WITH_TIMEOUT(done, 4000);
    QVERIFY(!reply.ok);
    QCOMPARE(reply.httpStatus, 0);
    QVERIFY(!reply.error.isEmpty());

    mDaemon->stop();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Stopped,
                             5000);
    QVERIFY(mDaemon->lastStopWasKill());
    QVERIFY(!QFileInfo::exists(QString("/proc/%1").arg(pid)));
  }

  // T-E2E-DAEMON-FAIL
  void T_E2E_DAEMON_FAIL() {
    SetRclone("/bin/false");
    mDaemon = new RcloneDaemon;
    QSignalSpy failedSpy(mDaemon, &RcloneDaemon::failed);
    mDaemon->start();
    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() == 1, 5000);
    QVERIFY(mDaemon->state() == RcloneDaemon::State::Failed);
    QVERIFY(!mDaemon->lastError().isEmpty());
    QCOMPARE(failedSpy.at(0).at(0).toString(), mDaemon->lastError());
    QCOMPARE(mDaemon->port(), 0);
    QVERIFY(!mDaemon->client()->hasEndpoint());

    // Failed -> start() with a working rclone
    SetRclone(rbRclone());
    const RcCredentials before = mDaemon->credentials();
    mDaemon->start();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                             10000);
    QVERIFY(mDaemon->credentials().pass != before.pass);
  }
};

QTEST_GUILESS_MAIN(TestE2eDaemon)
#include "tst_e2e_daemon.moc"
