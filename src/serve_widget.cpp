#include "serve_widget.h"

#include "utils.h"

namespace {

const char *const kBuiltInTypes[] = {"dlna", "ftp",    "http", "nfs",
                                     "restic", "s3", "sftp", "webdav"};

QStringList builtInTypes() {
  QStringList out;
  for (const char *t : kBuiltInTypes) {
    out << QString::fromLatin1(t);
  }
  return out;
}

// The host part of "host:port", "[v6]:port" or ":port".
QString hostOf(const QString &addr) {
  if (addr.startsWith(QLatin1Char('['))) {
    const qsizetype end = addr.indexOf(QLatin1Char(']'));
    return end < 0 ? addr.mid(1) : addr.mid(1, end - 1);
  }
  if (addr.count(QLatin1Char(':')) > 1) {
    return addr; // a bare IPv6 address
  }
  const qsizetype colon = addr.lastIndexOf(QLatin1Char(':'));
  return colon < 0 ? addr : addr.left(colon);
}

bool isLoopbackAddr(const QString &addr) {
  const QString host = hostOf(addr).toLower();
  return host == QLatin1String("127.0.0.1") ||
         host == QLatin1String("localhost") || host == QLatin1String("::1");
}

// The port of "host:port" or "[v6]:port"; empty when there is none.
QString portOf(const QString &addr) {
  const qsizetype colon = addr.lastIndexOf(QLatin1Char(':'));
  if (colon < 0 || colon < addr.lastIndexOf(QLatin1Char(']'))) {
    return QString();
  }
  return addr.mid(colon + 1);
}

// kWebuiFlavor right after "webdav" when "webdav" is offered.
QStringList withWebuiFlavor(QStringList types) {
  const qsizetype webdav = types.indexOf(QStringLiteral("webdav"));
  if (webdav >= 0 && !types.contains(kWebuiFlavor)) {
    types.insert(webdav + 1, QString(kWebuiFlavor));
  }
  return types;
}

bool matchesDuration(const QString &text) {
  static const QRegularExpression re(QString::fromUtf8(
      "^(off|([0-9]+(\\.[0-9]+)?(ns|us|µs|ms|s|m|h|d|w|M|y))+)$"));
  return re.match(text).hasMatch();
}

bool matchesSize(const QString &text) {
  static const QRegularExpression re(
      QStringLiteral("^(off|[0-9]+(\\.[0-9]+)?([bkMGTPE]i?)?)$"));
  return re.match(text).hasMatch();
}

// split on ',', trim each, drop the empty ones
QStringList parsePublinkPaths(const QString &text) {
  QStringList out;
  const QStringList parts = text.split(QLatin1Char(','));
  for (const QString &part : parts) {
    const QString path = part.trimmed();
    if (!path.isEmpty()) {
      out << path;
    }
  }
  return out;
}

QString webuiKey(const char *goKey) {
  return QStringLiteral("Serve/webui/") + QLatin1String(goKey);
}

void setWarningColor(QLabel *label) {
  QPalette p = label->palette();
  p.setColor(QPalette::WindowText, QColor(217, 119, 6));
  label->setPalette(p);
}

} // namespace

ServeDialog::ServeDialog(const QString &fs, const QStringList &types,
                         QWidget *parent)
    : QDialog(parent), mFs(fs),
      mLastDefaultAddr(serveTypeRules(QString()).defaultAddr) {
  setWindowTitle(tr("Serve"));
  setMinimumWidth(480);

  QLabel *fsLabel = new QLabel(tr("Serve: %1").arg(fs), this);
  fsLabel->setObjectName("fsLabel");
  // a folder name is never markup
  fsLabel->setTextFormat(Qt::PlainText);
  fsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  fsLabel->setWordWrap(true);

  mType = new QComboBox(this);
  mType->setObjectName("type");

  mAddr = new QLineEdit(mLastDefaultAddr, this);
  mAddr->setObjectName("addr");

  mUser = new QLineEdit(this);
  mUser->setObjectName("user");

  mPass = new QLineEdit(this);
  mPass->setObjectName("pass");
  mPass->setEchoMode(QLineEdit::Password);

  mReadOnly = new QCheckBox(tr("Read only"), this);
  mReadOnly->setObjectName("readOnly");
  mReadOnly->setChecked(true);

  mHint = new QLabel(tr("use 0.0.0.0:PORT to expose on the network"), this);
  mHint->setObjectName("hint");
  mHint->setWordWrap(true);

  mWarning = new QLabel(tr("Anyone on your network can reach this serve"), this);
  mWarning->setObjectName("warning");
  mWarning->setWordWrap(true);
  setWarningColor(mWarning);
  mWarning->setVisible(false);

  mProblem = new QLabel(this);
  mProblem->setObjectName("problem");
  mProblem->setTextFormat(Qt::PlainText);
  mProblem->setWordWrap(true);
  setWarningColor(mProblem);
  mProblem->setVisible(false);

  mButtons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

  QFormLayout *form = new QFormLayout;
  form->addRow(tr("Type"), mType);
  form->addRow(tr("Address"), mAddr);
  form->addRow(QString(), mHint);
  form->addRow(QString(), mWarning);
  form->addRow(tr("User"), mUser);
  form->addRow(tr("Password"), mPass);
  form->addRow(QString(), mReadOnly);

  buildWebuiGroup();

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(fsLabel);
  layout->addLayout(form);
  layout->addWidget(mWebuiGroup);
  layout->addWidget(mProblem);
  layout->addWidget(mButtons);

  loadWebuiSettings();

  setTypes(types.isEmpty() ? builtInTypes() : types);

  connect(mType, &QComboBox::currentTextChanged, this, [this]() {
    applyRules();
    updateWarning();
    updateOk();
  });
  connect(mAddr, &QLineEdit::textChanged, this, [this]() {
    updateWarning();
    updateOk();
  });
  connect(mUser, &QLineEdit::textChanged, this, [this]() { updateOk(); });
  connect(mPass, &QLineEdit::textChanged, this, [this]() { updateOk(); });
  for (QLineEdit *edit :
       {mSearchTimeout, mPublinkPaths, mSecret, mTokenExpiry, mResumeDir,
        mResumeRetention, mEditMaxSize}) {
    connect(edit, &QLineEdit::textChanged, this, [this]() { updateOk(); });
  }
  connect(mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  applyRules();
  updateWarning();
  updateOk();
}

QString ServeDialog::type() const { return mType->currentText().trimmed(); }

ServeParams ServeDialog::params() const {
  const ServeTypeRules rules = serveTypeRules(type());
  ServeParams p;
  p.type = type();
  p.fs = mFs;
  p.addr = mAddr->text().trimmed();
  if (rules.loginAllowed) {
    p.user = mUser->text().trimmed();
    p.pass = mPass->text();
  }
  p.readOnly = rules.readOnlyAllowed && mReadOnly->isChecked();
  if (type() == kWebuiFlavor) {
    p.type = QStringLiteral("webdav");
    p.flavor = QString(kWebuiFlavor);
    p.webui = webuiOptions();
    const QString mode = mVfsCacheMode->currentText();
    p.vfsCacheMode = mode == QLatin1String("off") ? QString() : mode;
  }
  return p;
}

WebuiOptions ServeDialog::webuiOptions() const {
  WebuiOptions o;
  o.allowDelete = mAllowDelete->isChecked();
  o.search = mSearch->isChecked();
  o.searchMaxDepth = mSearchMaxDepth->value();
  o.searchMaxResults = mSearchMaxResults->value();
  o.searchTimeout = mSearchTimeout->text().trimmed();
  o.publinkPaths = parsePublinkPaths(mPublinkPaths->text());
  o.secret = mSecret->text();
  o.tokenExpiry = mTokenExpiry->text().trimmed();
  o.resume = mResume->isChecked();
  o.resumeDir = mResumeDir->text().trimmed();
  o.resumeRetention = mResumeRetention->text().trimmed();
  o.playerMaxRetry = mPlayerMaxRetry->value();
  o.playerReconnectInterval = mPlayerReconnectInterval->value();
  o.diskStats = mDiskStats->isChecked();
  o.uploadSettings = mUploadSettings->isChecked();
  o.editMaxSize = mEditMaxSize->text().trimmed();
  o.dirCount = mDirCount->isChecked();
  o.multiZipMax = mMultiZipMax->value();
  return o;
}

void ServeDialog::setWebuiSupport(ServeManager::WebuiSupport support) {
  mWebuiSupport = support;
  updateOk();
}

void ServeDialog::accept() {
  if (type() == kWebuiFlavor) {
    saveWebuiSettings();
  }
  QDialog::accept();
}

void ServeDialog::setTypes(const QStringList &offered) {
  if (offered.isEmpty()) {
    return;
  }
  const QStringList types = withWebuiFlavor(offered);
  QStringList current;
  for (int i = 0; i < mType->count(); ++i) {
    current << mType->itemText(i);
  }
  if (current == types) {
    return;
  }
  const QString selected = mType->currentText();
  {
    const QSignalBlocker blocker(mType);
    mType->clear();
    mType->addItems(types);
    int index = mType->findText(selected);
    if (index < 0) {
      index = mType->findText(QStringLiteral("http"));
    }
    mType->setCurrentIndex(index < 0 ? 0 : index);
  }
  // the signals were blocked: the rules follow a changed selection here
  if (mType->currentText() != selected) {
    applyRules();
    updateWarning();
    updateOk();
  }
}

void ServeDialog::applyRules() {
  const ServeTypeRules rules = serveTypeRules(type());
  mWebuiGroup->setVisible(type() == kWebuiFlavor);

  if (mAddr->text().trimmed() == mLastDefaultAddr) {
    mAddr->setText(rules.defaultAddr);
  }
  mLastDefaultAddr = rules.defaultAddr;

  QString loginTip;
  if (!rules.loginAllowed) {
    loginTip = type() == QLatin1String("s3")
                   ? tr("s3 is served without access keys (anonymous access)")
                   : tr("nfs and dlna have no login");
    mUser->clear();
    mPass->clear();
  }
  mUser->setEnabled(rules.loginAllowed);
  mPass->setEnabled(rules.loginAllowed);
  mUser->setToolTip(loginTip);
  mPass->setToolTip(loginTip);

  if (!rules.readOnlyAllowed) {
    mReadOnly->setChecked(false);
    mReadOnly->setEnabled(false);
    mReadOnly->setToolTip(tr("restic has no read-only mode"));
  } else {
    if (!mReadOnly->isEnabled()) {
      mReadOnly->setChecked(true);
    }
    mReadOnly->setEnabled(true);
    mReadOnly->setToolTip(QString());
  }
}

void ServeDialog::updateWarning() {
  const QString addr = mAddr->text().trimmed();
  mWarning->setVisible(!addr.isEmpty() && !isLoopbackAddr(addr));
}

void ServeDialog::updateOk() {
  const ServeTypeRules rules = serveTypeRules(type());
  const QString addr = mAddr->text().trimmed();
  QString problem;
  bool ok = !addr.isEmpty();
  if (ok && rules.needsPort && portOf(addr).toUShort() == 0) {
    problem = tr("FTP cannot report a random port; give one");
    ok = false;
  } else if (ok && rules.loginRequired &&
             (mUser->text().trimmed().isEmpty() || mPass->text().isEmpty())) {
    problem = tr("SFTP needs a user name and a password");
    ok = false;
  } else if (ok && rules.loginAllowed && mUser->text().trimmed().isEmpty() &&
             !mPass->text().isEmpty()) {
    // rclone turns the login on only with a user name: a password alone would
    // start an open serve
    problem = tr("A password needs a user name");
    ok = false;
  }
  if (type() == kWebuiFlavor) {
    if (mWebuiSupport == ServeManager::WebuiSupport::Yes) {
      mWebuiUnsupported->setVisible(false);
    } else {
      mWebuiUnsupported->setText(
          mWebuiSupport == ServeManager::WebuiSupport::No
              ? QString(kWebuiUnsupported)
              : tr("Checking whether this rclone supports webdav-webui..."));
      mWebuiUnsupported->setVisible(true);
      ok = false;
    }
    if (problem.isEmpty()) {
      auto *advanced = qobject_cast<QFormLayout *>(mWebuiAdvanced->layout());
      auto rowLabel = [advanced](QLineEdit *edit) {
        QWidget *label = advanced->labelForField(edit);
        return label ? static_cast<QLabel *>(label)->text() : QString();
      };
      for (QLineEdit *edit :
           {mSearchTimeout, mTokenExpiry, mResumeRetention}) {
        if (!matchesDuration(edit->text().trimmed())) {
          problem = tr("Invalid duration: %1").arg(rowLabel(edit));
          break;
        }
      }
      if (problem.isEmpty() && !matchesSize(mEditMaxSize->text().trimmed())) {
        problem = tr("Invalid size: %1").arg(rowLabel(mEditMaxSize));
      }
      if (!problem.isEmpty()) {
        ok = false;
      }
    }
  } else {
    mWebuiUnsupported->setVisible(false);
  }
  mProblem->setText(problem);
  mProblem->setVisible(!problem.isEmpty());
  mButtons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void ServeDialog::buildWebuiGroup() {
  mWebuiGroup = new QGroupBox(tr("Web interface"), this);
  mWebuiGroup->setObjectName("webuiGroup");
  mWebuiGroup->setVisible(false);

  const WebuiOptions d;

  mWebuiUnsupported = new QLabel(mWebuiGroup);
  mWebuiUnsupported->setObjectName("webuiUnsupported");
  mWebuiUnsupported->setTextFormat(Qt::PlainText);
  mWebuiUnsupported->setWordWrap(true);
  setWarningColor(mWebuiUnsupported);
  mWebuiUnsupported->setVisible(false);

  auto checkBox = [this](const QString &name, const QString &text,
                         bool value) {
    auto *box = new QCheckBox(text, mWebuiGroup);
    box->setObjectName(name);
    box->setChecked(value);
    return box;
  };
  auto spinBox = [this](const QString &name, int min, int max, int value) {
    auto *spin = new QSpinBox(mWebuiGroup);
    spin->setObjectName(name);
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
  };
  auto lineEdit = [this](const QString &name, const QString &value) {
    auto *edit = new QLineEdit(value, mWebuiGroup);
    edit->setObjectName(name);
    return edit;
  };

  mAllowDelete = checkBox("webuiAllowDelete",
                          tr("Allow deleting, moving and overwriting"),
                          d.allowDelete);
  mSearch = checkBox("webuiSearch", tr("Search"), d.search);
  mPublinkPaths = lineEdit("webuiPublinkPaths", QString());
  mResume = checkBox("webuiResume", tr("Remember playback positions"),
                     d.resume);
  mDiskStats = checkBox("webuiDiskStats", tr("Disk usage statistics"),
                        d.diskStats);
  mUploadSettings = checkBox("webuiUploadSettings",
                             tr("Concurrent upload settings"),
                             d.uploadSettings);
  mDirCount = checkBox("webuiDirCount",
                       tr("Show the number of entries of folders"),
                       d.dirCount);

  mVfsCacheMode = new QComboBox(mWebuiGroup);
  mVfsCacheMode->setObjectName("vfsCacheMode");
  mVfsCacheMode->addItems({"off", "minimal", "writes", "full"});
  QLabel *vfsHint = new QLabel(
      tr("writes or full is needed to resume interrupted uploads"),
      mWebuiGroup);
  vfsHint->setObjectName("vfsHint");
  vfsHint->setWordWrap(true);

  QFormLayout *basic = new QFormLayout;
  basic->addRow(mAllowDelete);
  basic->addRow(mSearch);
  basic->addRow(tr("Public link paths"), mPublinkPaths);
  basic->addRow(mResume);
  basic->addRow(mDiskStats);
  basic->addRow(mUploadSettings);
  basic->addRow(mDirCount);
  basic->addRow(tr("VFS cache mode"), mVfsCacheMode);
  basic->addRow(QString(), vfsHint);

  mSearchMaxDepth = spinBox("webuiSearchMaxDepth", 1, 1000, d.searchMaxDepth);
  mSearchMaxResults =
      spinBox("webuiSearchMaxResults", 1, 1000000, d.searchMaxResults);
  mSearchTimeout = lineEdit("webuiSearchTimeout", d.searchTimeout);
  mSecret = lineEdit("webuiSecret", QString());
  mSecret->setEchoMode(QLineEdit::Password);
  mSecret->setToolTip(
      tr("Signs download links and public links. Empty: a new random secret "
         "at every start, so earlier links stop working."));
  mTokenExpiry = lineEdit("webuiTokenExpiry", d.tokenExpiry);
  mResumeDir = lineEdit("webuiResumeDir", QString());
  mResumeDir->setPlaceholderText(tr("rclone cache folder"));
  mResumeRetention = lineEdit("webuiResumeRetention", d.resumeRetention);
  mPlayerMaxRetry = spinBox("webuiPlayerMaxRetry", 0, 10000, d.playerMaxRetry);
  mPlayerReconnectInterval = spinBox("webuiPlayerReconnectInterval", 1, 3600,
                                     d.playerReconnectInterval);
  mEditMaxSize = lineEdit("webuiEditMaxSize", d.editMaxSize);
  mMultiZipMax = spinBox("webuiMultiZipMax", 1, 1000000, d.multiZipMax);

  mWebuiAdvanced = new QWidget(mWebuiGroup);
  mWebuiAdvanced->setObjectName("webuiAdvanced");
  QFormLayout *advanced = new QFormLayout(mWebuiAdvanced);
  advanced->setContentsMargins(0, 0, 0, 0);
  advanced->addRow(tr("Search max depth"), mSearchMaxDepth);
  advanced->addRow(tr("Search max results"), mSearchMaxResults);
  advanced->addRow(tr("Search timeout"), mSearchTimeout);
  advanced->addRow(tr("Secret"), mSecret);
  advanced->addRow(tr("Token expiry"), mTokenExpiry);
  advanced->addRow(tr("Resume folder"), mResumeDir);
  advanced->addRow(tr("Resume retention"), mResumeRetention);
  advanced->addRow(tr("Player max retries"), mPlayerMaxRetry);
  advanced->addRow(tr("Player reconnect interval (s)"),
                   mPlayerReconnectInterval);
  advanced->addRow(tr("Edit max size"), mEditMaxSize);
  advanced->addRow(tr("Multi-zip max files"), mMultiZipMax);
  mWebuiAdvanced->setVisible(false);

  mWebuiAdvancedToggle = new QToolButton(mWebuiGroup);
  mWebuiAdvancedToggle->setObjectName("webuiAdvancedToggle");
  mWebuiAdvancedToggle->setText(tr("Advanced"));
  mWebuiAdvancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  mWebuiAdvancedToggle->setArrowType(Qt::RightArrow);
  mWebuiAdvancedToggle->setCheckable(true);
  mWebuiAdvancedToggle->setAutoRaise(true);
  connect(mWebuiAdvancedToggle, &QToolButton::toggled, this,
          [this](bool open) {
            mWebuiAdvancedToggle->setArrowType(open ? Qt::DownArrow
                                                    : Qt::RightArrow);
            mWebuiAdvanced->setVisible(open);
          });

  QVBoxLayout *box = new QVBoxLayout(mWebuiGroup);
  box->addWidget(mWebuiUnsupported);
  box->addLayout(basic);
  box->addWidget(mWebuiAdvancedToggle);
  box->addWidget(mWebuiAdvanced);
}

// The 17 remembered option keys (Enabled is the flavor, the secret is never
// remembered, I-2) and the VFS cache mode: 18 settings keys.
void ServeDialog::loadWebuiSettings() {
  const auto settings = GetSettings();
  auto flag = [&](const char *key, QCheckBox *box) {
    const QString k = webuiKey(key);
    if (settings->contains(k)) {
      box->setChecked(settings->value(k).toBool());
    }
  };
  auto number = [&](const char *key, QSpinBox *spin) {
    const QString k = webuiKey(key);
    if (settings->contains(k)) {
      spin->setValue(settings->value(k).toInt());
    }
  };
  auto text = [&](const char *key, QLineEdit *edit) {
    const QString k = webuiKey(key);
    if (settings->contains(k)) {
      edit->setText(settings->value(k).toString());
    }
  };
  flag("AllowDelete", mAllowDelete);
  flag("Search", mSearch);
  number("SearchMaxDepth", mSearchMaxDepth);
  number("SearchMaxResults", mSearchMaxResults);
  text("SearchTimeout", mSearchTimeout);
  text("PublinkPaths", mPublinkPaths);
  text("TokenExpiry", mTokenExpiry);
  flag("Resume", mResume);
  text("ResumeDir", mResumeDir);
  text("ResumeRetention", mResumeRetention);
  number("PlayerMaxRetry", mPlayerMaxRetry);
  number("PlayerReconnectInterval", mPlayerReconnectInterval);
  flag("DiskStats", mDiskStats);
  flag("UploadSettings", mUploadSettings);
  text("EditMaxSize", mEditMaxSize);
  flag("DirCount", mDirCount);
  number("MultiZipMax", mMultiZipMax);
  const QString mode = settings->value(webuiKey("VfsCacheMode")).toString();
  const int index = mVfsCacheMode->findText(mode);
  if (index >= 0) {
    mVfsCacheMode->setCurrentIndex(index);
  }
}

void ServeDialog::saveWebuiSettings() const {
  const auto settings = GetSettings();
  settings->setValue(webuiKey("AllowDelete"), mAllowDelete->isChecked());
  settings->setValue(webuiKey("Search"), mSearch->isChecked());
  settings->setValue(webuiKey("SearchMaxDepth"), mSearchMaxDepth->value());
  settings->setValue(webuiKey("SearchMaxResults"), mSearchMaxResults->value());
  settings->setValue(webuiKey("SearchTimeout"), mSearchTimeout->text().trimmed());
  settings->setValue(webuiKey("PublinkPaths"), mPublinkPaths->text());
  settings->setValue(webuiKey("TokenExpiry"), mTokenExpiry->text().trimmed());
  settings->setValue(webuiKey("Resume"), mResume->isChecked());
  settings->setValue(webuiKey("ResumeDir"), mResumeDir->text().trimmed());
  settings->setValue(webuiKey("ResumeRetention"),
                     mResumeRetention->text().trimmed());
  settings->setValue(webuiKey("PlayerMaxRetry"), mPlayerMaxRetry->value());
  settings->setValue(webuiKey("PlayerReconnectInterval"),
                     mPlayerReconnectInterval->value());
  settings->setValue(webuiKey("DiskStats"), mDiskStats->isChecked());
  settings->setValue(webuiKey("UploadSettings"), mUploadSettings->isChecked());
  settings->setValue(webuiKey("EditMaxSize"), mEditMaxSize->text().trimmed());
  settings->setValue(webuiKey("DirCount"), mDirCount->isChecked());
  settings->setValue(webuiKey("MultiZipMax"), mMultiZipMax->value());
  settings->setValue(webuiKey("VfsCacheMode"), mVfsCacheMode->currentText());
}
