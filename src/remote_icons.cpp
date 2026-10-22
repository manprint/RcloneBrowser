#include "remote_icons.h"

#include <QFile>
#include <QRegularExpression>

const QMap<QString, QString> &remoteIconAliases() {
  static const QMap<QString, QString> aliases{
      {QStringLiteral("gcs"), QStringLiteral("google_cloud_storage")},
      {QStringLiteral("googlecloudstorage"),
       QStringLiteral("google_cloud_storage")},
      {QStringLiteral("gphotos"), QStringLiteral("google_photos")},
      {QStringLiteral("googlephotos"), QStringLiteral("google_photos")},
  };
  return aliases;
}

QString remoteIconBaseName(const QString &remoteType) {
  static const QRegularExpression valid(QStringLiteral("^[a-z0-9_]+$"));

  const QString key =
      remoteType.trimmed().toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
  if (!valid.match(key).hasMatch()) {
    return QStringLiteral("unknown");
  }
  const QString base = remoteIconAliases().value(key, key);
  if (QFile::exists(QStringLiteral(":media/images/remotes_icons/") + base +
                    QStringLiteral(".png"))) {
    return base;
  }
  return QStringLiteral("unknown");
}
