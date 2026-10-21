#include "item_model.h"
#include "global.h"
#include "icon_cache.h"
#include "lsjson_parser.h"
#include "rc_client.h"
#include "rclone_daemon.h"
#include "rclone_format.h"
#include "rclone_process.h"
#include "utils.h"
#include <algorithm>

namespace {
static void advanceSpinner(QString &text) {
  int spinnerPos = (int)((size_t)text.length() - 2);
  QChar current = text[spinnerPos];
  static const QChar spinner[] = {'-', '\\', '|', '/'};
  size_t spinnerCount = sizeof(spinner) / sizeof(*spinner);
  const QChar *found = std::find(spinner, spinner + spinnerCount, current);
  size_t idx = found - spinner;
  size_t next = idx == spinnerCount - 1 ? 0 : idx + 1;
  text[spinnerPos] = spinner[next];
}

// The single warning row that replaces a failed listing (both paths).
static Item *errorItem(Item *parent, const QString &msg) {
  Item *error = new Item();
  error->parent = parent;
  error->state = Item::Special;
  error->isFolder = false;
  error->name = QString::fromUtf8("\xE2\x9A\xA0 ") + msg;
  return error;
}
} // namespace

class ItemSorter {
public:
  inline ItemSorter(int column, Qt::SortOrder order)
      : mColumn(column), mOrder(order) {
    mCompare.setNumericMode(true);
  }

  bool operator()(const Item *a, const Item *b) const {
    switch (mColumn) {
    case 0:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      return mOrder == Qt::AscendingOrder
                 ? mCompare.compare(a->name, b->name) < 0
                 : mCompare.compare(b->name, a->name) < 0;

    case 1:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      if (a->size == b->size) {
        return mOrder == Qt::AscendingOrder
                   ? mCompare.compare(a->name, b->name) < 0
                   : mCompare.compare(b->name, a->name) < 0;
      }
      return mOrder == Qt::AscendingOrder ? a->size < b->size
                                          : b->size < a->size;

    case 2:
      if (a->isFolder != b->isFolder) {
        return a->isFolder;
      }
      if (a->modified == b->modified) {
        return mOrder == Qt::AscendingOrder
                   ? mCompare.compare(a->name, b->name) < 0
                   : mCompare.compare(b->name, a->name) < 0;
      }
      return mOrder == Qt::AscendingOrder ? a->modified < b->modified
                                          : b->modified < a->modified;
    }
    Q_ASSERT(false);
    return false;
  }

private:
  QCollator mCompare;
  int mColumn;
  Qt::SortOrder mOrder;
};

ItemModel::ItemModel(IconCache *icons, const QString &remote, QObject *parent)
    : QAbstractItemModel(parent), mRemote(remote),
      mFixedFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)) {
  QStyle *style = qApp->style();
  mDriveIcon = style->standardIcon(QStyle::SP_DriveNetIcon);
  mFolderIcon = style->standardIcon(QStyle::SP_DirIcon);
  mFileIcon = style->standardIcon(QStyle::SP_FileIcon);

  auto settings = GetSettings();
  mFolderIcons = settings->value("Settings/showFolderIcons", true).toBool();
  mFileIcons = settings->value("Settings/showFileIcons", true).toBool();

  mRoot = new Item();
  mRoot->isFolder = true;
  mRoot->state = Item::Ready;

  QObject::connect(this, &ItemModel::getIcon, icons, &IconCache::getIcon);
  QObject::connect(
      icons, &IconCache::iconReady, this,
      [=](Item *item, const QPersistentModelIndex &parent, const QIcon &icon) {
        item->state = Item::Ready;
        QString ext = QFileInfo(item->name).suffix();
        if (!mLoadedIcons.contains(ext)) {
          mLoadedIcons.insert(ext, icon);
        }

        if (item->isDeleted) {
          delete item;
          return;
        }

        QModelIndex idx = index(item->num(), 0, parent);
        emit dataChanged(idx, idx, QVector<int>{Qt::DecorationRole});
      });
}

ItemModel::~ItemModel() {
  delete mRoot;

  // when remote widget terminated free global rclone ls processes
  global.rcloneLsProcessCount =
      global.rcloneLsProcessCount - mLocalRcloneLsProcessCount;

  if (global.rcloneLsProcessCount < 0) {
    global.rcloneLsProcessCount = 0;
  }
}

const QDir &ItemModel::path(const QModelIndex &index) const {
  return get(index)->path;
}

bool ItemModel::isLoading(const QModelIndex &index) const {
  // if invalid index return false instead of crashing
  if ((index.row() != -1) && (index.column() != -1)) {
    return get(index)->parent->isLoading();
  } else {
    return false;
  }
}

void ItemModel::refresh(const QModelIndex &index) {
  Item *item = get(index);
  Item *folderItem = item->isFolder ? item : item->parent;
  if (folderItem->isLoading()) {
    return;
  }
  load(item->isFolder ? index : index.parent(), folderItem);
}

void ItemModel::rename(const QModelIndex &index, const QString &name) {
  Item *item = get(index);
  if (item->state == Item::Special) {
    return;
  }
  item->name = name;
  item->path.setPath(item->parent->path.filePath(item->name));
  emit dataChanged(index, index, QVector<int>{Qt::DisplayRole});
}

bool ItemModel::isTopLevel(const QModelIndex &index) const {
  return get(index)->parent == mRoot;
}

bool ItemModel::isFolder(const QModelIndex &index) const {
  return get(index)->isFolder;
}

bool ItemModel::isSpecial(const QModelIndex &index) const {
  return index.isValid() && get(index)->state == Item::Special;
}

Item *ItemModel::itemFromEntry(const ListEntry &e, Item *parent) {
  Item *item = new Item();
  item->parent = parent;
  item->isFolder = e.isDir;
  item->name = e.name;
  item->modified = lsjsonModifiedText(e.modTime);
  item->size = e.isDir ? -1 : e.size;
  return item;
}

QModelIndex ItemModel::addRoot(const QString &name, const QString &path) {
  emit layoutAboutToBeChanged();

  Item *item = new Item();
  item->isFolder = true;
  item->name = name;
  item->path.setPath(path);
  item->parent = mRoot;
  mRoot->childs.append(item);

  emit layoutChanged();

  return createIndex(item->num(), 0, item);
}

QModelIndex ItemModel::index(int row, int column,
                             const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  Item *item = get(parent);
  return createIndex(row, column, item->childs[row]);
}

QModelIndex ItemModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) {
    return QModelIndex();
  }

  Item *child = get(index);
  if (child->parent == mRoot) {
    return QModelIndex();
  }

  return createIndex(child->parent->num(), 0, child->parent);
}

bool ItemModel::hasChildren(const QModelIndex &parent) const {
  Item *item = get(parent);
  if (item->isFolder) {
    if (item->state == Item::Ready) {
      return !item->childs.isEmpty();
    }
    return true;
  }
  return false;
}

int ItemModel::rowCount(const QModelIndex &parent) const {
  Item *item = get(parent);
  if (item->isFolder) {
    if (item->state == Item::Unknown) {
      const_cast<ItemModel *>(this)->load(parent, item);
    }
  }
  return item->childs.count();
}

int ItemModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 3;
}

void ItemModel::sort(int column, Qt::SortOrder order) {
  mSortColumn = column;
  mSortOrder = order;
  sort(QModelIndex(), mRoot);
}

QVariant ItemModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  const Item *item = get(index);

  if (role == Qt::DecorationRole && index.column() == 0) {
    if (item->state == Item::Special) {
      return QIcon();
    }

    if (item->isFolder) {
      if (mFolderIcons) {
        return item->parent == mRoot ? mDriveIcon : mFolderIcon;
      }
      return QIcon();
    }

    if (mFileIcons) {
      QString ext = QFileInfo(item->name).suffix();
      auto it = mLoadedIcons.find(ext);
      if (it == mLoadedIcons.end()) {
        return mFileIcon;
      }

      return it.value();
    }

    return QIcon();
  }

  if (role == Qt::TextAlignmentRole) {
    if (index.column() == 1) {
      return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case 0:
      return item->name;
    case 1:
      if (item->isFolder || item->state == Item::Special) {
        return QString();
      } else {
        return formatNiceSize(item->size);
      }
    case 2:
      return item->modified;
    }
    Q_ASSERT(false);
  }
  return QVariant();
}

QVariant ItemModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return "Name";
    case 1:
      return "Size";
    case 2:
      return "Modified";
    }
  }

  return QVariant();
}

bool ItemModel::removeRows(int row, int count, const QModelIndex &parent) {
  if (!hasIndex(row, 0, parent)) {
    return false;
  }

  Item *item = get(parent);
  if (row + count > item->childs.count()) {
    return false;
  }

  emit beginRemoveRows(parent, row, row + count - 1);

  for (int i = row; i < row + count; i++) {
    Item *node = item->childs.at(i);
    if (node->isLoading() || node->state == Item::LoadingIcon) {
      node->isDeleted = true;
    } else {
      delete node;
    }
  }
  item->childs.remove(row, count);

  emit endRemoveRows();

  return true;
}

Qt::ItemFlags ItemModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

  if (!index.isValid()) {
    return defaultFlags;
  }

  // the loading / error rows are not a drop target
  if (get(index)->state == Item::Special) {
    return defaultFlags;
  }

  return Qt::ItemIsDropEnabled | defaultFlags;
}

bool ItemModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column,
                                const QModelIndex &parent) const {
  Q_UNUSED(row);
  Q_UNUSED(column);

  if (get(parent)->state == Item::Special) {
    return false;
  }

  if (action != Qt::CopyAction && action != Qt::MoveAction) {
    return false;
  }

  if (!data->hasUrls()) {
    return false;
  }

  auto urls = data->urls();
  if (urls.count() == 1) {
    return urls.front().isLocalFile();
  }

  return false;
}

bool ItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                             int row, int column, const QModelIndex &parent) {
  if (!canDropMimeData(data, action, row, column, parent)) {
    return false;
  }

  QDir path = QDir(data->urls().front().toLocalFile());
  Item *item = get(parent);

  emit drop(path, item->isFolder ? parent : parent.parent());

  return false;
}

Item *ItemModel::get(const QModelIndex &index) const {
  return index.isValid() ? static_cast<Item *>(index.internalPointer()) : mRoot;
}

void ItemModel::load(const QPersistentModelIndex &parentIndex, Item *parent) {
  // rc listing only while the private daemon runs and the user has no default
  // rclone flags (arbitrary flags cannot be mapped to rc parameters)
  const bool useRc = global.daemon &&
                     global.daemon->state() == RcloneDaemon::State::Running &&
                     GetDefaultOptionsList("defaultRcloneOptions").isEmpty();
  mLastLoadUsedRc = useRc;

  auto cache = new QVector<Item *>();

  Item *loading = new Item();
  loading->state = Item::Special;
  loading->name = "... loading [-]";
  loading->parent = parent;

  QTimer *timer = new QTimer(this);

  QObject::connect(timer, &QTimer::timeout, this, [=]() {
    advanceSpinner(loading->name);
    auto loadingIndex = createIndex(loading->num(), 0, loading);
    emit dataChanged(loadingIndex, loadingIndex, QVector<int>{Qt::DisplayRole});
  });

  // common to both listing paths: merges the cache into the tree
  auto rcloneFinished = [=]() {
    // free rclone ls count (global and local)
    mRcloneLsProcessCountMutex.lock();
    if (global.rcloneLsProcessCount > 0) {
      global.rcloneLsProcessCount--;
    }
    if (mLocalRcloneLsProcessCount > 0) {
      mLocalRcloneLsProcessCount--;
    }
    mRcloneLsProcessCountMutex.unlock();

    parent->state =
        parent->state == Item::Loading1 ? Item::Loading2 : Item::Ready;
    if (parent->state != Item::Ready) {
      return;
    }

    timer->stop();
    timer->deleteLater();

    if (parent->isDeleted) {
      qDeleteAll(*cache);
      delete cache;
      delete parent;
      return;
    }

    QHash<QString, int> existing;
    for (int i = 0; i < parent->childs.count(); i++) {
      if (parent->childs[i] != loading) {
        existing.insert(parent->childs[i]->name, i);
      }
    }

    QVector<Item *> todo;

    bool modified = false;
    for (auto &item : *cache) {
      auto it = existing.find(item->name);
      if (it == existing.end()) {
        item->path.setPath(parent->path.filePath(item->name));
        // the error row is Special: it keeps its state and has no icon
        if (!item->isFolder && item->state != Item::Special && mFileIcons) {
          QString ext = QFileInfo(item->name).suffix();
          if (!mLoadedIcons.contains(ext)) {
            item->state = Item::LoadingIcon;
            emit getIcon(item, parentIndex);
          }
        }
        todo.append(item);
        item = nullptr;
      } else {
        Item *old = parent->childs[it.value()];
        if (old->isFolder != item->isFolder ||
            old->modified != item->modified || old->size != item->size) {
          old->state = Item::Unknown;
          old->isFolder = item->isFolder;
          old->modified = item->modified;
          old->size = item->size;
          modified = true;
          emit dataChanged(createIndex(it.value(), 0, parent),
                           createIndex(it.value(), 2, parent),
                           QVector<int>{Qt::DisplayRole});
        }
        existing.erase(it);
      }
    }

    qDeleteAll(*cache);
    delete cache;

    for (int i = 0; i < parent->childs.count(); i++) {
      if (parent->childs[i] == loading ||
          existing.contains(parent->childs[i]->name)) {
        emit beginRemoveRows(parentIndex, i, i);
        delete parent->childs.takeAt(i);
        emit endRemoveRows();
        i--;
      }
    }

    if (!todo.isEmpty()) {
      modified = true;
      emit beginInsertRows(parentIndex, parent->childs.count(),
                           parent->childs.count() + todo.count() - 1);
      parent->childs += todo;
      emit endInsertRows();
    }

    if (modified) {
      sort(parentIndex, parent);
    }
  };

  // a single load: Loading2 makes its finish take the Loading2 -> Ready path
  parent->state = Item::Loading2;

  emit beginInsertRows(parentIndex, 0, 0);
  parent->childs.prepend(loading);
  emit endInsertRows();

  timer->start(100);

  // keep track of number of lsjson rclone processes
  mRcloneLsProcessCountMutex.lock();
  global.rcloneLsProcessCount++;
  mLocalRcloneLsProcessCount++;
  mRcloneLsProcessCountMutex.unlock();

  if (useRc) {
    QJsonObject params;
    params["fs"] = rcListFs(
        mRemote, parent->path.path(),
        GetSettings()->value("Settings/remoteMode", "main").toString());
    params["remote"] = "";
    params["opt"] = QJsonObject{{"noMimeType", true}};
    if (!GetSettings()->value("Settings/showHidden", true).toBool()) {
      // the same rules as GetShowHidden()'s --exclude flags
      params["_filter"] = QJsonObject{
          {"ExcludeRule", QJsonArray{QStringLiteral(".*/**"),
                                     QStringLiteral(".*")}}};
    }

    // no timeout: rclone builds the whole reply before it sends it
    global.daemon->client()->call(
        "operations/list", params, this,
        [=](const RcReply &r) {
          bool locked = false;
          if (r.ok && r.result.value("list").isArray()) {
            for (const QJsonValue &v : r.result.value("list").toArray()) {
              if (!v.isObject()) {
                continue;
              }
              const auto e = lsjsonEntryFromObject(v.toObject());
              if (e) {
                cache->append(itemFromEntry(*e, parent));
              }
            }
          } else {
            qDeleteAll(*cache);
            cache->clear();
            QString msg;
            if (r.ok) {
              msg = "invalid rc listing reply";
            } else {
              const ConfigLoadProblem problem =
                  classifyConfigLoadError(1, r.error.toUtf8());
              locked = problem == ConfigLoadProblem::NeedsPassword ||
                       problem == ConfigLoadProblem::WrongPassword;
              msg = locked ? QString("configuration is locked")
                           : (r.error.isEmpty()
                                  ? QString("rc operations/list failed")
                                  : r.error);
            }
            cache->append(errorItem(parent, msg));
          }

          rcloneFinished();
          if (locked) {
            emit configLocked();
          }
        },
        0);
    return;
  }

  // one lsjson process per folder: stdout carries the JSON listing, stderr the
  // text log used for the error message
  auto ls = new QProcess(this);
  ls->setProcessChannelMode(QProcess::SeparateChannels);

  auto parser = new LsjsonStreamParser();
  auto errBytes = new QByteArray();

  // runs once per load: drains the parsers, turns a failure into the single
  // error row, then merges the result into the tree
  auto listingDone = [=](int exitCode, QProcess::ExitStatus exitStatus,
                         const QString &startError) {
    parser->feed(ls->readAllStandardOutput());
    parser->finish();
    for (const ListEntry &e : parser->takeEntries()) {
      cache->append(itemFromEntry(e, parent));
    }
    errBytes->append(ls->readAllStandardError());

    if (exitCode != 0 || exitStatus == QProcess::CrashExit) {
      qDeleteAll(*cache);
      cache->clear();

      QString msg = startError.isEmpty() ? lastRcloneStderrMessage(*errBytes)
                                         : startError;
      cache->append(errorItem(
          parent, msg.isEmpty()
                      ? QString("rclone lsjson failed (exit %1)").arg(exitCode)
                      : msg));
    }

    delete parser;
    delete errBytes;

    ls->deleteLater();
    rcloneFinished();
  };

  QObject::connect(ls,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
                     listingDone(exitCode, exitStatus, QString());
                   });

  // a process that cannot be started never emits finished()
  QObject::connect(ls, &QProcess::errorOccurred, this,
                   [=](QProcess::ProcessError error) {
                     if (error == QProcess::FailedToStart) {
                       // deferred: the signal may be emitted inside start()
                       const QString startError = ls->errorString();
                       QTimer::singleShot(0, this, [=]() {
                         listingDone(-1, QProcess::NormalExit, startError);
                       });
                     }
                   });

  QObject::connect(ls, &QProcess::readyReadStandardOutput, this, [=]() {
    parser->feed(ls->readAllStandardOutput());
    for (const ListEntry &e : parser->takeEntries()) {
      cache->append(itemFromEntry(e, parent));
    }
  });

  QObject::connect(ls, &QProcess::readyReadStandardError, this,
                   [=]() { errBytes->append(ls->readAllStandardError()); });

  ApplyRcloneEnvironment(ls);

  ls->start(GetRclone(),
            QStringList() << "lsjson"
                          << "--no-mimetype" << GetRcloneConf()
                          << GetRemoteModeRcloneOptions() << GetShowHidden()
                          << GetDefaultOptionsList("defaultRcloneOptions")
                          << mRemote + ":" + parent->path.path(),
            QIODevice::ReadOnly);
}

void ItemModel::sortRecursive(Item *item, const ItemSorter &sorter) {
  std::sort(item->childs.begin(), item->childs.end(), sorter);

  for (auto child : item->childs) {
    sortRecursive(child, sorter);
  }
}

void ItemModel::sort(const QModelIndex &parent, Item *item) {
  if (item->childs.isEmpty()) {
    return;
  }

  QList<QPersistentModelIndex> parents;
  parents << parent;
  emit layoutAboutToBeChanged(parents, QAbstractItemModel::VerticalSortHint);

  QModelIndexList oldList = persistentIndexList();
  QVector<QPair<Item *, int>> oldNodes;
  oldNodes.reserve(oldList.count());
  for (const auto &index : oldList) {
    oldNodes.append(qMakePair(get(index), index.column()));
  }

  ItemSorter sorter(mSortColumn, mSortOrder);
  sortRecursive(item, sorter);

  QModelIndexList newList;
  newList.reserve(oldNodes.size());
  for (const auto &node : oldNodes) {
    Item *child = node.first;
    int column = node.second;
    int row = child->num();
    newList.append(createIndex(row, column, child));
  }

  changePersistentIndexList(oldList, newList);

  emit layoutChanged(parents, QAbstractItemModel::VerticalSortHint);
}
