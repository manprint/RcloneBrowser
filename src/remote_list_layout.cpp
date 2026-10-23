#include "remote_list_layout.h"

#include <QListWidget>

RemotesListGeometry remotesListGeometry(const QString &layout, int iconPx,
                                        const QFontMetrics &fm,
                                        const QStringList &names) {
  iconPx = qMax(iconPx, 8);
  const int ch = qMax(1, fm.averageCharWidth());
  int maxText = 0;
  for (const QString &name : names) {
    maxText = qMax(maxText, fm.horizontalAdvance(name));
  }
  auto label = [&](int lo, int hi) { return qBound(lo * ch, maxText, hi * ch); };
  const int textH = fm.height();

  RemotesListGeometry g;
  g.iconSize = QSize(iconPx, iconPx);
  if (layout == QLatin1String("list")) {
    g.viewMode = QListView::ListMode;
    g.wrapping = true;
    g.elideMode = Qt::ElideRight;
    g.itemSize = QSize(iconPx + kPad + label(6, 32) + 2 * kPad,
                       qMax(iconPx, textH) + kPad);
    g.gridSize = g.itemSize + QSize(2 * kPad, kPad);
    g.spacing = 0;
  } else if (layout == QLatin1String("longlist")) {
    g.viewMode = QListView::ListMode;
    g.wrapping = false;
    g.elideMode = Qt::ElideRight;
    g.itemSize = QSize(iconPx + kPad + label(6, 64) + 2 * kPad,
                       qMax(iconPx, textH) + kPad);
    g.gridSize = QSize();
    g.spacing = kPad / 2;
  } else {
    // tiles, and any unknown layout
    g.viewMode = QListView::IconMode;
    g.wrapping = true;
    g.elideMode = Qt::ElideMiddle;
    g.itemSize = QSize(qMax(iconPx, label(6, 16)) + 2 * kPad,
                       iconPx + fm.lineSpacing() + 2 * kPad);
    g.gridSize = g.itemSize + QSize(2 * kPad, 2 * kPad);
    g.spacing = 0;
  }
  return g;
}

void applyRemotesListGeometry(QListWidget *list, const RemotesListGeometry &g) {
  list->setViewMode(g.viewMode);
  // disable drag and drop
  list->setMovement(QListView::Static);
  // always adjust icons after the window is resized
  list->setResizeMode(QListView::Adjust);
  list->setWrapping(g.wrapping);
  list->setIconSize(g.iconSize);
  list->setGridSize(g.gridSize);
  list->setSpacing(g.spacing);
  list->setTextElideMode(g.elideMode);
  list->setWordWrap(false);
  list->setUniformItemSizes(true);
  for (int i = 0; i < list->count(); ++i) {
    list->item(i)->setSizeHint(g.itemSize);
  }
}
