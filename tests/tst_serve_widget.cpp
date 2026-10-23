#include <QtTest>
#include <QtWidgets>

#include "rb_test_support.h"
#include "serve_manager.h"
#include "serve_widget.h"
#include "utils.h"

namespace {

QPushButton *okButton(QDialog &dlg) {
  return dlg.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
}

} // namespace

class TestServeWidget : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-SRV-DIALOG: the defaults, the network warning and the serveTypeRules
  // applied on type change (fields, default address, OK gating, params()).
  void T_SRV_DIALOG() {
    ServeDialog dlg("r:a b", QStringList());
    dlg.show();
    auto *type = dlg.findChild<QComboBox *>("type");
    auto *addr = dlg.findChild<QLineEdit *>("addr");
    auto *user = dlg.findChild<QLineEdit *>("user");
    auto *pass = dlg.findChild<QLineEdit *>("pass");
    auto *readOnly = dlg.findChild<QCheckBox *>("readOnly");
    auto *warning = dlg.findChild<QLabel *>("warning");
    auto *problem = dlg.findChild<QLabel *>("problem");
    auto *fsLabel = dlg.findChild<QLabel *>("fsLabel");
    QVERIFY(type && addr && user && pass && readOnly && warning && problem &&
            fsLabel);
    QPushButton *ok = okButton(dlg);
    QVERIFY(ok);

    // no serve/types reply yet: the built-in list, http selected
    QStringList items;
    for (int i = 0; i < type->count(); ++i) {
      items << type->itemText(i);
    }
    QCOMPARE(items, (QStringList{"dlna", "ftp", "http", "nfs", "restic", "s3",
                                 "sftp", "webdav", "webdav-webui"}));
    QCOMPARE(type->currentText(), QString("http"));
    QCOMPARE(addr->text(), QString("127.0.0.1:0"));
    QVERIFY(readOnly->isChecked());
    QCOMPARE(pass->echoMode(), QLineEdit::Password);
    QVERIFY(!warning->isVisible());
    QVERIFY(ok->isEnabled());
    QCOMPARE(fsLabel->textFormat(), Qt::PlainText);
    QCOMPARE(fsLabel->text(), QString("Serve: r:a b"));

    // anything but the loopback address is reachable from the network
    const QList<QPair<QString, bool>> exposure = {
        {"0.0.0.0:8080", true},   {":8080", true},
        {"192.168.1.2:80", true}, {"[::]:80", true},
        {"localhost:80", false},  {"[::1]:80", false},
        {"127.0.0.1:9", false}};
    for (const auto &row : exposure) {
      addr->setText(row.first);
      QVERIFY2(warning->isVisible() == row.second, qPrintable(row.first));
    }
    addr->clear();
    QVERIFY(!ok->isEnabled());
    addr->setText("127.0.0.1:0");
    QVERIFY(ok->isEnabled());

    // rclone turns the login on only with a user name
    pass->setText("p");
    QVERIFY(!ok->isEnabled());
    QVERIFY(problem->isVisible());
    QCOMPARE(problem->text(), QString("A password needs a user name"));
    user->setText("u");
    QVERIFY(ok->isEnabled());
    QVERIFY(!problem->isVisible());

    // type, addr and user trimmed; pass as typed; fs as given
    user->setText(" u ");
    pass->setText(" p ");
    addr->setText(" 127.0.0.1:0 ");
    ServeParams p = dlg.params();
    QCOMPARE(p.type, QString("http"));
    QCOMPARE(p.fs, QString("r:a b"));
    QCOMPARE(p.addr, QString("127.0.0.1:0"));
    QCOMPARE(p.user, QString("u"));
    QCOMPARE(p.pass, QString(" p "));
    QVERIFY(p.readOnly);

    // s3: no login; the fields are cleared and disabled
    type->setCurrentText("s3");
    QVERIFY(!user->isEnabled());
    QVERIFY(!pass->isEnabled());
    QVERIFY(user->text().isEmpty());
    QVERIFY(pass->text().isEmpty());
    QVERIFY(!user->toolTip().isEmpty());
    p = dlg.params();
    QVERIFY(p.user.isEmpty());
    QVERIFY(p.pass.isEmpty());
    QVERIFY(p.readOnly);
    QVERIFY(ok->isEnabled());

    // restic: no read-only mode; it comes back checked for http
    type->setCurrentText("restic");
    QVERIFY(user->isEnabled());
    QVERIFY(!readOnly->isEnabled());
    QVERIFY(!readOnly->isChecked());
    QVERIFY(!dlg.params().readOnly);
    type->setCurrentText("http");
    QVERIFY(readOnly->isEnabled());
    QVERIFY(readOnly->isChecked());

    // ftp: its default replaces an untouched default, and a port is needed
    addr->setText("127.0.0.1:0");
    type->setCurrentText("ftp");
    QCOMPARE(addr->text(), QString("127.0.0.1:2121"));
    QVERIFY(ok->isEnabled());
    addr->setText("127.0.0.1:0");
    QVERIFY(!ok->isEnabled());
    QCOMPARE(problem->text(),
             QString("FTP cannot report a random port; give one"));
    addr->setText("127.0.0.1");
    QVERIFY(!ok->isEnabled());
    addr->setText("127.0.0.1:2121");
    QVERIFY(ok->isEnabled());
    type->setCurrentText("http");
    QCOMPARE(addr->text(), QString("127.0.0.1:0"));
    // an address the user typed stays
    addr->setText("127.0.0.1:8080");
    type->setCurrentText("ftp");
    QCOMPARE(addr->text(), QString("127.0.0.1:8080"));
    QVERIFY(ok->isEnabled());

    // sftp: a user name and a password
    type->setCurrentText("sftp");
    user->clear();
    pass->clear();
    QVERIFY(!ok->isEnabled());
    QCOMPARE(problem->text(), QString("SFTP needs a user name and a password"));
    user->setText("u");
    QVERIFY(!ok->isEnabled());
    pass->setText("p");
    QVERIFY(ok->isEnabled());
    p = dlg.params();
    QCOMPARE(p.type, QString("sftp"));
    QCOMPARE(p.user, QString("u"));
    QCOMPARE(p.pass, QString("p"));

    // the serve/types list replaces the built-in one, keeping the selection
    // (webdav-webui follows webdav in every list that offers webdav)
    dlg.setTypes({"webdav", "sftp"});
    QCOMPARE(type->count(), 3);
    QCOMPARE(type->currentText(), QString("sftp"));
    dlg.setTypes({"webdav", "http"});
    QCOMPARE(type->currentText(), QString("http"));
    QVERIFY(readOnly->isEnabled());
    dlg.setTypes(QStringList());
    QCOMPARE(type->count(), 3);
  }

  // T-WEBUI-DIALOG: the webdav-webui type, its option block, the capability
  // gate, the validation and params().
  void T_WEBUI_DIALOG() {
    ServeDialog dlg("r:a", QStringList());
    dlg.show();
    auto *type = dlg.findChild<QComboBox *>("type");
    QVERIFY(type);
    auto items = [type]() {
      QStringList out;
      for (int i = 0; i < type->count(); ++i) {
        out << type->itemText(i);
      }
      return out;
    };
    QCOMPARE(type->itemText(items().indexOf("webdav") + 1),
             QString(kWebuiFlavor));
    dlg.setTypes({"http", "webdav"});
    QCOMPARE(items(), (QStringList{"http", "webdav", "webdav-webui"}));
    dlg.setTypes({"http"});
    QCOMPARE(items(), (QStringList{"http"}));
    dlg.setTypes({"http", "webdav"});

    auto *group = dlg.findChild<QGroupBox *>("webuiGroup");
    auto *advanced = dlg.findChild<QWidget *>("webuiAdvanced");
    auto *toggle = dlg.findChild<QToolButton *>("webuiAdvancedToggle");
    auto *unsupported = dlg.findChild<QLabel *>("webuiUnsupported");
    QVERIFY(group && advanced && toggle && unsupported);
    QPushButton *ok = okButton(dlg);
    auto *problem = dlg.findChild<QLabel *>("problem");
    QVERIFY(ok && problem);

    type->setCurrentText("webdav");
    QVERIFY(!group->isVisible());
    QVERIFY(ok->isEnabled());
    type->setCurrentText(kWebuiFlavor);
    QVERIFY(group->isVisible());
    QVERIFY(!advanced->isVisible());
    QVERIFY(unsupported->text().startsWith("Checking"));
    QVERIFY(!ok->isEnabled());
    dlg.setWebuiSupport(ServeManager::WebuiSupport::No);
    QVERIFY(!ok->isEnabled());
    QVERIFY(unsupported->isVisible());
    QCOMPARE(unsupported->text(), QString(kWebuiUnsupported));
    dlg.setWebuiSupport(ServeManager::WebuiSupport::Yes);
    QVERIFY(ok->isEnabled());
    QVERIFY(!unsupported->isVisible());

    toggle->click();
    QVERIFY(advanced->isVisible());
    QCOMPARE(toggle->arrowType(), Qt::DownArrow);
    toggle->click();
    QVERIFY(!advanced->isVisible());
    toggle->click();

    // defaults of the table
    QVERIFY(dlg.findChild<QCheckBox *>("webuiAllowDelete")->isChecked());
    QCOMPARE(dlg.findChild<QSpinBox *>("webuiSearchMaxDepth")->value(), 8);
    QCOMPARE(dlg.findChild<QLineEdit *>("webuiTokenExpiry")->text(),
             QString("72h"));
    QCOMPARE(dlg.findChild<QComboBox *>("vfsCacheMode")->currentText(),
             QString("off"));
    QVERIFY(dlg.findChild<QLabel *>("vfsHint"));
    QCOMPARE(dlg.findChild<QLineEdit *>("webuiSecret")->echoMode(),
             QLineEdit::Password);

    dlg.findChild<QCheckBox *>("webuiAllowDelete")->setChecked(false);
    dlg.findChild<QLineEdit *>("webuiPublinkPaths")->setText(" /a , ,/b ");
    dlg.findChild<QSpinBox *>("webuiSearchMaxDepth")->setValue(3);
    dlg.findChild<QLineEdit *>("webuiEditMaxSize")->setText("8Mi");
    dlg.findChild<QLineEdit *>("webuiTokenExpiry")->setText("1h");
    dlg.findChild<QLineEdit *>("webuiSecret")->setText("s3cr3t");
    auto *vfs = dlg.findChild<QComboBox *>("vfsCacheMode");
    vfs->setCurrentText("writes");
    ServeParams p = dlg.params();
    QCOMPARE(p.type, QString("webdav"));
    QCOMPARE(p.flavor, QString("webdav-webui"));
    QVERIFY(!p.webui.allowDelete);
    QCOMPARE(p.webui.publinkPaths, (QStringList{"/a", "/b"}));
    QCOMPARE(p.webui.searchMaxDepth, 3);
    QCOMPARE(p.webui.editMaxSize, QString("8Mi"));
    QCOMPARE(p.webui.tokenExpiry, QString("1h"));
    QVERIFY(p.webui.secret == QString("s3cr3t"));
    QCOMPARE(p.vfsCacheMode, QString("writes"));

    auto *timeout = dlg.findChild<QLineEdit *>("webuiSearchTimeout");
    timeout->setText("abc");
    QVERIFY(!ok->isEnabled());
    QVERIFY(problem->text().startsWith("Invalid duration: "));
    timeout->setText("30s");
    QVERIFY(ok->isEnabled());
    auto *editMax = dlg.findChild<QLineEdit *>("webuiEditMaxSize");
    editMax->setText("8 Mi");
    QVERIFY(!ok->isEnabled());
    QCOMPARE(problem->text(), QString("Invalid size: Edit max size"));
    editMax->setText("8Mi");
    QVERIFY(ok->isEnabled());

    vfs->setCurrentText("off");
    QVERIFY(dlg.params().vfsCacheMode.isEmpty());

    type->setCurrentText("webdav");
    p = dlg.params();
    QCOMPARE(p.type, QString("webdav"));
    QVERIFY(p.flavor.isEmpty());
    QVERIFY(p.vfsCacheMode.isEmpty());
    QVERIFY(p.webui.secret.isEmpty());
    QVERIFY(p.webui.allowDelete);
  }

  // T-WEBUI-DIALOG-CHECK-FAILED: a failed capability check replaces "Checking"
  // with its text (plain), and OK stays disabled; any new answer or a new
  // check (Unknown) clears the failure
  void T_WEBUI_DIALOG_CHECK_FAILED() {
    ServeDialog dlg("r:a", QStringList());
    dlg.show();
    auto *type = dlg.findChild<QComboBox *>("type");
    QVERIFY(type);
    dlg.setTypes({"http", "webdav"});
    type->setCurrentText(kWebuiFlavor);
    auto *unsupported = dlg.findChild<QLabel *>("webuiUnsupported");
    QPushButton *ok = okButton(dlg);
    QVERIFY(unsupported && ok);
    QVERIFY(unsupported->text().startsWith("Checking"));
    QVERIFY(!ok->isEnabled());

    dlg.setWebuiCheckFailed("x");
    QCOMPARE(unsupported->text(), QString(kWebuiCheckFailed) + QString("x"));
    QCOMPARE(unsupported->textFormat(), Qt::PlainText);
    QVERIFY(unsupported->isVisible());
    QVERIFY(!ok->isEnabled());

    // a new check starts: back to "Checking", the failure is gone
    dlg.setWebuiSupport(ServeManager::WebuiSupport::Unknown);
    QVERIFY(unsupported->text().startsWith("Checking"));
    QVERIFY(!ok->isEnabled());

    dlg.setWebuiCheckFailed("x");
    dlg.setWebuiCheckFailed(QString());
    QVERIFY(unsupported->text().startsWith("Checking"));
    QVERIFY(!ok->isEnabled());

    // an answer replaces the failure: No shows the unsupported text
    dlg.setWebuiCheckFailed("x");
    dlg.setWebuiSupport(ServeManager::WebuiSupport::No);
    QCOMPARE(unsupported->text(), QString(kWebuiUnsupported));
    QVERIFY(!ok->isEnabled());

    dlg.setWebuiCheckFailed("x");
    dlg.setWebuiSupport(ServeManager::WebuiSupport::Yes);
    QVERIFY(!unsupported->isVisible());
    QVERIFY(ok->isEnabled());
  }

  // T-WEBUI-REMEMBER: the options persist through GetSettings() when a
  // webdav-webui dialog is accepted, never the secret.
  void T_WEBUI_REMEMBER() {
    GetSettings()->remove("Serve/webui");
    auto childKeys = []() {
      auto s = GetSettings();
      s->beginGroup("Serve/webui");
      return s->childKeys();
    };
    QVERIFY(childKeys().isEmpty());

    {
      ServeDialog a("r:a", QStringList());
      a.show();
      a.setWebuiSupport(ServeManager::WebuiSupport::Yes);
      a.findChild<QComboBox *>("type")->setCurrentText(kWebuiFlavor);
      a.findChild<QCheckBox *>("webuiAllowDelete")->setChecked(false);
      a.findChild<QLineEdit *>("webuiPublinkPaths")->setText(" /a , ,/b ");
      a.findChild<QSpinBox *>("webuiSearchMaxDepth")->setValue(3);
      a.findChild<QLineEdit *>("webuiEditMaxSize")->setText("8Mi");
      a.findChild<QLineEdit *>("webuiTokenExpiry")->setText("1h");
      a.findChild<QLineEdit *>("webuiSecret")->setText("s3cr3t");
      a.findChild<QComboBox *>("vfsCacheMode")->setCurrentText("writes");
      a.accept();
    }
    const QStringList keys = childKeys();
    QCOMPARE(keys.size(), 18);
    for (const QString &k : keys) {
      QVERIFY2(!k.contains("Secret"), qPrintable(k));
    }
    {
      auto s = GetSettings();
      QCOMPARE(s->value("Serve/webui/SearchMaxDepth").toInt(), 3);
      QCOMPARE(s->value("Serve/webui/VfsCacheMode").toString(),
               QString("writes"));
      QVERIFY(!s->value("Serve/webui/AllowDelete").toBool());
      QVERIFY(!s->contains("Serve/webui/Secret"));
    }

    {
      ServeDialog b("r:a", QStringList());
      QCOMPARE(b.findChild<QSpinBox *>("webuiSearchMaxDepth")->value(), 3);
      QVERIFY(!b.findChild<QCheckBox *>("webuiAllowDelete")->isChecked());
      QCOMPARE(b.findChild<QLineEdit *>("webuiPublinkPaths")->text(),
               QString(" /a , ,/b "));
      QCOMPARE(b.findChild<QLineEdit *>("webuiEditMaxSize")->text(),
               QString("8Mi"));
      QCOMPARE(b.findChild<QLineEdit *>("webuiTokenExpiry")->text(),
               QString("1h"));
      QCOMPARE(b.findChild<QComboBox *>("vfsCacheMode")->currentText(),
               QString("writes"));
      QVERIFY(b.findChild<QLineEdit *>("webuiSecret")->text().isEmpty());
    }

    // a classic webdav accept writes nothing new
    GetSettings()->remove("Serve/webui");
    {
      ServeDialog c("r:a", QStringList());
      c.findChild<QComboBox *>("type")->setCurrentText("webdav");
      c.accept();
    }
    QVERIFY(childKeys().isEmpty());
  }
};

QTEST_MAIN(TestServeWidget)
#include "tst_serve_widget.moc"
