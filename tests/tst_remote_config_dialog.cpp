#include <QtTest>

#include "rb_test_support.h"
#include "remote_config_dialog.h"

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

QJsonObject provider(const QString &name, const QString &description,
                     bool hide = false) {
  return QJsonObject{{"Name", name},
                     {"Description", description},
                     {"Prefix", name},
                     {"Hide", hide},
                     {"Options", QJsonArray()}};
}

// A scripted RcCallFn. Like RcClient::call it always replies asynchronously,
// and (like MainWindow) only while the dialog is alive: `ctx` is the dialog.
// config/listremotes answers `remotes`, config/providers `providers`,
// config/oauthstop fails the held (blocked) call, config/create and
// config/update ask `script`, which may add to `remotes`; no answer from
// `script` holds the call open.
struct Fake {
  QVector<Call> calls;
  QStringList remotes;
  QJsonArray providers;
  QPointer<QObject> ctx;
  std::function<std::optional<RcReply>(const Call &)> script;
  std::function<void(const RcReply &)> held;

  RcCallFn fn() {
    return [this](const QString &path, const QJsonObject &params,
                  std::function<void(const RcReply &)> done, int timeoutMs) {
      calls.append(Call{path, params, timeoutMs});
      if (!ctx) {
        return;
      }
      if (path == "config/oauthstop") {
        const auto blocked = held;
        held = nullptr;
        QTimer::singleShot(0, ctx, [blocked, done]() {
          if (blocked) {
            RcReply r;
            r.httpStatus = 500;
            r.error = "config failed to refresh token: oauth authentication "
                      "was cancelled";
            blocked(r);
          }
          done(okReply());
        });
        return;
      }
      std::optional<RcReply> r;
      if (path == "config/listremotes") {
        QJsonArray a;
        for (const QString &n : remotes) {
          a.append(n);
        }
        r = okReply(QJsonObject{{"remotes", a}});
      } else if (path == "config/providers") {
        r = okReply(QJsonObject{{"providers", providers}});
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
      QTimer::singleShot(0, ctx, [done, reply]() { done(reply); });
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

  // The questions of a flow, in order; the end gives the done reply.
  void asks(QList<RcReply> steps) {
    script = [steps](const Call &c) mutable -> std::optional<RcReply> {
      if (c.path != "config/create" && c.path != "config/update") {
        return okReply();
      }
      return steps.isEmpty() ? doneReply() : steps.takeFirst();
    };
  }
};

QJsonObject optOf(const Call &c) { return c.params.value("opt").toObject(); }
QJsonObject paramsOf(const Call &c) {
  return c.params.value("parameters").toObject();
}

} // namespace

class TestRemoteConfigDialog : public QObject {
  Q_OBJECT

  static QWidget *editor(RemoteConfigDialog &d) {
    return d.findChild<QWidget *>("answerEditor");
  }
  static QPushButton *next(RemoteConfigDialog &d) {
    return d.findChild<QPushButton *>("next");
  }
  static QPushButton *cancel(RemoteConfigDialog &d) {
    return d.findChild<QPushButton *>("cancel");
  }
  // Counts the QMessageBox the dialog shows (the box is modal and would block):
  // a poller closes each one and records its text. With `watched` and
  // `rejectedCount` set it also records the state of the dialog at the moment
  // the box was on screen.
  struct BoxWatcher {
    QStringList texts;
    QVector<bool> dialogVisible;
    QVector<int> rejectedBefore;
    QPointer<QWidget> watched;
    std::function<int()> rejectedCount;
    QTimer timer;
    BoxWatcher() {
      timer.setInterval(10);
      QObject::connect(&timer, &QTimer::timeout, &timer, [this]() {
        if (auto *box =
                qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
          texts << box->text();
          dialogVisible << (watched && watched->isVisible());
          rejectedBefore << (rejectedCount ? rejectedCount() : -1);
          box->close();
        }
      });
      timer.start();
    }
  };
  // index of the first recorded call to `path`, -1 when absent
  static int indexOf(const Fake &f, const QString &path) {
    for (int i = 0; i < f.calls.size(); ++i) {
      if (f.calls.at(i).path == path) {
        return i;
      }
    }
    return -1;
  }
  // waits for the question page to show the question `name`
  static void waitQuestion(RemoteConfigDialog &d, const QString &name) {
    auto *title = d.findChild<QLabel *>("title");
    QVERIFY(title);
    QTRY_COMPARE_WITH_TIMEOUT(title->text(), name, 5000);
    QVERIFY(editor(d));
  }

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-RCD-PAGE1
  void T_RCD_PAGE1() {
    Fake fake;
    fake.remotes = {"taken"};
    fake.providers = {provider("sftp", "SSH/SFTP"),
                      provider("drive", "Google Drive"),
                      provider("alias", "Alias a remote", true),
                      // the name is not part of the description
                      provider("b2", "Backblaze")};
    RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Create,
                           QString());
    fake.ctx = &dlg;
    dlg.show();

    auto *providers = dlg.findChild<QListWidget *>("providers");
    auto *name = dlg.findChild<QLineEdit *>("name");
    auto *filter = dlg.findChild<QLineEdit *>("filter");
    auto *nameError = dlg.findChild<QLabel *>("nameError");
    QVERIFY(providers && name && filter && nameError);
    QTRY_COMPARE_WITH_TIMEOUT(providers->count(), 3, 5000);
    // sorted by description, the Hide entry is absent
    QCOMPARE(providers->item(0)->text(), QString("Backblaze (b2)"));
    QCOMPARE(providers->item(1)->text(), QString("Google Drive (drive)"));
    QCOMPARE(providers->item(1)->data(Qt::UserRole).toString(),
             QString("drive"));
    QCOMPARE(providers->item(2)->text(), QString("SSH/SFTP (sftp)"));

    QVERIFY(!next(dlg)->isEnabled());
    providers->setCurrentRow(0);
    name->setText("a:b");
    QVERIFY(!next(dlg)->isEnabled());
    name->setText("ok");
    QVERIFY(next(dlg)->isEnabled());
    QVERIFY(nameError->isHidden());
    providers->clearSelection();
    QVERIFY(!next(dlg)->isEnabled());
    providers->setCurrentRow(1);
    QVERIFY(next(dlg)->isEnabled());

    // a name that already exists
    name->setText("taken");
    QVERIFY(!next(dlg)->isEnabled());
    QVERIFY(!nameError->isHidden());
    QCOMPARE(nameError->text(), QString("remote taken already exists"));
    name->setText("fresh");
    QVERIFY(nameError->isHidden());
    QVERIFY(next(dlg)->isEnabled());

    // the filter matches description or name, case-insensitively
    filter->setText("GOOGLE");
    QCOMPARE(providers->count(), 1);
    QCOMPARE(providers->item(0)->data(Qt::UserRole).toString(),
             QString("drive"));
    filter->setText("sFtP");
    QCOMPARE(providers->count(), 1);
    QCOMPARE(providers->item(0)->data(Qt::UserRole).toString(),
             QString("sftp"));
    // a name that is not in the description
    filter->setText("B2");
    QCOMPARE(providers->count(), 1);
    QCOMPARE(providers->item(0)->data(Qt::UserRole).toString(), QString("b2"));
    // the hidden provider never shows, even when the filter matches it
    filter->setText("alias");
    QCOMPARE(providers->count(), 0);
    filter->clear();
    QCOMPARE(providers->count(), 3);

    // typing and filtering never fetched anything again
    QCOMPARE(fake.to("config/providers").size(), 1);
    QCOMPARE(fake.to("config/listremotes").size(), 1);

    // Cancel on page 1 has no flow to cancel: it rejects at once
    QSignalSpy rejected(&dlg, &QDialog::rejected);
    QVERIFY(dlg.isVisible());
    cancel(dlg)->click();
    QCOMPARE(rejected.count(), 1);
    QVERIFY(!dlg.isVisible());
    QVERIFY(!dlg.flowStarted());
    QVERIFY(fake.to("config/create").isEmpty());
    QVERIFY(fake.to("config/delete").isEmpty());
  }

  // T-RCD-EDITORS
  void T_RCD_EDITORS() {
    const QString longHelp = QStringLiteral("<b>x</b>") + QString(1292, 'y');
    QCOMPARE(longHelp.size(), 1300);
    const QJsonArray ab{QJsonObject{{"Value", "a"}, {"Help", "Alpha"}},
                        QJsonObject{{"Value", "b"}, {"Help", "Beta"}}};
    Fake fake;
    fake.asks(
        {stepReply("s1", "off",
                   {{"Type", "bool"}, {"DefaultStr", "false"},
                    {"Help", longHelp}},
                   "bad value"),
         stepReply("s2", "flag", {{"Type", "bool"}, {"DefaultStr", "true"}}),
         stepReply("s3", "mode",
                   {{"Exclusive", true},
                    {"DefaultStr", "b"},
                    {"Examples", ab}}),
         stepReply("s4", "region",
                   {{"DefaultStr", "r2"},
                    {"Examples",
                     QJsonArray{QJsonObject{{"Value", "r1"}, {"Help", ""}},
                                QJsonObject{{"Value", "r2"}, {"Help", ""}}}}}),
         // a stored value that is not one of the examples
         stepReply("s5", "stale",
                   {{"Exclusive", true},
                    {"DefaultStr", "c"},
                    {"Examples", ab}}),
         // an empty default that is not one of the examples
         stepReply("s6", "blank",
                   {{"Exclusive", true},
                    {"DefaultStr", ""},
                    {"Examples", ab}}),
         stepReply("s7", "pass",
                   {{"IsPassword", true}, {"DefaultStr", "OBSC"}}),
         stepReply("s8", "host", {{"DefaultStr", "h.example"}})});
    RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Update, "e");
    fake.ctx = &dlg;
    dlg.setShowErrors(false);
    dlg.show();
    QCOMPARE(dlg.windowTitle(), QString("Edit remote e"));

    // bool defaulting to false: unchecked; the answer is "false"
    waitQuestion(dlg, "off");
    auto *box = qobject_cast<QCheckBox *>(editor(dlg));
    QVERIFY(box);
    QVERIFY(!box->isChecked());
    // the re-ask error and the help of the question
    auto *error = dlg.findChild<QLabel *>("error");
    auto *help = dlg.findChild<QLabel *>("help");
    QVERIFY(error && help);
    QVERIFY(!error->isHidden());
    QCOMPARE(error->text(), QString("bad value"));
    QCOMPARE(help->textFormat(), Qt::PlainText);
    QVERIFY(help->wordWrap());
    QCOMPARE(help->text().size(), 1200);
    QCOMPARE(help->text(), longHelp.left(1200));
    next(dlg)->click();

    // bool defaulting to true: checked; unchecking answers "false"
    waitQuestion(dlg, "flag");
    QVERIFY(error->isHidden()); // no Error on this question
    box = qobject_cast<QCheckBox *>(editor(dlg));
    QVERIFY(box);
    QVERIFY(box->isChecked());
    box->setChecked(false);
    next(dlg)->click();

    // exclusive: the default is selected, the text sent is the value
    waitQuestion(dlg, "mode");
    auto *combo = qobject_cast<QComboBox *>(editor(dlg));
    QVERIFY(combo);
    QVERIFY(!combo->isEditable());
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->currentText(), QString("b"));
    QCOMPARE(combo->itemData(0, Qt::ToolTipRole).toString(), QString("Alpha"));
    combo->setCurrentIndex(0);
    next(dlg)->click();

    // editable: starts with the default (not the first example)
    waitQuestion(dlg, "region");
    combo = qobject_cast<QComboBox *>(editor(dlg));
    QVERIFY(combo);
    QVERIFY(combo->isEditable());
    QCOMPARE(combo->currentText(), QString("r2"));
    combo->setEditText("r9");
    next(dlg)->click();

    // exclusive with a default that is not listed: unchanged input answers it
    waitQuestion(dlg, "stale");
    combo = qobject_cast<QComboBox *>(editor(dlg));
    QVERIFY(combo);
    QCOMPARE(combo->currentText(), QString("c"));
    QCOMPARE(combo->count(), 3);
    next(dlg)->click();

    waitQuestion(dlg, "blank");
    combo = qobject_cast<QComboBox *>(editor(dlg));
    QVERIFY(combo);
    QCOMPARE(combo->currentText(), QString());
    QCOMPARE(combo->count(), 3);
    next(dlg)->click();

    waitQuestion(dlg, "pass");
    auto *pass = qobject_cast<QLineEdit *>(editor(dlg));
    QVERIFY(pass);
    QCOMPARE(pass->echoMode(), QLineEdit::Password);
    QVERIFY(pass->text().isEmpty());
    QCOMPARE(pass->placeholderText(), QString("(unchanged)"));
    next(dlg)->click();

    waitQuestion(dlg, "host");
    auto *host = qobject_cast<QLineEdit *>(editor(dlg));
    QVERIFY(host);
    QCOMPARE(host->echoMode(), QLineEdit::Normal);
    QCOMPARE(host->text(), QString("h.example"));
    QSignalSpy accepted(&dlg, &QDialog::accepted);
    QSignalSpy finished(&dlg, &QDialog::finished);
    next(dlg)->click();

    QTRY_COMPARE_WITH_TIMEOUT(accepted.count(), 1, 5000);
    QVERIFY(!dlg.isVisible());
    QCOMPARE(dlg.result(), int(QDialog::Accepted));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toInt(), int(QDialog::Accepted));
    // Update mode: no listing of the providers, no create
    QVERIFY(fake.to("config/providers").isEmpty());
    QVERIFY(fake.to("config/create").isEmpty());

    // updates.at(0) starts the flow, updates.at(k) answers question k
    const QVector<Call> updates = fake.to("config/update");
    QCOMPARE(updates.size(), 9);
    QCOMPARE(optOf(updates.at(1)).value("result").toString(), QString("false"));
    QCOMPARE(optOf(updates.at(2)).value("result").toString(), QString("false"));
    QCOMPARE(paramsOf(updates.at(2)).value("flag").toString(),
             QString("false"));
    QCOMPARE(optOf(updates.at(3)).value("result").toString(), QString("a"));
    QCOMPARE(paramsOf(updates.at(3)).value("mode").toString(), QString("a"));
    QCOMPARE(optOf(updates.at(4)).value("result").toString(), QString("r9"));
    QCOMPARE(paramsOf(updates.at(4)).value("region").toString(),
             QString("r9"));
    // an untouched stored value is answered as it is and not echoed
    QCOMPARE(optOf(updates.at(5)).value("result").toString(), QString("c"));
    QVERIFY(!paramsOf(updates.at(5)).contains("stale"));
    QCOMPARE(optOf(updates.at(6)).value("result").toString(), QString());
    QVERIFY(!paramsOf(updates.at(6)).contains("blank"));
    // the empty password keeps the stored one (R18) and is not echoed
    QCOMPARE(optOf(updates.at(7)).value("result").toString(), QString("OBSC"));
    QVERIFY(!paramsOf(updates.last()).contains("pass"));
  }

  // T-RCD-REQUIRED
  void T_RCD_REQUIRED() {
    {
      Fake fake;
      fake.asks({stepReply("s1", "host", {{"Required", true}})});
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Update, "e");
      fake.ctx = &dlg;
      dlg.setShowErrors(false);
      dlg.show();

      waitQuestion(dlg, "host");
      auto *host = qobject_cast<QLineEdit *>(editor(dlg));
      QVERIFY(host);
      QVERIFY(host->text().isEmpty());
      QVERIFY(!next(dlg)->isEnabled());
      host->setText("x");
      QVERIFY(next(dlg)->isEnabled());
      host->clear();
      QVERIFY(!next(dlg)->isEnabled());

      // Update mode: the flow has nothing to clean up and finishes at once
      QSignalSpy rejected(&dlg, &QDialog::rejected);
      cancel(dlg)->click();
      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
      QVERIFY(!dlg.isVisible());
      QVERIFY(fake.to("config/update").size() == 1);
    }

    // Create mode: a Required password needs a value
    {
      Fake fake;
      fake.providers = {provider("sftp", "SSH/SFTP")};
      fake.asks({stepReply("s1", "pass",
                           {{"Required", true}, {"IsPassword", true}})});
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Create,
                             QString());
      fake.ctx = &dlg;
      dlg.setShowErrors(false);
      dlg.show();
      auto *providers = dlg.findChild<QListWidget *>("providers");
      QTRY_COMPARE_WITH_TIMEOUT(providers->count(), 1, 5000);
      providers->setCurrentRow(0);
      dlg.findChild<QLineEdit *>("name")->setText("req1");
      next(dlg)->click();

      waitQuestion(dlg, "pass");
      auto *pass = qobject_cast<QLineEdit *>(editor(dlg));
      QVERIFY(pass);
      QVERIFY(pass->text().isEmpty());
      QVERIFY(!next(dlg)->isEnabled());
      pass->setText("x");
      QVERIFY(next(dlg)->isEnabled());
      pass->clear();
      QVERIFY(!next(dlg)->isEnabled());

      QSignalSpy rejected(&dlg, &QDialog::rejected);
      cancel(dlg)->click();
      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
    }

    // Update mode: an empty Required password keeps the stored one
    {
      Fake fake;
      fake.asks({stepReply("s1", "pass",
                           {{"Required", true},
                            {"IsPassword", true},
                            {"DefaultStr", "OBSC"}})});
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Update, "e");
      fake.ctx = &dlg;
      dlg.setShowErrors(false);
      dlg.show();

      waitQuestion(dlg, "pass");
      auto *pass = qobject_cast<QLineEdit *>(editor(dlg));
      QVERIFY(pass);
      QVERIFY(pass->text().isEmpty());
      QVERIFY(next(dlg)->isEnabled());
      QSignalSpy accepted(&dlg, &QDialog::accepted);
      next(dlg)->click();
      QTRY_COMPARE_WITH_TIMEOUT(accepted.count(), 1, 5000);
      const QVector<Call> updates = fake.to("config/update");
      QCOMPARE(updates.size(), 2);
      QCOMPARE(optOf(updates.at(1)).value("result").toString(),
               QString("OBSC"));
      QVERIFY(!paramsOf(updates.at(1)).contains("pass"));
    }
  }

  // T-RCD-FLOW
  void T_RCD_FLOW() {
    {
      Fake fake;
      fake.providers = {provider("sftp", "SSH/SFTP")};
      fake.asks({stepReply("s1", "host"), stepReply("s2", "user"),
                 stepReply("s3", "pass",
                           {{"IsPassword", true}, {"DefaultStr", "OBSC"}})});
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Create,
                             QString());
      fake.ctx = &dlg;
      dlg.setShowErrors(false);
      dlg.show();
      QCOMPARE(dlg.windowTitle(), QString("New remote"));

      auto *providers = dlg.findChild<QListWidget *>("providers");
      QTRY_COMPARE_WITH_TIMEOUT(providers->count(), 1, 5000);
      providers->setCurrentRow(0);
      dlg.findChild<QLineEdit *>("name")->setText("new1");
      QVERIFY(next(dlg)->isEnabled());
      next(dlg)->click();
      // the page is disabled until the first question arrives
      QVERIFY(!next(dlg)->isEnabled());

      QSignalSpy accepted(&dlg, &QDialog::accepted);
      QSignalSpy finished(&dlg, &QDialog::finished);
      const QStringList answers{"h", "u", "pw"};
      const QStringList names{"host", "user", "pass"};
      for (int i = 0; i < 3; ++i) {
        waitQuestion(dlg, names.at(i));
        auto *edit = qobject_cast<QLineEdit *>(editor(dlg));
        QVERIFY(edit);
        QVERIFY(edit->isEnabled());
        if (i == 2) {
          // Create mode: a password is never prefilled, whatever its default
          QCOMPARE(edit->echoMode(), QLineEdit::Password);
          QVERIFY(edit->text().isEmpty());
          QVERIFY(edit->placeholderText().isEmpty());
        }
        edit->setText(answers.at(i));
        next(dlg)->click();
        // disabled until the next signal: the button and the answer field
        QVERIFY(!next(dlg)->isEnabled());
        QVERIFY(!edit->isEnabled());
      }
      QTRY_COMPARE_WITH_TIMEOUT(accepted.count(), 1, 5000);
      QVERIFY(!dlg.isVisible());
      QCOMPARE(dlg.result(), int(QDialog::Accepted));
      QCOMPARE(finished.count(), 1);
      QCOMPARE(finished.first().first().toInt(), int(QDialog::Accepted));

      const QVector<Call> creates = fake.to("config/create");
      QCOMPARE(creates.size(), 4);
      QCOMPARE(creates.first().params.value("name").toString(),
               QString("new1"));
      QCOMPARE(creates.first().params.value("type").toString(),
               QString("sftp"));
      const QJsonObject last = paramsOf(creates.last());
      QCOMPARE(last.value("host").toString(), QString("h"));
      QCOMPARE(last.value("user").toString(), QString("u"));
      QCOMPARE(last.value("pass").toString(), QString("pw"));
      QCOMPARE(optOf(creates.last()).value("result").toString(), QString("pw"));
      QVERIFY(fake.to("config/delete").isEmpty());
    }

    // A failing flow shows the message once in a QMessageBox, then rejects:
    // the dialog is still open (and not yet rejected) while the box is up.
    {
      Fake fake;
      fake.script = [](const Call &c) -> std::optional<RcReply> {
        if (c.path == "config/update") {
          RcReply r;
          r.httpStatus = 500;
          r.error = "backend exploded: bad option";
          return r;
        }
        return okReply();
      };
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Update, "e");
      fake.ctx = &dlg;
      // showErrors stays at its default (true)
      QSignalSpy rejected(&dlg, &QDialog::rejected);
      BoxWatcher boxes;
      boxes.watched = &dlg;
      boxes.rejectedCount = [&rejected]() { return int(rejected.count()); };
      dlg.show();

      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
      QCOMPARE(boxes.texts.size(), 1);
      QCOMPARE(boxes.texts.first(), QString("backend exploded: bad option"));
      QVERIFY(boxes.dialogVisible.first());
      QCOMPARE(boxes.rejectedBefore.first(), 0);
      QVERIFY(!dlg.isVisible());

      // with the hook off the same failure rejects without a box
      Fake quiet;
      quiet.script = fake.script;
      RemoteConfigDialog dlg2(quiet.fn(), RemoteConfigDialog::Mode::Update,
                              "e");
      quiet.ctx = &dlg2;
      dlg2.setShowErrors(false);
      QSignalSpy rejected2(&dlg2, &QDialog::rejected);
      dlg2.show();
      QTRY_COMPARE_WITH_TIMEOUT(rejected2.count(), 1, 5000);
      QCOMPARE(boxes.texts.size(), 1);
    }

    // The MainWindow binding (daemonRcCall) without a running daemon: calls
    // fail asynchronously with a fixed text, and never before the context is
    // bound. A default RcloneDaemon stays Stopped and starts no process.
    {
      RcloneDaemon stopped;
      QVERIFY(stopped.state() == RcloneDaemon::State::Stopped);
      const QList<QPointer<RcloneDaemon>> daemons{QPointer<RcloneDaemon>(),
                                                  QPointer<RcloneDaemon>(
                                                      &stopped)};
      for (const QPointer<RcloneDaemon> &daemon : daemons) {
        QObject owner;
        auto ctx = std::make_shared<QPointer<QObject>>();
        const RcCallFn fn = daemonRcCall(daemon, ctx);
        int replies = 0;
        RcReply got;
        auto done = [&replies, &got](const RcReply &r) {
          ++replies;
          got = r;
        };

        // the context is not bound yet: nothing is called
        fn("core/version", QJsonObject(), done, 1000);
        QTest::qWait(50);
        QCOMPARE(replies, 0);

        *ctx = &owner;
        fn("core/version", QJsonObject(), done, 1000);
        QCOMPARE(replies, 0); // never from inside the call
        QTRY_COMPARE_WITH_TIMEOUT(replies, 1, 5000);
        QVERIFY(!got.ok);
        QCOMPARE(got.error, QString("rclone daemon is not running"));
        QTest::qWait(50);
        QCOMPARE(replies, 1);
      }

      // through the dialog: the failure reaches the user and the dialog closes
      auto ctx = std::make_shared<QPointer<QObject>>();
      RemoteConfigDialog dlg(daemonRcCall(&stopped, ctx),
                             RemoteConfigDialog::Mode::Update, "e");
      *ctx = &dlg;
      QSignalSpy rejected(&dlg, &QDialog::rejected);
      BoxWatcher boxes;
      boxes.watched = &dlg;
      boxes.rejectedCount = [&rejected]() { return int(rejected.count()); };
      dlg.show();
      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
      QCOMPARE(boxes.texts,
               QStringList{QString("rclone daemon is not running")});
      QVERIFY(boxes.dialogVisible.first());
      QCOMPARE(boxes.rejectedBefore.first(), 0);
    }
  }

  // T-RCD-CANCEL
  void T_RCD_CANCEL() {
    // Create, cancelled on a question: the partial remote is deleted first.
    // Message boxes stay on: a cancel is not an error.
    {
      Fake fake;
      fake.providers = {provider("sftp", "SSH/SFTP")};
      fake.script = [&fake](const Call &c) -> std::optional<RcReply> {
        if (c.path == "config/create") {
          // rclone writes the remote with the first step
          fake.remotes << c.params.value("name").toString();
          return stepReply("s1", "host");
        }
        return okReply();
      };
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Create,
                             QString());
      fake.ctx = &dlg;
      dlg.show();

      auto *providers = dlg.findChild<QListWidget *>("providers");
      QTRY_COMPARE_WITH_TIMEOUT(providers->count(), 1, 5000);
      providers->setCurrentRow(0);
      dlg.findChild<QLineEdit *>("name")->setText("c1");
      next(dlg)->click();
      waitQuestion(dlg, "host");
      QVERIFY(dlg.flowStarted());

      QSignalSpy rejected(&dlg, &QDialog::rejected);
      QSignalSpy finished(&dlg, &QDialog::finished);
      int callsAtReject = -1;
      QObject::connect(&dlg, &QDialog::rejected,
                       [&]() { callsAtReject = int(fake.calls.size()); });
      BoxWatcher boxes;

      cancel(dlg)->click();
      // the cleanup is asynchronous: the dialog stays open until the flow is
      // done
      QVERIFY(dlg.isVisible());
      QCOMPARE(rejected.count(), 0);
      // idempotent: the buttons are disabled; a second click and a direct
      // reject() neither cancel again nor close the dialog early
      QVERIFY(!cancel(dlg)->isEnabled());
      QVERIFY(!next(dlg)->isEnabled());
      cancel(dlg)->click();
      dlg.reject();
      QVERIFY(dlg.isVisible());
      QCOMPARE(rejected.count(), 0);
      QCOMPARE(finished.count(), 0);

      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
      QVERIFY(!dlg.isVisible());
      // later attempts do not close it a second time
      dlg.reject();
      cancel(dlg)->click();
      QTest::qWait(50);
      QCOMPARE(rejected.count(), 1);
      QCOMPARE(finished.count(), 1);

      // the partial remote was deleted, once, before the dialog closed
      const QVector<Call> deletes = fake.to("config/delete");
      QCOMPARE(deletes.size(), 1);
      QCOMPARE(deletes.first().params.value("name").toString(), QString("c1"));
      const int deleteIndex = indexOf(fake, "config/delete");
      QVERIFY(deleteIndex >= 0);
      QVERIFY(callsAtReject > deleteIndex);
      // a cancel is not an error: no message box
      QVERIFY(boxes.texts.isEmpty());
    }

    // The 10 s fallback: the flow never finishes, the dialog rejects anyway.
    {
      Fake fake;
      fake.providers = {provider("sftp", "SSH/SFTP")};
      fake.script = [&fake](const Call &c) -> std::optional<RcReply> {
        if (c.path == "config/create") {
          fake.remotes << c.params.value("name").toString();
          return stepReply("s1", "host");
        }
        if (c.path == "config/delete") {
          return std::nullopt; // the cleanup never answers
        }
        return okReply();
      };
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Create,
                             QString());
      fake.ctx = &dlg;
      dlg.setShowErrors(false);
      dlg.show();

      auto *providers = dlg.findChild<QListWidget *>("providers");
      QTRY_COMPARE_WITH_TIMEOUT(providers->count(), 1, 5000);
      providers->setCurrentRow(0);
      dlg.findChild<QLineEdit *>("name")->setText("c2");
      next(dlg)->click();
      waitQuestion(dlg, "host");

      QSignalSpy rejected(&dlg, &QDialog::rejected);
      QElapsedTimer clock;
      clock.start();
      cancel(dlg)->click();
      QTRY_COMPARE_WITH_TIMEOUT(fake.to("config/delete").size(), 1, 5000);
      // still waiting for the flow well before the deadline
      QTest::qWait(1000);
      QVERIFY(dlg.isVisible());
      QCOMPARE(rejected.count(), 0);
      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 12000);
      QVERIFY(clock.elapsed() >= 9000);
      QVERIFY(!dlg.isVisible());
      QCOMPARE(fake.to("config/delete").size(), 1);
    }

    // The OAuth page: the link, one opener call, and Cancel stopping the
    // blocked call. Message boxes stay on: this cancel is not an error either.
    {
      Fake fake;
      fake.script = [](const Call &c) -> std::optional<RcReply> {
        if (c.path == "config/update" && !optOf(c).contains("continue")) {
          return stepReply("s1", "config_is_local",
                           {{"Type", "bool"}, {"DefaultStr", "true"}});
        }
        if (c.path == "config/update") {
          return std::nullopt; // the blocked call
        }
        if (c.path == "config/oauthstatus") {
          return okReply(QJsonObject{
              {"status", "running"},
              {"authUrl", "http://127.0.0.1:53682/auth?state=x"}});
        }
        return okReply();
      };
      RemoteConfigDialog dlg(fake.fn(), RemoteConfigDialog::Mode::Update, "g");
      fake.ctx = &dlg;
      QList<QUrl> opened;
      dlg.setUrlOpener([&opened](const QUrl &u) { opened << u; });
      BoxWatcher boxes;
      dlg.show();

      waitQuestion(dlg, "config_is_local");
      next(dlg)->click();
      auto *link = dlg.findChild<QLabel *>("oauthLink");
      QVERIFY(link);
      QTRY_COMPARE_WITH_TIMEOUT(opened.size(), 1, 5000);
      QCOMPARE(opened.first().toString(),
               QString("http://127.0.0.1:53682/auth?state=x"));
      QVERIFY(link->text().contains("http://127.0.0.1:53682/auth?state=x"));
      QVERIFY(!next(dlg)->isVisible());

      QSignalSpy rejected(&dlg, &QDialog::rejected);
      int callsAtReject = -1;
      QObject::connect(&dlg, &QDialog::rejected,
                       [&]() { callsAtReject = int(fake.calls.size()); });
      cancel(dlg)->click();
      // the blocked call is stopped first; the dialog closes when it failed
      QVERIFY(dlg.isVisible());
      QCOMPARE(rejected.count(), 0);
      QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 5000);
      QVERIFY(!dlg.isVisible());
      QCOMPARE(fake.to("config/oauthstop").size(), 1);
      QVERIFY(callsAtReject > indexOf(fake, "config/oauthstop"));
      QCOMPARE(opened.size(), 1);
      QVERIFY(boxes.texts.isEmpty());
    }
  }
};

QTEST_MAIN(TestRemoteConfigDialog)
#include "tst_remote_config_dialog.moc"
