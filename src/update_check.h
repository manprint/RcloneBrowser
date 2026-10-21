#pragma once
#include <QtCore>
#include <QtNetwork>
#include <optional>

#include "rclone_version.h"

struct ReleaseInfo {
  QString tag;           // "tag_name" as sent by the server
  RcloneVersion version; // fromTag(tag)
  QUrl htmlUrl;          // "html_url"; empty when missing
};
Q_DECLARE_METATYPE(ReleaseInfo)

// Needs a string tag_name that RcloneVersion::fromTag accepts; html_url is
// optional. nullopt for anything else (bad JSON, arrays, "latest", ...).
std::optional<ReleaseInfo> parseLatestRelease(const QByteArray &json);
// ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$
bool isValidRepoSlug(const QString &slug);
// True unless storedDate == today.toString(Qt::ISODate).
bool updateCheckDue(const QString &storedDate, const QDate &today);

// Asks the GitHub API for the latest release of a repository, asynchronously.
// Exactly one of the three signals is emitted per check() call.
class UpdateChecker : public QObject {
  Q_OBJECT

public:
  explicit UpdateChecker(QObject *parent = nullptr);

  // Default https://api.github.com ; tests use http://127.0.0.1:<port>.
  void setApiBase(const QUrl &base);
  // GET <base>/repos/<slug>/releases/latest, Accept: application/vnd.github+json,
  // transfer timeout 15 s.
  void check(const QString &slug, const RcloneVersion &current);

signals:
  void newerAvailable(const ReleaseInfo &release);
  void upToDate();
  // HTTP error, timeout, bad JSON, invalid slug or unknown current version.
  void failed(const QString &reason);

private:
  QNetworkAccessManager mNetwork;
  QUrl mApiBase;
};
