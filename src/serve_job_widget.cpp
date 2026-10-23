#include "serve_job_widget.h"
#include "utils.h"

namespace {

QString formatTime(const QDateTime &t) {
  return QLocale(QLocale::English)
      .toString(t, "ddd, dd/MMM/yyyy HH:mm:ss t");
}

QLineEdit *readOnlyLine(const QString &name, QWidget *parent) {
  auto *e = new QLineEdit(parent);
  e->setObjectName(name);
  e->setReadOnly(true);
  return e;
}

QToolButton *iconButton(const QString &name, QWidget *parent) {
  auto *b = new QToolButton(parent);
  b->setObjectName(name);
  b->setStyleSheet("QToolButton { border: 0; }\n"
                   "QToolButton:pressed { border: 4; border-radius: 10px;"
                   " border-style: inset; border-color: rgba(1, 1, 1, 0); }");
  b->setIconSize(QSize(24, 24));
  return b;
}

} // namespace

ServeJobWidget::ServeJobWidget(const ServeEntry &entry,
                               const QString &displayType,
                               const ServeParams &params, QWidget *parent)
    : QWidget(parent), mEntry(entry), mDisplayType(displayType),
      mParams(params) {
  mWhiteIcons = GetSettings()->value("Settings/iconsColour").toString() ==
                QLatin1String("white");

  auto *outer = new QVBoxLayout(this);
  outer->setSpacing(0);

  // top row
  auto *top = new QWidget(this);
  auto *topLayout = new QHBoxLayout(top);
  topLayout->setContentsMargins(0, 0, 0, 0);

  mShowDetails = new QToolButton(top);
  mShowDetails->setObjectName("showDetails");
  mShowDetails->setCheckable(true);
  mShowDetails->setAutoRaise(true);
  mShowDetails->setMinimumWidth(125);
  mShowDetails->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  mShowDetails->setIconSize(QSize(24, 24));
  mShowDetails->setIcon(QIcon(iconPath("vrightarrow")));
  mShowDetails->setToolTip("Show details");
  mShowDetails->setStatusTip("Show details");
  topLayout->addWidget(mShowDetails);

  mInfo = readOnlyLine("info", top);
  mInfo->setContextMenuPolicy(Qt::NoContextMenu);
  mInfo->setFrame(false);
  mInfo->setStyleSheet("* { background-color: rgba(0, 0, 0, 0); }");
  topLayout->addWidget(mInfo, 1);

  mCopyUrl = iconButton("copyUrl", top);
  mCopyUrl->setIcon(QIcon(iconPath("copy")));
  mCopyUrl->setToolTip("Copy URL");
  mCopyUrl->setStatusTip("Copy URL");
  topLayout->addWidget(mCopyUrl);

  mOpenBrowser = iconButton("openBrowser", top);
  mOpenBrowser->setIcon(QIcon(iconPath("link")));
  mOpenBrowser->setToolTip("Open in browser");
  mOpenBrowser->setStatusTip("Open in browser");
  topLayout->addWidget(mOpenBrowser);

  mRestart = iconButton("restart", top);
  mRestart->setIcon(QIcon(iconPath("refresh")));
  mRestart->setToolTip("Restart");
  mRestart->setStatusTip("Restart");
  topLayout->addWidget(mRestart);

  mCancel = iconButton("cancel", top);
  mCancel->setIcon(QIcon(iconPath("cancel")));
  topLayout->addWidget(mCancel);

  outer->addWidget(top);

  // details panel
  mDetails = new QWidget(this);
  mDetails->setObjectName("details");
  auto *form = new QFormLayout(mDetails);
  form->setContentsMargins(0, 0, 0, 10);
  mType = readOnlyLine("type", mDetails);
  mSource = readOnlyLine("source", mDetails);
  mUrl = readOnlyLine("url", mDetails);
  mAddress = readOnlyLine("address", mDetails);
  mStartFinishInfo = readOnlyLine("le_StartFinishInfo", mDetails);
  mStartFinishInfo->setContextMenuPolicy(Qt::NoContextMenu);
  mStartFinishInfo->setFrame(false);
  mStartFinishInfo->setStyleSheet("* { background-color: rgba(0, 0, 0, 0); }");
  form->addRow("Type:", mType);
  form->addRow("Source:", mSource);
  form->addRow("URL:", mUrl);
  form->addRow("Address:", mAddress);
  form->addRow(QString(), mStartFinishInfo);
  mDetails->setVisible(false);
  outer->addWidget(mDetails);

  mMessage = new QLabel(this);
  mMessage->setObjectName("message");
  mMessage->setTextFormat(Qt::PlainText);
  mMessage->setWordWrap(true);
  mMessage->setVisible(false);
  outer->addWidget(mMessage);

  QObject::connect(mShowDetails, &QToolButton::toggled, this,
                   [this](bool checked) {
                     mDetails->setVisible(checked);
                     mShowDetails->setIcon(QIcon(
                         iconPath(checked ? "vdownarrow" : "vrightarrow")));
                   });

  QObject::connect(mCopyUrl, &QToolButton::clicked, this, [this]() {
    QGuiApplication::clipboard()->setText(serveUrl(mEntry));
  });

  QObject::connect(mOpenBrowser, &QToolButton::clicked, this, [this]() {
    emit openUrlRequested(QUrl(serveBrowserUrl(mEntry)));
  });

  QObject::connect(mRestart, &QToolButton::clicked, this,
                   [this]() { emit restartRequested(); });

  QObject::connect(mCancel, &QToolButton::clicked, this,
                   [this]() { cancel(); });

  applyDisplayType();
  applyEntry();
  applyState();
  updateStartFinishInfo();
}

QString ServeJobWidget::iconPath(const QString &name) const {
  return ":media/images/qbutton_icons/" + name +
         (mWhiteIcons ? "_inv" : "") + ".png";
}

ServeJobWidget::State ServeJobWidget::state() const { return mState; }

QDateTime ServeJobWidget::getStartDateTime() { return mStartDateTime; }

QString ServeJobWidget::getStatus() {
  switch (mState) {
  case State::Starting:
    return "0_zserve_starting";
  case State::Serving:
    return "0_zserve_serving";
  case State::Finished:
    return "1_zserve_finished";
  case State::Error:
    break;
  }
  return "1_zserve_error";
}

QStringList ServeJobWidget::filterFields() const {
  return {mDisplayType, mEntry.fs, serveUrl(mEntry)};
}

QString ServeJobWidget::serveId() const { return mEntry.id; }

ServeParams ServeJobWidget::params() const { return mParams; }

bool ServeJobWidget::canRestart() const { return !mParams.type.isEmpty(); }

void ServeJobWidget::adoptParams(const ServeParams &p,
                                 const QString &displayType) {
  mParams = p;
  mDisplayType = displayType;
  applyDisplayType();
  applyState();
}

void ServeJobWidget::markStarting() {
  if (!canRestart() ||
      (mState != State::Finished && mState != State::Error)) {
    return;
  }
  mState = State::Starting;
  isRunning = true;
  mStopping = false;
  mEntry.id.clear();
  mStartDateTime = QDateTime::currentDateTime();
  mFinishDateTime = QDateTime();
  showMessage(QString());
  applyEntry();
  applyState();
  updateStartFinishInfo();
}

void ServeJobWidget::markRunning(const ServeEntry &e) {
  if (mState != State::Starting && mState != State::Serving) {
    return;
  }
  if (mState == State::Serving && e.id == mEntry.id) {
    // the same serve listed again: only its address may differ
    if (mEntry.addr != e.addr) {
      mEntry.addr = e.addr;
      applyEntry();
    }
    // still listed after a stop request: the stop failed, Stop is usable again
    if (mStopping) {
      mStopping = false;
      applyState();
    }
    return;
  }
  mState = State::Serving;
  isRunning = true;
  mStopping = false;
  mEntry = e;
  mStartDateTime = QDateTime::currentDateTime();
  mFinishDateTime = QDateTime();
  showMessage(QString());
  applyEntry();
  applyState();
  updateStartFinishInfo();
}

void ServeJobWidget::markFinished(const QString &reason) {
  if (mState != State::Starting && mState != State::Serving) {
    return;
  }
  mState = State::Finished;
  isRunning = false;
  mStopping = false;
  mFinishDateTime = QDateTime::currentDateTime();
  showMessage(reason);
  applyState();
  updateStartFinishInfo();
  emit finished();
}

void ServeJobWidget::markStartFailed(const QString &message) {
  if (mState != State::Starting) {
    return;
  }
  mState = State::Error;
  isRunning = false;
  mStopping = false;
  mFinishDateTime = QDateTime::currentDateTime();
  showMessage(message);
  applyState();
  updateStartFinishInfo();
  emit finished();
}

void ServeJobWidget::cancel() {
  switch (mState) {
  case State::Serving:
    if (mStopping) {
      return;
    }
    mStopping = true;
    applyState();
    emit stopRequested(mEntry.id);
    return;
  case State::Finished:
  case State::Error:
    emit closed();
    return;
  case State::Starting:
    return;
  }
}

void ServeJobWidget::applyDisplayType() {
  mType->setText(mDisplayType);
  mType->setCursorPosition(0);
  mOpenBrowser->setVisible(serveOpensInBrowser(mDisplayType));
  mInfo->setText(mDisplayType + " " + mEntry.fs);
  mInfo->setCursorPosition(0);
  mInfo->setToolTip(mInfo->text());
  mRestart->setVisible(canRestart());
}

void ServeJobWidget::applyEntry() {
  mInfo->setText(mDisplayType + " " + mEntry.fs);
  mInfo->setCursorPosition(0);
  mInfo->setToolTip(mInfo->text());
  mSource->setText(mEntry.fs);
  mSource->setCursorPosition(0);
  mUrl->setText(serveUrl(mEntry));
  mUrl->setCursorPosition(0);
  mAddress->setText(mEntry.addr);
  mAddress->setCursorPosition(0);
}

void ServeJobWidget::applyState() {
  const bool serving = mState == State::Serving;
  const bool active = serving || mState == State::Starting;

  QString text;
  QString style;
  switch (mState) {
  case State::Starting:
    text = "  Starting";
    style = "QToolButton { border: 0; color: green; font-weight: bold;}";
    break;
  case State::Serving:
    text = mStopping ? "  Stopping" : "  Serving";
    style = "QToolButton { border: 0; color: green; font-weight: bold;}";
    break;
  case State::Finished:
    text = "  Finished";
    style = mWhiteIcons
                ? "QToolButton { border: 0; font-weight: bold;}"
                : "QToolButton { border: 0; color: black; font-weight: bold;}";
    break;
  case State::Error:
    text = "  Error";
    style = "QToolButton { border: 0; color: red; font-weight: bold;}";
    break;
  }
  mShowDetails->setStyleSheet(style);
  mShowDetails->setText(text);

  const QString cancelTip = active ? "Stop" : "Close";
  mCancel->setToolTip(cancelTip);
  mCancel->setStatusTip(cancelTip);
  mCancel->setEnabled(!active || (serving && !mStopping));

  mCopyUrl->setEnabled(serving);
  mOpenBrowser->setEnabled(serving);
  mRestart->setVisible(canRestart());
  mRestart->setEnabled(!active);
}

void ServeJobWidget::updateStartFinishInfo() {
  mStartFinishInfo->setText(
      "Started:   " + formatTime(mStartDateTime) + "              " +
      "Finished:  " + formatTime(mFinishDateTime));
}

void ServeJobWidget::showMessage(const QString &text) {
  mMessage->setText(text);
  mMessage->setVisible(!text.isEmpty());
}
