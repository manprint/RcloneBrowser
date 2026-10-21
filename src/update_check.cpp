#include "update_check.h"

#include <QRegularExpression>

std::optional<ReleaseInfo> parseLatestRelease(const QByteArray &json) {
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return std::nullopt;
  }
  const QJsonObject obj = doc.object();
  const QJsonValue tag = obj.value(QLatin1String("tag_name"));
  if (!tag.isString()) {
    return std::nullopt;
  }
  ReleaseInfo info;
  info.tag = tag.toString();
  info.version = RcloneVersion::fromTag(info.tag);
  if (!info.version.valid) {
    return std::nullopt;
  }
  const QJsonValue url = obj.value(QLatin1String("html_url"));
  if (url.isString()) {
    info.htmlUrl = QUrl(url.toString());
  }
  return info;
}

bool isValidRepoSlug(const QString &slug) {
  static const QRegularExpression re(QRegularExpression::anchoredPattern(
      QStringLiteral("[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")));
  return re.match(slug).hasMatch();
}

bool updateCheckDue(const QString &storedDate, const QDate &today) {
  return storedDate != today.toString(Qt::ISODate);
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent), mApiBase(QStringLiteral("https://api.github.com")) {}

void UpdateChecker::setApiBase(const QUrl &base) { mApiBase = base; }

void UpdateChecker::check(const QString &slug, const RcloneVersion &current) {
  if (!isValidRepoSlug(slug)) {
    emit failed(QStringLiteral("Invalid repository '%1' (expected owner/repo)")
                    .arg(slug));
    return;
  }
  if (!current.valid) {
    emit failed(QStringLiteral("The current version is unknown"));
    return;
  }

  QUrl url = mApiBase;
  QString path = url.path();
  while (path.endsWith(QLatin1Char('/'))) {
    path.chop(1);
  }
  url.setPath(path + QStringLiteral("/repos/") + slug +
              QStringLiteral("/releases/latest"));

  QNetworkRequest request(url);
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("RcloneBrowser"));
  request.setTransferTimeout(15000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = mNetwork.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, current]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      const int status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (status != 0) {
        emit failed(QStringLiteral("HTTP %1: %2")
                        .arg(status)
                        .arg(reply->errorString()));
      } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        emit failed(QStringLiteral("Timed out"));
      } else {
        emit failed(reply->errorString());
      }
      return;
    }
    const std::optional<ReleaseInfo> release =
        parseLatestRelease(reply->readAll());
    if (!release) {
      emit failed(QStringLiteral("Unexpected response (no usable tag_name)"));
      return;
    }
    if (release->version.compare(current) > 0) {
      emit newerAvailable(*release);
    } else {
      emit upToDate();
    }
  });
}
