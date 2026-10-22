#pragma once
#include <QtCore>
#include <QQueue>

#include "config_flow.h"

// What `rclone serve` is started with (serve/start). The credentials only ever
// travel in the rc POST body (I-5).
struct ServeParams {
  QString type, fs, addr = QStringLiteral("127.0.0.1:0"), user, pass;
  bool readOnly = false;
};

// {type, fs, addr}; "user" when not empty, "pass" when not empty, "read_only"
// (JSON bool true) when readOnly; for sftp always "authorized_keys": "" (see
// the .cpp). Nothing is trimmed and no other key is ever added: rclone rejects
// keys a serve type does not know.
QJsonObject serveStartParams(const ServeParams &p);

// A running serve as serve/list reports it. There is deliberately no password
// field: serve/list echoes params.pass and params.user in clear text.
struct ServeEntry {
  QString id, addr, type, fs;
};
Q_DECLARE_METATYPE(ServeEntry)

// The display type of a WebDAV serve started with the web UI flavor: the rc
// type stays "webdav", the Jobs list shows this name instead.
extern const QLatin1String kWebuiFlavor;

// reply["list"]; entries without a string id are skipped, type and fs come
// from the object "params" (type falls back to the id prefix before the last
// '-'), user and pass are never copied. A non-array "list" gives no entries.
QVector<ServeEntry> parseServeList(const QJsonObject &reply);

// reply["types"], strings only, in order.
QStringList parseServeTypes(const QJsonObject &reply);

// The address to give a client: http, webdav, restic, s3 and dlna are
// "http://<addr>/", ftp, sftp and nfs use their own scheme, an unknown type
// gives the bare address. The address is used literally ("[::]:4321" stays).
QString serveUrl(const ServeEntry &e);

// What the dialog may offer per serve type (rclone 1.75.2, probed against a
// real rcd): restic has no read-only mode, s3, nfs and dlna take no user/pass,
// sftp needs both, ftp cannot report a random port. Unknown types: defaults.
struct ServeTypeRules {
  bool readOnlyAllowed = true;
  bool loginAllowed = true;
  bool loginRequired = false;
  bool needsPort = false;
  QString defaultAddr = QStringLiteral("127.0.0.1:0");
};
ServeTypeRules serveTypeRules(const QString &type);

// serveUrl(e) for a browser: for http, webdav and webdav-webui an unspecified
// host (empty, "0.0.0.0", "::" or "[::]") is replaced by 127.0.0.1, so
// "[::]:4321" gives "http://127.0.0.1:4321/". Every other entry gives
// serveUrl(e) unchanged.
QString serveBrowserUrl(const ServeEntry &e);

// Whether a serve of this display type is a web page: "http", "webdav" and
// kWebuiFlavor.
bool serveOpensInBrowser(const QString &displayType);

// Drives serve/start, serve/stop, serve/list and serve/types through the
// injected RcCallFn, which must reply asynchronously. Starts run one at a time,
// in the order of the start() calls. Replies that arrive after the manager is
// gone are ignored. Nothing here logs params or replies
// (serve/list carries the credentials).
class ServeManager : public QObject {
  Q_OBJECT

public:
  explicit ServeManager(RcCallFn call, QObject *parent = nullptr);

  // Queues a serve/start and returns its token (1, 2, ...), which started()
  // or startFailed() carries back. Both start() and stop() are followed by
  // refresh(), after their signal.
  quint64 start(const ServeParams &p);
  void stop(const QString &id);
  void refresh();
  void loadTypes();
  // The last typesChanged payload; empty before the first success.
  QStringList types() const;

signals:
  void started(quint64 token, const ServeEntry &entry);
  // A failed start: emitted before error(), which carries the same text.
  void startFailed(quint64 token, const QString &message);
  void listChanged(const QVector<ServeEntry> &entries);
  void typesChanged(const QStringList &types);
  void error(const QString &message);
  // refresh() failures only; shown inline, never in a message box.
  void listFailed(const QString &message);

private:
  struct PendingStart {
    quint64 token;
    ServeParams params;
  };
  void processStartQueue();

  RcCallFn mCall;
  QStringList mTypes;
  quint64 mRefreshSeq = 0;
  QQueue<PendingStart> mStartQueue;
  bool mStartInFlight = false;
  quint64 mNextToken = 0;
};
