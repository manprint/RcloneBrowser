#include <QtTest>
#include <QtWidgets>

#include "job_widget.h"
#include "rb_test_support.h"
#include "rclone_format.h"
#include "rclone_log.h"
#include "transfer_monitor.h"

namespace {

QList<QByteArray> readLines(const QString &rel) {
  QFile f(rbFixture(rel));
  if (!f.open(QIODevice::ReadOnly)) {
    qWarning() << "cannot open fixture" << f.fileName();
    return {};
  }
  QList<QByteArray> lines = f.readAll().split('\n');
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  return lines;
}

RcloneStats lastStatsOf(const QList<QByteArray> &lines) {
  RcloneStats last;
  for (const QByteArray &l : lines) {
    const RcloneLogEvent e = parseRcloneLogLine(l);
    if (e.kind == RcloneLogEvent::Kind::Stats) {
      last = e.stats;
    }
  }
  return last;
}

} // namespace

class TestJobWidgetRender : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-JOB-RENDER
  void T_JOB_RENDER() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_ok.jsonl");
    QVERIFY(!lines.isEmpty());
    const RcloneStats fs = lastStatsOf(lines);

    QObject holder;
    auto *proc = new QProcess(&holder); // never started
    JobWidget widget(proc, QStringLiteral("info"), {"copy", "s", "d"},
                     QStringLiteral("s"), QStringLiteral("d"),
                     QStringLiteral("uid"), QStringLiteral("copy"),
                     QStringLiteral("req"));
    for (const QByteArray &l : lines) {
      widget.monitor()->consumeLine(l);
    }

    auto text = [&](const char *name) {
      auto *e = widget.findChild<QLineEdit *>(name);
      return e ? e->text() : QStringLiteral("<missing %1>").arg(name);
    };
    QCOMPARE(text("transferred"), QStringLiteral("6 / 6, 100%"));
    QCOMPARE(text("size"), formatBytes(fs.bytes) + QStringLiteral(", 100%"));
    QCOMPARE(text("errors"), QStringLiteral("0"));
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 0);

    auto *output = widget.findChild<QPlainTextEdit *>("output");
    QVERIFY(output);
    int copied = 0;
    for (const QString &l : output->toPlainText().split(QLatin1Char('\n'))) {
      if (l.contains(QStringLiteral(": Copied (new)"))) {
        ++copied;
      }
    }
    QCOMPARE(copied, 6);
  }

  // T-JOB-RENDER-ERR
  void T_JOB_RENDER_ERR() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_errors.jsonl");
    QVERIFY(!lines.isEmpty());

    QObject holder;
    auto *proc = new QProcess(&holder);
    JobWidget widget(proc, QStringLiteral("info"), {"copy", "s", "d"},
                     QStringLiteral("s"), QStringLiteral("d"),
                     QStringLiteral("uid"), QStringLiteral("copy"),
                     QStringLiteral("req"));
    for (const QByteArray &l : lines) {
      widget.monitor()->consumeLine(l);
    }

    auto *errors = widget.findChild<QLineEdit *>("errors");
    QVERIFY(errors);
    const QRegularExpression rx(QStringLiteral("^[1-9]\\d*") +
                                QStringLiteral(" ") + QChar(0x2014) +
                                QStringLiteral(" "));
    QVERIFY2(rx.match(errors->text()).hasMatch(), qPrintable(errors->text()));
    QVERIFY2(errors->toolTip().contains(QStringLiteral("permission denied")),
             qPrintable(errors->toolTip()));
  }
};

QTEST_MAIN(TestJobWidgetRender)
#include "tst_jobwidget_render.moc"
