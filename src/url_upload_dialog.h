#pragma once

#include "pch.h"

// "Upload from URL...": asks for an http(s) URL, optionally the file name, and
// whether an existing file is kept (rclone copyurl --no-clobber). Built in
// code; the OK button needs a valid URL and, unless the name comes from the
// URL, a valid file name.
class UrlUploadDialog : public QDialog {
  Q_OBJECT

public:
  explicit UrlUploadDialog(const QString &destDir, QWidget *parent = nullptr);

  // The trimmed text as typed, passed to rclone unchanged.
  QString url() const;
  // True when rclone names the file from the URL (--auto-filename).
  bool autoName() const;
  QString fileName() const;
  bool noClobber() const;

private:
  void updateOk();

  QLineEdit *mUrl;
  QCheckBox *mAutoName;
  QLineEdit *mFileName;
  QCheckBox *mNoClobber;
  QDialogButtonBox *mButtons;
};
