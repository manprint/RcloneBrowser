#include <QtTest>

#include "config_flow.h"
#include "rb_test_support.h"

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

RcReply remotesReply(const QStringList &names) {
  QJsonArray a;
  for (const QString &n : names) {
    a.append(n);
  }
  return okReply(QJsonObject{{"remotes", a}});
}

RcReply doneReply() {
  return okReply(QJsonObject{{"State", ""}, {"Error", ""}, {"Result", ""}});
}

// A step asking `name`; the extra keys of the Option come from `extra`.
RcReply stepReply(const QString &state, const QString &name,
                  const QJsonObject &extra = QJsonObject(),
                  const QString &error = QString()) {
  QJsonObject option{{"Name", name}, {"Type", "string"}, {"DefaultStr", ""}};
  for (auto it = extra.begin(); it != extra.end(); ++it) {
    option.insert(it.key(), it.value());
  }
  return okReply(QJsonObject{
      {"State", state}, {"Option", option}, {"Error", error}, {"Result", ""}});
}

// A scripted RcCallFn. Like RcClient::call it always replies asynchronously.
// config/listremotes answers from `listReplies` (the last entry repeats),
// config/oauthstop fails the held (blocked) call, everything else asks
// `script`; no answer from `script` holds the call open.
struct Fake {
  QVector<Call> calls;
  QList<QStringList> listReplies;
  std::function<std::optional<RcReply>(const Call &)> script;
  std::function<void(const RcReply &)> held;

  RcCallFn fn() {
    return [this](const QString &path, const QJsonObject &params,
                  std::function<void(const RcReply &)> done, int timeoutMs) {
      calls.append(Call{path, params, timeoutMs});
      if (path == "config/oauthstop") {
        const auto blocked = held;
        held = nullptr;
        QTimer::singleShot(0, [blocked, done]() {
          if (blocked) {
            blocked(errReply(500, "config failed to refresh token: oauth "
                                  "authentication was cancelled"));
          }
          done(okReply());
        });
        return;
      }
      std::optional<RcReply> r;
      if (path == "config/listremotes") {
        QStringList names;
        if (listReplies.size() > 1) {
          names = listReplies.takeFirst();
        } else if (!listReplies.isEmpty()) {
          names = listReplies.first();
        }
        r = remotesReply(names);
      } else if (script) {
        r = script(calls.last());
      } else {
        r = okReply();
      }
      if (!r) {
        held = done;
        return;
      }
      const RcReply reply = *r;
      QTimer::singleShot(0, [done, reply]() { done(reply); });
    };
  }

  QVector<Call> to(const QString &path) const {
    QVector<Call> out;
    for (const Call &c : calls) {
      if (c.path == path) {
        out.append(c);
      }
    }
    return out;
  }

  int indexOf(const QString &path, int from = 0) const {
    for (int i = from; i < calls.size(); ++i) {
      if (calls.at(i).path == path) {
        return i;
      }
    }
    return -1;
  }
};

QJsonObject optOf(const Call &c) { return c.params.value("opt").toObject(); }
QJsonObject paramsOf(const Call &c) {
  return c.params.value("parameters").toObject();
}
bool isStart(const Call &c) { return !optOf(c).contains("continue"); }

} // namespace

class TestConfigFlow : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-CFG-OPTION
  void T_CFG_OPTION() {
    const QByteArray json = R"({"Name":"pass","FieldName":"","Help":"SSH password, leave blank to use ssh-agent.","Default":"","Value":null,"DefaultStr":"","ValueStr":"","Required":false,"IsPassword":true,"Sensitive":true,"Advanced":false,"Exclusive":false,"Hide":0,"NoPrefix":false,"Type":"string","Examples":null})";
    const ConfigOption o = configOptionFromJson(
        QJsonDocument::fromJson(json).object());
    QCOMPARE(o.name, QString("pass"));
    QCOMPARE(o.help, QString("SSH password, leave blank to use ssh-agent."));
    QCOMPARE(o.type, QString("string"));
    QVERIFY(o.isPassword);
    QVERIFY(!o.required);
    QVERIFY(!o.exclusive);
    QVERIFY(!o.advanced);
    QVERIFY(o.defaultStr.isEmpty());
    QVERIFY(o.examples.isEmpty());

    const ConfigOption e = configOptionFromJson(
        QJsonDocument::fromJson(
            R"({"Name":"mode","Type":"string","DefaultStr":"a","Required":true,"Exclusive":true,"Advanced":true,"Examples":[{"Value":"a","Help":"Alpha"},{"Value":"b","Help":"Beta"}]})")
            .object());
    QCOMPARE(e.name, QString("mode"));
    QCOMPARE(e.defaultStr, QString("a"));
    QVERIFY(e.required);
    QVERIFY(e.exclusive);
    QVERIFY(e.advanced);
    QVERIFY(!e.isPassword);
    QCOMPARE(e.examples.size(), 2);
    QCOMPARE(e.examples.at(0).first, QString("a"));
    QCOMPARE(e.examples.at(0).second, QString("Alpha"));
    QCOMPARE(e.examples.at(1).first, QString("b"));
    QCOMPARE(e.examples.at(1).second, QString("Beta"));

    // missing keys give defaults
    const ConfigOption d = configOptionFromJson(QJsonObject());
    QVERIFY(d.name.isEmpty());
    QVERIFY(d.type.isEmpty());
    QVERIFY(!d.required && !d.isPassword && !d.exclusive && !d.advanced);
    QVERIFY(d.examples.isEmpty());

    // answer text
    ConfigOption b;
    b.type = "bool";
    QCOMPARE(configAnswerText(b, true), QString("true"));
    QCOMPARE(configAnswerText(b, false), QString("false"));
    QCOMPARE(configAnswerText(o, "x"), QString("x"));
  }

  // T-CFG-STEP
  void T_CFG_STEP() {
    const ConfigStep done = parseConfigStep(
        QJsonObject{{"State", ""}, {"Error", ""}, {"Result", ""}});
    QVERIFY(done.done);
    QVERIFY(!done.option);
    QVERIFY(done.error.isEmpty());

    const ConfigStep more = parseConfigStep(QJsonObject{
        {"State", "*all,1,"},
        {"Option", QJsonObject{{"Name", "host"}}},
        {"Error", "bad"},
        {"Result", "r"}});
    QVERIFY(!more.done);
    QCOMPARE(more.state, QString("*all,1,"));
    QVERIFY(more.option.has_value());
    QCOMPARE(more.option->name, QString("host"));
    QCOMPARE(more.error, QString("bad"));
    QCOMPARE(more.result, QString("r"));
  }

  // T-CFG-PROVIDERS
  void T_CFG_PROVIDERS() {
    const QJsonObject reply = QJsonDocument::fromJson(
        R"({"providers":[
          {"Name":"s3","Description":"Amazon S3","Prefix":"s3","Hide":false,"Options":[]},
          {"Name":"alias","Description":"alias for an existing remote","Prefix":"alias","Hide":0,"Options":[]},
          {"Name":"b2","Description":"Backblaze B2","Prefix":"b2","Hide":1,"Options":[]}]})")
                                    .object();
    const QVector<ProviderInfo> p = parseProviders(reply);
    QCOMPARE(p.size(), 3);
    QCOMPARE(p.at(0).name, QString("alias"));
    QCOMPARE(p.at(1).name, QString("s3"));
    QCOMPARE(p.at(2).name, QString("b2"));
    QCOMPARE(p.at(1).description, QString("Amazon S3"));
    QCOMPARE(p.at(1).prefix, QString("s3"));
    QVERIFY(!p.at(0).hide);
    QVERIFY(!p.at(1).hide);
    QVERIFY(p.at(2).hide);

    const QVector<ProviderInfo> q = parseProviders(QJsonObject{
        {"providers",
         QJsonArray{QJsonObject{{"Name", "z"}, {"Description", "zeta"}, {"Hide", true}},
                    QJsonObject{{"Name", "a"}, {"Description", "Alpha"}}}}});
    QCOMPARE(q.size(), 2);
    QCOMPARE(q.at(0).name, QString("a"));
    QVERIFY(q.at(1).hide);
    QVERIFY(parseProviders(QJsonObject()).isEmpty());
  }

  // T-CFG-NAME
  void T_CFG_NAME() {
    for (const char *ok : {"a", "my remote", "x.y@z+1-2", "_a", "a-", "Å b"}) {
      QVERIFY2(isValidRemoteName(QString::fromUtf8(ok)), ok);
    }
    for (const char *bad : {" a", "a ", "-a", "a:b", "a/b", "a\\b", "", " ",
                            "a\n"}) {
      QVERIFY2(!isValidRemoteName(QString::fromUtf8(bad)), bad);
    }
  }

  // T-CFG-ANSWER
  void T_CFG_ANSWER() {
    Fake fake;
    fake.script = [](const Call &c) -> std::optional<RcReply> {
      if (isStart(c)) {
        return stepReply("s1", "host", QJsonObject{{"Required", true}});
      }
      const QString state = optOf(c).value("state").toString();
      if (state == "s1") {
        // a step with a non-empty Error re-asks the same option
        return stepReply("s1b", "host", QJsonObject{{"Required", true}}, "bad");
      }
      if (state == "s1b") {
        return stepReply("s2", "pass", QJsonObject{{"IsPassword", true}});
      }
      return doneReply();
    };
    ConfigFlow flow(fake.fn());
    QSignalSpy question(&flow, &ConfigFlow::question);
    QSignalSpy finished(&flow, &ConfigFlow::finished);

    flow.startCreate("r", "sftp");
    QTRY_COMPARE(question.count(), 1);
    QCOMPARE(fake.calls.at(0).path, QString("config/listremotes"));
    QCOMPARE(fake.calls.at(1).path, QString("config/create"));
    QCOMPARE(fake.calls.at(1).params.value("name").toString(), QString("r"));
    QCOMPARE(fake.calls.at(1).params.value("type").toString(),
             QString("sftp"));
    QVERIFY(paramsOf(fake.calls.at(1)).isEmpty());
    QCOMPARE(optOf(fake.calls.at(1)),
             (QJsonObject{{"nonInteractive", true}, {"all", true}}));
    QCOMPARE(question.at(0).at(0).value<ConfigOption>().name, QString("host"));
    QVERIFY(question.at(0).at(1).toString().isEmpty());

    flow.answer("h");
    QTRY_COMPARE(question.count(), 2);
    const Call &c1 = fake.calls.at(2);
    QCOMPARE(c1.path, QString("config/create"));
    QCOMPARE(c1.timeoutMs, 30000);
    QCOMPARE(c1.params.value("type").toString(), QString("sftp"));
    QCOMPARE(paramsOf(c1), (QJsonObject{{"host", "h"}}));
    QCOMPARE(optOf(c1).value("result").toString(), QString("h"));
    QCOMPARE(optOf(c1).value("continue").toBool(), true);
    QCOMPARE(optOf(c1).value("nonInteractive").toBool(), true);
    QCOMPARE(optOf(c1).value("state").toString(), QString("s1"));
    QCOMPARE(question.at(1).at(0).value<ConfigOption>().name, QString("host"));
    QCOMPARE(question.at(1).at(1).toString(), QString("bad"));
    QCOMPARE(finished.count(), 0);

    flow.answer("h2");
    QTRY_COMPARE(question.count(), 3);
    const Call &c1b = fake.calls.at(3);
    QCOMPARE(paramsOf(c1b), (QJsonObject{{"host", "h2"}}));
    QCOMPARE(optOf(c1b).value("state").toString(), QString("s1b"));
    QCOMPARE(question.at(2).at(0).value<ConfigOption>().name, QString("pass"));
    QVERIFY(question.at(2).at(0).value<ConfigOption>().isPassword);
    QVERIFY(question.at(2).at(1).toString().isEmpty());

    QCOMPARE(finished.count(), 0);
    flow.answer("pw");
    QTRY_COMPARE(finished.count(), 1);
    const Call &c2 = fake.calls.at(4);
    QCOMPARE(paramsOf(c2), (QJsonObject{{"host", "h2"}, {"pass", "pw"}}));
    QCOMPARE(optOf(c2).value("result").toString(), QString("pw"));
    QCOMPARE(optOf(c2).value("state").toString(), QString("s2"));
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    QVERIFY(finished.at(0).at(1).toString().isEmpty());
    QCOMPARE(fake.calls.size(), 5); // no cleanup after success
    QTest::qWait(50);
    QCOMPARE(finished.count(), 1);

    // answer() without a question is ignored
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("answer"));
    flow.answer("late");
    QCOMPARE(fake.calls.size(), 5);
  }

  // T-CFG-ADVANCED
  void T_CFG_ADVANCED() {
    Fake fake;
    fake.script = [](const Call &c) -> std::optional<RcReply> {
      if (isStart(c)) {
        return stepReply("a1", "config_fs_advanced",
                         QJsonObject{{"Type", "bool"},
                                     {"Exclusive", true},
                                     {"DefaultStr", "false"}});
      }
      const QString state = optOf(c).value("state").toString();
      if (state == "a1") {
        return stepReply("a2", "adv_opt",
                         QJsonObject{{"Advanced", true}, {"DefaultStr", "dv"}});
      }
      if (state == "a2") {
        return stepReply("a3", "adv_other",
                         QJsonObject{{"Advanced", true}, {"DefaultStr", ""}});
      }
      return doneReply();
    };
    ConfigFlow flow(fake.fn());
    QSignalSpy question(&flow, &ConfigFlow::question);
    QSignalSpy finished(&flow, &ConfigFlow::finished);

    flow.startCreate("r", "sftp");
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(question.count(), 0);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    const QVector<Call> create = fake.to("config/create");
    QCOMPARE(create.size(), 4);
    QCOMPARE(optOf(create.at(1)).value("result").toString(), QString("false"));
    QCOMPARE(optOf(create.at(2)).value("result").toString(), QString("dv"));
    QCOMPARE(optOf(create.at(3)).value("result").toString(), QString(""));
    // the default of an advanced option is never echoed
    QVERIFY(paramsOf(create.at(3)).isEmpty());
  }

  // T-CFG-PARAMS (R18)
  void T_CFG_PARAMS() {
    Fake fake;
    int n = 0;
    fake.script = [&n](const Call &c) -> std::optional<RcReply> {
      if (isStart(c)) {
        n = 0;
      }
      switch (n++) {
      case 0:
        return stepReply("p1", "client_id");
      case 1:
        return stepReply("p2", "port", QJsonObject{{"Type", "int"}, {"DefaultStr", "22"}});
      case 2:
        return stepReply("p3", "config_note");
      case 3:
        return stepReply("p4", "pass", QJsonObject{{"IsPassword", true}});
      case 4:
        return stepReply("p5", "client_id"); // asked a second time
      case 5:
        return stepReply("p6", "flag", QJsonObject{{"Type", "bool"}, {"DefaultStr", "false"}});
      default:
        return doneReply();
      }
    };
    ConfigFlow flow(fake.fn());
    QSignalSpy question(&flow, &ConfigFlow::question);
    QSignalSpy finished(&flow, &ConfigFlow::finished);

    flow.startCreate("r", "x");
    QTRY_COMPARE(question.count(), 1);
    flow.answer("id1");
    QTRY_COMPARE(question.count(), 2);
    QCOMPARE(paramsOf(fake.calls.last()), (QJsonObject{{"client_id", "id1"}}));

    // the default is sent as result and not echoed
    flow.answer("22");
    QTRY_COMPARE(question.count(), 3);
    QCOMPARE(optOf(fake.calls.last()).value("result").toString(), QString("22"));
    QCOMPARE(paramsOf(fake.calls.last()), (QJsonObject{{"client_id", "id1"}}));

    // config_* answers are sent as result only
    flow.answer("z");
    QTRY_COMPARE(question.count(), 4);
    QCOMPARE(optOf(fake.calls.last()).value("result").toString(), QString("z"));
    QVERIFY(!paramsOf(fake.calls.last()).contains("config_note"));

    // an empty Create password is not echoed
    flow.answer("");
    QTRY_COMPARE(question.count(), 5);
    QCOMPARE(optOf(fake.calls.last()).value("result").toString(), QString(""));
    QVERIFY(!paramsOf(fake.calls.last()).contains("pass"));

    // a repeated option answered with its default drops the earlier entry
    flow.answer("");
    QTRY_COMPARE(question.count(), 6);
    QVERIFY(!paramsOf(fake.calls.last()).contains("client_id"));

    flow.answer(true);
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(optOf(fake.calls.last()).value("result").toString(), QString("true"));
    QCOMPARE(paramsOf(fake.calls.last()), (QJsonObject{{"flag", "true"}}));
    for (const Call &c : fake.calls) {
      for (const QString &k : paramsOf(c).keys()) {
        QVERIFY2(!k.startsWith("config_"), qPrintable(k));
      }
    }

    // Update: an empty password answer keeps the stored (obscured) value
    Fake up;
    up.script = [](const Call &c) -> std::optional<RcReply> {
      if (isStart(c)) {
        return stepReply("u1", "pass",
                         QJsonObject{{"IsPassword", true}, {"DefaultStr", "OBSC"}});
      }
      return doneReply();
    };
    ConfigFlow update(up.fn());
    QSignalSpy uq(&update, &ConfigFlow::question);
    QSignalSpy uf(&update, &ConfigFlow::finished);
    update.startUpdate("r");
    QTRY_COMPARE(uq.count(), 1);
    QCOMPARE(up.calls.at(1).path, QString("config/update"));
    QVERIFY(!up.calls.at(1).params.contains("type"));
    update.answer("");
    QTRY_COMPARE(uf.count(), 1);
    QCOMPARE(up.calls.last().path, QString("config/update"));
    QVERIFY(!up.calls.last().params.contains("type"));
    QCOMPARE(optOf(up.calls.last()).value("result").toString(), QString("OBSC"));
    QVERIFY(paramsOf(up.calls.last()).isEmpty());

    // Update: a new password is echoed
    Fake up2;
    up2.script = up.script;
    ConfigFlow update2(up2.fn());
    QSignalSpy uq2(&update2, &ConfigFlow::question);
    QSignalSpy uf2(&update2, &ConfigFlow::finished);
    update2.startUpdate("r");
    QTRY_COMPARE(uq2.count(), 1);
    update2.answer("n3w");
    QTRY_COMPARE(uf2.count(), 1);
    QCOMPARE(paramsOf(up2.calls.last()), (QJsonObject{{"pass", "n3w"}}));
    QCOMPARE(optOf(up2.calls.last()).value("result").toString(), QString("n3w"));
  }

  // T-CFG-LOOP
  void T_CFG_LOOP() {
    // the same auto-answered State again
    Fake fake;
    fake.listReplies = {{}, {"r"}};
    fake.script = [](const Call &) -> std::optional<RcReply> {
      return stepReply("L1", "config_fs_advanced", QJsonObject{{"Type", "bool"}});
    };
    ConfigFlow flow(fake.fn());
    QSignalSpy question(&flow, &ConfigFlow::question);
    QSignalSpy finished(&flow, &ConfigFlow::finished);
    flow.startCreate("r", "sftp");
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    const QString msg = finished.at(0).at(1).toString();
    QVERIFY2(msg.contains("did not finish"), qPrintable(msg));
    QVERIFY2(msg.contains("L1"), qPrintable(msg));
    QCOMPARE(question.count(), 0);
    // cleanup: listremotes, then delete as the fake lists the name
    QCOMPARE(fake.calls.at(fake.calls.size() - 2).path,
             QString("config/listremotes"));
    QCOMPARE(fake.calls.last().path, QString("config/delete"));
    QCOMPARE(fake.calls.last().params.value("name").toString(), QString("r"));
    QTest::qWait(50);
    QCOMPARE(finished.count(), 1);

    // Update never deletes
    Fake up;
    up.script = fake.script;
    ConfigFlow update(up.fn());
    QSignalSpy uf(&update, &ConfigFlow::finished);
    update.startUpdate("r");
    QTRY_COMPARE(uf.count(), 1);
    QVERIFY(uf.at(0).at(1).toString().contains("did not finish"));
    QCOMPARE(up.to("config/delete").size(), 0);
    QCOMPARE(up.to("config/listremotes").size(), 1);

    // more than 500 steps, each with a new State
    Fake many;
    int n = 0;
    many.script = [&n](const Call &) -> std::optional<RcReply> {
      return stepReply(QString("m%1").arg(n++), "opt",
                       QJsonObject{{"Advanced", true}});
    };
    ConfigFlow longFlow(many.fn());
    QSignalSpy lf(&longFlow, &ConfigFlow::finished);
    longFlow.startUpdate("r");
    QTRY_COMPARE_WITH_TIMEOUT(lf.count(), 1, 20000);
    QCOMPARE(lf.at(0).at(0).toBool(), false);
    QVERIFY(lf.at(0).at(1).toString().contains("did not finish"));
    QVERIFY(many.calls.size() <= 504);
    // the guard is keyed on the State, not on the option name (every step
    // asks "opt"), so the flow runs on until step 501
    QVERIFY(many.to("config/update").size() >= 500);
    QVERIFY2(lf.at(0).at(1).toString().contains("m500"),
             qPrintable(lf.at(0).at(1).toString()));
  }

  // T-CFG-OAUTH
  void T_CFG_OAUTH() {
    Fake fake;
    fake.listReplies = {{}, {"g"}};
    int polls = 0;
    fake.script = [&polls](const Call &c) -> std::optional<RcReply> {
      if (c.path == "config/oauthstatus") {
        return okReply(polls++ == 0
                           ? QJsonObject{{"status", "running"}}
                           : QJsonObject{{"status", "running"},
                                         {"authUrl", "http://127.0.0.1:53682/auth?state=x"}});
      }
      if (isStart(c)) {
        return stepReply("o1", "config_is_local",
                         QJsonObject{{"Type", "bool"}, {"DefaultStr", "true"}});
      }
      return std::nullopt; // the blocked call
    };
    ConfigFlow flow(fake.fn());
    QSignalSpy question(&flow, &ConfigFlow::question);
    QSignalSpy oauth(&flow, &ConfigFlow::oauthUrl);
    QSignalSpy finished(&flow, &ConfigFlow::finished);

    flow.startCreate("g", "drive");
    QTRY_COMPARE(question.count(), 1);
    QVERIFY(!flow.isOAuthRunning());
    flow.answer(true);
    QVERIFY(flow.isOAuthRunning());
    const Call &blocked = fake.calls.last();
    QCOMPARE(blocked.path, QString("config/create"));
    QCOMPARE(blocked.timeoutMs, 900000);
    QCOMPARE(optOf(blocked).value("result").toString(), QString("true"));

    QTRY_COMPARE_WITH_TIMEOUT(oauth.count(), 1, 5000);
    QCOMPARE(oauth.at(0).at(0).toUrl(),
             QUrl("http://127.0.0.1:53682/auth?state=x"));
    for (const Call &c : fake.to("config/oauthstatus")) {
      QCOMPARE(c.timeoutMs, 5000);
    }
    QTest::qWait(1200); // polling goes on, the URL is emitted once
    QCOMPARE(oauth.count(), 1);
    QVERIFY(flow.isOAuthRunning());
    QCOMPARE(finished.count(), 0);

    flow.cancel();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!flow.isOAuthRunning());
    QCOMPARE(finished.at(0).at(0).toBool(), false);
    QCOMPARE(finished.at(0).at(1).toString(), QString("cancelled"));
    const int stop = fake.indexOf("config/oauthstop");
    QVERIFY(stop > 0);
    const int list = fake.indexOf("config/listremotes", stop);
    QVERIFY(list > stop);
    QCOMPARE(fake.calls.last().path, QString("config/delete"));
    QCOMPARE(fake.calls.last().params.value("name").toString(), QString("g"));
    QCOMPARE(fake.to("config/delete").size(), 1);

    // no poll after the cancel
    const int pollsAfter = fake.to("config/oauthstatus").size();
    QTest::qWait(1200);
    QCOMPARE(fake.to("config/oauthstatus").size(), pollsAfter);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(oauth.count(), 1);
  }

  // T-CFG-CANCEL-EXISTING
  void T_CFG_CANCEL_EXISTING() {
    auto asking = [](const Call &c) -> std::optional<RcReply> {
      if (isStart(c)) {
        return stepReply("s1", "host");
      }
      return stepReply("s2", "user");
    };

    // Update: cancel never deletes
    {
      Fake fake;
      fake.listReplies = {{"r"}};
      fake.script = asking;
      ConfigFlow flow(fake.fn());
      QSignalSpy question(&flow, &ConfigFlow::question);
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startUpdate("r");
      QTRY_COMPARE(question.count(), 1);
      flow.cancel();
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(finished.at(0).at(0).toBool(), false);
      QCOMPARE(finished.at(0).at(1).toString(), QString("cancelled"));
      QCOMPARE(fake.to("config/delete").size(), 0);
      QCOMPARE(fake.to("config/listremotes").size(), 1);
      flow.cancel(); // no second finished
      QTest::qWait(50);
      QCOMPARE(finished.count(), 1);
    }

    // Create over an existing name
    {
      Fake fake;
      fake.listReplies = {{"a", "r"}};
      fake.script = asking;
      ConfigFlow flow(fake.fn());
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(finished.at(0).at(0).toBool(), false);
      QVERIFY(finished.at(0).at(1).toString().contains("already exists"));
      QVERIFY(finished.at(0).at(1).toString().contains("r"));
      QCOMPARE(fake.to("config/create").size(), 0);
      QCOMPARE(fake.to("config/delete").size(), 0);
      QCOMPARE(fake.calls.size(), 1);
    }

    // Create: cancel while a step call is in flight waits for its reply
    {
      Fake fake;
      fake.listReplies = {{}, {"r"}};
      fake.script = asking;
      ConfigFlow flow(fake.fn());
      QSignalSpy question(&flow, &ConfigFlow::question);
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      QTRY_COMPARE(question.count(), 1);
      flow.answer("h");
      flow.cancel(); // the continue call is in flight
      QCOMPARE(fake.calls.last().path, QString("config/create"));
      QCOMPARE(finished.count(), 0);
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(question.count(), 1); // the reply was ignored
      QCOMPARE(finished.at(0).at(1).toString(), QString("cancelled"));
      QCOMPARE(fake.calls.at(fake.calls.size() - 2).path,
               QString("config/listremotes"));
      QCOMPARE(fake.calls.last().path, QString("config/delete"));
    }

    // Create: cancel while a step call is in flight whose reply says done
    // still cleans up (cancel wins over done)
    {
      Fake fake;
      fake.listReplies = {{}, {"r"}};
      fake.script = [](const Call &c) -> std::optional<RcReply> {
        if (isStart(c)) {
          return stepReply("s1", "host");
        }
        return doneReply();
      };
      ConfigFlow flow(fake.fn());
      QSignalSpy question(&flow, &ConfigFlow::question);
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      QTRY_COMPARE(question.count(), 1);
      flow.answer("h");
      flow.cancel(); // the continue call is in flight and will return done
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(finished.at(0).at(0).toBool(), false);
      QCOMPARE(finished.at(0).at(1).toString(), QString("cancelled"));
      QCOMPARE(fake.calls.last().path, QString("config/delete"));
      QCOMPARE(fake.calls.last().params.value("name").toString(), QString("r"));
      QTest::qWait(50);
      QCOMPARE(finished.count(), 1);
    }

    // Create: cancel with a question outstanding cleans up at once; a name
    // that is not listed is not deleted
    {
      Fake fake;
      fake.listReplies = {{}};
      fake.script = asking;
      ConfigFlow flow(fake.fn());
      QSignalSpy question(&flow, &ConfigFlow::question);
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      QTRY_COMPARE(question.count(), 1);
      flow.cancel();
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(fake.to("config/delete").size(), 0);
      QCOMPARE(fake.to("config/listremotes").size(), 2);
    }

    // cancel while the start listing is in flight
    {
      Fake fake;
      fake.script = asking;
      ConfigFlow flow(fake.fn());
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      flow.cancel();
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(finished.at(0).at(1).toString(), QString("cancelled"));
      QCOMPARE(fake.calls.size(), 1);
    }

    // an HTTP error cleans up and reports the error; a destroyed flow is
    // never touched by late replies
    {
      Fake fake;
      fake.listReplies = {{}, {"r"}};
      fake.script = [](const Call &) -> std::optional<RcReply> {
        return errReply(500, "boom");
      };
      ConfigFlow flow(fake.fn());
      QSignalSpy finished(&flow, &ConfigFlow::finished);
      flow.startCreate("r", "sftp");
      QTRY_COMPARE(finished.count(), 1);
      QCOMPARE(finished.at(0).at(0).toBool(), false);
      QCOMPARE(finished.at(0).at(1).toString(), QString("boom"));
      QCOMPARE(fake.calls.last().path, QString("config/delete"));
    }
    {
      Fake fake;
      fake.script = asking;
      auto *flow = new ConfigFlow(fake.fn());
      flow->startCreate("r", "sftp");
      delete flow;
      QTest::qWait(50);
      QCOMPARE(fake.calls.size(), 1);
    }
  }
};

QTEST_GUILESS_MAIN(TestConfigFlow)
#include "tst_config_flow.moc"
