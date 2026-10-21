#include <QtNetwork>
#include <QtTest>

#include "rb_test_support.h"
#include "rclone_process.h"
#include "rclone_version.h"
#include "update_check.h"

namespace {

// Answers every request with one canned response. The request path is
// recorded; the full request head is read before replying.
class CannedServer : public QObject {
public:
  CannedServer(int code, const QByteArray &reason, const QByteArray &body) {
    mCode = code;
    mReason = reason;
    mBody = body;
    connect(&mServer, &QTcpServer::newConnection, this, [this]() {
      while (QTcpSocket *s = mServer.nextPendingConnection()) {
        s->setParent(this);
        connect(s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead(s); });
      }
    });
  }
  bool listen() { return mServer.listen(QHostAddress::LocalHost, 0); }
  quint16 port() const { return mServer.serverPort(); }
  QString lastPath() const { return mPath; }
  QByteArray lastAccept() const { return mAccept; }

private:
  void onReadyRead(QTcpSocket *s) {
    QByteArray &buf = mBuffers[s];
    buf += s->readAll();
    const int end = buf.indexOf("\r\n\r\n");
    if (end < 0) {
      return;
    }
    const QList<QByteArray> lines = buf.left(end).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    mPath = QString::fromLatin1(first.value(1));
    for (const QByteArray &l : lines) {
      if (l.toLower().startsWith("accept:")) {
        mAccept = l.mid(7).trimmed();
      }
    }
    mBuffers.remove(s);
    const QByteArray reply =
        "HTTP/1.1 " + QByteArray::number(mCode) + " " + mReason +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        QByteArray::number(mBody.size()) + "\r\nConnection: close\r\n\r\n" +
        mBody;
    s->write(reply);
    s->disconnectFromHost();
  }

  QTcpServer mServer;
  QHash<QTcpSocket *, QByteArray> mBuffers;
  int mCode = 200;
  QByteArray mReason, mBody;
  QString mPath;
  QByteArray mAccept;
};

QUrl baseOf(const CannedServer &s) {
  return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(s.port()));
}

} // namespace

class TestE2eUpdateCheck : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    rbInitTestEnvironment();
    qRegisterMetaType<ReleaseInfo>();
  }

  void T_E2E_UPD_NEWER() {
    CannedServer server(200, "OK",
                        "{\"tag_name\":\"v9.9.9\",\"html_url\":\"https://example.invalid/r\"}");
    QVERIFY(server.listen());
    UpdateChecker checker;
    checker.setApiBase(baseOf(server));
    QSignalSpy newer(&checker, &UpdateChecker::newerAvailable);
    QSignalSpy same(&checker, &UpdateChecker::upToDate);
    QSignalSpy failed(&checker, &UpdateChecker::failed);
    checker.check("rclone/rclone", RcloneVersion::fromTag(u"1.75.2"));
    QTRY_COMPARE_WITH_TIMEOUT(newer.count(), 1, 5000);
    const ReleaseInfo r = newer.at(0).at(0).value<ReleaseInfo>();
    QCOMPARE(r.tag, QString("v9.9.9"));
    QCOMPARE(r.htmlUrl, QUrl("https://example.invalid/r"));
    QCOMPARE(server.lastPath(), QString("/repos/rclone/rclone/releases/latest"));
    QCOMPARE(server.lastAccept(), QByteArray("application/vnd.github+json"));
    QCOMPARE(same.count(), 0);
    QCOMPARE(failed.count(), 0);
  }

  void T_E2E_UPD_SAME() {
    CannedServer server(200, "OK", "{\"tag_name\":\"v1.75.2\"}");
    QVERIFY(server.listen());
    UpdateChecker checker;
    checker.setApiBase(baseOf(server));
    QSignalSpy newer(&checker, &UpdateChecker::newerAvailable);
    QSignalSpy same(&checker, &UpdateChecker::upToDate);
    QSignalSpy failed(&checker, &UpdateChecker::failed);
    checker.check("rclone/rclone", RcloneVersion::fromTag(u"1.75.2"));
    QTRY_COMPARE_WITH_TIMEOUT(same.count(), 1, 5000);
    QCOMPARE(newer.count(), 0);
    QCOMPARE(failed.count(), 0);
  }

  void T_E2E_UPD_404() {
    CannedServer server(404, "Not Found", "{\"message\":\"Not Found\"}");
    QVERIFY(server.listen());
    UpdateChecker checker;
    checker.setApiBase(baseOf(server));
    QSignalSpy newer(&checker, &UpdateChecker::newerAvailable);
    QSignalSpy same(&checker, &UpdateChecker::upToDate);
    QSignalSpy failed(&checker, &UpdateChecker::failed);
    checker.check("rclone/rclone", RcloneVersion::fromTag(u"1.75.2"));
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
    QVERIFY2(failed.at(0).at(0).toString().contains("404"),
             qPrintable(failed.at(0).at(0).toString()));
    QCOMPARE(newer.count(), 0);
    QCOMPARE(same.count(), 0);
  }

  void T_E2E_VERSION_REAL() {
    QVERIFY2(!rbRclone().isEmpty(), "RB_TEST_RCLONE is not set");
    QProcess p;
    ApplyRcloneEnvironment(&p);
    p.start(rbRclone(), QStringList() << "version");
    QVERIFY(p.waitForFinished(10000));
    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QCOMPARE(p.exitCode(), 0);
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const RcloneVersion v = RcloneVersion::fromVersionOutput(out);
    QVERIFY2(v.valid, qPrintable(out));
    QVERIFY2(isSupportedRclone(v), qPrintable(out));
    QVERIFY(rcloneVersionProblem(out).isEmpty());
  }
};

QTEST_MAIN(TestE2eUpdateCheck)
#include "tst_e2e_update_check.moc"
