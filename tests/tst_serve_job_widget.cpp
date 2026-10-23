#include <QtTest>
#include <QtWidgets>

#include "rb_test_support.h"
#include "serve_job_widget.h"
#include "serve_manager.h"

namespace {

ServeEntry httpEntry() {
  ServeEntry e;
  e.id = "http-1";
  e.addr = "127.0.0.1:4000";
  e.type = "http";
  e.fs = "loc:/w";
  return e;
}

ServeParams httpParams() {
  ServeParams p;
  p.type = "http";
  p.fs = "loc:/w";
  return p;
}

template <class T> T *child(QWidget &w, const char *name) {
  T *c = w.findChild<T *>(name);
  Q_ASSERT(c);
  return c;
}

} // namespace

class TestServeJobWidget : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  void T_SJW_STATE() {
    ServeJobWidget w(httpEntry(), "http", httpParams());
    w.show();
    auto *url = child<QLineEdit>(w, "url");
    auto *info = child<QLineEdit>(w, "info");
    auto *cancel = child<QToolButton>(w, "cancel");
    auto *message = child<QLabel>(w, "message");
    QSignalSpy stop(&w, &ServeJobWidget::stopRequested);
    QSignalSpy fin(&w, &ServeJobWidget::finished);
    QSignalSpy closed(&w, &ServeJobWidget::closed);

    QCOMPARE(w.state(), ServeJobWidget::State::Serving);
    QVERIFY(w.isRunning);
    QCOMPARE(w.getStatus(), QString("0_zserve_serving"));
    QCOMPARE(url->text(), QString("http://127.0.0.1:4000/"));
    QCOMPARE(info->text(), QString("http loc:/w"));
    QVERIFY(cancel->isEnabled());
    QCOMPARE(cancel->toolTip(), QString("Stop"));
    QVERIFY(message->isHidden());

    cancel->click();
    QCOMPARE(stop.count(), 1);
    QCOMPARE(stop.at(0).at(0).toString(), QString("http-1"));
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(child<QToolButton>(w, "showDetails")->text(),
             QString("  Stopping"));
    // a second request while stopping is ignored
    w.cancel();
    QCOMPARE(stop.count(), 1);
    // listed again after the request: the stop failed, Stop works again
    w.markRunning(httpEntry());
    QVERIFY(cancel->isEnabled());
    QCOMPARE(child<QToolButton>(w, "showDetails")->text(), QString("  Serving"));
    cancel->click();
    QCOMPARE(stop.count(), 2);

    w.markFinished("Stopped");
    QCOMPARE(fin.count(), 1);
    QCOMPARE(w.state(), ServeJobWidget::State::Finished);
    QCOMPARE(w.getStatus(), QString("1_zserve_finished"));
    QVERIFY(!w.isRunning);
    QVERIFY(cancel->isEnabled());
    QCOMPARE(cancel->toolTip(), QString("Close"));
    QVERIFY(message->isVisible());
    QCOMPARE(message->text(), QString("Stopped"));
    QCOMPARE(child<QToolButton>(w, "showDetails")->text(),
             QString("  Finished"));

    w.markFinished("again");
    QCOMPARE(fin.count(), 1);
    QCOMPARE(message->text(), QString("Stopped"));

    cancel->click();
    QCOMPARE(closed.count(), 1);
    QCOMPARE(stop.count(), 2);
  }

  void T_SJW_RESTART() {
    ServeJobWidget w(httpEntry(), "http", httpParams());
    w.show();
    auto *restart = child<QToolButton>(w, "restart");
    auto *cancel = child<QToolButton>(w, "cancel");
    auto *url = child<QLineEdit>(w, "url");
    auto *message = child<QLabel>(w, "message");
    QSignalSpy fin(&w, &ServeJobWidget::finished);
    QSignalSpy again(&w, &ServeJobWidget::restartRequested);

    // a running serve cannot be restarted
    QVERIFY(restart->isVisible());
    QVERIFY(!restart->isEnabled());

    w.markFinished("Stopped");
    QVERIFY(restart->isVisible());
    QVERIFY(restart->isEnabled());
    restart->click();
    QCOMPARE(again.count(), 1);

    w.markStarting();
    QCOMPARE(w.state(), ServeJobWidget::State::Starting);
    QVERIFY(w.isRunning);
    QCOMPARE(w.getStatus(), QString("0_zserve_starting"));
    QCOMPARE(w.serveId(), QString(""));
    QVERIFY(!restart->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QVERIFY(message->isHidden());

    ServeEntry e2 = httpEntry();
    e2.id = "http-2";
    e2.addr = "127.0.0.1:4001";
    w.markRunning(e2);
    QCOMPARE(w.state(), ServeJobWidget::State::Serving);
    QCOMPARE(w.getStatus(), QString("0_zserve_serving"));
    QCOMPARE(url->text(), QString("http://127.0.0.1:4001/"));
    QCOMPARE(w.serveId(), QString("http-2"));
    QVERIFY(cancel->isEnabled());
    QCOMPARE(fin.count(), 1);

    // the same serve listed again: no change, no signal
    w.markRunning(e2);
    QCOMPARE(fin.count(), 1);

    w.markFinished("Stopped");
    QCOMPARE(fin.count(), 2);
    w.markStarting();
    w.markStartFailed("port in use");
    QCOMPARE(fin.count(), 3);
    QCOMPARE(w.state(), ServeJobWidget::State::Error);
    QCOMPARE(w.getStatus(), QString("1_zserve_error"));
    QVERIFY(!w.isRunning);
    QCOMPARE(message->text(), QString("port in use"));
    QCOMPARE(message->textFormat(), Qt::PlainText);
    QVERIFY(restart->isEnabled());
    QVERIFY(cancel->isEnabled());

    // transitions out of place are no-ops
    w.markStartFailed("x");
    w.markFinished("x");
    w.markRunning(e2);
    QCOMPARE(w.state(), ServeJobWidget::State::Error);
    QCOMPARE(fin.count(), 3);
  }

  void T_SJW_ADOPTED() {
    ServeJobWidget w(httpEntry(), "http", ServeParams());
    w.show();
    auto *restart = child<QToolButton>(w, "restart");
    QVERIFY(!w.canRestart());
    QVERIFY(restart->isHidden());

    w.markFinished("Stopped");
    QCOMPARE(w.state(), ServeJobWidget::State::Finished);
    QVERIFY(restart->isHidden());
    w.markStarting();
    QCOMPARE(w.state(), ServeJobWidget::State::Finished);
    QVERIFY(!w.isRunning);

    w.adoptParams(httpParams(), "http");
    QVERIFY(w.canRestart());
    QVERIFY(restart->isVisible());
    QVERIFY(restart->isEnabled());
    QCOMPARE(w.params().type, QString("http"));
  }

  void T_SJW_BUTTONS() {
    for (const QString t : {"http", "webdav", "webdav-webui"}) {
      ServeEntry e = httpEntry();
      e.type = t == "webdav-webui" ? "webdav" : t;
      ServeJobWidget w(e, t, httpParams());
      w.show();
      QVERIFY2(child<QToolButton>(w, "openBrowser")->isVisible(),
               qPrintable(t));
    }
    {
      ServeEntry e = httpEntry();
      e.type = "sftp";
      ServeJobWidget w(e, "sftp", httpParams());
      w.show();
      QVERIFY(child<QToolButton>(w, "openBrowser")->isHidden());
    }

    ServeEntry e;
    e.id = "webdav-9";
    e.addr = "[::]:4321";
    e.type = "webdav";
    e.fs = "r:a b";
    ServeParams p;
    p.type = "webdav";
    ServeJobWidget w(e, "webdav", p);
    w.show();
    QSignalSpy open(&w, &ServeJobWidget::openUrlRequested);
    child<QToolButton>(w, "openBrowser")->click();
    QCOMPARE(open.count(), 1);
    QCOMPARE(open.at(0).at(0).toUrl(), QUrl("http://127.0.0.1:4321/"));

    child<QToolButton>(w, "copyUrl")->click();
    QCOMPARE(QGuiApplication::clipboard()->text(),
             QString("http://[::]:4321/"));

    const QStringList expect{"webdav", "r:a b", "http://[::]:4321/"};
    QCOMPARE(w.filterFields(), expect);

    // the details panel follows the toggle
    auto *details = child<QWidget>(w, "details");
    QVERIFY(details->isHidden());
    child<QToolButton>(w, "showDetails")->click();
    QVERIFY(details->isVisible());
    QCOMPARE(child<QLineEdit>(w, "url")->text(), QString("http://[::]:4321/"));
    QCOMPARE(child<QLineEdit>(w, "address")->text(), QString("[::]:4321"));
  }
};

QTEST_MAIN(TestServeJobWidget)
#include "tst_serve_job_widget.moc"
