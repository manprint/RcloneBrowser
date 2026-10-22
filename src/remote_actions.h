#pragma once

#include <QString>
#include <QStringList>

// The rclone command lines of the remote-browser actions "Copy to..." (copyto)
// and "Upload from URL..." (copyurl), kept free of QSettings, QDir and widgets
// so they can be tested. The args carry no --config and no mode options:
// MainWindow::addTransfer adds --config, and the caller adds the default
// rclone options.

// {"copyto", src, dst}.
QStringList buildCopytoArgs(const QString &src, const QString &dst);

// True for a valid http or https URL with a host. Pure: the caller trims.
bool isHttpUrl(const QString &s);

// dir + name with one separator: "r:" + "a" gives "r:a" (the base of a
// remote), "r:a" and "r:a/" + "b" give "r:a/b". Unlike
// RemoteOps::joinPath, a bare remote name keeps no '/' ("r:/" is the file
// system root on sftp, ftp and local).
QString joinRclonePath(const QString &dir, const QString &name);

// copyurl <url> <destDir>/ --auto-filename --print-filename when `fileName` is
// empty (rclone names the file from the URL and prints the name), else
// copyurl <url> <destDir>/<fileName>. `noClobber` appends --no-clobber.
QStringList buildCopyurlArgs(const QString &url, const QString &destDir,
                             const QString &fileName, bool noClobber);
