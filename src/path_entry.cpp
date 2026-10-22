#include "path_entry.h"

#include <QDir>

QString stripRemotePrefix(const QString &text, const QString &remote) {
  if (remote.isEmpty()) {
    return text;
  }
  const QString prefix = remote + QLatin1Char(':');
  if (!text.startsWith(prefix)) {
    return text;
  }
  return text.mid(prefix.size());
}

QString normalizeEnteredPath(const QString &text, const QString &remote,
                             bool isLocal) {
  // no second trim: a path may start with a space after "r:"
  const QString path = stripRemotePrefix(text.trimmed(), remote);
  if (isLocal) {
    QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    // a bare drive ("C:") is the current directory of that drive for rclone;
    // the drive root is meant
    if (clean.size() == 2 && clean.at(0).isLetter() &&
        clean.at(1) == QLatin1Char(':')) {
      clean += QLatin1Char('/');
    }
    return clean;
  }

  QString result;
  result.reserve(path.size());
  for (const QChar c : path) {
    if (c == QLatin1Char('/') && result.endsWith(QLatin1Char('/'))) {
      continue;
    }
    result += c;
  }
  if (result.size() > 1 && result.endsWith(QLatin1Char('/'))) {
    result.chop(1);
  }
  return result;
}

bool remoteTypeNeedsPathEntry(const QString &remoteType) {
  return remoteType == QLatin1String("internetarchive");
}
