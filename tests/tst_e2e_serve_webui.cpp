#include <QtNetwork>
#include <QtTest>

#include <memory>

#include "rb_test_support.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "serve_manager.h"
#include "utils.h"

namespace {

// The marker of the fork's web UI page: the prefix of its asset URLs.
const QByteArray kMarker("__webui_");

struct HttpReply {
  int status = 0; // 0: no reply in 10 s
  QByteArray body;
};

// GET `url` as a client with User-Agent `ua` that accepts HTML (a browser gets
// the web UI, a curl-like client the classic listing).
HttpReply httpGet(const QString &url, const QByteArray &ua) {
  QNetworkAccessManager nam;
  nam.setProxy(QNetworkProxy::NoProxy);
  QNetworkRequest request{QUrl(url)};
  request.setRawHeader("Accept", "text/html");
  request.setRawHeader("User-Agent", ua);
  QNetworkReply *reply = nam.get(request);
  const bool done =
      QTest::qWaitFor([reply]() { return reply->isFinished(); }, 10000);
  HttpReply out;
  if (done) {
    out.status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    out.body = reply->readAll();
  }
  reply->abort();
  reply->deleteLater();
  return out;
}

const QByteArray kBrowser("Mozilla/5.0");
const QByteArray kCurl("curl/8.0");

} // namespace

// Acceptance of webdav-webui (I-3, I-5, I-10): ServeManager against a real rcd
// of the webui fork (RB_TEST_RCLONE_WEBUI) and of upstream rclone
// (RB_TEST_RCLONE), with an isolated temporary config (I-9: never the host
// config). Every serve binds to 127.0.0.1. The secret and any password are only
// compared, never printed.
class TestE2eServeWebui : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mFork;
  QString mUpstream;
  RcloneDaemon *mDaemon = nullptr;
  std::unique_ptr<ServeManager> mManager;

  QString tree() const { return mDir.filePath("w"); }

  // SetRclone(path), then a fresh rcd and a ServeManager on it.
  bool startDaemon(const QString &rclonePath) {
    SetRclone(rclonePath);
    mDaemon = new RcloneDaemon;
    mDaemon->start();
    const bool running = QTest::qWaitFor(
        [this]() { return mDaemon->state() == RcloneDaemon::State::Running; },
        10000);
    if (!running) {
      qWarning().noquote() << "rcd did not start:" << mDaemon->lastError();
      return false;
    }
    mManager = std::make_unique<ServeManager>(callFn());
    return true;
  }

  void stopDaemon() {
    mManager.reset(); // before the daemon: its callbacks run on the client
    delete mDaemon;
    mDaemon = nullptr;
  }

  RcCallFn callFn() {
    RcClient *client = mDaemon->client();
    return [this, client](const QString &path, const QJsonObject &params,
                          std::function<void(const RcReply &)> done,
                          int timeoutMs) {
      client->call(path, params, this, std::move(done), timeoutMs);
    };
  }

  // A synchronous rc call through the daemon client.
  RcReply rcGet(const QString &path, const QJsonObject &params) {
    RcReply out;
    bool finished = false;
    mDaemon->client()->call(path, params, this,
                            [&out, &finished](const RcReply &reply) {
                              out = reply;
                              finished = true;
                            },
                            10000);
    if (!QTest::qWaitFor([&finished]() { return finished; }, 15000)) {
      out.ok = false;
      out.error = QStringLiteral("rc call timed out");
    }
    return out;
  }

  // A webdav-webui ServeParams; the options are non-default so that the
  // read-back proves they travelled.
  ServeParams webuiParams() const {
    ServeParams p;
    p.type = QStringLiteral("webdav");
    p.fs = QStringLiteral("loc:") + tree();
    p.readOnly = false;
    p.flavor = QString(kWebuiFlavor);
    p.vfsCacheMode = QStringLiteral("writes");
    WebuiOptions &o = p.webui;
    o.allowDelete = false;
    o.search = false;
    o.searchMaxDepth = 3;
    o.searchMaxResults = 500;
    o.searchTimeout = QStringLiteral("45s");
    o.publinkPaths = QStringList{QStringLiteral("/")};
    o.secret = QStringLiteral("e2e-secret");
    o.tokenExpiry = QStringLiteral("1h");
    o.resume = true;
    o.resumeDir = mDir.filePath("resume");
    o.resumeRetention = QStringLiteral("720h");
    o.playerMaxRetry = 5;
    o.playerReconnectInterval = 7;
    o.diskStats = true;
    o.uploadSettings = true;
    o.editMaxSize = QStringLiteral("8Mi");
    o.dirCount = true;
    o.multiZipMax = 50;
    return p;
  }

  ServeParams classicParams() const {
    ServeParams p;
    p.type = QStringLiteral("webdav");
    p.fs = QStringLiteral("loc:") + tree();
    return p;
  }

  // Starts `p` and waits for the result. Returns the token; the entry is the
  // started() payload, empty with `failure` set when the start failed.
  struct StartResult {
    quint64 token = 0;
    ServeEntry entry;
    QString failure;
    bool done = false;
  };
  StartResult startAndWait(const ServeParams &p) {
    StartResult r;
    QSignalSpy started(mManager.get(), &ServeManager::started);
    QSignalSpy failed(mManager.get(), &ServeManager::startFailed);
    r.token = mManager->start(p);
    r.done = QTest::qWaitFor(
        [&]() { return started.count() + failed.count() >= 1; }, 15000);
    if (started.count() >= 1) {
      r.entry = qvariant_cast<ServeEntry>(started.at(0).at(1));
    } else if (failed.count() >= 1) {
      r.failure = failed.at(0).at(1).toString();
    }
    return r;
  }

  // 200 and a body with (ui) or without (!ui) the web UI marker, for a
  // browser; a curl-like client never gets the UI.
  void expectUi(const ServeEntry &e, bool ui, const char *what) {
    const HttpReply browser = httpGet(serveUrl(e), kBrowser);
    QVERIFY2(browser.status == 200, what);
    QVERIFY2(browser.body.contains(kMarker) == ui, what);
    const HttpReply curl = httpGet(serveUrl(e), kCurl);
    QVERIFY2(curl.status == 200, what);
    QVERIFY2(!curl.body.contains(kMarker), what);
  }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    mFork = qEnvironmentVariable("RB_TEST_RCLONE_WEBUI");
    // I-9: once registered the test fails, it never skips.
    QVERIFY2(!mFork.isEmpty() && QFileInfo(mFork).isExecutable(),
             "RB_TEST_RCLONE_WEBUI must point to the rclone webui fork");
    mUpstream = rbRclone();
    QVERIFY2(!mUpstream.isEmpty() && QFileInfo(mUpstream).isExecutable(),
             "RB_TEST_RCLONE must point to upstream rclone");
    qRegisterMetaType<ServeEntry>();
    qRegisterMetaType<QVector<ServeEntry>>();
    QVERIFY(mDir.isValid());
    // rcd inherits these: nothing of the real user (cache, home)
    qputenv("RCLONE_CACHE_DIR", mDir.filePath("cache").toUtf8());
    const QString home = mDir.filePath("home");
    QVERIFY(QDir().mkpath(home));
    qputenv("HOME", home.toUtf8());
    // I-9: rcd uses this temporary config, never the host one.
    const QString conf = mDir.filePath("rclone.conf");
    QFile f(conf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[loc]\ntype = local\n");
    f.close();
    SetRcloneConf(conf);
    // without it rclone fails with "didn't find section in config file (loc)"
    GetSettings()->setValue(QStringLiteral("Settings/rcloneConf"), conf);

    QVERIFY(QDir().mkpath(tree()));
    QFile file(QDir(tree()).filePath("a b.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hi");
    file.close();
  }

  void cleanup() { stopDaemon(); }

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

  // T-E2E-WEBUI-CAPS: the fork lists the "webdav_webui" block.
  void T_E2E_WEBUI_CAPS() {
    QVERIFY(startDaemon(mFork));
    QCOMPARE(mManager->webuiSupport(), ServeManager::WebuiSupport::Unknown);
    mManager->loadCapabilities();
    QTRY_VERIFY_WITH_TIMEOUT(mManager->webuiSupport() !=
                                 ServeManager::WebuiSupport::Unknown,
                             10000);
    QCOMPARE(mManager->webuiSupport(), ServeManager::WebuiSupport::Yes);
  }

  // T-E2E-WEBUI-PAGE: the flavor serves the web UI to a browser only, and the
  // vfs cache mode reaches serve/list.
  void T_E2E_WEBUI_PAGE() {
    QVERIFY(startDaemon(mFork));
    const StartResult r = startAndWait(webuiParams());
    QVERIFY2(r.done && r.failure.isEmpty(), qPrintable(r.failure));
    QVERIFY(!r.entry.id.isEmpty());

    expectUi(r.entry, true, "webdav-webui page");
    if (QTest::currentTestFailed()) {
      return;
    }

    const RcReply list = rcGet(QStringLiteral("serve/list"), {});
    QVERIFY2(list.ok, qPrintable(list.error));
    bool found = false;
    const QJsonArray entries = list.result.value("list").toArray();
    for (const QJsonValue &v : entries) {
      const QJsonObject o = v.toObject();
      if (o.value("id").toString() != r.entry.id) {
        continue;
      }
      found = true;
      QCOMPARE(o.value("params").toObject().value("vfs_cache_mode").toString(),
               QStringLiteral("writes"));
    }
    QVERIFY(found);
  }

  // T-E2E-WEBUI-OPTS (I-10): options/get reads back the 19 keys, durations and
  // sizes converted by rclone.
  void T_E2E_WEBUI_OPTS() {
    QVERIFY(startDaemon(mFork));
    const ServeParams p = webuiParams();
    const StartResult r = startAndWait(p);
    QVERIFY2(r.done && r.failure.isEmpty(), qPrintable(r.failure));

    const RcReply reply = rcGet(QStringLiteral("options/get"),
                                {{"blocks", QStringLiteral("webdav_webui")}});
    QVERIFY2(reply.ok, qPrintable(reply.error));
    const QJsonObject block = reply.result.value("webdav_webui").toObject();

    const WebuiOptions &o = p.webui;
    const QJsonObject expected{
        {"Enabled", true},
        {"AllowDelete", false},
        {"Search", false},
        {"SearchMaxDepth", 3},
        {"SearchMaxResults", 500},
        {"SearchTimeout", 45000000000.0},
        {"PublinkPaths", QJsonArray{QStringLiteral("/")}},
        {"TokenExpiry", 3600000000000.0},
        {"Resume", true},
        {"ResumeDir", o.resumeDir},
        {"ResumeRetention", 2592000000000000.0},
        {"PlayerMaxRetry", 5},
        {"PlayerReconnectInterval", 7},
        {"DiskStats", true},
        {"UploadSettings", true},
        {"EditMaxSize", 8388608},
        {"DirCount", true},
        {"MultiZipMax", 50}};
    QCOMPARE(webuiOptionKeys().size(), 19);
    for (const QString &key : webuiOptionKeys()) {
      QVERIFY2(block.contains(key), qPrintable(key + ": missing"));
      if (key == QLatin1String("Secret")) {
        // a credential: compared, never printed
        QVERIFY(block.value(key).toString() == QStringLiteral("e2e-secret"));
        continue;
      }
      QVERIFY2(block.value(key) == expected.value(key),
               qPrintable(key + ": got " +
                          QString::fromUtf8(QJsonDocument(QJsonArray{
                                                              block.value(key)})
                                                .toJson(QJsonDocument::Compact))));
    }
    QCOMPARE(expected.size(), 18);
  }

  // T-E2E-WEBUI-CLASSIC (I-3): the process-global Enabled switch follows each
  // start, and a running serve keeps its own UI setting.
  void T_E2E_WEBUI_CLASSIC() {
    QVERIFY(startDaemon(mFork));
    const StartResult a = startAndWait(webuiParams());
    QVERIFY2(a.done && a.failure.isEmpty(), qPrintable(a.failure));
    const StartResult b = startAndWait(classicParams());
    QVERIFY2(b.done && b.failure.isEmpty(), qPrintable(b.failure));
    const StartResult c = startAndWait(webuiParams());
    QVERIFY2(c.done && c.failure.isEmpty(), qPrintable(c.failure));

    expectUi(a.entry, true, "A (webdav-webui) has the UI");
    expectUi(b.entry, false, "B (classic webdav) has no UI");
    expectUi(c.entry, true, "C (webdav-webui) has the UI");
  }

  // T-E2E-WEBUI-SEQ: two starts queued back-to-back keep their own options.
  void T_E2E_WEBUI_SEQ() {
    QVERIFY(startDaemon(mFork));
    QSignalSpy started(mManager.get(), &ServeManager::started);
    QSignalSpy failed(mManager.get(), &ServeManager::startFailed);
    const quint64 dToken = mManager->start(webuiParams());
    const quint64 eToken = mManager->start(classicParams());
    QVERIFY(dToken != eToken);
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 2, 20000);
    QCOMPARE(failed.count(), 0);

    ServeEntry d, e;
    for (const QList<QVariant> &args : started) {
      const quint64 token = args.at(0).toULongLong();
      if (token == dToken) {
        d = qvariant_cast<ServeEntry>(args.at(1));
      } else if (token == eToken) {
        e = qvariant_cast<ServeEntry>(args.at(1));
      }
    }
    QVERIFY(!d.id.isEmpty() && !e.id.isEmpty());
    // FIFO: D started before E
    QCOMPARE(started.at(0).at(0).toULongLong(), dToken);

    expectUi(d, true, "D (webdav-webui) has the UI");
    expectUi(e, false, "E (classic webdav) has no UI");
  }

  // T-E2E-WEBUI-UPSTREAM (I-5): upstream rclone has no webdav_webui block.
  void T_E2E_WEBUI_UPSTREAM() {
    QVERIFY(startDaemon(mUpstream));
    QSignalSpy caps(mManager.get(), &ServeManager::capabilitiesChanged);
    mManager->loadCapabilities();
    QTRY_VERIFY_WITH_TIMEOUT(mManager->webuiSupport() !=
                                 ServeManager::WebuiSupport::Unknown,
                             10000);
    QCOMPARE(mManager->webuiSupport(), ServeManager::WebuiSupport::No);
    QVERIFY(caps.count() >= 1);

    const StartResult refused = startAndWait(webuiParams());
    QVERIFY(refused.done);
    QVERIFY(refused.failure == QString(kWebuiUnsupported));
    const RcReply list = rcGet(QStringLiteral("serve/list"), {});
    QVERIFY2(list.ok, qPrintable(list.error));
    QVERIFY(list.result.value("list").toArray().isEmpty());

    const StartResult classic = startAndWait(classicParams());
    QVERIFY2(classic.done && classic.failure.isEmpty(),
             qPrintable(classic.failure));
    expectUi(classic.entry, false, "classic webdav on upstream has no UI");
  }
};

QTEST_GUILESS_MAIN(TestE2eServeWebui)
#include "tst_e2e_serve_webui.moc"
