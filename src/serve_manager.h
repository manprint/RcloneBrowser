#pragma once
#include <QtCore>
#include <QQueue>

#include "config_flow.h"

// The webdav-webui options (the fork's process-global rc option block
// "webdav_webui"): field defaults are the fork's. `secret` is a credential: it
// only ever travels in the options/set POST body (I-2).
struct WebuiOptions {
  bool allowDelete = true;
  bool search = true;
  int searchMaxDepth = 8;
  int searchMaxResults = 1000;
  QString searchTimeout = QStringLiteral("30s");
  QStringList publinkPaths;
  QString secret;
  QString tokenExpiry = QStringLiteral("72h");
  bool resume = false;
  QString resumeDir;
  QString resumeRetention = QStringLiteral("720h");
  int playerMaxRetry = 30;
  int playerReconnectInterval = 10;
  bool diskStats = false;
  bool uploadSettings = false;
  QString editMaxSize = QStringLiteral("4Mi");
  bool dirCount = false;
  int multiZipMax = 200;
};

// What `rclone serve` is started with (serve/start). The credentials only ever
// travel in the rc POST body (I-5).
// `flavor` is the display flavor of a webdav serve (kWebuiFlavor or empty);
// `webui` is used only for the webui flavor; a non-empty `vfsCacheMode`
// (minimal, writes, full) is sent as "vfs_cache_mode".
struct ServeParams {
  QString type, fs, addr = QStringLiteral("127.0.0.1:0"), user, pass;
  bool readOnly = false;
  QString flavor;
  WebuiOptions webui;
  QString vfsCacheMode;
};

// The 19 Go field names of the "webdav_webui" block, in the table order
// (Enabled first).
const QStringList &webuiOptionKeys();

// {"webdav_webui": {<the 19 keys>}} for options/set; Enabled = `enabled`.
// Durations and sizes are strings, PublinkPaths an array of strings.
QJsonObject webuiOptionsBlock(const WebuiOptions &o, bool enabled);

// {type, fs, addr}; "user" when not empty, "pass" when not empty, "read_only"
// (JSON bool true) when readOnly; for sftp always "authorized_keys": "" (see
// the .cpp); "vfs_cache_mode" when vfsCacheMode is not empty. Nothing is
// trimmed and no other key is ever added (never a webui* key): rclone rejects
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

// The message for a webdav-webui start on an rclone without the fork's
// "webdav_webui" block (the dialog and the manager show this one text).
extern const QLatin1String kWebuiUnsupported;

// The start message while the capability check failed: followed by rclone's
// text (webuiCheckError()), never a claim that the rclone lacks the block.
extern const QLatin1String kWebuiCheckFailed;

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
  // Whether the rclone behind the rc has the "webdav_webui" option block.
  enum class WebuiSupport { Unknown, Yes, No };
  Q_ENUM(WebuiSupport)

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

  WebuiSupport webuiSupport() const;
  // options/blocks: Yes when it lists "webdav_webui", else No; a failure
  // leaves the value as it was. No-op while a load is in flight.
  void loadCapabilities();
  // Back to Unknown (the backend restarted or stopped); a load still in flight
  // is abandoned: its reply changes nothing.
  void resetCapabilities();
  // rclone's text of the last failed check; empty after a successful one and
  // after resetCapabilities().
  QString webuiCheckError() const;

signals:
  void started(quint64 token, const ServeEntry &entry);
  // A failed start: emitted before error(), which carries the same text.
  void startFailed(quint64 token, const QString &message);
  void listChanged(const QVector<ServeEntry> &entries);
  void typesChanged(const QStringList &types);
  void error(const QString &message);
  // refresh() failures only; shown inline, never in a message box.
  void listFailed(const QString &message);
  void capabilitiesChanged(ServeManager::WebuiSupport support);
  // A failed options/blocks check (not a stale one): the text is rclone's.
  void capabilitiesCheckFailed(const QString &text);

private:
  struct PendingStart {
    quint64 token;
    ServeParams params;
  };
  void processStartQueue();
  void sendServeStart(const PendingStart &pending);

  RcCallFn mCall;
  QStringList mTypes;
  quint64 mRefreshSeq = 0;
  QQueue<PendingStart> mStartQueue;
  bool mStartInFlight = false;
  quint64 mNextToken = 0;
  WebuiSupport mWebui = WebuiSupport::Unknown;
  QString mWebuiCheckError;
  bool mCapsInFlight = false;
  bool mCapsTriedForFront = false;
  quint64 mCapsGeneration = 0;
};
