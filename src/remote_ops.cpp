#include "remote_ops.h"

namespace RemoteOps {

QString joinPath(const QString &parent, const QString &name) {
  if (parent.isEmpty()) {
    return name;
  }
  if (parent.endsWith(QLatin1Char('/'))) {
    return parent + name;
  }
  return parent + QLatin1Char('/') + name;
}

QString lastComponent(const QString &path) {
  qsizetype end = path.size();
  while (end > 0 && path.at(end - 1) == QLatin1Char('/')) {
    --end;
  }
  if (end == 0) {
    return QString();
  }
  const qsizetype slash = path.lastIndexOf(QLatin1Char('/'), end - 1);
  return path.mid(slash + 1, end - slash - 1);
}

QString folderMoveDest(const QString &srcPath, const QString &destParent) {
  return joinPath(destParent, lastComponent(srcPath));
}

QStringList moveArgs(const QString &remote, const QString &srcPath,
                     const QString &destPath, const QStringList &common,
                     const QStringList &extra) {
  QStringList args;
  args << QStringLiteral("move") << common << remote + QLatin1Char(':') + srcPath
       << remote + QLatin1Char(':') + destPath << extra;
  return args;
}

QStringList folderMoveArgs(const QString &remote, const QString &srcPath,
                           const QString &destParent,
                           const QStringList &common,
                           const QStringList &extra) {
  return moveArgs(remote, srcPath, folderMoveDest(srcPath, destParent), common,
                  extra);
}

QStringList sourceCleanupArgs(const QString &remote, const QString &srcPath,
                              const QStringList &common) {
  QStringList args;
  args << QStringLiteral("rmdir") << common
       << remote + QLatin1Char(':') + srcPath;
  return args;
}

namespace {

// A pflag bool value: everything but the spellings of false means true.
bool isFalseValue(const QString &value) {
  const QString v = value.toLower();
  return v == QLatin1String("false") || v == QLatin1String("f") ||
         v == QLatin1String("0");
}

// True when the argument turns on --dry-run or --interactive: the long names
// (alone or with a value), or a short flag or cluster of short flags that has
// -n or -i in it (-n, -i, -vn, -nv, ...).
bool vetoesCleanup(const QString &arg) {
  for (const QString &name :
       {QStringLiteral("--dry-run"), QStringLiteral("--interactive")}) {
    if (arg == name) {
      return true;
    }
    if (arg.startsWith(name + QLatin1Char('='))) {
      return !isFalseValue(arg.mid(name.size() + 1));
    }
  }
  if (arg.size() > 1 && arg.at(0) == QLatin1Char('-') &&
      arg.at(1) != QLatin1Char('-')) {
    const qsizetype eq = arg.indexOf(QLatin1Char('='));
    const QString letters = arg.mid(1, eq < 0 ? -1 : eq - 1);
    if (letters.isEmpty()) {
      return false;
    }
    for (const QChar ch : letters) {
      if (!ch.isLetter() || ch.unicode() > 127) {
        return false; // not a flag cluster (a filter rule, a path, ...)
      }
    }
    return letters.contains(QLatin1Char('n')) ||
           letters.contains(QLatin1Char('i'));
  }
  return false;
}

QString trimSlashes(const QString &path) {
  qsizetype begin = 0;
  qsizetype end = path.size();
  while (begin < end && path.at(begin) == QLatin1Char('/')) {
    ++begin;
  }
  while (end > begin && path.at(end - 1) == QLatin1Char('/')) {
    --end;
  }
  return path.mid(begin, end - begin);
}

} // namespace

bool wantsSourceCleanup(const QStringList &extra) {
  if (!extra.contains(QStringLiteral("--delete-empty-src-dirs"))) {
    return false;
  }
  for (const QString &arg : extra) {
    if (vetoesCleanup(arg)) {
      return false;
    }
  }
  return true;
}

bool isSameLocation(const QString &src, const QString &dest) {
  return trimSlashes(src) == trimSlashes(dest);
}

bool isBucketRemoteType(const QString &remoteType) {
  static const QStringList types = {
      QStringLiteral("s3"),
      QStringLiteral("b2"),
      QStringLiteral("google cloud storage"),
      QStringLiteral("googlecloudstorage"),
      QStringLiteral("gcs"),
      QStringLiteral("azureblob"),
      QStringLiteral("swift"),
      QStringLiteral("oracleobjectstorage"),
      QStringLiteral("oos"),
      QStringLiteral("qingstor"),
      QStringLiteral("storj"),
      QStringLiteral("tardigrade")};
  return types.contains(remoteType);
}

bool shouldCleanSource(bool isFolderMove, const QString &remoteType,
                       const QString &srcPath, const QString &destPath,
                       QProcess::ProcessState state,
                       QProcess::ProcessError error,
                       QProcess::ExitStatus status, int exitCode,
                       const QStringList &extra) {
  if (!isFolderMove || state != QProcess::NotRunning ||
      error == QProcess::FailedToStart || error == QProcess::Crashed ||
      status != QProcess::NormalExit || exitCode != 0) {
    return false;
  }
  if (!wantsSourceCleanup(extra) || isSameLocation(srcPath, destPath)) {
    return false;
  }
  // rmdir on a top-level folder of a bucket-based remote deletes the bucket
  if (isBucketRemoteType(remoteType) &&
      !trimSlashes(srcPath).contains(QLatin1Char('/'))) {
    return false;
  }
  return true;
}

bool isValidFolderName(const QString &name) {
  if (name.trimmed().isEmpty() || name.contains(QLatin1Char('/'))) {
    return false;
  }
  return name != QLatin1String(".") && name != QLatin1String("..");
}

QStringList mkdirArgs(const QString &remote, const QString &parent,
                      const QString &name, const QStringList &common) {
  QStringList args;
  args << QStringLiteral("mkdir") << common
       << remote + QLatin1Char(':') + joinPath(parent, name);
  return args;
}

} // namespace RemoteOps
