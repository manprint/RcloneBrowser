#include "rc_client.h"

namespace {

QByteArray randomBytes(int count) {
  QByteArray bytes(count, '\0');
  int i = 0;
  while (i < count) {
    quint32 word = QRandomGenerator::system()->generate();
    for (int j = 0; j < 4 && i < count; ++j, ++i) {
      bytes[i] = static_cast<char>(word & 0xff);
      word >>= 8;
    }
  }
  return bytes;
}

QString base64Url(const QByteArray &bytes) {
  return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding |
                                            QByteArray::OmitTrailingEquals));
}

} // namespace

RcCredentials generateRcCredentials() {
  RcCredentials c;
  c.user = QStringLiteral("rb-") + base64Url(randomBytes(9));
  c.pass = base64Url(randomBytes(24));
  return c;
}

void applyRcCredentials(QProcessEnvironment &env, const RcCredentials &c) {
  env.insert(QStringLiteral("RCLONE_RC_USER"), c.user);
  env.insert(QStringLiteral("RCLONE_RC_PASS"), c.pass);
}

std::optional<int> parseRcServingPort(QStringView logMsgOrLine) {
  const QLatin1String marker("Serving remote control on http://");
  const qsizetype at = logMsgOrLine.indexOf(marker);
  if (at < 0) {
    return std::nullopt;
  }
  QStringView rest = logMsgOrLine.mid(at + marker.size());
  static const QLatin1String hosts[] = {QLatin1String("127.0.0.1:"),
                                        QLatin1String("localhost:"),
                                        QLatin1String("[::1]:")};
  bool hostFound = false;
  for (const QLatin1String &host : hosts) {
    if (rest.startsWith(host)) {
      rest = rest.mid(host.size());
      hostFound = true;
      break;
    }
  }
  if (!hostFound) {
    return std::nullopt;
  }
  int port = 0;
  qsizetype digits = 0;
  while (digits < rest.size() && rest.at(digits) >= QLatin1Char('0') &&
         rest.at(digits) <= QLatin1Char('9')) {
    port = port * 10 + (rest.at(digits).unicode() - u'0');
    if (port > 65535) {
      return std::nullopt;
    }
    ++digits;
  }
  if (digits == 0 || port < 1) {
    return std::nullopt;
  }
  return port;
}

QString rcListFs(const QString &remote, const QString &path,
                 const QString &remoteMode) {
  if (remoteMode == QLatin1String("shared")) {
    return remote + QStringLiteral(",shared_with_me=true:") + path;
  }
  if (remoteMode == QLatin1String("trash")) {
    return remote + QStringLiteral(",trashed_only=true:") + path;
  }
  return remote + QLatin1Char(':') + path;
}

RcReply parseRcReply(int httpStatus, const QByteArray &body,
                     const QString &networkError) {
  RcReply reply;
  reply.httpStatus = httpStatus;
  if (httpStatus == 0) {
    reply.error =
        networkError.isEmpty() ? QStringLiteral("no reply") : networkError;
    return reply;
  }
  if (httpStatus >= 200 && httpStatus < 300) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      reply.ok = true;
      reply.result = doc.object();
    } else {
      reply.error = QStringLiteral("invalid JSON reply");
    }
    return reply;
  }
  if (httpStatus >= 400) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QJsonValue message =
        doc.isObject() ? doc.object().value(QLatin1String("error"))
                       : QJsonValue();
    if (message.isString()) {
      reply.error = message.toString();
    } else if (!networkError.isEmpty()) {
      reply.error = networkError;
    } else {
      reply.error = QStringLiteral("HTTP %1").arg(httpStatus);
    }
    return reply;
  }
  reply.error = QStringLiteral("unexpected HTTP status %1").arg(httpStatus);
  return reply;
}

RcClient::RcClient(QObject *parent)
    : QObject(parent), mNetwork(new QNetworkAccessManager(this)) {
  mNetwork->setProxy(QNetworkProxy::NoProxy);
}

RcClient::~RcClient() {
  const auto pending = mPending;
  mPending.clear();
  for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
    for (const QMetaObject::Connection &connection : it.value()) {
      QObject::disconnect(connection);
    }
    it.key()->abort();
    delete it.key();
  }
}

void RcClient::setEndpoint(const QUrl &base, const RcCredentials &c) {
  mBase = base;
  mCredentials = c;
}

void RcClient::clearEndpoint() {
  mBase = QUrl();
  mCredentials = RcCredentials();
}

bool RcClient::hasEndpoint() const { return mBase.isValid() && !mBase.isEmpty(); }

void RcClient::call(const QString &path, const QJsonObject &params,
                    QObject *context, Callback cb, int timeoutMs) {
  QObject *receiver = context ? context : this;

  if (!hasEndpoint()) {
    QPointer<RcClient> self(this);
    QMetaObject::invokeMethod(
        receiver,
        [self, cb]() {
          if (!self || !cb) {
            return;
          }
          RcReply reply;
          reply.error = QStringLiteral("rc endpoint not set");
          cb(reply);
        },
        Qt::QueuedConnection);
    return;
  }

  QNetworkRequest request(mBase.resolved(QUrl(path)));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  const QByteArray login = (mCredentials.user + QLatin1Char(':') +
                            mCredentials.pass).toUtf8();
  request.setRawHeader("Authorization", "Basic " + login.toBase64());
  if (timeoutMs > 0) {
    request.setTransferTimeout(timeoutMs);
  }

  QNetworkReply *reply = mNetwork->post(
      request, QJsonDocument(params).toJson(QJsonDocument::Compact));

  QList<QMetaObject::Connection> connections;
  // Releases the reply; it is connected first, so it runs before the callback.
  connections << connect(reply, &QNetworkReply::finished, reply,
                         [this, reply]() {
                           mPending.remove(reply);
                           reply->deleteLater();
                         });
  connections << connect(
      reply, &QNetworkReply::finished, receiver, [reply, cb]() {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // an aborted or timed-out reply is closed: nothing to read
        const QByteArray body = reply->isOpen() ? reply->readAll() : QByteArray();
        const QString networkError = reply->error() != QNetworkReply::NoError
                                         ? reply->errorString()
                                         : QString();
        if (cb) {
          cb(parseRcReply(status, body, networkError));
        }
      });
  mPending.insert(reply, connections);
}
