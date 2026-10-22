#pragma once

#include <QString>

// The path typed into the path field of a remote tab (Enter or Go), kept free
// of widgets and QSettings so it can be tested.

// "r:a/b" with the remote "r" gives "a/b" and "r:" gives "". Another prefix,
// no prefix or an empty `remote` leave `text` unchanged. Case-sensitive, like
// the rclone remote names.
QString stripRemotePrefix(const QString &text, const QString &remote);

// The path a typed `text` stands for: trimmed, without the "remote:" prefix.
// For a local remote (Windows drives) the separators become '/' and the path
// is cleaned like QDir does. Otherwise runs of '/' collapse into one and a
// trailing '/' goes away, except for the root "/". An empty result means
// there is nothing to open: "" is the home of the remote and is not typed.
QString normalizeEnteredPath(const QString &text, const QString &remote,
                             bool isLocal);

// True for the remotes that cannot list their top level, so the user has to
// type where to go (true only for "internetarchive").
bool remoteTypeNeedsPathEntry(const QString &remoteType);
