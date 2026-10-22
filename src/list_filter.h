#pragma once

#include <QString>
#include <QStringList>

// The search filter of the Tasks and Jobs lists (#21). `filter` is split on
// whitespace; every token must occur, case-insensitively, in at least one of
// `fields`. An empty filter matches everything.
bool filterMatches(const QString &filter, const QStringList &fields);
