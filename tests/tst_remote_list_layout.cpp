#include <QtTest>

#include <QListWidget>
#include <QPixmap>

#include "rb_test_support.h"
#include "remote_list_layout.h"

namespace {

const QStringList &remoteNames() {
  static const QStringList names{
      QStringLiteral("a"),
      QStringLiteral("gd"),
      QStringLiteral("drive"),
      QStringLiteral("s3"),
      QStringLiteral("photos"),
      QStringLiteral("backup-2026"),
      QStringLiteral("a_remote_with_a_rather_long_name_xyz_1234"),
      QString::fromUtf8("\xC3\x84\xC3\x96\xC3\x9C-umlauts"),
      QStringLiteral("onedrive_business"),
      QStringLiteral("x"),
      QStringLiteral("pcloud"),
      QStringLiteral("mega"),
  };
  return names;
}

QIcon filledIcon() {
  QPixmap pm(64, 64);
  pm.fill(Qt::darkCyan);
  return QIcon(pm);
}

// The properties MainWindow's .ui sets on `remotes`; applied before any layout.
void fillList(QListWidget &w) {
  w.setWordWrap(true);
  w.setTextElideMode(Qt::ElideMiddle);
  w.setSelectionBehavior(QAbstractItemView::SelectRows);
  w.resize(260, 300);
  for (const QString &n : remoteNames()) {
    w.addItem(new QListWidgetItem(filledIcon(), n));
  }
}

// Number of I-1 violations: pairs of intersecting item rects, items whose
// rect centre does not hit the item itself, and (with a valid grid) item rects
// that do not fit inside gridSize, and items without room for their label (C4).
int violations(QListWidget &w) {
  int bad = 0;
  const int n = w.count();
  const QFontMetrics fm = w.fontMetrics();
  const int ch = fm.averageCharWidth();
  QList<QRect> rects;
  for (int i = 0; i < n; ++i) {
    rects.append(w.visualItemRect(w.item(i)));
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (rects[i].intersects(rects[j])) {
        ++bad;
      }
    }
    const QModelIndex hit = w.indexAt(rects[i].center());
    if (!hit.isValid() || hit.row() != i) {
      ++bad;
    }
    const QSize grid = w.gridSize();
    if (grid.isValid() &&
        (rects[i].width() > grid.width() || rects[i].height() > grid.height())) {
      ++bad;
    }
    // C4 label room: the rect must leave room for the label (or 6 average
    // characters of it), after the icon and the focus-frame margins.
    const int m = w.style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr,
                                         &w) + 1;
    const int textW = fm.horizontalAdvance(w.item(i)->text());
    const int avail = w.viewMode() == QListView::IconMode
                          ? rects[i].width() - 2 * m
                          : rects[i].width() - w.iconSize().width() - 3 * m;
    if (avail < qMin(textW, 6 * ch)) {
      ++bad;
    }
  }
  return bad;
}

} // namespace

class TestRemoteListLayout : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { rbInitTestEnvironment(); }

  // T-ICON-GRID-LEGACY: the settings of the old rcloneListRemotes (grid from
  // the icon size only), on a list with the .ui properties, at scale 1.0.
  // Pins that the I-1 oracle sees the bugs the new geometry fixes: C4 label
  // room in tiles, overlapping wrapped items in longlist. `list` is logged only.
  void T_ICON_GRID_LEGACY() {
    const int size = 16;
    int tiles = 0, list = 0, longlist = 0;
    {
      QListWidget w;
      fillList(w);
      w.setIconSize(QSize(size, size));
      w.setViewMode(QListWidget::IconMode);
      w.setMovement(QListView::Static);
      w.setResizeMode(QListView::Adjust);
      w.setWrapping(true);
      w.setGridSize(QSize(size + 20, size + 40));
      w.setSpacing(10);
      w.setTextElideMode(Qt::ElideMiddle);
      w.show();
      QVERIFY(QTest::qWaitForWindowExposed(&w));
      w.doItemsLayout();
      tiles = violations(w);
    }
    {
      QListWidget w;
      fillList(w);
      w.setIconSize(QSize(size, size));
      w.setViewMode(QListWidget::ListMode);
      w.setResizeMode(QListView::Adjust);
      w.setWrapping(true);
      w.setGridSize(QSize(size + 100, size + 20));
      w.setSpacing(10);
      w.show();
      QVERIFY(QTest::qWaitForWindowExposed(&w));
      w.doItemsLayout();
      list = violations(w);
    }
    {
      QListWidget w;
      fillList(w);
      w.setIconSize(QSize(size, size));
      w.setViewMode(QListWidget::ListMode);
      w.setResizeMode(QListView::Adjust);
      w.setWrapping(false);
      w.setGridSize(QSize(size + 8, size + 8));
      w.show();
      QVERIFY(QTest::qWaitForWindowExposed(&w));
      w.doItemsLayout();
      longlist = violations(w);
    }
    qInfo() << "legacy violations: tiles" << tiles << "list" << list
            << "longlist" << longlist;
    QVERIFY(tiles > 0);
    QVERIFY(longlist > 0);
  }

  void T_ICON_GRID_data() {
    QTest::addColumn<QString>("layout");
    QTest::addColumn<double>("scale");
    for (const QString layout : {"tiles", "list", "longlist"}) {
      for (const double scale : {0.5, 1.0, 1.5, 2.0, 4.0, 8.0}) {
        QTest::newRow(qPrintable(QString("%1 x%2").arg(layout).arg(scale)))
            << layout << scale;
      }
    }
  }

  // T-ICON-GRID: with the D1 geometry no item overlaps, every item is
  // pickable, fits its cell and has room for its label (I-1).
  void T_ICON_GRID() {
    QFETCH(QString, layout);
    QFETCH(double, scale);
    const int iconPx = qRound(scale * 16);
    QListWidget w;
    fillList(w);
    const RemotesListGeometry g =
        remotesListGeometry(layout, iconPx, w.fontMetrics(), remoteNames());
    applyRemotesListGeometry(&w, g);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.doItemsLayout();
    QCOMPARE(violations(w), 0);
    QCOMPARE(w.iconSize(), QSize(qMax(iconPx, 8), qMax(iconPx, 8)));
    if (layout == "tiles") {
      QVERIFY(g.itemSize.height() >=
              g.iconSize.height() + w.fontMetrics().height());
    }
  }

  // T-ICON-GRID-EMPTY: no remotes still gives a usable item size.
  void T_ICON_GRID_EMPTY() {
    QListWidget w;
    const QFontMetrics fm = w.fontMetrics();
    for (const QString layout : {"tiles", "list", "longlist"}) {
      const RemotesListGeometry g =
          remotesListGeometry(layout, 16, fm, QStringList());
      QVERIFY(g.itemSize.isValid());
      QVERIFY(g.itemSize.width() >= 6 * fm.averageCharWidth());
    }
  }
};

QTEST_MAIN(TestRemoteListLayout)
#include "tst_remote_list_layout.moc"
