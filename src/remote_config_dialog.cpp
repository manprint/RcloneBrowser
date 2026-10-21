#include "remote_config_dialog.h"

#include <QDesktopServices>
#include <QPointer>

namespace {

const int kRcTimeoutMs = 30000;
const int kCancelWaitMs = 10000;
const int kMaxHelpChars = 1200;

} // namespace

RcCallFn daemonRcCall(QPointer<RcloneDaemon> daemon,
                      std::shared_ptr<QPointer<QObject>> ctx) {
  return [daemon, ctx](const QString &path, const QJsonObject &params,
                       std::function<void(const RcReply &)> done,
                       int timeoutMs) {
    QObject *context = ctx ? ctx->data() : nullptr;
    if (!context) {
      return;
    }
    if (!daemon || daemon->state() != RcloneDaemon::State::Running) {
      RcReply reply;
      reply.error = QStringLiteral("rclone daemon is not running");
      QTimer::singleShot(0, context, [done, reply]() { done(reply); });
      return;
    }
    daemon->client()->call(path, params, context, std::move(done), timeoutMs);
  };
}

RemoteConfigDialog::RemoteConfigDialog(RcCallFn call, Mode mode,
                                       const QString &existingName,
                                       QWidget *parent)
    : QDialog(parent), mCall(std::move(call)), mMode(mode),
      mExistingName(existingName), mFlow(new ConfigFlow(mCall, this)),
      mCancelTimer(new QTimer(this)) {
  setWindowTitle(mode == Mode::Update ? tr("Edit remote %1").arg(existingName)
                                      : tr("New remote"));
  setMinimumWidth(480);

  mOpenUrl = [](const QUrl &url) { QDesktopServices::openUrl(url); };

  mStack = new QStackedWidget(this);

  if (mode == Mode::Create) {
    mPage1 = new QWidget(mStack);
    auto *layout = new QVBoxLayout(mPage1);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel(tr("Name of the new remote:"), mPage1));
    mName = new QLineEdit(mPage1);
    mName->setObjectName("name");
    layout->addWidget(mName);
    mNameError = new QLabel(mPage1);
    mNameError->setObjectName("nameError");
    mNameError->setStyleSheet("color: red;");
    mNameError->hide();
    layout->addWidget(mNameError);

    layout->addWidget(new QLabel(tr("Storage provider:"), mPage1));
    mFilter = new QLineEdit(mPage1);
    mFilter->setObjectName("filter");
    mFilter->setPlaceholderText(tr("Filter providers"));
    mFilter->setClearButtonEnabled(true);
    layout->addWidget(mFilter);
    mProviders = new QListWidget(mPage1);
    mProviders->setObjectName("providers");
    layout->addWidget(mProviders);
    mProvidersError = new QLabel(mPage1);
    mProvidersError->setObjectName("providersError");
    mProvidersError->setStyleSheet("color: red;");
    mProvidersError->setWordWrap(true);
    mProvidersError->hide();
    layout->addWidget(mProvidersError);

    mStack->addWidget(mPage1);

    QObject::connect(mName, &QLineEdit::textChanged, this, [this]() {
      updateNameError();
      updateButtons();
    });
    QObject::connect(mFilter, &QLineEdit::textChanged, this,
                     [this]() { fillProviders(); });
    QObject::connect(mProviders, &QListWidget::itemSelectionChanged, this,
                     [this]() { updateButtons(); });
    QObject::connect(mProviders, &QListWidget::itemDoubleClicked, this,
                     [this]() {
                       if (mNext->isEnabled()) {
                         onNext();
                       }
                     });
  }

  mQuestionPage = new QWidget(mStack);
  {
    auto *layout = new QVBoxLayout(mQuestionPage);
    layout->setContentsMargins(0, 0, 0, 0);
    mTitle = new QLabel(mQuestionPage);
    mTitle->setObjectName("title");
    QFont bold = mTitle->font();
    bold.setBold(true);
    mTitle->setFont(bold);
    layout->addWidget(mTitle);

    auto *scroll = new QScrollArea(mQuestionPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMaximumHeight(220);
    mHelp = new QLabel(scroll);
    mHelp->setObjectName("help");
    mHelp->setTextFormat(Qt::PlainText);
    mHelp->setWordWrap(true);
    mHelp->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mHelp->setTextInteractionFlags(Qt::TextSelectableByMouse);
    scroll->setWidget(mHelp);
    layout->addWidget(scroll);

    mError = new QLabel(mQuestionPage);
    mError->setObjectName("error");
    mError->setStyleSheet("color: red;");
    mError->setWordWrap(true);
    mError->hide();
    layout->addWidget(mError);

    mEditorLayout = new QVBoxLayout();
    layout->addLayout(mEditorLayout);
    layout->addStretch(1);
  }
  mStack->addWidget(mQuestionPage);

  mOAuthPage = new QWidget(mStack);
  {
    auto *layout = new QVBoxLayout(mOAuthPage);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *text = new QLabel(
        tr("Complete the sign-in in your browser. If it did not open, use "
           "this link:"),
        mOAuthPage);
    text->setWordWrap(true);
    layout->addWidget(text);
    mOAuthLink = new QLabel(mOAuthPage);
    mOAuthLink->setObjectName("oauthLink");
    mOAuthLink->setTextFormat(Qt::RichText);
    mOAuthLink->setWordWrap(true);
    mOAuthLink->setTextInteractionFlags(Qt::LinksAccessibleByMouse |
                                        Qt::TextSelectableByMouse);
    layout->addWidget(mOAuthLink);
    layout->addStretch(1);
    QObject::connect(mOAuthLink, &QLabel::linkActivated, this,
                     [this](const QString &link) { mOpenUrl(QUrl(link)); });
  }
  mStack->addWidget(mOAuthPage);

  mNext = new QPushButton(tr("Next"), this);
  mNext->setObjectName("next");
  mNext->setDefault(true);
  mCancel = new QPushButton(tr("Cancel"), this);
  mCancel->setObjectName("cancel");
  mCancel->setAutoDefault(false);

  auto *buttons = new QHBoxLayout();
  buttons->addStretch(1);
  buttons->addWidget(mNext);
  buttons->addWidget(mCancel);

  auto *main = new QVBoxLayout(this);
  main->addWidget(mStack, 1);
  main->addLayout(buttons);

  mCancelTimer->setSingleShot(true);
  QObject::connect(mCancelTimer, &QTimer::timeout, this, [this]() {
    // the flow did not answer in time: give up on it
    if (!mFinished) {
      mFinished = true;
      QDialog::reject();
    }
  });

  QObject::connect(mNext, &QPushButton::clicked, this,
                   [this]() { onNext(); });
  QObject::connect(mCancel, &QPushButton::clicked, this,
                   [this]() { reject(); });

  QObject::connect(mFlow, &ConfigFlow::question, this,
                   [this](const ConfigOption &o, const QString &error) {
                     showQuestion(o, error);
                   });
  QObject::connect(mFlow, &ConfigFlow::oauthUrl, this,
                   [this](const QUrl &url) { showOAuth(url); });
  QObject::connect(mFlow, &ConfigFlow::finished, this,
                   [this](bool ok, const QString &message) {
                     onFinished(ok, message);
                   });

  if (mode == Mode::Create) {
    mStack->setCurrentWidget(mPage1);
    // deferred: the owner may bind the RcCallFn context after construction
    QTimer::singleShot(0, this, [this]() {
      loadRemoteNames();
      loadProviders();
    });
  } else {
    mStack->setCurrentWidget(mQuestionPage);
    setBusy(true);
    mTitle->setText(tr("Loading..."));
    QTimer::singleShot(0, this, [this]() {
      if (!mFinished) {
        mFlowStarted = true;
        mFlow->startUpdate(mExistingName);
      }
    });
  }
  updateButtons();
}

void RemoteConfigDialog::setShowErrors(bool show) { mShowErrors = show; }

void RemoteConfigDialog::setUrlOpener(
    std::function<void(const QUrl &)> opener) {
  mOpenUrl = std::move(opener);
}

void RemoteConfigDialog::reject() {
  if (mFinished) {
    return; // already closing or closed: never reject (or signal) twice
  }
  if (!mFlowStarted) {
    // nothing to clean up (the page 1 of Create has not started the flow)
    mFinished = true;
    QDialog::reject();
    return;
  }
  requestCancel();
}

void RemoteConfigDialog::requestCancel() {
  if (mCancelRequested) {
    return;
  }
  mCancelRequested = true;
  updateButtons();
  mCancelTimer->start(kCancelWaitMs);
  mFlow->cancel();
}

void RemoteConfigDialog::loadRemoteNames() {
  QPointer<RemoteConfigDialog> self(this);
  mCall("config/listremotes", QJsonObject(),
        [self](const RcReply &reply) {
          if (!self || !reply.ok) {
            return;
          }
          const QJsonArray remotes =
              reply.result.value(QStringLiteral("remotes")).toArray();
          for (const QJsonValue &v : remotes) {
            self->mExistingNames.insert(v.toString());
          }
          self->updateNameError();
          self->updateButtons();
        },
        kRcTimeoutMs);
}

void RemoteConfigDialog::loadProviders() {
  QPointer<RemoteConfigDialog> self(this);
  mCall("config/providers", QJsonObject(),
        [self](const RcReply &reply) {
          if (!self) {
            return;
          }
          if (!reply.ok) {
            self->mProvidersError->setText(
                tr("Cannot load the provider list: %1").arg(reply.error));
            self->mProvidersError->show();
            return;
          }
          self->mProviderInfos = parseProviders(reply.result);
          self->fillProviders();
        },
        kRcTimeoutMs);
}

void RemoteConfigDialog::fillProviders() {
  const QString keep = selectedProvider();
  const QString needle = mFilter->text().trimmed();
  QSignalBlocker block(mProviders);
  mProviders->clear();
  for (const ProviderInfo &p : mProviderInfos) {
    if (p.hide) {
      continue;
    }
    if (!needle.isEmpty() &&
        !p.description.contains(needle, Qt::CaseInsensitive) &&
        !p.name.contains(needle, Qt::CaseInsensitive)) {
      continue;
    }
    auto *item = new QListWidgetItem(
        p.description.isEmpty()
            ? p.name
            : QStringLiteral("%1 (%2)").arg(p.description, p.name),
        mProviders);
    item->setData(Qt::UserRole, p.name);
    if (p.name == keep) {
      mProviders->setCurrentItem(item);
    }
  }
  block.unblock();
  updateButtons();
}

void RemoteConfigDialog::updateNameError() {
  const QString name = mName->text();
  if (mExistingNames.contains(name)) {
    mNameError->setText(tr("remote %1 already exists").arg(name));
    mNameError->show();
  } else {
    mNameError->clear();
    mNameError->hide();
  }
}

QString RemoteConfigDialog::selectedProvider() const {
  if (!mProviders) {
    return QString();
  }
  const auto items = mProviders->selectedItems();
  return items.isEmpty() ? QString()
                         : items.first()->data(Qt::UserRole).toString();
}

void RemoteConfigDialog::setBusy(bool busy) {
  mBusy = busy;
  mQuestionPage->setEnabled(!busy);
  updateButtons();
}

QVariant RemoteConfigDialog::editorValue() const {
  if (auto *box = qobject_cast<QCheckBox *>(mEditor)) {
    return box->isChecked();
  }
  if (auto *combo = qobject_cast<QComboBox *>(mEditor)) {
    return combo->currentText();
  }
  if (auto *edit = qobject_cast<QLineEdit *>(mEditor)) {
    return edit->text();
  }
  return QString();
}

bool RemoteConfigDialog::editorEmpty() const {
  if (qobject_cast<QCheckBox *>(mEditor)) {
    return false;
  }
  return editorValue().toString().isEmpty();
}

void RemoteConfigDialog::updateButtons() {
  const QWidget *page = mStack->currentWidget();
  bool next = false;
  if (!mBusy && !mCancelRequested && !mFinished) {
    if (page == mPage1) {
      const QString name = mName->text();
      next = isValidRemoteName(name) && !mExistingNames.contains(name) &&
             !selectedProvider().isEmpty();
    } else if (page == mQuestionPage && mHasQuestion) {
      // an empty password in Update mode keeps the stored one (R18)
      const bool keepsStored = mMode == Mode::Update && mOption.isPassword &&
                               !mOption.defaultStr.isEmpty();
      next = !(mOption.required && editorEmpty() && !keepsStored);
    }
  }
  mNext->setEnabled(next);
  mNext->setVisible(page != mOAuthPage);
  mCancel->setEnabled(!mCancelRequested && !mFinished);
}

void RemoteConfigDialog::showQuestion(const ConfigOption &option,
                                      const QString &error) {
  mOption = option;
  mHasQuestion = true;
  mTitle->setText(option.name);
  mHelp->setText(option.help.left(kMaxHelpChars));
  mError->setText(error);
  mError->setVisible(!error.isEmpty());

  if (mEditor) {
    // renamed first: findChild must never see the editor of the last question
    mEditorLayout->removeWidget(mEditor);
    mEditor->hide();
    mEditor->setObjectName(QString());
    mEditor->deleteLater();
    mEditor = nullptr;
  }

  QWidget *editor = nullptr;
  if (option.type == QLatin1String("bool")) {
    auto *box = new QCheckBox(tr("Enabled"), mQuestionPage);
    box->setChecked(option.defaultStr == QLatin1String("true"));
    editor = box;
  } else if (!option.examples.isEmpty()) {
    auto *combo = new QComboBox(mQuestionPage);
    for (int i = 0; i < option.examples.size(); ++i) {
      combo->addItem(option.examples.at(i).first);
      combo->setItemData(i, option.examples.at(i).second, Qt::ToolTipRole);
    }
    if (option.exclusive) {
      int index = combo->findText(option.defaultStr);
      if (index < 0) {
        // not listed (set outside the dialog, or an unlisted empty default):
        // keep it as an entry so unchanged input answers defaultStr
        combo->insertItem(0, option.defaultStr);
        index = 0;
      }
      combo->setCurrentIndex(index);
    } else {
      combo->setEditable(true);
      combo->setEditText(option.defaultStr);
    }
    QObject::connect(combo, &QComboBox::currentTextChanged, this,
                     [this]() { updateButtons(); });
    editor = combo;
  } else {
    auto *edit = new QLineEdit(mQuestionPage);
    if (option.isPassword) {
      edit->setEchoMode(QLineEdit::Password);
      if (mMode == Mode::Update && !option.defaultStr.isEmpty()) {
        edit->setPlaceholderText(tr("(unchanged)"));
      }
    } else {
      edit->setText(option.defaultStr);
    }
    QObject::connect(edit, &QLineEdit::textChanged, this,
                     [this]() { updateButtons(); });
    editor = edit;
  }
  editor->setObjectName("answerEditor");
  mEditorLayout->addWidget(editor);
  mEditor = editor;

  mStack->setCurrentWidget(mQuestionPage);
  setBusy(false);
  editor->setFocus();
}

void RemoteConfigDialog::showOAuth(const QUrl &url) {
  const QString encoded = QString::fromUtf8(url.toEncoded());
  mOAuthLink->setText(QStringLiteral("<a href=\"%1\">%1</a>")
                          .arg(encoded.toHtmlEscaped()));
  mStack->setCurrentWidget(mOAuthPage);
  updateButtons();
  mOpenUrl(url);
}

void RemoteConfigDialog::onNext() {
  if (mFinished || mCancelRequested) {
    return;
  }
  if (mStack->currentWidget() == mPage1) {
    const QString name = mName->text();
    const QString provider = selectedProvider();
    mFlowStarted = true;
    mHasQuestion = false;
    mTitle->setText(tr("Please wait..."));
    mHelp->clear();
    mError->hide();
    mStack->setCurrentWidget(mQuestionPage);
    setBusy(true);
    mFlow->startCreate(name, provider);
  } else if (mStack->currentWidget() == mQuestionPage && mHasQuestion) {
    const QVariant value = editorValue();
    setBusy(true);
    mFlow->answer(value);
  }
}

void RemoteConfigDialog::onFinished(bool ok, const QString &message) {
  if (mFinished) {
    return;
  }
  mFinished = true;
  mCancelTimer->stop();
  updateButtons();
  if (ok) {
    accept();
    return;
  }
  if (message != QLatin1String("cancelled") && mShowErrors) {
    QMessageBox::warning(this, tr("Remote configuration"), message);
  }
  QDialog::reject();
}
