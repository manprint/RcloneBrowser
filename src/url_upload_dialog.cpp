#include "url_upload_dialog.h"

#include "remote_actions.h"
#include "remote_ops.h"

UrlUploadDialog::UrlUploadDialog(const QString &destDir, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Upload from URL"));
  setMinimumWidth(480);

  QLabel *dest = new QLabel(tr("Destination: %1").arg(destDir), this);
  // a folder name is never markup
  dest->setTextFormat(Qt::PlainText);
  dest->setTextInteractionFlags(Qt::TextSelectableByMouse);
  dest->setWordWrap(true);

  mUrl = new QLineEdit(this);
  mUrl->setObjectName("url");
  mUrl->setPlaceholderText(tr("https://example.com/file.zip"));

  mAutoName = new QCheckBox(tr("Name the file from the URL"), this);
  mAutoName->setObjectName("autoName");
  mAutoName->setChecked(true);

  mFileName = new QLineEdit(this);
  mFileName->setObjectName("fileName");
  mFileName->setEnabled(false);

  mNoClobber = new QCheckBox(tr("Do not overwrite an existing file"), this);
  mNoClobber->setObjectName("noClobber");

  mButtons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

  QFormLayout *form = new QFormLayout;
  form->addRow(tr("URL"), mUrl);
  form->addRow(QString(), mAutoName);
  form->addRow(tr("File name"), mFileName);
  form->addRow(QString(), mNoClobber);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(dest);
  layout->addLayout(form);
  layout->addWidget(mButtons);

  connect(mUrl, &QLineEdit::textChanged, this, [this]() { updateOk(); });
  connect(mFileName, &QLineEdit::textChanged, this, [this]() { updateOk(); });
  connect(mAutoName, &QCheckBox::toggled, this, [this](bool checked) {
    mFileName->setEnabled(!checked);
    updateOk();
  });
  connect(mButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  updateOk();
}

QString UrlUploadDialog::url() const { return mUrl->text().trimmed(); }

bool UrlUploadDialog::autoName() const { return mAutoName->isChecked(); }

QString UrlUploadDialog::fileName() const {
  return mFileName->text().trimmed();
}

bool UrlUploadDialog::noClobber() const { return mNoClobber->isChecked(); }

void UrlUploadDialog::updateOk() {
  const bool ok = isHttpUrl(url()) &&
                  (autoName() || RemoteOps::isValidFolderName(fileName()));
  mButtons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
