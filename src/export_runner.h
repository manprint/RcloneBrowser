#pragma once

#include <QString>
#include <QStringList>

class ProgressDialog;
class QWidget;

// Runs `rclone <conf/default options> <exportOptions>` (lsjson -R --files-only,
// see ExportDialog::getOptions) and writes the files list to `dst`: one quoted
// path per line when onlyNames is set, otherwise "path","modified",size.
//
// Returns the (not yet shown) ProgressDialog that owns the process and the
// output file, or nullptr when `dst` cannot be opened (a warning is shown).
// The process is a child of the dialog (findChild<QProcess *>()).
ProgressDialog *startExport(const QString &remote, const QString &path,
                            const QStringList &exportOptions,
                            const QString &dst, bool onlyNames,
                            QWidget *parent);
