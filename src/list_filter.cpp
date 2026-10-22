#include "list_filter.h"

bool filterMatches(const QString &filter, const QStringList &fields) {
  const QStringList tokens =
      filter.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString &token : tokens) {
    bool found = false;
    for (const QString &field : fields) {
      if (field.contains(token, Qt::CaseInsensitive)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}
