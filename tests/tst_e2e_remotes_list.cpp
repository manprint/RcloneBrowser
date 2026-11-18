#include <QtTest>
#include <QtWidgets>

#include "main_window.h"
#include "rb_test_support.h"
#include "utils.h"

// The remotes list of the real window, filled by "rclone listremotes" from an
// isolated temporary config with fake remotes (I-9): never the user's remotes.

namespace {

// A modal box that nobody expects would block the test: it is recorded and
// closed, and the test fails at the end.
class ModalWatch : public QObject {
public:
  QStringList unexpected;

  ModalWatch() {
    mTimer.setInterval(50);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
      if (QWidget *modal = QApplication::activeModalWidget()) {
        QString text = QString::fromLatin1(modal->metaObject()->className()) +
                       QStringLiteral(": ") + modal->windowTitle();
        if (auto *box = qobject_cast<QMessageBox *>(modal)) {
          text += QStringLiteral(" / ") + box->text();
        }
        unexpected << text;
        modal->close();
      }
    });
    mTimer.start();
  }

private:
  QTimer mTimer;
};

QStringList remoteNames(MainWindow &w) {
  QStringList out;
  auto *list = w.findChild<QListWidget *>(QStringLiteral("remotes"));
  for (int i = 0; list && i < list->count(); ++i) {
    out << list->item(i)->text();
  }
  return out;
}

} // namespace

class TestE2eRemotesList : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    const QString home = mDir.filePath(QStringLiteral("home"));
    QVERIFY(QDir().mkpath(home));
    qputenv("HOME", home.toUtf8());
    qputenv("RCLONE_CACHE_DIR", mDir.filePath(QStringLiteral("cache")).toUtf8());
    SetRclone(rbRclone());
    const QString conf = mDir.filePath(QStringLiteral("rclone.conf"));
    QFile f(conf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[fakeB]\ntype = local\n\n[fakeA]\ntype = alias\nremote = " +
            mDir.path().toUtf8() + "\n");
    f.close();
    SetRcloneConf(conf);

    auto settings = GetSettings();
    settings->setValue(QStringLiteral("Settings/rclone"), rbRclone());
    settings->setValue(QStringLiteral("Settings/rcloneConf"), conf);
    settings->setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
    settings->setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"),
                       false);
    settings->setValue(QStringLiteral("Settings/notifyFinishedTransfers"),
                       false);
    settings->setValue(QStringLiteral("Settings/soundNotif"), false);
  }

  // T-E2E-REMOTES-REFRESH: the fake remotes are listed in name order; Refresh
  // clicked again before the last listing ended lists every remote once (the
  // older listing used to add them a second time).
  void T_E2E_REMOTES_REFRESH() {
    ModalWatch watch;
    MainWindow w;
    auto *refresh = w.findChild<QAbstractButton *>(QStringLiteral("refresh"));
    QVERIFY(refresh);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    const QStringList expected = {QStringLiteral("fakeA"),
                                  QStringLiteral("fakeB")};
    QTRY_COMPARE_WITH_TIMEOUT(remoteNames(w), expected, 20000);

    refresh->click();
    refresh->click();
    refresh->click();
    QTRY_COMPARE_WITH_TIMEOUT(remoteNames(w), expected, 10000);
    // the older listings end later as well: still once each
    QTest::qWait(2000);
    QCOMPARE(remoteNames(w), expected);
    QVERIFY2(watch.unexpected.isEmpty(), qPrintable(watch.unexpected.join('|')));
  }
};

QTEST_MAIN(TestE2eRemotesList)
#include "tst_e2e_remotes_list.moc"
