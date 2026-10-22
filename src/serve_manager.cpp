#include "serve_manager.h"

#include <QPointer>

namespace {

const int kServeTimeoutMs = 30000;

// rclone's own text, "HTTP <status>" or "no reply" only when there is none.
QString failureText(const RcReply &reply) {
  if (!reply.error.isEmpty()) {
    return reply.error;
  }
  return reply.httpStatus > 0 ? QStringLiteral("HTTP %1").arg(reply.httpStatus)
                              : QStringLiteral("no reply");
}

} // namespace

const QLatin1String kWebuiFlavor("webdav-webui");

QJsonObject serveStartParams(const ServeParams &p) {
  QJsonObject o{{QStringLiteral("type"), p.type},
                {QStringLiteral("fs"), p.fs},
                {QStringLiteral("addr"), p.addr}};
  if (!p.user.isEmpty()) {
    o.insert(QStringLiteral("user"), p.user);
  }
  if (!p.pass.isEmpty()) {
    o.insert(QStringLiteral("pass"), p.pass);
  }
  if (p.readOnly) {
    o.insert(QStringLiteral("read_only"), true);
  }
  if (p.type == QLatin1String("sftp")) {
    // without it rclone 1.75.2 reads ~/.ssh/authorized_keys of the backend's
    // account: it accepts those keys under any user name, drops their
    // options, and loops forever on a line that is not a key (a trailing
    // comment), so the serve never starts and serve/* stops answering
    o.insert(QStringLiteral("authorized_keys"), QString());
  }
  return o;
}

QVector<ServeEntry> parseServeList(const QJsonObject &reply) {
  QVector<ServeEntry> out;
  const QJsonValue list = reply.value(QStringLiteral("list"));
  if (!list.isArray()) {
    return out;
  }
  const QJsonArray items = list.toArray();
  for (const QJsonValue &v : items) {
    const QJsonObject o = v.toObject();
    const QJsonValue idValue = o.value(QStringLiteral("id"));
    if (!idValue.isString() || idValue.toString().isEmpty()) {
      continue;
    }
    ServeEntry e;
    e.id = idValue.toString();
    e.addr = o.value(QStringLiteral("addr")).toString();
    const QJsonObject params = o.value(QStringLiteral("params")).toObject();
    e.type = params.value(QStringLiteral("type")).toString();
    if (e.type.isEmpty()) {
      const qsizetype dash = e.id.lastIndexOf(QLatin1Char('-'));
      if (dash > 0) {
        e.type = e.id.left(dash);
      }
    }
    e.fs = params.value(QStringLiteral("fs")).toString();
    out.append(e);
  }
  return out;
}

QStringList parseServeTypes(const QJsonObject &reply) {
  QStringList out;
  const QJsonValue types = reply.value(QStringLiteral("types"));
  if (!types.isArray()) {
    return out;
  }
  const QJsonArray items = types.toArray();
  for (const QJsonValue &v : items) {
    if (v.isString()) {
      out << v.toString();
    }
  }
  return out;
}

QString serveUrl(const ServeEntry &e) {
  static const QStringList httpTypes = {
      QStringLiteral("http"), QStringLiteral("webdav"),
      QStringLiteral("restic"), QStringLiteral("s3"), QStringLiteral("dlna")};
  if (httpTypes.contains(e.type)) {
    return QStringLiteral("http://") + e.addr + QLatin1Char('/');
  }
  if (e.type == QLatin1String("ftp") || e.type == QLatin1String("sftp") ||
      e.type == QLatin1String("nfs")) {
    return e.type + QStringLiteral("://") + e.addr + QLatin1Char('/');
  }
  return e.addr;
}

ServeTypeRules serveTypeRules(const QString &type) {
  ServeTypeRules r;
  if (type == QLatin1String("ftp")) {
    r.needsPort = true;
    r.defaultAddr = QStringLiteral("127.0.0.1:2121");
  } else if (type == QLatin1String("sftp")) {
    r.loginRequired = true;
  } else if (type == QLatin1String("restic")) {
    r.readOnlyAllowed = false;
  } else if (type == QLatin1String("s3") || type == QLatin1String("nfs") ||
             type == QLatin1String("dlna")) {
    r.loginAllowed = false;
  }
  return r;
}

bool serveOpensInBrowser(const QString &displayType) {
  return displayType == QLatin1String("http") ||
         displayType == QLatin1String("webdav") || displayType == kWebuiFlavor;
}

QString serveBrowserUrl(const ServeEntry &e) {
  if (!serveOpensInBrowser(e.type)) {
    return serveUrl(e);
  }
  const qsizetype colon = e.addr.lastIndexOf(QLatin1Char(':'));
  QString host = colon < 0 ? e.addr : e.addr.left(colon);
  if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
    host = host.mid(1, host.size() - 2);
  }
  if (host.isEmpty() || host == QLatin1String("0.0.0.0") ||
      host == QLatin1String("::")) {
    return QStringLiteral("http://127.0.0.1:") + e.addr.mid(colon + 1) +
           QLatin1Char('/');
  }
  return serveUrl(e);
}

ServeManager::ServeManager(RcCallFn call, QObject *parent)
    : QObject(parent), mCall(std::move(call)) {}

QStringList ServeManager::types() const { return mTypes; }

quint64 ServeManager::start(const ServeParams &p) {
  const quint64 token = ++mNextToken;
  mStartQueue.enqueue({token, p});
  processStartQueue();
  return token;
}

void ServeManager::processStartQueue() {
  if (mStartInFlight || mStartQueue.isEmpty()) {
    return;
  }
  const PendingStart pending = mStartQueue.dequeue();
  mStartInFlight = true;
  const QPointer<ServeManager> self(this);
  const quint64 token = pending.token;
  const QString type = pending.params.type;
  const QString fs = pending.params.fs;
  mCall(QStringLiteral("serve/start"), serveStartParams(pending.params),
        [self, token, type, fs](const RcReply &reply) {
          if (!self) {
            return;
          }
          if (reply.ok) {
            ServeEntry e;
            e.id = reply.result.value(QStringLiteral("id")).toString();
            e.addr = reply.result.value(QStringLiteral("addr")).toString();
            e.type = type;
            e.fs = fs;
            if (!e.id.isEmpty() && !e.addr.isEmpty()) {
              emit self->started(token, e);
            } else {
              const QString text = QStringLiteral("invalid serve/start reply");
              emit self->startFailed(token, text);
              if (self) {
                emit self->error(text);
              }
            }
          } else {
            const QString text = failureText(reply);
            emit self->startFailed(token, text);
            if (self) {
              emit self->error(text);
            }
          }
          if (self) {
            self->mStartInFlight = false;
            self->refresh();
            self->processStartQueue();
          }
        },
        kServeTimeoutMs);
}

void ServeManager::stop(const QString &id) {
  const QPointer<ServeManager> self(this);
  mCall(QStringLiteral("serve/stop"),
        QJsonObject{{QStringLiteral("id"), id}},
        [self](const RcReply &reply) {
          if (!self) {
            return;
          }
          if (!reply.ok) {
            emit self->error(failureText(reply));
          }
          if (self) {
            self->refresh();
          }
        },
        kServeTimeoutMs);
}

void ServeManager::refresh() {
  const QPointer<ServeManager> self(this);
  const quint64 seq = ++mRefreshSeq;
  mCall(QStringLiteral("serve/list"), QJsonObject(),
        [self, seq](const RcReply &reply) {
          // a newer refresh was started: this reply is stale
          if (!self || seq != self->mRefreshSeq) {
            return;
          }
          if (reply.ok) {
            emit self->listChanged(parseServeList(reply.result));
          } else {
            emit self->listFailed(failureText(reply));
          }
        },
        kServeTimeoutMs);
}

void ServeManager::loadTypes() {
  const QPointer<ServeManager> self(this);
  mCall(QStringLiteral("serve/types"), QJsonObject(),
        [self](const RcReply &reply) {
          if (!self) {
            return;
          }
          if (!reply.ok) {
            qWarning().noquote() << "serve/types failed:" << failureText(reply);
            return;
          }
          self->mTypes = parseServeTypes(reply.result);
          emit self->typesChanged(self->mTypes);
        },
        kServeTimeoutMs);
}
