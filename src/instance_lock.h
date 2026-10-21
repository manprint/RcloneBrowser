#pragma once

#include <QString>

// Name of the single-instance lock file (created in the temp directory).
// The per-user name carries the uid, so users who share /tmp do not block each
// other (#162). perUser == false gives the historical shared name.
QString instanceLockFileName(bool perUser, quint64 uid);
