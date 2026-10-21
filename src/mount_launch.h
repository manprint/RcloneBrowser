#pragma once
#include <QtCore>

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
