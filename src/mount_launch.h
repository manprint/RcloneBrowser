#pragma once
#include <QtCore>
#include <functional>

#include "rc_client.h"

// How a mount is started (I-5: the rc credentials go through the environment of
// the mount process, never through its argv).
struct MountLaunch {
  QStringList args;
  QProcessEnvironment env;
  RcCredentials creds;
  bool rcEnabled = false;
};

// Removes every token matching ^--rc-(user|pass)(=.*)?$ and, for the form
// without '=', the value token that follows it; all other tokens keep their
// order. rcEnabled = `args` contains the token "--rc". env = `base`, plus
// applyRcCredentials(creds) only when rcEnabled. `creds` is stored either way.
MountLaunch buildMountLaunch(const QStringList &args,
                             const QProcessEnvironment &base,
                             const RcCredentials &creds);

// The program and arguments which unmount the FUSE mount at `mountPoint`:
// fusermount, or fusermount3 when only fuse3 is installed. `findExecutable`
// returns the path of a program in PATH, or an empty string.
QStringList fuseUnmountCommand(
    const QString &mountPoint,
    const std::function<QString(const QString &)> &findExecutable =
        [](const QString &program) {
          return QStandardPaths::findExecutable(program);
        });
