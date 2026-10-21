#include "mount_launch.h"

MountLaunch buildMountLaunch(const QStringList &args,
                             const QProcessEnvironment &base,
                             const RcCredentials &creds) {
  static const QRegularExpression credentialFlag(
      QRegularExpression::anchoredPattern("--rc-(user|pass)(=.*)?"),
      QRegularExpression::DotMatchesEverythingOption);

  MountLaunch ml;
  ml.creds = creds;
  for (int i = 0; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    const QRegularExpressionMatch m = credentialFlag.match(arg);
    if (m.hasMatch()) {
      if (m.capturedLength(2) == 0 && i + 1 < args.size()) {
        ++i; // the value of "--rc-user x"
      }
      continue;
    }
    ml.args << arg;
  }
  ml.rcEnabled = args.contains("--rc");
  ml.env = base;
  if (ml.rcEnabled) {
    applyRcCredentials(ml.env, creds);
  }
  return ml;
}
