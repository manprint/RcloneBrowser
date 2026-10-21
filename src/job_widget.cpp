#include "job_widget.h"
#include "rclone_format.h"
#include "rclone_log.h"
#include "transfer_monitor.h"
#include "utils.h"

namespace {
QString transferTooltip(const RcloneTransferItem &item) {
  return QStringLiteral("File name: %1\nSize: %2 / %3\nSpeed: %4 (avg %5)\nETA: %6")
      .arg(item.name, formatBytes(item.bytes),
           item.size >= 0 ? formatBytes(item.size) : QStringLiteral("unknown"),
           formatRate(item.speed), formatRate(item.speedAvg),
           formatDuration(item.eta));
}
} // namespace

JobWidget::JobWidget(QProcess *process, const QString &info,
                     const QStringList &args, const QString &source,
                     const QString &dest, const QString &uniqueID,
                     const QString &transferMode, const QString &requestId,
                     QWidget *parent)
    : QWidget(parent), mProcess(process) {
  ui.setupUi(this);

  updateStartFinishInfo();

  mArgs = GetRcloneCmd(args);

  ui.showOutput->setToolTip(mArgs.join(" "));

  ui.source->setText(source);
  ui.source->setCursorPosition(0);
  ui.source->setToolTip(source);

  ui.dest->setText(dest);
  ui.dest->setCursorPosition(0);
  ui.dest->setToolTip(dest);

  QString infoTrimmed;

  mRequestId = requestId;
  mUniqueID = uniqueID;
  mTransferMode = transferMode;

  if (info.length() > 140) {
    infoTrimmed = info.left(57) + "..." + info.right(80);
  } else {
    infoTrimmed = info;
  }

  ui.info->setText(infoTrimmed);
  ui.info->setCursorPosition(0);

  ui.details->setVisible(false);

  auto settings = GetSettings();

  int fontsize = 0;
  fontsize = (settings->value("Settings/fontSize").toInt());

#if !defined(Q_OS_MACOS)
  fontsize--;
#endif

  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  QFontMetrics fm(font);

  font.setPointSize(font.pointSize() + fontsize);

  ui.output->setFont(font);
  ui.output->setMaximumBlockCount(10000);
  ui.output->setVisible(false);

  mMonitor = new TransferMonitor(this);

  QString iconsColour = settings->value("Settings/iconsColour").toString();

  QString img_add = "";

  if (iconsColour == "white") {
    img_add = "_inv";
  }

  ui.showDetails->setIcon(
      QIcon(":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
  ui.showOutput->setIcon(
      QIcon(":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));

  ui.showDetails->setIconSize(QSize(24, 24));
  ui.showOutput->setIconSize(QSize(24, 24));

  QObject::connect(
      ui.showDetails, &QToolButton::toggled, this, [=](bool checked) {
        ui.details->setVisible(checked);

        if (checked) {
          ui.showDetails->setIcon(QIcon(
              ":media/images/qbutton_icons/vdownarrow" + img_add + ".png"));
          ui.showDetails->setIconSize(QSize(24, 24));
        } else {
          ui.showDetails->setIcon(QIcon(
              ":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
          ui.showDetails->setIconSize(QSize(24, 24));
        }
      });

  QObject::connect(
      ui.showOutput, &QToolButton::toggled, this, [=](bool checked) {
        ui.output->setVisible(checked);

        if (checked) {
          ui.showOutput->setIcon(QIcon(
              ":media/images/qbutton_icons/vdownarrow" + img_add + ".png"));
          ui.showOutput->setIconSize(QSize(24, 24));
        } else {
          ui.showOutput->setIcon(QIcon(
              ":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
          ui.showOutput->setIconSize(QSize(24, 24));
        }
      });

  ui.cancel->setIcon(
      QIcon(":media/images/qbutton_icons/cancel" + img_add + ".png"));
  ui.cancel->setIconSize(QSize(24, 24));

  QObject::connect(ui.cancel, &QToolButton::clicked, this, [=]() {
    if (isRunning) {
      int button = QMessageBox::question(
          this, "Transfer",
          QString(
              "rclone process is still running.\n\nDo you want to stop it?"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (button == QMessageBox::Yes) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  ui.copy->setIcon(
      QIcon(":media/images/qbutton_icons/copy" + img_add + ".png"));
  ui.copy->setIconSize(QSize(24, 24));

  QObject::connect(ui.copy, &QToolButton::clicked, this, [=]() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(mArgs.join(" "));
  });

  QObject::connect(mMonitor, &TransferMonitor::statsUpdated, this,
                   [=](const RcloneStats &stats) {
    const QString percent = formatPercent(stats.bytes, stats.totalBytes);
    ui.size->setText(formatBytes(stats.bytes) + QStringLiteral(", ") + percent);
    ui.totalsize->setText(formatBytes(stats.totalBytes));
    ui.bandwidth->setText(formatRate(stats.speed));
    ui.eta->setText(formatDuration(stats.eta));
    ui.transferred->setText(
        QStringLiteral("%1 / %2, %3")
            .arg(stats.transfers)
            .arg(stats.totalTransfers)
            .arg(formatPercent(stats.transfers, stats.totalTransfers)));
    QString checks = QStringLiteral("%1 / %2, %3")
                         .arg(stats.checks)
                         .arg(stats.totalChecks)
                         .arg(formatPercent(stats.checks, stats.totalChecks));
    if (stats.listed > 0) {
      checks += QStringLiteral(", Listed %1").arg(stats.listed);
    }
    ui.checks->setText(checks);
    ui.elapsed->setText(formatElapsed(stats.elapsedTime));
    ui.progress_info->setText(QStringLiteral("(") + percent +
                              QStringLiteral(")"));
    ui.progress_info->setStyleSheet(
        mMonitor->errorCount() > 0
            ? "QLabel { color: red; font-weight: bold;}"
            : "QLabel { color: green; font-weight: bold;}");
  });

  QObject::connect(
      mMonitor, &TransferMonitor::errorsChanged, this,
      [=](qint64 count, const QString &last) {
        ui.errors->setText(count == 0 ? QStringLiteral("0")
                                      : QStringLiteral("%1 \u2014 %2")
                                            .arg(count)
                                            .arg(last));
        ui.errors->setToolTip(mMonitor->recentErrors().join(QLatin1Char('\n')));
        if (count > 0) {
          ui.progress_info->setStyleSheet(
              "QLabel { color: red; font-weight: bold;}");
          ui.errors->setStyleSheet(
              "QLineEdit { color: red; font-weight: normal;}");
        }
      });

  QObject::connect(
      mMonitor, &TransferMonitor::activeTransfersChanged, this,
      [=](const QVector<RcloneTransferItem> &items) {
        QSet<QString> names;
        for (const RcloneTransferItem &item : items) {
          names.insert(item.name);
          updateProgress(item.name, clampedPercent(item),
                         transferTooltip(item));
        }
        for (auto it = mActive.begin(); it != mActive.end();) {
          if (names.contains(it.key())) {
            ++it;
          } else {
            QLabel *label = it.value();
            it = mActive.erase(it);
            removeProgressRow(label);
          }
        }
      });

  QObject::connect(mMonitor, &TransferMonitor::logEvent, this,
                   [=](const RcloneLogEvent &e) {
                     appendOutputLine(renderRcloneLogEvent(e));
                   });

  QObject::connect(mProcess, &QProcess::readyRead, this,
                   [=]() { consumeProcessOutput(false); });

  QObject::connect(
      mProcess,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int code, QProcess::ExitStatus st) {
        mProcess->deleteLater();

        // 1. Drain what is still buffered.
        consumeProcessOutput(true);

        // 2. Let the model decide the outcome.
        mMonitor->finish(code, st, mJobFinalStatus == "stopped");

        // 3. Final stats block, rendered once, then close the log file.
        if (!mMonitor->lastStatsText().isEmpty()) {
          RcloneLogEvent stats;
          stats.kind = RcloneLogEvent::Kind::Stats;
          stats.time = QDateTime::currentDateTime();
          stats.level = RcloneLogLevel::Notice;
          stats.msg = mMonitor->lastStatsText();
          appendOutputLine(renderRcloneLogEvent(stats));
        }
        if (mLogFile) {
          mLogFile->close();
        }

        // 4. UI branch.
        switch (mMonitor->result()) {
        case TransferMonitor::Result::Finished:
          if (iconsColour == "white") {
            ui.showDetails->setStyleSheet(
                "QToolButton { border: 0; font-weight: bold;}");
          } else {
            ui.showDetails->setStyleSheet(
                "QToolButton { border: 0; color: black; font-weight: bold;}");
          }
          ui.showDetails->setText("  Finished");
          mJobFinalStatus = "finished";
          mStatus = "1_transfer_finished";
          ui.progress_info->hide();
          break;
        case TransferMonitor::Result::Stopped:
          ui.showDetails->setStyleSheet(
              "QToolButton { border: 0; color: red; font-weight: bold;}");
          ui.showDetails->setText("  Stopped");
          ui.progress_info->hide();
          break;
        case TransferMonitor::Result::Error:
        case TransferMonitor::Result::Running:
          ui.showDetails->setStyleSheet(
              "QToolButton { border: 0; color: red; font-weight: bold;}");
          ui.showDetails->setText("  Error");
          mJobFinalStatus = "error";
          mStatus = "2_transfer_error";
          ui.progress_info->hide();
          break;
        }

        // 5. Cleanup.
        clearProgressRows();
        isRunning = false;
        mFinishDateTime = QDateTime::currentDateTime();
        updateStartFinishInfo();

        ui.cancel->setToolTip("Close");
        ui.cancel->setStatusTip("Close");

        // 6. Notify.
        emit finished(ui.info->text(), mJobFinalStatus);
      });

  ui.showDetails->setStyleSheet(
      "QToolButton { border: 0; color: green; font-weight: bold;}");
  ui.showDetails->setText("  Running");
}

JobWidget::~JobWidget() {}

void JobWidget::showDetails() { ui.showDetails->setChecked(true); }

void JobWidget::updateProgress(const QString &name, int value,
                               const QString &toolTip) {
  auto it = mActive.find(name);

  QLabel *label;
  QProgressBar *bar;
  if (it == mActive.end()) {
    label = new QLabel();

    QString nameTrimmed;

    if (name.length() > 47) {
      nameTrimmed = name.left(25) + "..." + name.right(19);
    } else {
      nameTrimmed = name;
    }

    label->setText(nameTrimmed);

    bar = new QProgressBar();
    bar->setMinimum(0);
    bar->setMaximum(100);
    bar->setTextVisible(true);

    label->setBuddy(bar);

    ui.progress->addRow(label, bar);

    mActive.insert(name, label);
  } else {
    label = it.value();
    bar = static_cast<QProgressBar *>(label->buddy());
  }

  if (value < 0) {
    bar->setRange(0, 0);
  } else {
    bar->setRange(0, 100);
    bar->setValue(value);
  }
  bar->setToolTip(toolTip);
}

void JobWidget::removeProgressRow(QLabel *label) {
  QWidget *bar = label->buddy();
  ui.progress->removeWidget(bar);
  ui.progress->removeWidget(label);
  delete bar;
  delete label;
}

void JobWidget::clearProgressRows() {
  const auto labels = mActive.values();
  mActive.clear();
  for (QLabel *label : labels) {
    removeProgressRow(label);
  }
}

void JobWidget::appendOutputLine(const QString &text) {
  ui.output->appendPlainText(text);
  if (mLogFile && mLogFile->isOpen()) {
    mLogFile->write((text + QLatin1Char('\n')).toUtf8());
    mLogFile->flush();
  }
}

void JobWidget::consumeProcessOutput(bool final) {
  while (mProcess->canReadLine()) {
    mMonitor->consumeLine(mProcess->readLine());
  }
  if (final) {
    const QByteArray rest = mProcess->readAll();
    if (!rest.isEmpty()) {
      mMonitor->consumeLine(rest);
    }
  }
}

void JobWidget::setLogFile(const QString &path) {
  if (mLogFile) {
    delete mLogFile;
    mLogFile = nullptr;
  }
  auto file = new QFile(path, this);
  if (!file->open(QIODevice::Append | QIODevice::Text)) {
    ui.output->appendPlainText(
        QStringLiteral("Cannot write log file %1: %2")
            .arg(path, file->errorString()));
    delete file;
    return;
  }
  mLogFile = file;
  mLogFile->write((QStringLiteral("# Rclone Browser transfer log ") +
                   QDateTime::currentDateTime().toString(Qt::ISODate) +
                   QStringLiteral(": ") + mArgs.join(QLatin1Char(' ')) +
                   QLatin1Char('\n'))
                      .toUtf8());
  mLogFile->flush();
}

void JobWidget::noteIgnoredArgs(const QStringList &ignored) {
  if (ignored.isEmpty()) {
    return;
  }
  ui.output->appendPlainText(
      QStringLiteral("Rclone Browser manages rclone logging; ignored options: ") +
      ignored.join(QLatin1Char(' ')));
}

void JobWidget::cancel() {
  if (!isRunning) {
    return;
  }

  mJobFinalStatus = "stopped";
  mStatus = "2_transfer_stopped";

  mProcess->kill();
  mProcess->waitForFinished();

  ui.showDetails->setStyleSheet(
      "QToolButton { border: 0; color: red; font-weight: bold;}");
  ui.showDetails->setText("  Stopped");
  ui.progress_info->hide();
  ui.cancel->setToolTip("Close");
  ui.cancel->setStatusTip("Close");
}

QString JobWidget::getUniqueID() { return mUniqueID; }

QString JobWidget::getRequestId() { return mRequestId; }

QString JobWidget::getTransferMode() { return mTransferMode; }

QDateTime JobWidget::getStartDateTime() { return mStartDateTime; }

void JobWidget::updateStartFinishInfo() {

  ui.le_StartFinishInfo->setText(
      "Started:   " +
      QLocale(QLocale::English)
          .toString(mStartDateTime, "ddd, dd/MMM/yyyy HH:mm:ss t") +
      "              " + "Finished:  " +
      QLocale(QLocale::English)
          .toString(mFinishDateTime, "ddd, dd/MMM/yyyy HH:mm:ss t"));
}

QString JobWidget::getStatus() { return mStatus; }
