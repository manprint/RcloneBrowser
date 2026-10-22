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

  RcCallFn fn() {
    return [this](const QString &path, const QJsonObject &params,
                  std::function<void(const RcReply &)> done, int timeoutMs) {
      calls.append(Call{path, params, timeoutMs});
      const RcReply reply = script.contains(path) ? script.value(path)
                                                  : okReply();
      QTimer::singleShot(0, [done, reply]() { done(reply); });
    };
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
      connect(&manager, &ServeManager::started, &manager,
              [&](const ServeEntry &e) {
                log << "started";
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
      manager.start(p);
      QTRY_COMPARE_WITH_TIMEOUT(log.size(), 2, 2000);
      QCOMPARE(log, (QStringList{"started", "listChanged"}));
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
              [&](const ServeEntry &) { ++started; });
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
              [&](const ServeEntry &) { ++emitted; });
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
