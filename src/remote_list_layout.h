#pragma once

#include <QFontMetrics>
#include <QListView>
#include <QSize>
#include <QString>
#include <QStringList>

class QListWidget;

// Geometry of the remotes list (1.1): derived from the icon size and the font
// metrics, so a label always has room in its cell at any scale and layout.
// Qt Widgets only.

constexpr int kPad = 6;

struct RemotesListGeometry {
  QListView::ViewMode viewMode;
  bool wrapping;
  QSize iconSize;
  QSize gridSize; // QSize() = no grid
  QSize itemSize; // set as Qt::SizeHintRole on every item
  int spacing;
  Qt::TextElideMode elideMode;
};

// layout: the "iconsLayout" setting ("tiles", "list" or "longlist"; anything
// else is "tiles"). iconPx is clamped to at least 8. names are the remote
// names the list will show (the widest one sizes the label, within limits).
RemotesListGeometry remotesListGeometry(const QString &layout, int iconPx,
                                        const QFontMetrics &fm,
                                        const QStringList &names);

// Applies g to a list that already holds its items.
void applyRemotesListGeometry(QListWidget *list, const RemotesListGeometry &g);
