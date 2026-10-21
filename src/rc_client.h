#pragma once
#include <QtCore>
#include <QtNetwork>
#include <functional>
#include <optional>

// Credentials of the private rclone rcd (I-5: only ever passed through the
// environment of rcd, never through an argv).
struct RcCredentials {
  QString user, pass;
};

// user = "rb-" + base64url(9 random bytes) (12 chars), pass =
// base64url(24 random bytes) (32 chars); QRandomGenerator::system().
RcCredentials generateRcCredentials();
// Inserts exactly RCLONE_RC_USER and RCLONE_RC_PASS.
void applyRcCredentials(QProcessEnvironment &env, const RcCredentials &c);
// Finds "Serving remote control on http://" followed by "127.0.0.1:",
// "localhost:" or "[::1]:" and digits; the port when it is 1..65535, nullopt
// for anything else.
std::optional<int> parseRcServingPort(QStringView logMsgOrLine);
// "main" (and any other mode) -> "r:path"; "shared" -> "r,shared_with_me=true:path";
// "trash" -> "r,trashed_only=true:path". `path` is passed through unchanged.
QString rcListFs(const QString &remote, const QString &path,
                 const QString &remoteMode);

struct RcReply {
  bool ok = false;
  int httpStatus = 0; // 0 when no HTTP reply arrived
  QJsonObject result;
  QString error;
};

// 0: error is networkError ("no reply" when empty); 200..299: a JSON object is
// ok, anything else is "invalid JSON reply"; >= 400: the string `error` of a
// JSON object body, else networkError, else "HTTP <status>"; any other status:
// "unexpected HTTP status <status>". httpStatus is always copied.
RcReply parseRcReply(int httpStatus, const QByteArray &body,
                     const QString &networkError);

// Asynchronous client for the rcd HTTP API. Its QNetworkAccessManager never
// uses a proxy: Basic-auth traffic to loopback must not leave the machine.
class RcClient : public QObject {
  Q_OBJECT

public:
  explicit RcClient(QObject *parent = nullptr);
  // Pending requests are aborted; no callback runs once this has started.
  ~RcClient() override;

  // base is "http://127.0.0.1:<port>/".
  void setEndpoint(const QUrl &base, const RcCredentials &c);
  void clearEndpoint();
  bool hasEndpoint() const;

  using Callback = std::function<void(const RcReply &)>;
  // POST base+path with `params` as JSON and Basic auth. timeoutMs > 0 sets
  // QNetworkRequest::setTransferTimeout, timeoutMs <= 0 disables the timeout.
  // `cb` runs at most once, never from inside call(), and not at all when
  // `context` (the client itself when null) is destroyed first. Without an
  // endpoint it is queued on `context` with error "rc endpoint not set".
  void call(const QString &path, const QJsonObject &params, QObject *context,
            Callback cb, int timeoutMs = 30000);

private:
  QNetworkAccessManager *mNetwork;
  QUrl mBase;
  RcCredentials mCredentials;
  // in-flight replies and the connections made to them
  QHash<QNetworkReply *, QList<QMetaObject::Connection>> mPending;
};
