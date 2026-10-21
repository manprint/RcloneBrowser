#include "rclone_version.h"

#include <QRegularExpression>

namespace {

bool fillTriple(const QRegularExpressionMatch &m, RcloneVersion &v) {
  bool ok1 = false, ok2 = false, ok3 = false;
  v.major = m.captured(1).toInt(&ok1);
  v.minor = m.captured(2).toInt(&ok2);
  v.patch = m.captured(3).toInt(&ok3);
  v.valid = ok1 && ok2 && ok3;
  if (v.valid) {
    v.suffix = m.captured(4);
  } else {
    v = RcloneVersion();
  }
  return v.valid;
}

QString firstLine(QStringView text) {
  const qsizetype nl = text.indexOf(QLatin1Char('\n'));
  QStringView line = nl < 0 ? text : text.left(nl);
  if (line.endsWith(QLatin1Char('\r'))) {
    line.chop(1);
  }
  return line.toString();
}

} // namespace

RcloneVersion RcloneVersion::fromVersionOutput(QStringView output) {
  static const QRegularExpression re(QStringLiteral(
      "^rclone v?(\\d+)\\.(\\d+)\\.(\\d+)(?:-([0-9A-Za-z.+-]+))?\\s*$"));
  RcloneVersion v;
  const QRegularExpressionMatch m = re.match(firstLine(output));
  if (m.hasMatch()) {
    fillTriple(m, v);
  }
  return v;
}

RcloneVersion RcloneVersion::fromTag(QStringView tag) {
  // anchoredPattern: '$' alone would also match before a final '\n'.
  static const QRegularExpression re(QRegularExpression::anchoredPattern(
      QStringLiteral("v?(\\d+)\\.(\\d+)\\.(\\d+)(?:-([0-9A-Za-z.+-]+))?")));
  RcloneVersion v;
  const QRegularExpressionMatch m = re.match(tag.toString());
  if (m.hasMatch()) {
    fillTriple(m, v);
  }
  return v;
}

QString RcloneVersion::toString() const {
  if (!valid) {
    return QString();
  }
  QString s = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
  if (!suffix.isEmpty()) {
    s += QLatin1Char('-') + suffix;
  }
  return s;
}

int RcloneVersion::compare(const RcloneVersion &o) const {
  if (major != o.major) {
    return major < o.major ? -1 : 1;
  }
  if (minor != o.minor) {
    return minor < o.minor ? -1 : 1;
  }
  if (patch != o.patch) {
    return patch < o.patch ? -1 : 1;
  }
  if (suffix.isEmpty() && o.suffix.isEmpty()) {
    return 0;
  }
  if (suffix.isEmpty()) {
    return 1;
  }
  if (o.suffix.isEmpty()) {
    return -1;
  }
  const int c = QString::compare(suffix, o.suffix);
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

RcloneVersion minimumRcloneVersion() {
  RcloneVersion v;
  v.major = 1;
  v.minor = 75;
  v.patch = 0;
  v.valid = true;
  return v;
}

bool isSupportedRclone(const RcloneVersion &v) {
  if (!v.valid) {
    return false;
  }
  const RcloneVersion min = minimumRcloneVersion();
  RcloneVersion triple = v;
  triple.suffix.clear();
  return triple.compare(min) >= 0;
}

QString rcloneVersionProblem(const QString &versionOutput) {
  const RcloneVersion v = RcloneVersion::fromVersionOutput(versionOutput);
  if (isSupportedRclone(v)) {
    return QString();
  }
  const QString head =
      QStringLiteral("Rclone Browser 2 requires rclone 1.75.0 or newer.\n");
  if (!v.valid) {
    return head +
           QStringLiteral("Could not recognize the output of 'rclone "
                          "version':\n") +
           firstLine(versionOutput).left(200);
  }
  return head + QStringLiteral("Found: rclone v") + v.toString();
}
