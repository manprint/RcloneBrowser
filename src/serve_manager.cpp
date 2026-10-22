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

ServeManager::ServeManager(RcCallFn call, QObject *parent)
    : QObject(parent), mCall(std::move(call)) {}

QStringList ServeManager::types() const { return mTypes; }

void ServeManager::start(const ServeParams &p) {
  const QPointer<ServeManager> self(this);
  const QString type = p.type;
  const QString fs = p.fs;
  mCall(QStringLiteral("serve/start"), serveStartParams(p),
        [self, type, fs](const RcReply &reply) {
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
              emit self->started(e);
            } else {
              emit self->error(QStringLiteral("invalid serve/start reply"));
            }
          } else {
            emit self->error(failureText(reply));
          }
          if (self) {
            self->refresh();
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
