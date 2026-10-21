#include "mount_widget.h"
#include "global.h"
#include "rclone_process.h"
#include "utils.h"

MountWidget::MountWidget(QProcess *process, const QString &remote,
                         const QString &folder, const QStringList &args,
                         const QString &script, const QString &uniqueID,
                         const QString &info, QWidget *parent)
    : QWidget(parent), mProcess(process), mRc(new RcClient(this)) {
  ui.setupUi(this);

  updateStartFinishInfo();

  mUniqueID = uniqueID;
  QProcess *mScriptProcess = new QProcess();

  mArgs.append(QDir::toNativeSeparators(GetRclone()));
  mArgs.append(args);

  ui.showOutput->setToolTip(mArgs.join(" "));

  QString screenInfo;
  if (info == "") {
    screenInfo = QString("%1 on %2").arg(remote).arg(folder);
  } else {
    screenInfo = info;
  }

  QString screenInfoTrimmed;
  if (screenInfo.length() > 140) {
    screenInfoTrimmed = screenInfo.left(57) + "..." + screenInfo.right(80);
  } else {
    screenInfoTrimmed = screenInfo;
  }

  ui.info->setText(screenInfoTrimmed);
  ui.info->setCursorPosition(0);

  ui.remote->setText(remote);
  ui.remote->setCursorPosition(0);
  ui.remote->setToolTip(remote);

  ui.folder->setText(folder);
  ui.folder->setCursorPosition(0);
  ui.folder->setToolTip(folder);

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
  ui.sOutput->setFont(font);

  ui.details->setVisible(false);
  ui.output->setVisible(false);
  ui.sOutput->setVisible(false);
  ui.l_script->setVisible(false);

  if (script.isEmpty()) {
    ui.showScriptOutput->setVisible(false);
    ui.l_script->setVisible(false);
  } else {
    ui.l_script->setStyleSheet("QLabel { color: green; font-weight: bold;}");
    ui.l_script->setText("Running");
    ui.showScriptOutput->setToolTip(script);
  }

  QString iconsColour = settings->value("Settings/iconsColour").toString();

  QString img_add = "";

  if (iconsColour == "white") {
    img_add = "_inv";
  }

  ui.showDetails->setIcon(
      QIcon(":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
  ui.showDetails->setIconSize(QSize(24, 24));
  ui.showOutput->setIcon(
      QIcon(":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
  ui.showOutput->setIconSize(QSize(24, 24));

  ui.showScriptOutput->setIcon(
      QIcon(":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
  ui.showScriptOutput->setIconSize(QSize(24, 24));

  ui.cancel->setIcon(
      QIcon(":media/images/qbutton_icons/cancel" + img_add + ".png"));
  ui.cancel->setIconSize(QSize(24, 24));

  ui.copy->setIcon(
      QIcon(":media/images/qbutton_icons/copy" + img_add + ".png"));
  ui.copy->setIconSize(QSize(24, 24));

  ui.showDetails->setStyleSheet(
      "QToolButton { border: 0; color: green; font-weight: bold;}");
  ui.showDetails->setText("  Mounted");
  ui.showDetails->setToolTip("Show details");
  ui.showDetails->setStatusTip("Show details");

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

  QObject::connect(mScriptProcess, &QProcess::readyRead, this, [=]() {
    QString line;

    while (mScriptProcess->canReadLine()) {
      line = mScriptProcess->readLine().trimmed();
      ui.sOutput->appendPlainText(line);
    }
  });

  QObject::connect(mScriptProcess, &QProcess::started, this,
                   [=]() { mScriptRunning = true; });

  QObject::connect(
      mScriptProcess, &QProcess::errorOccurred, this,
      [=](QProcess::ProcessError error) {
        mScriptRunning = false;
        QString errorString =
            QMetaEnum::fromType<QProcess::ProcessError>().valueToKey(error);

        ui.l_script->setStyleSheet("QLabel { color: red; font-weight: bold;}");
        ui.l_script->setText("Process error: " + errorString);
      });

  QObject::connect(
      mScriptProcess,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int status, QProcess::ExitStatus) {
        mScriptRunning = false;
        mScriptProcess->deleteLater();
        if (status == 0) {

          if (iconsColour == "white") {

            ui.l_script->setStyleSheet("QLabel {font-weight: bold;}");
            ui.l_script->setText("Finished");

          } else {

            ui.l_script->setStyleSheet(
                "QLabel { color: black; font-weight: bold;}");
            ui.l_script->setText("Finished (returned error code: " +
                                 QString::number(status) + ")");
          }

        } else {

          ui.l_script->setStyleSheet(
              "QLabel { color: red; font-weight: bold;}");
          ui.l_script->setText(
              "Error (returned error code: " + QString::number(status) + ")");
        }
      });

  QObject::connect(ui.refreshCache, &QToolButton::clicked, this, [this]() {
    // rclone's vfs/refresh rejects a JSON boolean for "recursive" (it wants
    // the string, as `rclone rc vfs/refresh recursive=true` sends it)
    mRc->call("vfs/refresh", QJsonObject{{"recursive", "true"}}, this,
              [this](const RcReply &reply) {
                if (reply.ok) {
                  ui.output->appendPlainText("Refresh cache: OK");
                } else {
                  ui.output->appendPlainText("Refresh cache failed: " +
                                             reply.error);
                }
              });
  });

  QObject::connect(ui.copy, &QToolButton::clicked, this, [=]() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(mArgs.join(" "));
  });

  QObject::connect(
      ui.showScriptOutput, &QToolButton::toggled, this, [=](bool checked) {
        ui.sOutput->setVisible(checked);
        ui.l_script->setVisible(checked);

        if (checked) {
          ui.showScriptOutput->setIcon(QIcon(
              ":media/images/qbutton_icons/vdownarrow" + img_add + ".png"));
          ui.showScriptOutput->setIconSize(QSize(24, 24));
        } else {
          ui.showScriptOutput->setIcon(QIcon(
              ":media/images/qbutton_icons/vrightarrow" + img_add + ".png"));
          ui.showScriptOutput->setIconSize(QSize(24, 24));
        }
      });

  QObject::connect(
      ui.showOutput, &QToolButton::toggled, this, [=](bool checked) {
        ui.output->setVisible(checked);
        // ui.l_rclone->setVisible(checked);

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

  QObject::connect(ui.cancel, &QToolButton::clicked, this, [=]() {
    if (isRunning) {
      int button = QMessageBox::question(
          this, "Unmount",
          QString("Do you want to unmount?\n\n %2 \n\n mounted to \n\n %1")
              .arg(folder)
              .arg(remote),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (button == QMessageBox::Yes) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  QObject::connect(mProcess, &QProcess::readyRead, this, [=]() {
    QString line;
    QRegularExpression rx(QRegularExpression::anchoredPattern(
        "^.+Serving\\sremote\\scontrol\\son\\s\\S+$"));
    QRegularExpression rxUrl("Serving\\sremote\\scontrol\\son\\s(\\S+)$");

    while (mProcess->canReadLine()) {
      line = mProcess->readLine().trimmed();
      ui.output->appendPlainText(line);
      // we capture RC port here from rclone output
      if (rx.match(line).hasMatch()) {
        // the URL as printed by rclone (it binds the --rc-addr host as given)
        QUrl url(rxUrl.match(line).captured(1));
        line.replace("/", "");
        mRcPort = line.right(line.length() - line.lastIndexOf(":") - 1);

        if (!url.isValid() || url.scheme() != "http" || url.host().isEmpty()) {
          url = QUrl("http://127.0.0.1:" + mRcPort + "/");
        }
        mRcUrl = url;
        mRc->setEndpoint(mRcUrl, mRcCredentials);
        ui.refreshCache->setEnabled(mRcEnabled);
      }

      // we only start custome script when rclone reports RC port
      if (mRcPort != "0" && mScriptStarted == false) {

        mScriptStarted = true;
        QStringList sargs;

        sargs << GetRclone();
        sargs << mRcPort;

        // positional credentials are the one I-5 exception (D7), kept for
        // existing scripts; the environment carries them as well
        sargs << mRcCredentials.user;
        sargs << mRcCredentials.pass;
        sargs << folder;

        QProcessEnvironment env = RcloneEnvironment(RcloneEnvMode::Parsed);
        applyRcCredentials(env, mRcCredentials);
        env.insert("RB_RC_PORT", mRcPort);
        mScriptProcess->setProcessEnvironment(env);

        mScriptProcess->start(QDir::toNativeSeparators(script), sargs,
                              QIODevice::ReadOnly);
      }
    }
  });

  QObject::connect(
      mProcess,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int status, QProcess::ExitStatus) {
        mProcess->deleteLater();
        isRunning = false;
        ui.refreshCache->setEnabled(false);
        mRc->clearEndpoint();

        // we terminate script as well
        if (mScriptRunning) {
          mScriptProcess->kill();
        }

        QString info = "Mounted " + ui.info->text();
        mStatus = "0_zmount_mounted";
        ui.showDetails->setToolTip("Show details");
        ui.showDetails->setStatusTip("Show details");
        QString infoTrimmed;
        if (info.length() > 140) {
          infoTrimmed = info.left(57) + "..." + info.right(80);
        } else {
          infoTrimmed = info;
        }
        ui.info->setText(infoTrimmed);
        ui.info->setCursorPosition(0);

        if (status == 0) {
          if (iconsColour == "white") {
            ui.showDetails->setStyleSheet(
                "QToolButton { border: 0; wfont-weight: bold;}");
          } else {
            ui.showDetails->setStyleSheet(
                "QToolButton { border: 0; color: black; font-weight: bold;}");
          }
          ui.showDetails->setText("  Finished");
          mStatus = "1_zmount_finished";
          ui.showDetails->setToolTip("Show details");
          ui.showDetails->setStatusTip("Show details");
        } else {
          ui.showDetails->setStyleSheet(
              "QToolButton { border: 0; color: red; font-weight: bold;}");
          ui.showDetails->setText("  Error");
          mStatus = "1_zmount_erro";
          ui.showDetails->setToolTip("Show details");
          ui.showDetails->setStatusTip("Show details");
        }
        ui.cancel->setToolTip("Close");
        ui.cancel->setStatusTip("Close");
        ui.cancel->setEnabled(true);

        mFinishDateTime = QDateTime::currentDateTime();
        updateStartFinishInfo();
        emit finished();
      });
}

MountWidget::~MountWidget() {}

void MountWidget::cancel() {
  if (!isRunning) {
    return;
  }

  mUnmountingError = "0";

#if defined(Q_OS_WIN32)
  // requires rclone version at least 1.50
  mRc->call("core/quit", QJsonObject(), this, [this](const RcReply &reply) {
    if (!reply.ok) {
      ui.output->appendPlainText("core/quit error: " + reply.error);
      // the process may already have exited: do not revert the final state
      if (isRunning) {
        showUnmountFailed(reply.httpStatus != 0
                              ? QString::number(reply.httpStatus)
                              : QString("1"));
      }
    }
  });
#else
  QProcess *pUnmount = new QProcess();

  QObject::connect(
      pUnmount,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      this, [=](int status, QProcess::ExitStatus) {
        if (status != 0) {
          // unmounting failed
          showUnmountFailed(QString::number(status));
        } else {
          mUnmountingError = "0";
        }
      });

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
  pUnmount->start("umount", QStringList() << ui.folder->text());
#else
  pUnmount->start("fusermount", QStringList() << "-u" << ui.folder->text());
#endif
#endif

  //  mProcess->waitForFinished();

  ui.showDetails->setStyleSheet(
      "QToolButton { border: 0; color: green; font-weight: bold;}");
  ui.showDetails->setText("  Unmounting");
  ui.cancel->setEnabled(false);
  //  ui.cancel->setStatusTip("Close");
}

void MountWidget::showUnmountFailed(const QString &code) {
  mUnmountingError = code;
  ui.cancel->setEnabled(true);
  ui.showDetails->setStyleSheet(
      "QToolButton { border: 0; color: red; font-weight: bold;}");
  ui.showDetails->setText("  Mounted");
  mStatus = "0_zmount_mounted";
  ui.showDetails->setToolTip("Unmounting failed - check if mount"
                             " is not used by other programs");
  ui.showDetails->setStatusTip("Unmounting failed - check if mount"
                               " is not used by other programs");
}

void MountWidget::setRcCredentials(const RcCredentials &creds,
                                   bool rcEnabled) {
  mRcCredentials = creds;
  mRcEnabled = rcEnabled;
}

QString MountWidget::getUniqueID() { return mUniqueID; }
QString MountWidget::getUnmountingError() { return mUnmountingError; }
QDateTime MountWidget::getStartDateTime() { return mStartDateTime; }

void MountWidget::updateStartFinishInfo() {

  ui.le_StartFinishInfo->setText(
      "Started:   " +
      QLocale(QLocale::English)
          .toString(mStartDateTime, "ddd, dd/MMM/yyyy HH:mm:ss t") +
      "              " + "Finished:  " +
      QLocale(QLocale::English)
          .toString(mFinishDateTime, "ddd, dd/MMM/yyyy HH:mm:ss t"));
}

QString MountWidget::getStatus() { return mStatus; }
