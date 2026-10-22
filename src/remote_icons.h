#pragma once

#include <QMap>
#include <QString>

// Remote icon names (13.2): which file of images/remotes_icons shows a remote.
// Qt Core only.

// The icon base name for an rclone remote type (the "type" of listremotes
// --long --json, verbatim): base such that ":media/images/remotes_icons/" +
// base + ".png" exists, or "unknown" when the type has no icon.
// The type is trimmed, lower-cased and ' ' becomes '_'. A key that is not
// [a-z0-9_]+ (empty, '/', '.', ':' ...) gives "unknown", so the result can
// never leave the icon folder.
QString remoteIconBaseName(const QString &remoteType);

// Alias table: key (the spelling rclone accepts in a config, lower-case, no
// spaces) -> icon base. A key not in the table maps to itself. Every target
// needs base.png and base_inv.png in resources.qrc.
const QMap<QString, QString> &remoteIconAliases();
