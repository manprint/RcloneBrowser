#include <QtTest>
#include <QtWidgets>

#include "serve_manager.h"
#include "serve_widget.h"

namespace {

QPushButton *okButton(QDialog &dlg) {
  return dlg.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
}

} // namespace

class TestServeWidget : public QObject {
  Q_OBJECT

private slots:
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
                                 "sftp", "webdav"}));
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
    dlg.setTypes({"webdav", "sftp"});
    QCOMPARE(type->count(), 2);
    QCOMPARE(type->currentText(), QString("sftp"));
    dlg.setTypes({"webdav", "http"});
    QCOMPARE(type->currentText(), QString("http"));
    QVERIFY(readOnly->isEnabled());
    dlg.setTypes(QStringList());
    QCOMPARE(type->count(), 2);
  }
};

QTEST_MAIN(TestServeWidget)
#include "tst_serve_widget.moc"
