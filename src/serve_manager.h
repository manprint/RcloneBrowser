#pragma once
#include <QtCore>

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

// Drives serve/start, serve/stop, serve/list and serve/types through the
// injected RcCallFn, which must reply asynchronously. Replies that arrive after
// the manager is gone are ignored. Nothing here logs params or replies
// (serve/list carries the credentials).
class ServeManager : public QObject {
  Q_OBJECT

public:
  explicit ServeManager(RcCallFn call, QObject *parent = nullptr);

  // Both start() and stop() are followed by refresh(), after their signal.
  void start(const ServeParams &p);
  void stop(const QString &id);
  void refresh();
  void loadTypes();
  // The last typesChanged payload; empty before the first success.
  QStringList types() const;

signals:
  void started(const ServeEntry &entry);
  void listChanged(const QVector<ServeEntry> &entries);
  void typesChanged(const QStringList &types);
  void error(const QString &message);
  // refresh() failures only; shown inline, never in a message box.
  void listFailed(const QString &message);

private:
  RcCallFn mCall;
  QStringList mTypes;
  quint64 mRefreshSeq = 0;
};
