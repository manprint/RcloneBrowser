#pragma once
#include <QtCore>
// Call first in initTestCase(): makes QSettings/QStandardPaths hermetic.
inline QTemporaryDir &rbTestDir() { static QTemporaryDir d; return d; }
inline void rbInitTestEnvironment() {
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication::setOrganizationName("rclone-browser-tests");
  QCoreApplication::setApplicationName("rclone-browser-tests");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, rbTestDir().path());
  qputenv("XDG_CONFIG_HOME", (rbTestDir().path() + "/xdg").toUtf8());
  // I-9: any rclone child started without --config uses an empty temp config, never the host one.
  // The file must exist: a missing RCLONE_CONFIG file makes rclone print a NOTICE line (local probe 2026-10-21).
  const QString emptyConf = QDir(rbTestDir().path()).filePath("rclone-empty.conf");
  QFile f(emptyConf); f.open(QIODevice::WriteOnly); f.close();
  qputenv("RCLONE_CONFIG", emptyConf.toUtf8());
}
inline QString rbFixture(const QString &rel) {   // RB_FIXTURES is set by CTest
  return QDir(qEnvironmentVariable("RB_FIXTURES")).filePath(rel);
}
inline QString rbRclone() { return qEnvironmentVariable("RB_TEST_RCLONE"); }
