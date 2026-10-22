#include "upload_headers.h"

#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {

const qsizetype kMaxS3Tags = 10;

QString pct(const QString &s) {
  return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

} // namespace

std::optional<QVector<S3Tag>> parseS3Tags(const QString &text) {
  QVector<S3Tag> tags;
  QSet<QString> seen;
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString &raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const qsizetype eq = line.indexOf(QLatin1Char('='));
    if (eq < 0) {
      return std::nullopt;
    }
    const QString key = line.left(eq).trimmed();
    if (key.isEmpty() || seen.contains(key)) {
      return std::nullopt;
    }
    seen.insert(key);
    tags.append(S3Tag{key, line.mid(eq + 1).trimmed()});
    if (tags.size() > kMaxS3Tags) {
      return std::nullopt;
    }
  }
  return tags;
}

QString s3TaggingHeader(const QVector<S3Tag> &tags) {
  QStringList pairs;
  for (const S3Tag &tag : tags) {
    pairs << (pct(tag.key) + QLatin1Char('=') + pct(tag.value));
  }
  return QStringLiteral("X-Amz-Tagging: ") + pairs.join(QLatin1Char('&'));
}

std::optional<QStringList> parseUploadHeaders(const QString &text) {
  static const QRegularExpression nameRe(QRegularExpression::anchoredPattern(
      QStringLiteral("[!#$%&'*+.^_`|~0-9A-Za-z-]+")));
  QStringList headers;
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString &raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    const qsizetype colon = line.indexOf(QLatin1Char(':'));
    if (colon < 0) {
      return std::nullopt;
    }
    const QString name = line.left(colon).trimmed();
    if (!nameRe.match(name).hasMatch()) {
      return std::nullopt;
    }
    headers << (name + QStringLiteral(": ") + line.mid(colon + 1).trimmed());
  }
  return headers;
}

QStringList headerUploadArgs(const QStringList &headers) {
  QStringList args;
  for (const QString &header : headers) {
    args << QStringLiteral("--header-upload") << header;
  }
  return args;
}
