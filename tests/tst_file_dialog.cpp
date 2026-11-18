#include <QtTest>
#include <QtWidgets>

#include "file_dialog.h"
#include "rb_test_support.h"

namespace {

// A translation that renames every text, as a desktop language other than
// English does: no button reads "Open" or "Choose" any more.
class RenameAllTranslator : public QTranslator {
public:
  QString translate(const char *, const char *, const char *,
                    int) const override {
    return QStringLiteral("Esegui");
  }
  bool isEmpty() const override { return false; }
};

} // namespace

class TestFileDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-FILEDIALOG-UPLOAD-TRANSLATED: the upload dialog finds its accept button
  // by role, not by its English text. With every text translated it does not
  // crash, the button stays enabled with nothing selected, and clicking it
  // accepts the dialog (instead of opening the selected folder).
  void T_FILEDIALOG_UPLOAD_TRANSLATED() {
    RenameAllTranslator translator;
    QCoreApplication::installTranslator(&translator);
    auto removeTranslator = qScopeGuard(
        [&] { QCoreApplication::removeTranslator(&translator); });

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileDialog dialog(false);
    auto *box = dialog.findChild<QDialogButtonBox *>("buttonBox");
    QVERIFY(box);
    QPushButton *accept = box->button(QDialogButtonBox::Open);
    QVERIFY(accept);
    QVERIFY(!accept->text().contains("open", Qt::CaseInsensitive));
    QVERIFY(!accept->text().contains("choose", Qt::CaseInsensitive));

    dialog.setDirectory(dir.path());
    dialog.open();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QTRY_VERIFY(accept->isEnabled());
    accept->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QVERIFY(!dialog.isVisible());
  }

  // T-FILEDIALOG-DOWNLOAD-TRANSLATED: the download dialog keeps the normal
  // accept button of QFileDialog, also when its text is translated.
  void T_FILEDIALOG_DOWNLOAD_TRANSLATED() {
    RenameAllTranslator translator;
    QCoreApplication::installTranslator(&translator);
    auto removeTranslator = qScopeGuard(
        [&] { QCoreApplication::removeTranslator(&translator); });

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileDialog dialog(true);
    auto *box = dialog.findChild<QDialogButtonBox *>("buttonBox");
    QVERIFY(box);
    QVERIFY(box->button(QDialogButtonBox::Open));
    dialog.setDirectory(dir.path());
    dialog.open();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    dialog.reject();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
  }
};

QTEST_MAIN(TestFileDialog)
#include "tst_file_dialog.moc"
