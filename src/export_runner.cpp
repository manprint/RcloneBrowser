#include "export_runner.h"
#include "lsjson_parser.h"
#include "progress_dialog.h"
#include "rclone_process.h"
#include "utils.h"

#include <memory>

ProgressDialog *startExport(const QString &remote, const QString &path,
                            const QStringList &exportOptions,
                            const QString &dst, bool onlyNames,
                            QWidget *parent) {
  QFile *file = new QFile(dst);
  if (!file->open(QFile::WriteOnly)) {
    QMessageBox::warning(
        parent, "Error",
        QString("Cannot open file '%1' for writing!").arg(dst));
    delete file;
    return nullptr;
  }

  QProcess *process = new QProcess;
  ApplyRcloneEnvironment(process);
  process->setProgram(GetRclone());
  process->setArguments(QStringList()
                        << GetRcloneConf() << GetRemoteModeRcloneOptions()
                        << GetDefaultOptionsList("defaultRcloneOptions")
                        << exportOptions);

  // elided text measure, the progress dialog uses the same fonts
  QFontMetrics metrix(parent ? parent->font() : QApplication::font());

  QString toolTip =
      "\"" + remote + ":" + path + "\"" + "\nto " + "\"" + dst + "\"";

  // stdout (JSON) and stderr (log text) stay separate; the dialog shows both
  ProgressDialog *progress = new ProgressDialog(
      "Export", "Running... ",
      QString("Exporting content of ") + "\"" +
          metrix.elidedText(remote, Qt::ElideMiddle, 150) + ":" +
          metrix.elidedText(path, Qt::ElideMiddle, 500) + "\"" + "\nto " +
          "\"" + metrix.elidedText(dst, Qt::ElideMiddle, 500) + "\"",
      process, nullptr, false, false, toolTip, QProcess::SeparateChannels);

  file->setParent(progress);
  process->setParent(progress);

  // the parser lives as long as the connections below, i.e. as long as the
  // dialog
  auto parser = std::make_shared<LsjsonStreamParser>();

  auto writeEntries = [=]() {
    const QVector<ListEntry> entries = parser->takeEntries();
    if (entries.isEmpty() || !file->isOpen()) {
      return;
    }
    QTextStream out(file);
    out.setEncoding(QStringConverter::Utf8);
    for (const ListEntry &e : entries) {
      if (onlyNames) {
        out << "\"" << e.path << "\"" << '\n';
      } else {
        out << "\"" << e.path << "\""
            << ","
            << "\"" << lsjsonModifiedText(e.modTime) << "\""
            << "," << e.size << '\n';
      }
    }
  };

  QObject::connect(progress, &ProgressDialog::rawOutputAvailable, progress,
                   [=](const QByteArray &raw) {
                     parser->feed(raw);
                     writeEntries();
                   });

  QObject::connect(process,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   progress, [=](int, QProcess::ExitStatus) {
                     parser->feed(process->readAllStandardOutput());
                     parser->finish();
                     writeEntries();
                     file->close();
                   });

  return progress;
}
