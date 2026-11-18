#include <QtTest>
#include <QtWidgets>

#include "delete_progress_dialog.h"
#include "global.h"
#include "rb_test_support.h"
#include "remote_ops.h"
#include "utils.h"

// DeleteProgressDialog (the Delete action of a remote) with a real rclone. The
// remote is a fake one ("fake", an alias of a temporary folder) in a config of
// the test: no network, never the user's remotes.

namespace {

bool writeText(const QString &path, const QByteArray &data) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

// the Cancel button, "&Close" once the dialog can be closed
QPushButton *cancelButton(const QDialog &d) {
  for (QPushButton *b : d.findChildren<QPushButton *>()) {
    if (b->text() == QStringLiteral("&Cancel") ||
        b->text() == QStringLiteral("&Close")) {
      return b;
    }
  }
  return nullptr;
}

QString operation(const QDialog &d) {
  const auto *l = d.findChild<QLabel *>(QStringLiteral("labelOperation"));
  return l ? l->text() : QString();
}

QString outputOf(const QDialog &d) {
  const auto *o = d.findChild<QPlainTextEdit *>(QStringLiteral("output"));
  return o ? o->toPlainText() : QString();
}

} // namespace

class TestE2eDelete : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;
  QString mConf;

  QString src() const { return mDir.filePath(QStringLiteral("src")); }

  QStringList common() const { return {QStringLiteral("--config"), mConf}; }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QVERIFY(mDir.isValid());
    SetRclone(rbRclone());
    mConf = mDir.filePath(QStringLiteral("rclone.conf"));
    QVERIFY(writeText(mConf, QStringLiteral("[fake]\ntype = alias\nremote = %1\n")
                                 .arg(src())
                                 .toUtf8()));
  }

  void cleanup() {
    SetRclone(rbRclone());
    global.rcloneLsProcessCount = 0;
  }

  // T-E2E-DELETE-FAKE: a file and a folder of the fake remote, deleted as the
  // Delete action deletes them; the dialog accepts by itself.
  void T_E2E_DELETE_FAKE() {
    const QDir s(src());
    QVERIFY(writeText(s.filePath(QStringLiteral("a.txt")), "a"));
    QVERIFY(writeText(s.filePath(QStringLiteral("dir/b.txt")), "b"));
    QVERIFY(writeText(s.filePath(QStringLiteral("keep.txt")), "k"));

    const QList<QStringList> items = {
        RemoteOps::deleteArgs(QStringLiteral("fake"), QStringLiteral("a.txt"),
                              false, common()),
        RemoteOps::deleteArgs(QStringLiteral("fake"), QStringLiteral("dir"),
                              true, common())};
    DeleteProgressDialog dlg(items, nullptr, true);
    QSignalSpy accepted(&dlg, &QDialog::accepted);
    QTRY_COMPARE_WITH_TIMEOUT(accepted.count(), 1, 20000);
    QVERIFY(!s.exists(QStringLiteral("a.txt")));
    QVERIFY(!s.exists(QStringLiteral("dir")));
    QVERIFY(s.exists(QStringLiteral("keep.txt")));
    QCOMPARE(global.rcloneLsProcessCount, 0);
  }

  // T-E2E-DELETE-NOSTART: an rclone that cannot start emits no finished();
  // the dialog ends as an error and can be closed (it stayed "Running", with
  // its close button ignored).
  void T_E2E_DELETE_NOSTART() {
    SetRclone(mDir.filePath(QStringLiteral("no-rclone")));
    DeleteProgressDialog dlg(
        {RemoteOps::deleteArgs(QStringLiteral("fake"), QStringLiteral("x.txt"),
                               false, common())},
        nullptr, true);
    QSignalSpy rejected(&dlg, &QDialog::rejected);
    QTRY_COMPARE_WITH_TIMEOUT(cancelButton(dlg) ? cancelButton(dlg)->text()
                                                : QString(),
                              QStringLiteral("&Close"), 5000);
    QCOMPARE(operation(dlg), QStringLiteral("Error "));
    QVERIFY2(!outputOf(dlg).isEmpty(), "no start error in the output");
    QCOMPARE(global.rcloneLsProcessCount, 0);
    cancelButton(dlg)->click();
    QCOMPARE(rejected.count(), 1);
  }

  // T-E2E-DELETE-CANCEL-QUEUED: Cancel while the items wait for a free rclone
  // slot (no process running): the dialog becomes closable (it used to stay
  // "Cancelling" for good) and nothing is deleted.
  void T_E2E_DELETE_CANCEL_QUEUED() {
    const QDir s(src());
    QVERIFY(writeText(s.filePath(QStringLiteral("q.txt")), "q"));
    // every slot taken by listings
    global.rcloneLsProcessCount = 1000;
    DeleteProgressDialog dlg(
        {RemoteOps::deleteArgs(QStringLiteral("fake"), QStringLiteral("q.txt"),
                               false, common())},
        nullptr, true);
    // as the Delete action does
    dlg.allowToClose();
    QSignalSpy rejected(&dlg, &QDialog::rejected);
    QTest::qWait(500);
    QPushButton *cancel = cancelButton(dlg);
    QVERIFY(cancel);
    QCOMPARE(cancel->text(), QStringLiteral("&Cancel"));
    cancel->click();
    QCOMPARE(operation(dlg), QStringLiteral("Cancelled "));
    QCOMPARE(cancel->text(), QStringLiteral("&Close"));
    QCOMPARE(rejected.count(), 0);
    cancel->click();
    QCOMPARE(rejected.count(), 1);
    QTest::qWait(500);
    QVERIFY(s.exists(QStringLiteral("q.txt")));
    QCOMPARE(global.rcloneLsProcessCount, 1000);
  }
};

QTEST_MAIN(TestE2eDelete)
#include "tst_e2e_delete.moc"
