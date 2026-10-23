#include <QtNetwork>
#include <QtTest>

#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "serve_manager.h"
#include "utils.h"

// R15: ServeManager against a real rcd on 127.0.0.1 and an isolated temporary
// config with one local remote (I-9: never the host config). Every serve binds
// to 127.0.0.1.
class TestE2eServe : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;
  RcloneDaemon *mDaemon = nullptr;

  void startDaemon() {
    mDaemon = new RcloneDaemon;
    mDaemon->start();
    QTRY_VERIFY2_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Running,
                              qPrintable(mDaemon->lastError()), 10000);
  }

  RcCallFn callFn() {
    RcClient *client = mDaemon->client();
    return [this, client](const QString &path, const QJsonObject &params,
                          std::function<void(const RcReply &)> done,
                          int timeoutMs) {
      client->call(path, params, this, std::move(done), timeoutMs);
    };
  }

  QString tree() const { return mDir.filePath("w"); }

  ServeParams paramsFor(const QString &type) const {
    ServeParams p;
    p.type = type;
    p.fs = "loc:" + tree();
    return p;
  }

  static quint16 portOf(const QString &addr) {
    return static_cast<quint16>(addr.section(':', -1).toUInt());
  }

  // A QTcpSocket, not a reused QNetworkAccessManager: keep-alive could hide a
  // closed listener.
  static bool connectionRefused(quint16 port) {
    QTcpSocket s;
    s.connectToHost("127.0.0.1", port);
    if (s.waitForConnected(1000)) {
      s.abort();
      return false;
    }
    return s.error() == QAbstractSocket::ConnectionRefusedError;
  }

  // The newest list of `listed` has an entry with `id` (present) or none.
  static bool lastListHas(const QSignalSpy &listed, const QString &id) {
    if (listed.isEmpty()) {
      return false;
    }
    const QVector<ServeEntry> list =
        qvariant_cast<QVector<ServeEntry>>(listed.last().at(0));
    for (const ServeEntry &e : list) {
      if (e.id == id) {
        return true;
      }
    }
    return false;
  }

  // A list with no entry at all arrived after the first `from` lists.
  static bool emptyListAfter(const QSignalSpy &listed, int from) {
    for (int i = from; i < listed.count(); ++i) {
      if (qvariant_cast<QVector<ServeEntry>>(listed.at(i).at(0)).isEmpty()) {
        return true;
      }
    }
    return false;
  }

  static QNetworkReply *finishedReply(QNetworkReply *reply) {
    const bool done = QTest::qWaitFor([reply]() { return reply->isFinished(); },
                                      5000);
    return done ? reply : nullptr;
  }

  static ServeEntry entryOf(const QSignalSpy &started) {
    return qvariant_cast<ServeEntry>(started.at(0).at(1));
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<ServeEntry>();
    qRegisterMetaType<QVector<ServeEntry>>();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    // sftp writes its host keys into the rclone cache dir: keep it inside the
    // temporary dir, never the real user cache (inherited by rcd).
    qputenv("RCLONE_CACHE_DIR", mDir.filePath("cache").toUtf8());
    // and a HOME whose ~/.ssh/authorized_keys ends with a comment: rclone
    // 1.75.2 loops forever on it unless the sftp serve turns the file off
    const QString home = mDir.filePath("home");
    QVERIFY(QDir().mkpath(home + "/.ssh"));
    QFile keys(home + "/.ssh/authorized_keys");
    QVERIFY(keys.open(QIODevice::WriteOnly));
    keys.write("# no key here\n");
    keys.close();
    qputenv("HOME", home.toUtf8());
    // I-9: rcd uses this temporary config, never the host one.
    mConf = mDir.filePath("rclone.conf");
    QFile f(mConf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[loc]\ntype = local\n");
    f.close();
    SetRcloneConf(mConf);

    QVERIFY(QDir().mkpath(tree()));
    QFile file(QDir(tree()).filePath("a b.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hi");
    file.close();
  }

  void cleanup() {
    delete mDaemon;
    mDaemon = nullptr;
  }

  void cleanupTestCase() {
#ifdef Q_OS_UNIX
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
#endif
  }

  // T-E2E-SERVE-HTTP
  void T_E2E_SERVE_HTTP() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ServeManager manager(callFn());
    QSignalSpy started(&manager, &ServeManager::started);
    QSignalSpy listed(&manager, &ServeManager::listChanged);
    manager.start(paramsFor("http"));
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    const ServeEntry e = entryOf(started);
    QVERIFY(!e.id.isEmpty());
    QVERIFY(!e.addr.isEmpty());
    const quint16 port = portOf(e.addr);
    QVERIFY(port > 0);

    QNetworkAccessManager nam;
    nam.setProxy(QNetworkProxy::NoProxy);
    QNetworkReply *reply =
        nam.get(QNetworkRequest(QUrl(serveUrl(e) + "a%20b.txt")));
    QVERIFY(finishedReply(reply));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);
    QCOMPARE(reply->readAll(), QByteArray("hi"));
    reply->deleteLater();

    QTRY_VERIFY_WITH_TIMEOUT(lastListHas(listed, e.id), 5000);

    const int before = static_cast<int>(listed.count());
    manager.stop(e.id);
    QTRY_VERIFY_WITH_TIMEOUT(emptyListAfter(listed, before), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(connectionRefused(port), 5000);
  }

  // T-E2E-SERVE-RO
  void T_E2E_SERVE_RO() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ServeManager manager(callFn());
    QSignalSpy started(&manager, &ServeManager::started);
    ServeParams p = paramsFor("webdav");
    p.readOnly = true;
    manager.start(p);
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    const ServeEntry e = entryOf(started);

    QNetworkAccessManager nam;
    nam.setProxy(QNetworkProxy::NoProxy);
    QNetworkReply *reply =
        nam.put(QNetworkRequest(QUrl(serveUrl(e) + "new.txt")), QByteArray("x"));
    QVERIFY(finishedReply(reply));
    QVERIFY2(reply->error() != QNetworkReply::NoError,
             "the PUT to a read-only serve succeeded");
    reply->deleteLater();

    QVERIFY(!QFileInfo::exists(QDir(tree()).filePath("new.txt")));
    QFile original(QDir(tree()).filePath("a b.txt"));
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("hi"));
  }

  // T-E2E-SERVE-BADTYPE
  void T_E2E_SERVE_BADTYPE() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ServeManager manager(callFn());
    QSignalSpy started(&manager, &ServeManager::started);
    QSignalSpy errors(&manager, &ServeManager::error);
    manager.start(paramsFor("nosuch"));
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 5000);
    QVERIFY2(errors.at(0).at(0).toString().contains("could not find serve type"),
             qPrintable(errors.at(0).at(0).toString()));
    QCOMPARE(started.count(), 0);
  }

  // T-E2E-SERVE-QUIT
  void T_E2E_SERVE_QUIT() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    ServeManager manager(callFn());
    QSignalSpy started(&manager, &ServeManager::started);
    manager.start(paramsFor("http"));
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 5000);
    const quint16 port = portOf(entryOf(started).addr);
    QVERIFY(port > 0);
    QVERIFY(!connectionRefused(port));

    // core/quit ends the serves with the daemon
    mDaemon->stop();
    QTRY_VERIFY_WITH_TIMEOUT(mDaemon->state() == RcloneDaemon::State::Stopped,
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(connectionRefused(port), 5000);
  }

  // T-E2E-SERVE-TYPES
  void T_E2E_SERVE_TYPES() {
    startDaemon();
    if (QTest::currentTestFailed()) {
      return;
    }
    const QStringList types = {"http", "webdav", "ftp",  "sftp",
                               "restic", "s3",   "nfs",  "dlna"};
    QSet<QString> startedTypes;
    for (const QString &type : types) {
      const ServeTypeRules rules = serveTypeRules(type);
      ServeParams p = paramsFor(type);
      if (rules.needsPort) {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        p.addr = "127.0.0.1:" + QString::number(probe.serverPort());
        probe.close();
      }
      if (rules.loginRequired) {
        p.user = "u";
        p.pass = "p";
      }
      p.readOnly = rules.readOnlyAllowed;

      ServeManager manager(callFn());
      QSignalSpy started(&manager, &ServeManager::started);
      QSignalSpy errors(&manager, &ServeManager::error);
      QSignalSpy listed(&manager, &ServeManager::listChanged);
      manager.start(p);
      const bool environmental = type == "dlna" || type == "nfs";
      const int startWaitMs = environmental ? 10000 : 5000;
      QTRY_VERIFY_WITH_TIMEOUT(started.count() + errors.count() >= 1,
                               startWaitMs);
      if (errors.count() > 0) {
        const QString message = errors.at(0).at(0).toString();
        // only dlna and nfs may fail for an environment reason (no multicast,
        // no kernel support); every other type must start. A parameter error
        // is always a wrong rule.
        qWarning().noquote() << "serve type" << type << "did not start:"
                             << message;
        QVERIFY2(!message.contains("unknown parameters"),
                 qPrintable(type + ": " + message));
        QVERIFY2(!message.contains("no authorization found"),
                 qPrintable(type + ": " + message));
        QVERIFY2(environmental, qPrintable(type + ": " + message));
        continue;
      }
      startedTypes.insert(type);
      const ServeEntry e = entryOf(started);
      QVERIFY2(portOf(e.addr) > 0, qPrintable(type + ": " + e.addr));
      QTRY_VERIFY_WITH_TIMEOUT(lastListHas(listed, e.id), 5000);

      if (type == "s3") {
        QNetworkAccessManager nam;
        nam.setProxy(QNetworkProxy::NoProxy);
        QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(serveUrl(e))));
        QVERIFY(finishedReply(reply));
        QCOMPARE(
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
            200);
        reply->deleteLater();
      }

      const int before = static_cast<int>(listed.count());
      manager.stop(e.id);
      QTRY_VERIFY_WITH_TIMEOUT(emptyListAfter(listed, before), 5000);
    }
    for (const char *must :
         {"http", "webdav", "ftp", "sftp", "restic", "s3"}) {
      QVERIFY2(startedTypes.contains(QString::fromLatin1(must)),
               qPrintable(QString::fromLatin1(must) + " must start"));
    }
  }
};

QTEST_GUILESS_MAIN(TestE2eServe)
#include "tst_e2e_serve.moc"
