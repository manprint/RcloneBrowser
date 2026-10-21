#pragma once
#include <QtCore>

// Parsed rclone version ("1.75.2", "1.76.0-beta.1"). valid is false when the
// text did not match.
struct RcloneVersion {
  int major = 0, minor = 0, patch = 0;
  QString suffix;
  bool valid = false;

  // The first line of `rclone version` output must match
  // ^rclone v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.+-]+))?\s*$ (a trailing '\r'
  // is ignored; later lines are not looked at).
  static RcloneVersion fromVersionOutput(QStringView output);
  // A release tag: ^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.+-]+))?$
  static RcloneVersion fromTag(QStringView tag);

  // "1.75.2" or "1.76.0-beta.1"; "" when invalid.
  QString toString() const;
  // -1, 0 or 1. Triple first; equal triple: no suffix > suffix; two suffixes:
  // QString::compare.
  int compare(const RcloneVersion &o) const;
};

// 1.75.0
RcloneVersion minimumRcloneVersion();
// valid && numeric triple >= minimumRcloneVersion() (the suffix is ignored).
bool isSupportedRclone(const RcloneVersion &v);
// "" when supported, else the body text of the version-problem dialog.
QString rcloneVersionProblem(const QString &versionOutput);
