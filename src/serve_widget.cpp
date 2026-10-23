#include "serve_widget.h"

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

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(fsLabel);
  layout->addLayout(form);
  layout->addWidget(mProblem);
  layout->addWidget(mButtons);

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
  return p;
}

void ServeDialog::setTypes(const QStringList &types) {
  if (types.isEmpty()) {
    return;
  }
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
  mProblem->setText(problem);
  mProblem->setVisible(!problem.isEmpty());
  mButtons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
