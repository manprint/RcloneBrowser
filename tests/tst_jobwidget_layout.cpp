#include <QtTest>
#include <QtWidgets>

#include "job_widget.h"
#include "rb_test_support.h"
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

// The first fixture line that is a stats block with exactly `transferring`
// items (-1: the last stats block of the file, which has none).
QByteArray statsLine(const QList<QByteArray> &lines, qsizetype transferring) {
  QByteArray found;
  for (const QByteArray &l : lines) {
    const RcloneLogEvent e = parseRcloneLogLine(l);
    if (e.kind != RcloneLogEvent::Kind::Stats) {
      continue;
    }
    if (transferring < 0) {
      found = l;
    } else if (e.stats.hasTransferring &&
               e.stats.transferring.size() == transferring) {
      return l;
    }
  }
  return found;
}

// The details block is hidden by default and the nested layout hints refresh
// through posted LayoutRequest events: without these steps a measurement is
// taken on stale hints and the test passes vacuously.
int heightOf(JobWidget &w) {
  w.showDetails();
  w.show();
  QCoreApplication::processEvents();
  w.layout()->activate();
  return w.sizeHint().height();
}

} // namespace

class TestJobWidgetLayout : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-JW-STABLE (#169): the height of the job widget does not change between
  // "a file is transferring" and "no file is transferring".
  void T_JW_STABLE() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_ok.jsonl");
    QVERIFY(!lines.isEmpty());
    const QByteArray one = statsLine(lines, 1);
    const QByteArray two = statsLine(lines, 2);
    const QByteArray last = statsLine(lines, -1);
    QVERIFY(!one.isEmpty());
    QVERIFY(!two.isEmpty());
    QVERIFY(!last.isEmpty());

    QObject holder;
    auto *proc = new QProcess(&holder); // never started
    JobWidget widget(proc, QStringLiteral("info"), {"copy", "s", "d"},
                     QStringLiteral("s"), QStringLiteral("d"),
                     QStringLiteral("uid"), QStringLiteral("copy"),
                     QStringLiteral("req"));

    // lines without a transferring item
    widget.monitor()->consumeLine(lines.at(0));
    widget.monitor()->consumeLine(lines.at(1));
    const int h0 = heightOf(widget);
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 0);

    widget.monitor()->consumeLine(one);
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 1);
    const int h1 = heightOf(widget);
    qInfo() << "T_JW_STABLE H0 =" << h0 << "H1 =" << h1;
    QCOMPARE(h1, h0);

    // growth to a new maximum is allowed once
    widget.monitor()->consumeLine(two);
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 2);
    const int h2 = heightOf(widget);
    QCOMPARE(widget.findChild<QFormLayout *>("progress")->rowCount(), 2);
    QVERIFY2(h2 >= h1, qPrintable(QStringLiteral("H2 %1 < H1 %2").arg(h2).arg(h1)));

    // The reserve is the height of the two real rows, not more: finished files
    // are removed before the new ones are added, so a tick that swaps files
    // never counts both sets (the high-water mark would over-reserve).
    auto *grid = widget.findChild<QGridLayout *>(QStringLiteral("gridLayout_2"));
    auto *form = widget.findChild<QFormLayout *>(QStringLiteral("progress"));
    QVERIFY(grid);
    QVERIFY(form);
    int row = -1;
    for (int i = 0; i < grid->count() && row < 0; ++i) {
      if (grid->itemAt(i)->layout() == form) {
        int column = 0;
        int rowSpan = 0;
        int columnSpan = 0;
        grid->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
      }
    }
    QVERIFY(row >= 0);
    QCOMPARE(grid->rowMinimumHeight(row), form->sizeHint().height());
    QVERIFY(h2 > h1); // a second row did grow the area, by one real row

    // back to one file and to none while the job runs: no shrink
    widget.monitor()->consumeLine(one);
    QCOMPARE(widget.findChild<QFormLayout *>("progress")->rowCount(), 1);
    QCOMPARE(heightOf(widget), h2);
    widget.monitor()->consumeLine(last);
    QCOMPARE(widget.findChild<QFormLayout *>("progress")->rowCount(), 0);
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 0);
    const int h3 = heightOf(widget);
    qInfo() << "T_JW_STABLE H2 =" << h2 << "H3 =" << h3;
    QCOMPARE(h3, h2);

    // the job finishes: the reserve is released, one shift at the end. The
    // strut that kept the row "used" is hidden too, or the row would still
    // add its spacing.
    const int gridBefore = grid->sizeHint().height();
    const int reserve = grid->rowMinimumHeight(row);
    emit proc->finished(0, QProcess::NormalExit);
    const int h4 = heightOf(widget);
    qInfo() << "T_JW_STABLE H4 (finished) =" << h4;
    QCOMPARE(grid->rowMinimumHeight(row), 0);
    QCOMPARE(widget.findChildren<QProgressBar *>().size(), 0);
    QVERIFY(h4 < h3);
    int struts = 0; // the widgets that share the grid row of the file rows
    for (int i = 0; i < grid->count(); ++i) {
      int r = 0;
      int column = 0;
      int rowSpan = 0;
      int columnSpan = 0;
      grid->getItemPosition(i, &r, &column, &rowSpan, &columnSpan);
      if (r == row && grid->itemAt(i)->widget()) {
        ++struts;
        QVERIFY(grid->itemAt(i)->widget()->isHidden());
      }
    }
    QVERIFY(struts >= 1);
    // the whole row is gone: its reserve and its spacing
    QVERIFY2(gridBefore - grid->sizeHint().height() > reserve,
             "the progress row still counts in the grid");
  }

  // T-JW-OVERLAP (#83): at a viewport width of 600 px the status text and the
  // "Show output" button do not overlap the info, source and dest fields.
  void T_JW_OVERLAP() {
    const QList<QByteArray> lines = readLines("rclone-1.75/copy_ok.jsonl");
    QVERIFY(!lines.isEmpty());

    QObject holder;
    auto *proc = new QProcess(&holder); // never started
    // hosted like the jobs area of the main window: a resizable scroll area
    QScrollArea area;
    area.setWidgetResizable(true);
    area.resize(600 + 2 * area.frameWidth(), 400);
    auto *widget = new JobWidget(
        proc,
        QStringLiteral("Task: \"a long description of a copy task\", Copy from "
                       "remote:some/folder"),
        {"copy", "s", "d"}, QStringLiteral("remote:some/long/source/folder"),
        QStringLiteral("/home/user/some/long/destination/folder"),
        QStringLiteral("uid"), QStringLiteral("copy"), QStringLiteral("req"));
    area.setWidget(widget);
    widget->showDetails();
    area.show();
    for (const QByteArray &l : lines) {
      widget->monitor()->consumeLine(l);
      if (parseRcloneLogLine(l).kind == RcloneLogEvent::Kind::Stats) {
        break; // a transferring state with progress rows
      }
    }
    QCoreApplication::processEvents();
    widget->layout()->activate();
    QCoreApplication::processEvents();

    const int viewportWidth = area.viewport()->width();
    qInfo() << "T_JW_OVERLAP minimumSizeHint.width ="
            << widget->minimumSizeHint().width()
            << "effective JobWidget width =" << widget->width()
            << "viewport width =" << viewportWidth;
    QCOMPARE(viewportWidth, 600);

    auto rectOf = [&](const char *name) {
      auto *c = widget->findChild<QWidget *>(name);
      return c ? QRect(c->mapTo(widget, QPoint(0, 0)), c->size()) : QRect();
    };
    const QRect status = rectOf("showDetails");
    const QRect progressInfo = rectOf("progress_info");
    const QRect showOutput = rectOf("showOutput");
    const QRect info = rectOf("info");
    const QRect source = rectOf("source");
    const QRect dest = rectOf("dest");
    QVERIFY(status.isValid());
    QVERIFY(progressInfo.isValid());
    QVERIFY(showOutput.isValid());
    QVERIFY(info.isValid());
    QVERIFY(source.isValid());
    QVERIFY(dest.isValid());

    QVERIFY(!status.intersects(info));
    QVERIFY(!status.intersects(progressInfo));
    QVERIFY(!progressInfo.intersects(info));
    for (const QRect &a : {status, progressInfo, showOutput}) {
      QVERIFY(!a.intersects(source));
      QVERIFY(!a.intersects(dest));
      QVERIFY(!a.intersects(info));
    }
    QVERIFY(!showOutput.intersects(info));

    // no child lies outside the widget
    const QRect own(QPoint(0, 0), widget->size());
    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *c : children) {
      if (!c->isVisibleTo(widget)) {
        continue;
      }
      const QRect r(c->mapTo(widget, QPoint(0, 0)), c->size());
      if (r.isEmpty()) {
        continue; // QRect::contains() is false for an empty rectangle
      }
      QVERIFY2(own.contains(r),
               qPrintable(QStringLiteral("%1 (%2) %3,%4 %5x%6 outside the widget")
                              .arg(c->objectName(),
                                   QString::fromLatin1(c->metaObject()->className()))
                              .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())));
    }
  }
};

QTEST_MAIN(TestJobWidgetLayout)
#include "tst_jobwidget_layout.moc"
