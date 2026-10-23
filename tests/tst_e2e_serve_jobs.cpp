#include <QtNetwork>
#include <QtTest>
#include <QtWidgets>

#include "global.h"
#include "main_window.h"
#include "rb_test_support.h"
#include "rclone_daemon.h"
#include "serve_job_widget.h"
#include "serve_manager.h"
#include "utils.h"

namespace {

QVBoxLayout *jobsLayout(MainWindow &w) {
  return w.findChild<QVBoxLayout *>(QStringLiteral("jobs"));
}

// The HLine that follows `widget` in the jobs layout.
QWidget *lineAfter(MainWindow &w, QWidget *widget) {
  QVBoxLayout *jobs = jobsLayout(w);
  const int at = jobs->indexOf(widget);
  return at < 0 ? nullptr : jobs->itemAt(at + 1)->widget();
}

// A widget the filter hid: hidden AND flagged as hidden on purpose (see
// tst_e2e_filter).
bool hiddenOnPurpose(const QWidget *widget) {
  return widget->isHidden() &&
         widget->testAttribute(Qt::WA_WState_ExplicitShowHide);
}

// The serve widgets of the window (also one waiting for its deleteLater()).
QList<ServeJobWidget *> serveWidgets(MainWindow &w) {
  return w.findChildren<ServeJobWidget *>();
}

// The ServeJobWidgets that sit in the jobs layout.
int serveWidgetsInLayout(MainWindow &w) {
  int n = 0;
  QVBoxLayout *jobs = jobsLayout(w);
  for (ServeJobWidget *s : serveWidgets(w)) {
    if (jobs->indexOf(s) >= 0) {
      ++n;
    }
  }
  return n;
}

ServeJobWidget *widgetWithType(MainWindow &w, const QString &type) {
  for (ServeJobWidget *s : serveWidgets(w)) {
    if (s->filterFields().value(0) == type) {
      return s;
    }
  }
  return nullptr;
}

QString urlOf(const ServeJobWidget *s) {
  return s->findChild<QLineEdit *>(QStringLiteral("url"))->text();
}

quint16 portOfUrl(const ServeJobWidget *s) {
  return static_cast<quint16>(QUrl(urlOf(s)).port());
}

// A QTcpSocket, not a reused QNetworkAccessManager: keep-alive could hide a
// closed listener.
bool connectionRefused(quint16 port) {
  QTcpSocket s;
  s.connectToHost("127.0.0.1", port);
  if (s.waitForConnected(1000)) {
    s.abort();
    return false;
  }
  return s.error() == QAbstractSocket::ConnectionRefusedError;
}

// The status code of GET `url`, 0 when it did not finish in 10 s.
int httpGetStatus(const QString &url) {
  QNetworkAccessManager nam;
  nam.setProxy(QNetworkProxy::NoProxy);
  QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(url)));
  const bool done =
      QTest::qWaitFor([reply]() { return reply->isFinished(); }, 10000);
  const int status =
      done ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
           : 0;
  reply->abort();
  reply->deleteLater();
  return status;
}

// A modal box that nobody expects would block the test: it is recorded and
// closed, and the test fails at the end.
class ModalWatch : public QObject {
public:
  bool allowed = false;
  QStringList unexpected;

  ModalWatch() {
    mTimer.setInterval(50);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
      QWidget *modal = QApplication::activeModalWidget();
      if (modal && !allowed) {
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

} // namespace

// Serves in the Jobs tab on the real window, against a real rcd and an isolated
// temporary config with one local remote (I-9). Every serve binds to
// 127.0.0.1 and only successful starts are made.
class TestE2eServeJobs : public QObject {
  Q_OBJECT

  QTemporaryDir mDir;

  QString tree() const { return mDir.filePath("w"); }

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    QVERIFY(!rbRclone().isEmpty());
    QVERIFY(mDir.isValid());
    // the tasks file is read once, under this HOME
    const QString home = mDir.filePath("home");
    QVERIFY(QDir().mkpath(home));
    qputenv("HOME", home.toUtf8());
    qputenv("RCLONE_CACHE_DIR", mDir.filePath("cache").toUtf8());
    SetRclone(rbRclone());
    // I-9: rcd uses this temporary config, never the host one
    const QString conf = mDir.filePath("rclone.conf");
    QFile f(conf);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[loc]\ntype = local\n");
    f.close();
    SetRcloneConf(conf);

    QVERIFY(QDir().mkpath(tree()));
    QFile file(QDir(tree()).filePath("a b.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hi");
    file.close();

    auto settings = GetSettings();
    // without it the constructor waits in a modal box
    settings->setValue(QStringLiteral("Settings/rclone"), rbRclone());
    // the window loads the config path from the settings
    settings->setValue(QStringLiteral("Settings/rcloneConf"), conf);
    settings->setValue(QStringLiteral("Settings/checkRcloneUpdates"), false);
    settings->setValue(QStringLiteral("Settings/checkRcloneBrowserUpdates"),
                       false);
    settings->setValue(QStringLiteral("Settings/notifyFinishedTransfers"),
                       false);
    settings->setValue(QStringLiteral("Settings/soundNotif"), false);
  }

  // T-E2E-SERVE-JOBS: start, filter, stop, restart, sort, Clean not running,
  // backend stop, and no "Serves" action any more.
  void T_E2E_SERVE_JOBS() {
    ModalWatch watch;
    MainWindow w;
    auto *tabs = w.findChild<QTabWidget *>(QStringLiteral("tabs"));
    auto *filter = w.findChild<QLineEdit *>(QStringLiteral("jobsFilter"));
    QVERIFY(tabs && filter && jobsLayout(w));
    tabs->setCurrentIndex(1);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QTRY_VERIFY_WITH_TIMEOUT(
        global.daemon && global.daemon->state() == RcloneDaemon::State::Running,
        20000);

    // (1) an http serve appears as a Jobs widget and serves the tree
    ServeParams http;
    http.type = QStringLiteral("http");
    http.fs = QStringLiteral("loc:") + tree();
    http.addr = QStringLiteral("127.0.0.1:0");
    http.readOnly = true;
    w.startServe(http);
    QTRY_VERIFY2_WITH_TIMEOUT(serveWidgetsInLayout(w) == 1,
                              qPrintable(watch.unexpected.join('|')), 10000);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (1)"));
    ServeJobWidget *httpJob = widgetWithType(w, QStringLiteral("http"));
    QVERIFY(httpJob);
    QCOMPARE(httpJob->state(), ServeJobWidget::State::Serving);
    QVERIFY2(urlOf(httpJob).startsWith(QStringLiteral("http://127.0.0.1:")),
             qPrintable(urlOf(httpJob)));
    const QString firstId = httpJob->serveId();
    QVERIFY(!firstId.isEmpty());
    const quint16 firstPort = portOfUrl(httpJob);
    QVERIFY(firstPort > 0);
    QCOMPARE(httpGetStatus(urlOf(httpJob)), 200);

    // (2) a second serve (webdav); the Jobs filter hides the http one
    ServeParams dav = http;
    dav.type = QStringLiteral("webdav");
    w.startServe(dav);
    QTRY_COMPARE_WITH_TIMEOUT(serveWidgetsInLayout(w), 2, 10000);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (2)"));
    ServeJobWidget *davJob = widgetWithType(w, QStringLiteral("webdav"));
    QVERIFY(davJob);
    QVERIFY(davJob != httpJob);
    QWidget *httpLine = lineAfter(w, httpJob);
    QWidget *davLine = lineAfter(w, davJob);
    QVERIFY(httpLine && davLine);

    filter->setText(QStringLiteral("webdav"));
    QVERIFY(hiddenOnPurpose(httpJob));
    QVERIFY(hiddenOnPurpose(httpLine));
    QVERIFY(!davJob->isHidden());
    QVERIFY(!davLine->isHidden());
    filter->clear();
    QVERIFY(!httpJob->isHidden());
    QVERIFY(!httpLine->isHidden());
    QVERIFY(!davJob->isHidden());
    QVERIFY(!davLine->isHidden());

    // (3) Stop the http serve: it stays as Finished and the port is closed
    auto *cancel = httpJob->findChild<QToolButton *>(QStringLiteral("cancel"));
    QVERIFY(cancel && cancel->isEnabled());
    cancel->click();
    QTRY_COMPARE_WITH_TIMEOUT(httpJob->state(), ServeJobWidget::State::Finished,
                              10000);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (1)"));
    QTRY_VERIFY_WITH_TIMEOUT(connectionRefused(firstPort), 5000);

    // (4) Restart: a new serve with the same params
    auto *restart = httpJob->findChild<QToolButton *>(QStringLiteral("restart"));
    QVERIFY(restart && restart->isEnabled());
    restart->click();
    QTRY_COMPARE_WITH_TIMEOUT(httpJob->state(), ServeJobWidget::State::Serving,
                              10000);
    QVERIFY(!httpJob->serveId().isEmpty());
    QVERIFY(httpJob->serveId() != firstId);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (2)"));
    QCOMPARE(httpGetStatus(urlOf(httpJob)), 200);
    QCOMPARE(serveWidgetsInLayout(w), 2);

    // (5) sorting keeps both widgets in the layout
    auto *byStatus =
        w.findChild<QToolButton *>(QStringLiteral("buttonSortByStatus"));
    auto *byTime =
        w.findChild<QToolButton *>(QStringLiteral("buttonSortByTime"));
    QVERIFY(byStatus && byTime);
    QVERIFY(byStatus->isEnabled() && byTime->isEnabled());
    byStatus->click();
    QCOMPARE(serveWidgetsInLayout(w), 2);
    byTime->click();
    QCOMPARE(serveWidgetsInLayout(w), 2);

    // (6) Stop the http serve again, then Clean not running removes only it
    const quint16 secondPort = portOfUrl(httpJob);
    auto *cancel2 = httpJob->findChild<QToolButton *>(QStringLiteral("cancel"));
    QVERIFY(cancel2 && cancel2->isEnabled());
    cancel2->click();
    QTRY_COMPARE_WITH_TIMEOUT(httpJob->state(), ServeJobWidget::State::Finished,
                              10000);
    QTRY_VERIFY_WITH_TIMEOUT(connectionRefused(secondPort), 5000);
    QAction *clean =
        w.findChild<QAction *>(QStringLiteral("actionCleanNotRunning"));
    QVERIFY(clean);
    watch.allowed = true;
    QTimer::singleShot(0, [] {
      if (auto *b = qobject_cast<QMessageBox *>(
              QApplication::activeModalWidget())) {
        b->button(QMessageBox::Yes)->click();
      }
    });
    clean->trigger();
    watch.allowed = false;
    QTRY_COMPARE_WITH_TIMEOUT(serveWidgets(w).size(), 1, 5000);
    QCOMPARE(serveWidgetsInLayout(w), 1);
    QCOMPARE(serveWidgets(w).first(), davJob);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs (1)"));

    // (7) the backend stops: the remaining serve ends with a reason
    global.daemon->stop();
    QTRY_COMPARE_WITH_TIMEOUT(davJob->state(), ServeJobWidget::State::Finished,
                              10000);
    auto *message = davJob->findChild<QLabel *>(QStringLiteral("message"));
    QVERIFY(message);
    QCOMPARE(message->text(), QStringLiteral("Backend stopped"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Jobs"));

    // (8) the File menu has no Serves action any more
    QVERIFY(w.findChild<QAction *>(QStringLiteral("actionServes")) == nullptr);

    QVERIFY2(watch.unexpected.isEmpty(), qPrintable(watch.unexpected.join('|')));
  }
};

QTEST_MAIN(TestE2eServeJobs)
#include "tst_e2e_serve_jobs.moc"
