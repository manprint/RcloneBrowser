#include <QtTest>

#include "rb_test_support.h"
#include "serve_manager.h"

namespace {

struct Call {
  QString path;
  QJsonObject params;
  int timeoutMs = 0;
};

RcReply okReply(const QJsonObject &o = QJsonObject()) {
  RcReply r;
  r.ok = true;
  r.httpStatus = 200;
  r.result = o;
  return r;
}

RcReply errReply(int status, const QString &error) {
  RcReply r;
  r.httpStatus = status;
  r.error = error;
  return r;
}

// A scripted RcCallFn: records every call and, like RcClient::call, always
// replies asynchronously. `script` maps a path to its reply (ok and empty when
// the path is not in it).
struct Fake {
  QVector<Call> calls;
  QMap<QString, RcReply> script;
  // paths whose reply is kept back until releaseHeld()
  QSet<QString> hold;
  QVector<std::function<void()>> held;

  RcCallFn fn() {
    return [this](const QString &path, const QJsonObject &params,
                  std::function<void(const RcReply &)> done, int timeoutMs) {
      calls.append(Call{path, params, timeoutMs});
      const RcReply reply = script.contains(path) ? script.value(path)
                                                  : okReply();
      if (hold.contains(path)) {
        held.append([done, reply]() { done(reply); });
        return;
      }
      QTimer::singleShot(0, [done, reply]() { done(reply); });
    };
  }

  void releaseHeld() {
    const QVector<std::function<void()>> now = held;
    held.clear();
    for (const std::function<void()> &f : now) {
      QTimer::singleShot(0, f);
    }
  }

  QStringList paths() const {
    QStringList out;
    for (const Call &c : calls) {
      out << c.path;
    }
    return out;
  }
};

ServeEntry entry(const QString &type, const QString &addr) {
  ServeEntry e;
  e.type = type;
  e.addr = addr;
  return e;
}

const char kR15List[] =
    R"({"list":[
      {"id":"http-8ae93585","addr":"127.0.0.1:40855","params":{"type":"http","fs":"loc:/tmp/w","addr":"127.0.0.1:0","read_only":true}},
      {"id":"webdav-1f2e3d4c","addr":"[::]:4321","params":{"type":"webdav","fs":"r:a b","user":"u","pass":"p"}},
      {"addr":"127.0.0.1:1","params":{"type":"http","fs":"x:"}},
      {"id":"","addr":"127.0.0.1:2","params":{}}]})";

QJsonObject jsonOf(const char *text) {
  return QJsonDocument::fromJson(QByteArray(text)).object();
}

} // namespace

class TestServeManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<ServeEntry>();
    qRegisterMetaType<QVector<ServeEntry>>();
  }

  // T-SRV-PARAMS
  void T_SRV_PARAMS() {
    ServeParams p;
    p.type = "http";
    p.fs = "r:a";
    QCOMPARE(serveStartParams(p),
             (QJsonObject{{"type", "http"},
                          {"fs", "r:a"},
                          {"addr", "127.0.0.1:0"}}));

    p.user = "u";
    p.pass = "p";
    p.readOnly = true;
    const QJsonObject full = serveStartParams(p);
    QCOMPARE(full.value("user").toString(), QString("u"));
    QCOMPARE(full.value("pass").toString(), QString("p"));
    QVERIFY(full.value("read_only").isBool());
    QCOMPARE(full.value("read_only").toBool(), true);
    QCOMPARE(full.size(), 6);

    // a password without a user name is sent alone
    ServeParams q;
    q.type = "webdav";
    q.fs = "r:";
    q.pass = "p";
    const QJsonObject passOnly = serveStartParams(q);
    QVERIFY(!passOnly.contains("user"));
    QVERIFY(!passOnly.contains("read_only"));
    QCOMPARE(passOnly, (QJsonObject{{"type", "webdav"},
                                    {"fs", "r:"},
                                    {"addr", "127.0.0.1:0"},
                                    {"pass", "p"}}));

    // literal: nothing is trimmed
    ServeParams t;
    t.type = "http";
    t.fs = "r:a";
    t.addr = " 127.0.0.1:0 ";
    t.user = " u ";
    t.pass = " p ";
    QCOMPARE(serveStartParams(t), (QJsonObject{{"type", "http"},
                                               {"fs", "r:a"},
                                               {"addr", " 127.0.0.1:0 "},
                                               {"user", " u "},
                                               {"pass", " p "}}));

    // sftp: rclone's default authorized_keys file is always turned off
    ServeParams s;
    s.type = "sftp";
    s.fs = "r:";
    s.user = "u";
    s.pass = "p";
    QCOMPARE(serveStartParams(s), (QJsonObject{{"type", "sftp"},
                                               {"fs", "r:"},
                                               {"addr", "127.0.0.1:0"},
                                               {"user", "u"},
                                               {"pass", "p"},
                                               {"authorized_keys", ""}}));
  }

  // T-SRV-PARAMS (webui flavor, vfs cache mode)
  void T_SRV_PARAMS_WEBUI() {
    ServeParams p;
    p.type = "webdav";
    p.flavor = kWebuiFlavor;
    p.fs = "r:";
    p.vfsCacheMode = "writes";
    p.webui.secret = "s3cret";
    p.webui.searchMaxDepth = 3;
    const QJsonObject o = serveStartParams(p);
    QCOMPARE(o.value("type").toString(), QString("webdav"));
    QCOMPARE(o.value("vfs_cache_mode").toString(), QString("writes"));
    for (const QString &key : o.keys()) {
      QVERIFY2(!key.startsWith("webui"), "no webui* key in serve/start");
    }
    QCOMPARE(o.size(), 4);

    p.vfsCacheMode.clear();
    const QJsonObject plain = serveStartParams(p);
    QVERIFY(!plain.contains("vfs_cache_mode"));
    QCOMPARE(plain.size(), 3);
  }

  // T-WEBUI-BLOCK
  void T_WEBUI_BLOCK() {
    const QStringList expected = {
        "Enabled",          "AllowDelete",     "Search",
        "SearchMaxDepth",   "SearchMaxResults", "SearchTimeout",
        "PublinkPaths",     "Secret",          "TokenExpiry",
        "Resume",           "ResumeDir",       "ResumeRetention",
        "PlayerMaxRetry",   "PlayerReconnectInterval",
        "DiskStats",        "UploadSettings",  "EditMaxSize",
        "DirCount",         "MultiZipMax"};
    QCOMPARE(webuiOptionKeys(), expected);
    QCOMPARE(webuiOptionKeys().size(), 19);

    const QJsonObject top = webuiOptionsBlock(WebuiOptions(), true);
    QCOMPARE(top.keys(), QStringList{"webdav_webui"});
    const QJsonObject b = top.value("webdav_webui").toObject();
    QStringList keys = b.keys();
    QStringList sortedExpected = expected;
    keys.sort();
    sortedExpected.sort();
    QCOMPARE(keys, sortedExpected);

    QVERIFY(b.value("Enabled").isBool() && b.value("Enabled").toBool());
    const char *bools[] = {"AllowDelete", "Search",      "Resume",
                           "DiskStats",   "UploadSettings", "DirCount"};
    const bool boolDefaults[] = {true, true, false, false, false, false};
    for (int i = 0; i < 6; ++i) {
      QVERIFY2(b.value(bools[i]).isBool(), bools[i]);
      QVERIFY2(b.value(bools[i]).toBool() == boolDefaults[i], bools[i]);
    }
    const char *numbers[] = {"SearchMaxDepth", "SearchMaxResults",
                             "PlayerMaxRetry", "PlayerReconnectInterval",
                             "MultiZipMax"};
    const double numberDefaults[] = {8, 1000, 30, 10, 200};
    for (int i = 0; i < 5; ++i) {
      QVERIFY2(b.value(numbers[i]).isDouble(), numbers[i]);
      QVERIFY2(b.value(numbers[i]).toDouble() == numberDefaults[i], numbers[i]);
    }
    const char *strings[] = {"SearchTimeout", "TokenExpiry", "ResumeRetention",
                             "EditMaxSize",   "Secret",      "ResumeDir"};
    const char *stringDefaults[] = {"30s", "72h", "720h", "4Mi", "", ""};
    for (int i = 0; i < 6; ++i) {
      QVERIFY2(b.value(strings[i]).isString(), strings[i]);
      QVERIFY2(b.value(strings[i]).toString() == QLatin1String(stringDefaults[i]),
               strings[i]);
    }
    QVERIFY(b.value("PublinkPaths").isArray());
    QVERIFY(b.value("PublinkPaths").toArray().isEmpty());

    WebuiOptions o;
    o.publinkPaths = {"/a", "/b"};
    const QJsonArray paths = webuiOptionsBlock(o, false)
                                 .value("webdav_webui")
                                 .toObject()
                                 .value("PublinkPaths")
                                 .toArray();
    QCOMPARE(paths, (QJsonArray{"/a", "/b"}));
    const QJsonObject off =
        webuiOptionsBlock(o, false).value("webdav_webui").toObject();
    QVERIFY(off.value("Enabled").isBool());
    QVERIFY(!off.value("Enabled").toBool());
  }

  // T-WEBUI-CAPS
  void T_WEBUI_CAPS() {
    using Support = ServeManager::WebuiSupport;
    qRegisterMetaType<ServeManager::WebuiSupport>();

    // Yes: one signal
    {
      Fake fake;
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      ServeManager manager(fake.fn());
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QSignalSpy spy(&manager, &ServeManager::capabilitiesChanged);
      manager.loadCapabilities();
      manager.loadCapabilities(); // in flight: no second call
      QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
      QCOMPARE(manager.webuiSupport(), Support::Yes);
      QCOMPARE(spy.at(0).at(0).value<Support>(), Support::Yes);
      QCOMPARE(fake.paths(), QStringList{"options/blocks"});

      // same value again: no signal
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(fake.calls.size(), 2, 2000);
      QTest::qWait(50);
      QCOMPARE(spy.count(), 1);

      // reset from Yes: Unknown + signal
      manager.resetCapabilities();
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QCOMPARE(spy.count(), 2);
      QCOMPARE(spy.at(1).at(0).value<Support>(), Support::Unknown);
      // reset when already Unknown: no signal
      manager.resetCapabilities();
      QCOMPARE(spy.count(), 2);
    }

    // No
    {
      Fake fake;
      fake.script["options/blocks"] = okReply(jsonOf(R"({"options":["main"]})"));
      ServeManager manager(fake.fn());
      QSignalSpy spy(&manager, &ServeManager::capabilitiesChanged);
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
      QCOMPARE(manager.webuiSupport(), Support::No);
      QCOMPARE(spy.at(0).at(0).value<Support>(), Support::No);

      // a reply without an options array is No as well, never Yes
      fake.script["options/blocks"] = okReply(jsonOf(R"({"options":"webdav_webui"})"));
      manager.resetCapabilities();
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(manager.webuiSupport(), Support::No, 2000);
    }

    // a failure: still Unknown, rclone's text is logged, no signal
    {
      Fake fake;
      fake.script["options/blocks"] = errReply(500, "x");
      ServeManager manager(fake.fn());
      QSignalSpy spy(&manager, &ServeManager::capabilitiesChanged);
      QTest::ignoreMessage(QtWarningMsg, "options/blocks failed: x");
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(fake.calls.size(), 1, 2000);
      QTest::qWait(50);
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QCOMPARE(spy.count(), 0);

      // that check had no start waiting: the next webdav start asks again
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      fake.script["serve/start"] =
          okReply(QJsonObject{{"addr", "127.0.0.1:1"}, {"id", "webdav-1"}});
      QSignalSpy startedSpy(&manager, &ServeManager::started);
      ServeParams p;
      p.type = "webdav";
      p.fs = "r:";
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);
      QCOMPARE(manager.webuiSupport(), Support::Yes);
      QCOMPARE(fake.paths().mid(0, 4),
               (QStringList{"options/blocks", "options/blocks", "options/set",
                            "serve/start"}));
    }

    // a reset while a load is in flight: the stale reply changes nothing
    {
      Fake fake;
      fake.hold.insert("options/blocks");
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      ServeManager manager(fake.fn());
      QSignalSpy spy(&manager, &ServeManager::capabilitiesChanged);
      manager.loadCapabilities();
      QCOMPARE(fake.held.size(), 1);
      manager.resetCapabilities();
      QCOMPARE(spy.count(), 0); // was Unknown already
      fake.releaseHeld();
      QTest::qWait(50);
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QCOMPARE(spy.count(), 0);

      // the load is possible again after the reset, and counts
      fake.hold.clear();
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
      QCOMPARE(manager.webuiSupport(), Support::Yes);
      QCOMPARE(fake.paths(),
               (QStringList{"options/blocks", "options/blocks"}));
    }

    // a start waiting for an abandoned load is not left hanging by a reset
    {
      Fake fake;
      fake.hold.insert("options/blocks");
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      ServeManager manager(fake.fn());
      ServeParams p;
      p.type = "webdav";
      p.fs = "r:";
      QSignalSpy startedSpy(&manager, &ServeManager::started);
      fake.script["serve/start"] =
          okReply(QJsonObject{{"addr", "127.0.0.1:1"}, {"id", "webdav-1"}});
      manager.start(p);
      QCOMPARE(fake.paths(), QStringList{"options/blocks"});
      fake.hold.clear();
      manager.resetCapabilities(); // starts a fresh load, not held
      fake.releaseHeld();          // the stale reply: ignored
      QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);
      QCOMPARE(fake.paths().mid(0, 4),
               (QStringList{"options/blocks", "options/blocks", "options/set",
                            "serve/start"}));
      QCOMPARE(fake.paths().count("serve/start"), 1);
    }
  }

  // T-WEBUI-SEQ
  void T_WEBUI_SEQ() {
    const QJsonObject startOk{{"addr", "127.0.0.1:40855"},
                              {"id", "webdav-8ae93585"}};

    // (a) a capable rclone: blocks once, options/set before each webdav start
    {
      Fake fake;
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      fake.script["serve/start"] = okReply(startOk);
      ServeManager manager(fake.fn());
      QVector<quint64> tokens;
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 token, const ServeEntry &) { tokens << token; });
      ServeParams p1;
      p1.type = "webdav";
      p1.flavor = kWebuiFlavor;
      p1.fs = "r:a";
      p1.webui.searchMaxDepth = 5;
      p1.webui.secret = "s3cret";
      ServeParams p2;
      p2.type = "webdav";
      p2.fs = "r:b";
      ServeParams p3;
      p3.type = "http";
      p3.fs = "r:c";
      QCOMPARE(manager.start(p1), quint64(1));
      QCOMPARE(manager.start(p2), quint64(2));
      QCOMPARE(manager.start(p3), quint64(3));
      QTRY_COMPARE_WITH_TIMEOUT(fake.paths().size(), 9, 3000);
      QTest::qWait(50);
      QCOMPARE(fake.paths(),
               (QStringList{"options/blocks", "options/set", "serve/start",
                            "serve/list", "options/set", "serve/start",
                            "serve/list", "serve/start", "serve/list"}));
      QCOMPARE(tokens, (QVector<quint64>{1, 2, 3}));
      const QJsonObject set1 =
          fake.calls.at(1).params.value("webdav_webui").toObject();
      QVERIFY(set1.value("Enabled").toBool());
      QCOMPARE(set1.value("SearchMaxDepth").toInt(), 5);
      QVERIFY(set1.value("Secret").toString() == QLatin1String("s3cret"));
      const QJsonObject set2 =
          fake.calls.at(4).params.value("webdav_webui").toObject();
      QVERIFY(!set2.value("Enabled").toBool());
      QCOMPARE(set2.value("SearchMaxDepth").toInt(), 8);
      // the serve/start bodies never carry a webui key or the secret
      for (int i : {2, 5, 7}) {
        const QJsonObject body = fake.calls.at(i).params;
        for (const QString &key : body.keys()) {
          QVERIFY2(!key.startsWith("webui"), "no webui* key");
        }
        QVERIFY(!body.contains("Secret") && !body.contains("secret"));
      }
    }

    // (b) an rclone without the block: webui is refused, classic goes on
    {
      Fake fake;
      fake.script["options/blocks"] = okReply(jsonOf(R"({"options":["main"]})"));
      fake.script["serve/start"] = okReply(startOk);
      ServeManager manager(fake.fn());
      QVector<quint64> failedTokens, startedTokens;
      QStringList failedTexts, errorTexts;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &m) {
                failedTokens << t;
                failedTexts << m;
              });
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) { errorTexts << m; });
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 t, const ServeEntry &) { startedTokens << t; });
      ServeParams webui;
      webui.type = "webdav";
      webui.flavor = kWebuiFlavor;
      webui.fs = "r:a";
      QCOMPARE(manager.start(webui), quint64(1));
      QTRY_COMPARE_WITH_TIMEOUT(failedTokens.size(), 1, 2000);
      QCOMPARE(failedTokens, QVector<quint64>{1});
      QVERIFY(failedTexts.at(0).startsWith("This rclone does not support webdav-webui"));
      QVERIFY(failedTexts.at(0).endsWith(
          "Choose webdav for a WebDAV serve without the web interface."));
      QCOMPARE(errorTexts, failedTexts);
      QTest::qWait(50);
      QVERIFY(!fake.paths().contains("options/set"));
      QVERIFY(!fake.paths().contains("serve/start"));

      ServeParams classic;
      classic.type = "webdav";
      classic.fs = "r:b";
      QCOMPARE(manager.start(classic), quint64(2));
      QTRY_COMPARE_WITH_TIMEOUT(startedTokens.size(), 1, 2000);
      QCOMPARE(startedTokens, QVector<quint64>{2});
      QVERIFY(!fake.paths().contains("options/set"));
      QCOMPARE(fake.paths().count("serve/start"), 1);
      QCOMPARE(fake.paths().count("options/blocks"), 1);
    }

    // a failed capability check: classic webdav still starts, webui is refused
    {
      Fake fake;
      fake.script["options/blocks"] = errReply(500, "no rc");
      fake.script["serve/start"] = okReply(startOk);
      ServeManager manager(fake.fn());
      QVector<quint64> failedTokens, startedTokens;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &) { failedTokens << t; });
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 t, const ServeEntry &) { startedTokens << t; });
      ServeParams classic;
      classic.type = "webdav";
      classic.fs = "r:b";
      ServeParams webui = classic;
      webui.flavor = kWebuiFlavor;
      QTest::ignoreMessage(QtWarningMsg, "options/blocks failed: no rc");
      QTest::ignoreMessage(QtWarningMsg, "options/blocks failed: no rc");
      manager.start(classic);
      manager.start(webui);
      QTRY_COMPARE_WITH_TIMEOUT(failedTokens.size(), 1, 3000);
      QCOMPARE(startedTokens, QVector<quint64>{1});
      QCOMPARE(failedTokens, QVector<quint64>{2});
      QVERIFY(!fake.paths().contains("options/set"));
    }

    // (c) options/set fails: that start fails, no serve/start, the next runs
    {
      Fake fake;
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      fake.script["options/set"] = errReply(500, "bad suffix");
      fake.script["serve/start"] = okReply(startOk);
      ServeManager manager(fake.fn());
      QStringList order, failedTexts, errorTexts;
      QVector<quint64> failedTokens, startedTokens;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &m) {
                order << "startFailed";
                failedTokens << t;
                failedTexts << m;
              });
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) {
                order << "error";
                errorTexts << m;
              });
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 t, const ServeEntry &) { startedTokens << t; });
      ServeParams webui;
      webui.type = "webdav";
      webui.flavor = kWebuiFlavor;
      webui.fs = "r:a";
      ServeParams http;
      http.type = "http";
      http.fs = "r:b";
      manager.start(webui);
      manager.start(http);
      QTRY_COMPARE_WITH_TIMEOUT(startedTokens.size(), 1, 3000);
      QCOMPARE(failedTokens, QVector<quint64>{1});
      QVERIFY(failedTexts.at(0).startsWith("webdav-webui options: "));
      QVERIFY(failedTexts.at(0).contains("bad suffix"));
      QCOMPARE(errorTexts, failedTexts);
      QCOMPARE(order, (QStringList{"startFailed", "error"}));
      QCOMPARE(startedTokens, QVector<quint64>{2});
      QTest::qWait(50);
      QCOMPARE(fake.paths(),
               (QStringList{"options/blocks", "options/set", "serve/list",
                            "serve/start", "serve/list"}));
    }
  }

  // T-WEBUI-CHECK-FAILED: a webui start that waits for a failed capability
  // check names that check, never "not supported"; the text is kept until a
  // check succeeds, and a rclone without the block still gets kWebuiUnsupported
  void T_WEBUI_CHECK_FAILED() {
    using Support = ServeManager::WebuiSupport;
    qRegisterMetaType<ServeManager::WebuiSupport>();
    const QString checkText = QStringLiteral("rclone daemon is not running");
    const QString expected = QString(kWebuiCheckFailed) + checkText;
    const QJsonObject startOk{{"addr", "127.0.0.1:1"}, {"id", "webdav-1"}};
    ServeParams webui;
    webui.type = "webdav";
    webui.flavor = kWebuiFlavor;
    webui.fs = "r:a";

    // (i) the check fails: the start fails with the check's text, and nothing
    // reaches options/set or serve/start (I-5)
    {
      Fake fake;
      fake.script["options/blocks"] = errReply(500, checkText);
      ServeManager manager(fake.fn());
      QVector<quint64> failedTokens;
      QStringList failedTexts, errorTexts, checkTexts;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &m) {
                failedTokens << t;
                failedTexts << m;
              });
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) { errorTexts << m; });
      connect(&manager, &ServeManager::capabilitiesCheckFailed, &manager,
              [&](const QString &m) { checkTexts << m; });
      QTest::ignoreMessage(QtWarningMsg,
                           "options/blocks failed: rclone daemon is not running");
      QCOMPARE(manager.start(webui), quint64(1));
      QTRY_COMPARE_WITH_TIMEOUT(failedTokens.size(), 1, 3000);
      QTest::qWait(50);
      QCOMPARE(failedTokens, QVector<quint64>{1});
      QCOMPARE(failedTexts, QStringList{expected});
      QCOMPARE(errorTexts, QStringList{expected});
      QCOMPARE(checkTexts, QStringList{checkText});
      QCOMPARE(manager.webuiCheckError(), checkText);
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QVERIFY(!fake.paths().contains("options/set"));
      QVERIFY(!fake.paths().contains("serve/start"));
    }

    // (ii) a reset drains the queue while a check is in flight: the failed check
    // before it shows its text until the reset, which clears it at once; the
    // stale reply (a failure, the backend was restarting) changes nothing, the
    // fresh check fails and the start fails with its text
    {
      Fake fake;
      const QString staleText = QStringLiteral("rclone was restarting");
      fake.script["options/blocks"] = errReply(500, checkText);
      ServeManager manager(fake.fn());
      QVector<quint64> failedTokens;
      QStringList failedTexts;
      QSignalSpy spy(&manager, &ServeManager::capabilitiesChanged);
      QSignalSpy checkSpy(&manager, &ServeManager::capabilitiesCheckFailed);
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &m) {
                failedTokens << t;
                failedTexts << m;
              });
      QTest::ignoreMessage(QtWarningMsg,
                           "options/blocks failed: rclone daemon is not running");
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(checkSpy.count(), 1, 3000);
      QCOMPARE(manager.webuiCheckError(), checkText);
      // the start is queued while the next check is in flight: its reply is held
      fake.hold.insert("options/blocks");
      fake.script["options/blocks"] = errReply(500, staleText);
      QCOMPARE(manager.start(webui), quint64(1));
      QCOMPARE(fake.held.size(), 1);
      QCOMPARE(fake.paths().count("options/blocks"), 2);
      // the backend stopped: the next check fails, the held one is stale
      fake.script["options/blocks"] = errReply(500, checkText);
      fake.hold.clear();
      QTest::ignoreMessage(QtWarningMsg,
                           "options/blocks failed: rclone daemon is not running");
      manager.resetCapabilities();
      QVERIFY(manager.webuiCheckError().isEmpty());
      QCOMPARE(spy.count(), 1);
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      fake.releaseHeld();
      QTRY_COMPARE_WITH_TIMEOUT(failedTokens.size(), 1, 3000);
      QTest::qWait(50);
      QCOMPARE(failedTokens, QVector<quint64>{1});
      QCOMPARE(failedTexts, QStringList{expected});
      QCOMPARE(manager.webuiCheckError(), checkText);
      QCOMPARE(manager.webuiSupport(), Support::Unknown);
      QCOMPARE(spy.count(), 1);
      // the stale failure emitted nothing: only the two fresh failures did
      QCOMPARE(checkSpy.count(), 2);
      QCOMPARE(checkSpy.last().at(0).toString(), checkText);
      QCOMPARE(fake.paths().count("options/blocks"), 3);
      QVERIFY(!fake.paths().contains("options/set"));
      QVERIFY(!fake.paths().contains("serve/start"));

      // (iii) a later successful check clears the text; webui starts again
      fake.script["options/blocks"] =
          okReply(jsonOf(R"({"options":["main","webdav_webui"]})"));
      fake.script["serve/start"] = okReply(startOk);
      QSignalSpy startedSpy(&manager, &ServeManager::started);
      manager.loadCapabilities();
      QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 3000);
      QCOMPARE(manager.webuiSupport(), Support::Yes);
      QVERIFY(manager.webuiCheckError().isEmpty());
      const int before = fake.calls.size();
      QCOMPARE(manager.start(webui), quint64(2));
      QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 3000);
      QCOMPARE(failedTokens, QVector<quint64>{1});
      const QStringList later = fake.paths().mid(before);
      QVERIFY(later.size() >= 2);
      QCOMPARE(later.mid(0, 2),
               (QStringList{"options/set", "serve/start"}));
    }

    // (iv) an rclone without the block: still exactly kWebuiUnsupported
    {
      Fake fake;
      fake.script["options/blocks"] = okReply(jsonOf(R"({"options":["main"]})"));
      ServeManager manager(fake.fn());
      QSignalSpy checkSpy(&manager, &ServeManager::capabilitiesCheckFailed);
      QStringList failedTexts;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64, const QString &m) { failedTexts << m; });
      manager.start(webui);
      QTRY_COMPARE_WITH_TIMEOUT(failedTexts.size(), 1, 3000);
      QCOMPARE(failedTexts.at(0), QString(kWebuiUnsupported));
      QCOMPARE(checkSpy.count(), 0);
      QVERIFY(manager.webuiCheckError().isEmpty());
      QVERIFY(!fake.paths().contains("options/set"));
      QVERIFY(!fake.paths().contains("serve/start"));
    }

    // (v) a check fails while a classic start is in flight and the webui start
    // waits behind it; a second check is in flight when that start ends: the
    // webui start waits for that reply and names its error, not the old one
    {
      Fake fake;
      const QString firstText = QStringLiteral("rclone daemon is not running");
      const QString secondText = QStringLiteral("rclone is still starting");
      ServeParams classic;
      classic.type = "http";
      classic.fs = "loc:/tmp/w";
      fake.hold.insert("serve/start");
      fake.script["serve/start"] = okReply(startOk);
      fake.script["options/blocks"] = errReply(500, firstText);
      ServeManager manager(fake.fn());
      QVector<quint64> failedTokens;
      QStringList failedTexts;
      QSignalSpy startedSpy(&manager, &ServeManager::started);
      QSignalSpy checkSpy(&manager, &ServeManager::capabilitiesCheckFailed);
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 t, const QString &m) {
                failedTokens << t;
                failedTexts << m;
              });
      QCOMPARE(manager.start(classic), quint64(1));
      QCOMPARE(fake.held.size(), 1);
      QTest::ignoreMessage(QtWarningMsg,
                           "options/blocks failed: rclone daemon is not running");
      manager.loadCapabilities();
      QCOMPARE(manager.start(webui), quint64(2));
      QTRY_COMPARE_WITH_TIMEOUT(checkSpy.count(), 1, 3000);
      QCOMPARE(manager.webuiCheckError(), firstText);
      QVERIFY(failedTokens.isEmpty());

      // a second check starts while the classic start is still in flight
      fake.hold.insert("options/blocks");
      fake.script["options/blocks"] = errReply(500, secondText);
      QTest::ignoreMessage(QtWarningMsg,
                           "options/blocks failed: rclone is still starting");
      manager.loadCapabilities();
      QCOMPARE(fake.held.size(), 2);

      // the classic start ends first: the webui start still waits for the check
      const std::function<void()> startDone = fake.held.first();
      fake.held.removeFirst();
      QTimer::singleShot(0, startDone);
      QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 3000);
      QTest::qWait(50);
      QVERIFY(failedTokens.isEmpty());
      QCOMPARE(checkSpy.count(), 1);
      QCOMPARE(manager.webuiCheckError(), firstText);

      // the second check fails: the webui start is refused with its text
      const std::function<void()> checkDone = fake.held.first();
      fake.held.removeFirst();
      QTimer::singleShot(0, checkDone);
      QTRY_COMPARE_WITH_TIMEOUT(failedTokens.size(), 1, 3000);
      QTest::qWait(50);
      QCOMPARE(failedTokens, QVector<quint64>{2});
      QCOMPARE(failedTexts,
               QStringList{QString(kWebuiCheckFailed) + secondText});
      QCOMPARE(checkSpy.count(), 2);
      QCOMPARE(checkSpy.last().at(0).toString(), secondText);
      QCOMPARE(manager.webuiCheckError(), secondText);
      QCOMPARE(fake.paths().count("serve/start"), 1);
      QVERIFY(!fake.paths().contains("options/set"));
    }
  }

  // T-SRV-LIST
  void T_SRV_LIST() {
    const QVector<ServeEntry> list = parseServeList(jsonOf(kR15List));
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).id, QString("http-8ae93585"));
    QCOMPARE(list.at(0).addr, QString("127.0.0.1:40855"));
    QCOMPARE(list.at(0).type, QString("http"));
    QCOMPARE(list.at(0).fs, QString("loc:/tmp/w"));
    QCOMPARE(list.at(1).id, QString("webdav-1f2e3d4c"));
    QCOMPARE(list.at(1).addr, QString("[::]:4321"));
    QCOMPARE(list.at(1).type, QString("webdav"));
    QCOMPARE(list.at(1).fs, QString("r:a b"));

    // the type falls back to the id prefix
    const QVector<ServeEntry> noType = parseServeList(
        jsonOf(R"({"list":[{"id":"sftp-1a2b","addr":"127.0.0.1:5","params":{"fs":"r:"}}]})"));
    QCOMPARE(noType.size(), 1);
    QCOMPARE(noType.at(0).type, QString("sftp"));

    QVERIFY(parseServeList(QJsonObject()).isEmpty());
    QVERIFY(parseServeList(jsonOf(R"({"list":"x"})")).isEmpty());
    QVERIFY(parseServeList(jsonOf(R"({"list":null})")).isEmpty());

    const QStringList types = parseServeTypes(jsonOf(
        R"({"types":["dlna","ftp","http","nfs","restic","s3","sftp","webdav"]})"));
    QCOMPARE(types, (QStringList{"dlna", "ftp", "http", "nfs", "restic", "s3",
                                 "sftp", "webdav"}));
    QVERIFY(parseServeTypes(QJsonObject()).isEmpty());
    QVERIFY(parseServeTypes(jsonOf(R"({"types":"http"})")).isEmpty());
    QCOMPARE(parseServeTypes(jsonOf(R"({"types":["http",3,null,"ftp"]})")),
             (QStringList{"http", "ftp"}));
  }

  // T-SRV-URL
  void T_SRV_URL() {
    const QString addr = "127.0.0.1:40855";
    for (const char *type : {"http", "webdav", "restic", "s3", "dlna"}) {
      QCOMPARE(serveUrl(entry(QString::fromLatin1(type), addr)),
               QString("http://127.0.0.1:40855/"));
    }
    QCOMPARE(serveUrl(entry("ftp", addr)), QString("ftp://127.0.0.1:40855/"));
    QCOMPARE(serveUrl(entry("sftp", addr)), QString("sftp://127.0.0.1:40855/"));
    QCOMPARE(serveUrl(entry("nfs", addr)), QString("nfs://127.0.0.1:40855/"));
    QCOMPARE(serveUrl(entry("http", "[::]:4321")),
             QString("http://[::]:4321/"));
    QCOMPARE(serveUrl(entry("smb", "127.0.0.1:1")), QString("127.0.0.1:1"));
  }

  // T-SRV-RULES
  void T_SRV_RULES() {
    struct Row {
      const char *type;
      bool readOnlyAllowed, loginAllowed, loginRequired, needsPort;
      const char *defaultAddr;
    };
    const Row rows[] = {
        {"http", true, true, false, false, "127.0.0.1:0"},
        {"webdav", true, true, false, false, "127.0.0.1:0"},
        {"ftp", true, true, false, true, "127.0.0.1:2121"},
        {"sftp", true, true, true, false, "127.0.0.1:0"},
        {"restic", false, true, false, false, "127.0.0.1:0"},
        {"s3", true, false, false, false, "127.0.0.1:0"},
        {"nfs", true, false, false, false, "127.0.0.1:0"},
        {"dlna", true, false, false, false, "127.0.0.1:0"},
        {"smb", true, true, false, false, "127.0.0.1:0"},
        {"", true, true, false, false, "127.0.0.1:0"},
    };
    for (const Row &row : rows) {
      const ServeTypeRules r = serveTypeRules(QString::fromLatin1(row.type));
      const QByteArray what = QByteArray("type \"") + row.type + "\"";
      QVERIFY2(r.readOnlyAllowed == row.readOnlyAllowed,
               (what + " readOnlyAllowed").constData());
      QVERIFY2(r.loginAllowed == row.loginAllowed,
               (what + " loginAllowed").constData());
      QVERIFY2(r.loginRequired == row.loginRequired,
               (what + " loginRequired").constData());
      QVERIFY2(r.needsPort == row.needsPort, (what + " needsPort").constData());
      QVERIFY2(r.defaultAddr == QLatin1String(row.defaultAddr),
               (what + " defaultAddr").constData());
    }
  }

  // T-SRV-TOKENS
  void T_SRV_TOKENS() {
    ServeParams p;
    p.type = "http";
    p.fs = "loc:/tmp/w";

    // two starts in a row: one serve/start in flight, tokens in order, the
    // list after each
    {
      Fake fake;
      fake.script["serve/start"] =
          okReply(QJsonObject{{"addr", "127.0.0.1:40855"},
                              {"id", "http-8ae93585"}});
      ServeManager manager(fake.fn());
      QVector<quint64> tokens;
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 token, const ServeEntry &) { tokens << token; });
      QCOMPARE(manager.start(p), quint64(1));
      QCOMPARE(manager.start(p), quint64(2));
      QCOMPARE(fake.paths().count("serve/start"), 1);
      QTRY_COMPARE_WITH_TIMEOUT(tokens.size(), 2, 2000);
      QCOMPARE(tokens, (QVector<quint64>{1, 2}));
      QTRY_COMPARE_WITH_TIMEOUT(fake.paths().size(), 4, 2000);
      QCOMPARE(fake.paths(), (QStringList{"serve/start", "serve/list",
                                          "serve/start", "serve/list"}));
    }

    // a failed start: startFailed before error, the queue goes on
    {
      Fake fake;
      fake.script["serve/start"] = errReply(500, "boom");
      ServeManager manager(fake.fn());
      QStringList order;
      QVector<quint64> failedTokens, startedTokens;
      QStringList failedTexts, errorTexts;
      connect(&manager, &ServeManager::startFailed, &manager,
              [&](quint64 token, const QString &m) {
                order << "startFailed";
                failedTokens << token;
                failedTexts << m;
              });
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) {
                order << "error";
                errorTexts << m;
              });
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 token, const ServeEntry &) {
                startedTokens << token;
              });
      QCOMPARE(manager.start(p), quint64(1));
      QCOMPARE(manager.start(p), quint64(2));
      // the second start is sent only after the first reply: it succeeds
      fake.script["serve/start"] =
          okReply(QJsonObject{{"addr", "127.0.0.1:40855"},
                              {"id", "http-8ae93585"}});
      QTRY_COMPARE_WITH_TIMEOUT(startedTokens.size(), 1, 2000);
      QCOMPARE(startedTokens, QVector<quint64>{2});
      QCOMPARE(order, (QStringList{"startFailed", "error"}));
      QCOMPARE(failedTokens, QVector<quint64>{1});
      QCOMPARE(failedTexts, QStringList{"boom"});
      QCOMPARE(errorTexts, QStringList{"boom"});
      QCOMPARE(fake.paths().count("serve/start"), 2);

      // an ok reply without an id: startFailed too
      fake.script["serve/start"] = okReply(QJsonObject{{"addr", "x"}});
      order.clear();
      QCOMPARE(manager.start(p), quint64(3));
      QTRY_COMPARE_WITH_TIMEOUT(order.size(), 2, 2000);
      QCOMPARE(order, (QStringList{"startFailed", "error"}));
      QCOMPARE(failedTokens, (QVector<quint64>{1, 3}));
      QCOMPARE(failedTexts.last(), QString("invalid serve/start reply"));
    }
  }

  // T-SRV-BROWSER-URL
  void T_SRV_BROWSER_URL() {
    struct Row {
      const char *type, *addr, *url;
    };
    const Row rows[] = {
        {"http", "127.0.0.1:40855", "http://127.0.0.1:40855/"},
        {"webdav", "[::]:4321", "http://127.0.0.1:4321/"},
        {"webdav-webui", "[::]:4321", "http://127.0.0.1:4321/"},
        {"http", "0.0.0.0:8080", "http://127.0.0.1:8080/"},
        {"http", ":8080", "http://127.0.0.1:8080/"},
        {"http", "192.168.1.5:8080", "http://192.168.1.5:8080/"},
        {"sftp", "[::]:22", "sftp://[::]:22/"},
    };
    for (const Row &row : rows) {
      const ServeEntry e = entry(QString::fromLatin1(row.type),
                                 QString::fromLatin1(row.addr));
      const QByteArray what =
          QByteArray(row.type) + " " + QByteArray(row.addr);
      QVERIFY2(serveBrowserUrl(e) == QLatin1String(row.url), what.constData());
    }
    // a row that needs no change is serveUrl itself
    const ServeEntry plain = entry("http", "127.0.0.1:40855");
    QCOMPARE(serveBrowserUrl(plain), serveUrl(plain));

    for (const char *type : {"http", "webdav", "webdav-webui"}) {
      QVERIFY2(serveOpensInBrowser(QString::fromLatin1(type)), type);
    }
    QCOMPARE(QString(kWebuiFlavor), QString("webdav-webui"));
    for (const char *type :
         {"ftp", "sftp", "nfs", "dlna", "s3", "restic", ""}) {
      QVERIFY2(!serveOpensInBrowser(QString::fromLatin1(type)), type);
    }
  }

  // T-SRV-FLOW
  void T_SRV_FLOW() {
    ServeParams p;
    p.type = "http";
    p.fs = "loc:/tmp/w";
    p.readOnly = true;

    // (a) start ok: started, then the list
    {
      Fake fake;
      fake.script["serve/start"] =
          okReply(QJsonObject{{"addr", "127.0.0.1:40855"},
                              {"id", "http-8ae93585"}});
      fake.script["serve/list"] = okReply(jsonOf(kR15List));
      ServeManager manager(fake.fn());
      QStringList log;
      QVector<ServeEntry> started, listed;
      QVector<quint64> tokens;
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64 token, const ServeEntry &e) {
                log << "started";
                tokens << token;
                started << e;
              });
      connect(&manager, &ServeManager::listChanged, &manager,
              [&](const QVector<ServeEntry> &l) {
                log << "listChanged";
                listed = l;
              });
      QStringList errorTexts;
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) {
                log << "error";
                errorTexts << m;
              });
      connect(&manager, &ServeManager::listFailed, &manager,
              [&](const QString &) { log << "listFailed"; });
      QCOMPARE(manager.start(p), quint64(1));
      QTRY_COMPARE_WITH_TIMEOUT(log.size(), 2, 2000);
      QCOMPARE(log, (QStringList{"started", "listChanged"}));
      QCOMPARE(tokens, QVector<quint64>{1});
      QCOMPARE(fake.paths(), (QStringList{"serve/start", "serve/list"}));
      QCOMPARE(fake.calls.at(0).params, serveStartParams(p));
      for (const Call &c : fake.calls) {
        QCOMPARE(c.timeoutMs, 30000);
      }
      QCOMPARE(started.size(), 1);
      QCOMPARE(started.at(0).id, QString("http-8ae93585"));
      QCOMPARE(started.at(0).addr, QString("127.0.0.1:40855"));
      QCOMPARE(started.at(0).type, QString("http"));
      QCOMPARE(started.at(0).fs, QString("loc:/tmp/w"));
      QCOMPARE(listed.size(), 2);

      // an ok reply without id or without addr is an error, then the list again
      fake.script["serve/start"] = okReply(QJsonObject{{"addr", "x"}});
      log.clear();
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(log.size(), 2, 2000);
      QCOMPARE(log, (QStringList{"error", "listChanged"}));
      QCOMPARE(errorTexts, QStringList{"invalid serve/start reply"});

      fake.script["serve/start"] = okReply(QJsonObject{{"id", "http-1"}});
      log.clear();
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(log.size(), 2, 2000);
      QCOMPARE(log, (QStringList{"error", "listChanged"}));
      QCOMPARE(errorTexts, (QStringList{"invalid serve/start reply",
                                        "invalid serve/start reply"}));
      QCOMPARE(started.size(), 1);
    }

    // (b) start fails: rclone's text, no started, the list follows
    {
      Fake fake;
      const QString text = "could not find serve type=\"nosuch\"";
      fake.script["serve/start"] = errReply(500, text);
      ServeManager manager(fake.fn());
      QStringList errors;
      int started = 0, listed = 0, listFailed = 0;
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) { errors << m; });
      connect(&manager, &ServeManager::listFailed, &manager,
              [&](const QString &) { ++listFailed; });
      connect(&manager, &ServeManager::started, &manager,
              [&](quint64, const ServeEntry &) { ++started; });
      connect(&manager, &ServeManager::listChanged, &manager,
              [&](const QVector<ServeEntry> &) { ++listed; });
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(listed, 1, 2000);
      QCOMPARE(errors, QStringList{text});
      QCOMPARE(started, 0);
      QCOMPARE(fake.paths(), (QStringList{"serve/start", "serve/list"}));

      // no text from rclone: the status
      fake.script["serve/start"] = errReply(503, QString());
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 2, 2000);
      QCOMPARE(errors.at(1), QString("HTTP 503"));
      fake.script["serve/start"] = errReply(0, QString());
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 3, 2000);
      QCOMPARE(errors.at(2), QString("no reply"));
      QTRY_COMPARE_WITH_TIMEOUT(listed, 3, 2000);
      QCOMPARE(listFailed, 0);
    }

    // (c) stop: the id only, then the list; a failure keeps rclone's text
    {
      Fake fake;
      ServeManager manager(fake.fn());
      QStringList errors;
      int listed = 0, listFailed = 0;
      connect(&manager, &ServeManager::error, &manager,
              [&](const QString &m) { errors << m; });
      connect(&manager, &ServeManager::listFailed, &manager,
              [&](const QString &) { ++listFailed; });
      connect(&manager, &ServeManager::listChanged, &manager,
              [&](const QVector<ServeEntry> &) { ++listed; });
      manager.stop("http-1");
      QTRY_COMPARE_WITH_TIMEOUT(listed, 1, 2000);
      QCOMPARE(fake.paths(), (QStringList{"serve/stop", "serve/list"}));
      QCOMPARE(fake.calls.at(0).params,
               (QJsonObject{{"id", "http-1"}}));
      QVERIFY(errors.isEmpty());
      QCOMPARE(listFailed, 0);

      const QString text = "server with id=\"x\" not found";
      fake.script["serve/stop"] = errReply(500, text);
      manager.stop("x");
      QTRY_COMPARE_WITH_TIMEOUT(listed, 2, 2000);
      QCOMPARE(errors, QStringList{text});
      QCOMPARE(listFailed, 0);
    }

    // (d) refresh: a failure is listFailed only; a stale reply is ignored
    {
      Fake fake;
      fake.script["serve/list"] = errReply(500, "boom");
      ServeManager manager(fake.fn());
      QStringList failed;
      int listed = 0;
      connect(&manager, &ServeManager::listFailed, &manager,
              [&](const QString &m) { failed << m; });
      connect(&manager, &ServeManager::listChanged, &manager,
              [&](const QVector<ServeEntry> &) { ++listed; });
      manager.refresh();
      QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 2000);
      QCOMPARE(failed, QStringList{"boom"});
      QCOMPARE(listed, 0);

      fake.script["serve/list"] = okReply(jsonOf(kR15List));
      failed.clear();
      manager.refresh();
      manager.refresh();
      QTRY_COMPARE_WITH_TIMEOUT(listed, 1, 2000);
      QTest::qWait(50);
      QCOMPARE(listed, 1);
      QVERIFY(failed.isEmpty());

      // a stale failure after a newer success: neither listFailed nor listChanged
      // for the stale one
      {
        Fake f2;
        ServeManager m2(f2.fn());
        QStringList failed2;
        int listed2 = 0;
        connect(&m2, &ServeManager::listFailed, &m2,
                [&](const QString &m) { failed2 << m; });
        connect(&m2, &ServeManager::listChanged, &m2,
                [&](const QVector<ServeEntry> &) { ++listed2; });
        f2.script["serve/list"] = errReply(500, "stale");
        m2.refresh();
        f2.script["serve/list"] = okReply(jsonOf(kR15List));
        m2.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(listed2, 1, 2000);
        QTest::qWait(50);
        QCOMPARE(listed2, 1);
        QVERIFY(failed2.isEmpty());
      }

      // the converse: a stale success after a newer failure is ignored too
      {
        Fake f3;
        ServeManager m3(f3.fn());
        QStringList failed3;
        int listed3 = 0;
        connect(&m3, &ServeManager::listFailed, &m3,
                [&](const QString &m) { failed3 << m; });
        connect(&m3, &ServeManager::listChanged, &m3,
                [&](const QVector<ServeEntry> &) { ++listed3; });
        f3.script["serve/list"] = okReply(jsonOf(kR15List));
        m3.refresh();
        f3.script["serve/list"] = errReply(500, "fresh");
        m3.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(failed3.size(), 1, 2000);
        QTest::qWait(50);
        QCOMPARE(failed3, QStringList{"fresh"});
        QCOMPARE(listed3, 0);
      }
    }

    // (e) types
    {
      Fake fake;
      fake.script["serve/types"] = okReply(jsonOf(
          R"({"types":["dlna","ftp","http","nfs","restic","s3","sftp","webdav"]})"));
      ServeManager manager(fake.fn());
      QVERIFY(manager.types().isEmpty());
      QSignalSpy spy(&manager, &ServeManager::typesChanged);
      manager.loadTypes();
      QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
      const QStringList types = spy.at(0).at(0).toStringList();
      QCOMPARE(types.size(), 8);
      QCOMPARE(manager.types(), types);
      QCOMPARE(fake.paths(), QStringList{"serve/types"});

      // a failure only logs: the cache and the signal stay as they were
      fake.script["serve/types"] = errReply(500, "no");
      QTest::ignoreMessage(QtWarningMsg, "serve/types failed: no");
      manager.loadTypes();
      QTest::qWait(50);
      QCOMPARE(spy.count(), 1);
      QCOMPARE(manager.types(), types);
    }

    // (f) a manager deleted while a call is pending: the reply is dropped
    {
      Fake fake;
      auto *manager = new ServeManager(fake.fn());
      int emitted = 0;
      connect(manager, &ServeManager::started, this,
              [&](quint64, const ServeEntry &) { ++emitted; });
      connect(manager, &ServeManager::listChanged, this,
              [&](const QVector<ServeEntry> &) { ++emitted; });
      manager->start(p);
      manager->refresh();
      manager->stop("x");
      manager->loadTypes();
      delete manager;
      QTest::qWait(50);
      QCOMPARE(fake.calls.size(), 4);
      QCOMPARE(emitted, 0);
    }
  }
};

QTEST_GUILESS_MAIN(TestServeManager)
#include "tst_serve_manager.moc"
