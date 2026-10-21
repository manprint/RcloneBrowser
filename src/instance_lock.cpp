#include "instance_lock.h"

QString instanceLockFileName(bool perUser, quint64 uid) {
  if (!perUser) {
    return QStringLiteral(".RcloneBrowser_4q6RgLs2RpbJA.lock");
  }
  return QStringLiteral(".RcloneBrowser_4q6RgLs2RpbJA_%1.lock").arg(uid);
}
