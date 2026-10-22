#include "remote_actions.h"

#include <QUrl>

#include "remote_ops.h"

namespace {

// `dir` as a directory for copyurl --auto-filename: a '/' is appended unless
// `dir` ends with '/' or ':' ("r:" is the base of the remote, "r:/" is not).
QString dirWithSlash(const QString &dir) {
  if (dir.isEmpty() || dir.endsWith(QLatin1Char('/')) ||
      dir.endsWith(QLatin1Char(':'))) {
    return dir;
  }
  return dir + QLatin1Char('/');
}

} // namespace

QStringList buildCopytoArgs(const QString &src, const QString &dst) {
  return {QStringLiteral("copyto"), src, dst};
}

bool isHttpUrl(const QString &s) {
  const QUrl url(s);
  if (!url.isValid()) {
    return false;
  }
  const QString scheme = url.scheme().toLower();
  return (scheme == QLatin1String("http") || scheme == QLatin1String("https")) &&
         !url.host().isEmpty();
}

QString joinRclonePath(const QString &dir, const QString &name) {
  return dir.endsWith(QLatin1Char(':')) ? dir + name
                                        : RemoteOps::joinPath(dir, name);
}

QStringList buildCopyurlArgs(const QString &url, const QString &destDir,
                             const QString &fileName, bool noClobber) {
  QStringList args{QStringLiteral("copyurl"), url};
  if (fileName.isEmpty()) {
    args << dirWithSlash(destDir) << QStringLiteral("--auto-filename")
         << QStringLiteral("--print-filename");
  } else {
    args << joinRclonePath(destDir, fileName);
  }
  if (noClobber) {
    args << QStringLiteral("--no-clobber");
  }
  return args;
}
