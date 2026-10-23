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

const QLatin1String kWebuiUnsupported(
    "This rclone does not support webdav-webui: it needs rclone built from the "
    "webui fork. Choose webdav for a WebDAV serve without the web interface.");

const QStringList &webuiOptionKeys() {
  static const QStringList keys = {
      QStringLiteral("Enabled"),
      QStringLiteral("AllowDelete"),
      QStringLiteral("Search"),
      QStringLiteral("SearchMaxDepth"),
      QStringLiteral("SearchMaxResults"),
      QStringLiteral("SearchTimeout"),
      QStringLiteral("PublinkPaths"),
      QStringLiteral("Secret"),
      QStringLiteral("TokenExpiry"),
      QStringLiteral("Resume"),
      QStringLiteral("ResumeDir"),
      QStringLiteral("ResumeRetention"),
      QStringLiteral("PlayerMaxRetry"),
      QStringLiteral("PlayerReconnectInterval"),
      QStringLiteral("DiskStats"),
      QStringLiteral("UploadSettings"),
      QStringLiteral("EditMaxSize"),
      QStringLiteral("DirCount"),
      QStringLiteral("MultiZipMax")};
  return keys;
}

QJsonObject webuiOptionsBlock(const WebuiOptions &o, bool enabled) {
  const QJsonArray publink = QJsonArray::fromStringList(o.publinkPaths);
  const QJsonObject block{
      {QStringLiteral("Enabled"), enabled},
      {QStringLiteral("AllowDelete"), o.allowDelete},
      {QStringLiteral("Search"), o.search},
      {QStringLiteral("SearchMaxDepth"), o.searchMaxDepth},
      {QStringLiteral("SearchMaxResults"), o.searchMaxResults},
      {QStringLiteral("SearchTimeout"), o.searchTimeout},
      {QStringLiteral("PublinkPaths"), publink},
      {QStringLiteral("Secret"), o.secret},
      {QStringLiteral("TokenExpiry"), o.tokenExpiry},
      {QStringLiteral("Resume"), o.resume},
      {QStringLiteral("ResumeDir"), o.resumeDir},
      {QStringLiteral("ResumeRetention"), o.resumeRetention},
      {QStringLiteral("PlayerMaxRetry"), o.playerMaxRetry},
      {QStringLiteral("PlayerReconnectInterval"), o.playerReconnectInterval},
      {QStringLiteral("DiskStats"), o.diskStats},
      {QStringLiteral("UploadSettings"), o.uploadSettings},
      {QStringLiteral("EditMaxSize"), o.editMaxSize},
      {QStringLiteral("DirCount"), o.dirCount},
      {QStringLiteral("MultiZipMax"), o.multiZipMax}};
  return QJsonObject{{QStringLiteral("webdav_webui"), block}};
}

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
  if (!p.vfsCacheMode.isEmpty()) {
    o.insert(QStringLiteral("vfs_cache_mode"), p.vfsCacheMode);
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
  const bool isWebdav =
      mStartQueue.head().params.type == QLatin1String("webdav");
  // a webdav start needs to know what the rclone supports: resolve it first;
  // the reply comes back here (a failed check is tried once per start)
  if (isWebdav && mWebui == WebuiSupport::Unknown && !mCapsTriedForFront) {
    loadCapabilities();
    return;
  }
  const PendingStart pending = mStartQueue.dequeue();
  mCapsTriedForFront = false;
  mStartInFlight = true;
  if (!isWebdav) {
    sendServeStart(pending);
    return;
  }
  const bool webui = pending.params.flavor == kWebuiFlavor;
  if (webui && mWebui != WebuiSupport::Yes) {
    // never start the web UI on an rclone that cannot serve it (I-10)
    const QString text(kWebuiUnsupported);
    emit startFailed(pending.token, text);
    emit error(text);
    mStartInFlight = false;
    processStartQueue();
    return;
  }
  if (mWebui != WebuiSupport::Yes) {
    sendServeStart(pending);
    return;
  }
  // the option block is process-global and read when the server is built:
  // set it right before this start, which is the only one in flight (I-3)
  const QPointer<ServeManager> self(this);
  mCall(QStringLiteral("options/set"),
        webuiOptionsBlock(pending.params.webui, webui),
        [self, pending](const RcReply &reply) {
          if (!self) {
            return;
          }
          if (reply.ok) {
            self->sendServeStart(pending);
            return;
          }
          const QString text =
              QStringLiteral("webdav-webui options: ") + failureText(reply);
          emit self->startFailed(pending.token, text);
          if (self) {
            emit self->error(text);
          }
          if (self) {
            self->mStartInFlight = false;
            self->refresh();
            self->processStartQueue();
          }
        },
        kServeTimeoutMs);
}

void ServeManager::sendServeStart(const PendingStart &pending) {
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

ServeManager::WebuiSupport ServeManager::webuiSupport() const {
  return mWebui;
}

void ServeManager::loadCapabilities() {
  if (mCapsInFlight) {
    return;
  }
  mCapsInFlight = true;
  const QPointer<ServeManager> self(this);
  const quint64 generation = mCapsGeneration;
  mCall(QStringLiteral("options/blocks"), QJsonObject(),
        [self, generation](const RcReply &reply) {
          // a reset (backend restart) came in between: not this reply's answer
          if (!self || generation != self->mCapsGeneration) {
            return;
          }
          self->mCapsInFlight = false;
          // counts as the check of a waiting start only when one waits: a
          // failed check without one leaves the next webdav start to retry
          self->mCapsTriedForFront = !self->mStartQueue.isEmpty();
          if (reply.ok) {
            bool found = false;
            const QJsonValue blocks =
                reply.result.value(QStringLiteral("options"));
            if (blocks.isArray()) {
              const QJsonArray items = blocks.toArray();
              for (const QJsonValue &v : items) {
                if (v.isString() &&
                    v.toString() == QLatin1String("webdav_webui")) {
                  found = true;
                  break;
                }
              }
            }
            const WebuiSupport support =
                found ? WebuiSupport::Yes : WebuiSupport::No;
            if (support != self->mWebui) {
              self->mWebui = support;
              emit self->capabilitiesChanged(support);
            }
          } else {
            qWarning().noquote()
                << "options/blocks failed:" << failureText(reply);
          }
          if (self) {
            self->processStartQueue();
          }
        },
        kServeTimeoutMs);
}

void ServeManager::resetCapabilities() {
  ++mCapsGeneration;
  mCapsInFlight = false;
  mCapsTriedForFront = false;
  if (mWebui != WebuiSupport::Unknown) {
    mWebui = WebuiSupport::Unknown;
    emit capabilitiesChanged(mWebui);
  }
  // a start that waited for an abandoned load must not hang
  processStartQueue();
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
