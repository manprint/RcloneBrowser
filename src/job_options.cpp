#include "job_options.h"
#include "utils.h"
#include <qexception.h>
#include <qlogging.h>
#ifdef _WIN32
#pragma warning(disable : 4505)
#endif
JobOptions::JobOptions(bool isDownload) : JobOptions() {
  setJobType(isDownload);
  uniqueId = QUuid::createUuid();
}

JobOptions::JobOptions()
    : jobType(UnknownJobType), operation(UnknownOp), dryRun(false), sync(false),
      syncTiming(UnknownTiming), skipNewer(false), skipExisting(false),
      compare(false), compareOption(), verbose(false), sameFilesystem(false),
      dontUpdateModified(false), maxDepth(0), deleteExcluded(false),
      isFolder(false), DriveSharedWithMe(false), mountReadOnly(false),
      mountCacheLevel(UnknownCacheLevel), mountAutoStart(false),
      mountWinDriveMode(false), noTraverse(false), createEmptySrcDirs(false),
      deleteEmptySrcDirs(false), bisyncResync(false),
      bisyncConflictResolve("none"), bisyncResilient(false),
      bisyncRecover(false) {}

const qint32 JobOptions::classVersion = 9;

JobOptions::~JobOptions() {}

/*
 * Turn the options held here into a string list for
 * use in the rclone command.
 *
 * This logic was originally in transfer_dialog.cpp.
 *
 * This needs to change whenever e.g. new options are
 * added to the dialog.
 */
QStringList JobOptions::getOptions() const {
  QStringList list;

  if (operation == Copy) {
    list << "copy";
  } else if (operation == Move) {
    list << "move";
  } else if (operation == Sync) {
    list << "sync";
  } else if (operation == Bisync) {
    list << "bisync";
  }

  list << source;
  list << dest;

  if (!GetDefaultOptionsList("defaultRcloneOptions").isEmpty()) {
    list << GetDefaultOptionsList("defaultRcloneOptions");
  }

  if (jobType == JobOptions::JobType::Download) {
    if (!GetDefaultOptionsList("defaultDownloadOptions").isEmpty()) {
      list << GetDefaultOptionsList("defaultDownloadOptions");
    }
  }

  if (jobType == JobOptions::JobType::Upload) {
    if (!GetDefaultOptionsList("defaultUploadOptions").isEmpty()) {
      list << GetDefaultOptionsList("defaultUploadOptions");
    }
  }

  // bisync deletes on both sides itself and has no --delete-* flags
  if (sync && operation != Bisync) {
    switch (syncTiming) {
    case During:
      list << "--delete-during";
      break;
    case After:
      list << "--delete-after";
      break;
    case Before:
      list << "--delete-before";
      break;
    default:
      break;
      ;
    }
  }

  // rclone bisync ignores --update and --no-traverse and logs a WARNING for
  // each (cmd/bisync/compare.go notSupported), so a Bisync task leaves them out
  if (skipNewer && operation != Bisync) {
    list << "--update";
  }
  if (skipExisting) {
    list << "--ignore-existing";
  }

  if (compare) {
    switch (compareOption) {
    case Checksum:
      list << "--checksum";
      break;
    case IgnoreSize:
      list << "--ignore-size";
      break;
    case SizeOnly:
      list << "--size-only";
      break;
    case ChecksumIgnoreSize:
      list << "--checksum"
           << "--ignore-size";
      break;
    default:
      break;
    }
  }

  if (sameFilesystem) {
    list << "--one-file-system";
  }

  if (dontUpdateModified) {
    list << "--no-update-modtime";
  }

  if (noTraverse && operation != Bisync) {
    list << "--no-traverse";
  }

  if (createEmptySrcDirs) {
    list << "--create-empty-src-dirs";
  }

  // rclone 1.75.2 bisync rejects it as an unknown flag (exit 2)
  if (deleteEmptySrcDirs && operation != Bisync) {
    list << "--delete-empty-src-dirs";
  }

  list << "--transfers" << transfers;
  list << "--checkers" << checkers;

  if (!bandwidth.isEmpty()) {
    list << "--bwlimit" << bandwidth;
  }
  if (!minSize.isEmpty()) {
    list << "--min-size" << minSize;
  }
  if (!minAge.isEmpty()) {
    list << "--min-age" << minAge;
  }
  if (!maxAge.isEmpty()) {
    list << "--max-age" << maxAge;
  }

  if (maxDepth != 0) {
    list << "--max-depth" << QString::number(maxDepth);
  }

  list << "--contimeout" << (connectTimeout + "s");
  list << "--timeout" << (idleTimeout + "s");
  list << "--retries" << retries;
  list << "--low-level-retries" << lowLevelRetries;

  // bisync applies its deltas with a filter that keeps --delete-excluded, and
  // that makes sync.Sync delete every destination file outside the delta queue
  // (rclone 1.75.2 cmd/bisync/queue.go:254, fs/sync/sync.go:961), so a Bisync
  // task never emits it
  if (deleteExcluded && operation != Bisync) {
    list << "--delete-excluded";
  }

  if (!extra.isEmpty()) {

    for (auto line : extra.split('\n')) {
      // split on spaces but not if inside quotes e.g. --option-1
      // --option-2="arg1 arg2" --option-3 arg3 should generate "--option-1"
      // "--option-2=\"arg1 arg2\"" "--option-3" "arg3"
      for (QString arg :
           line.split(QRegularExpression(" (?=[^\"]*(\"[^\"]*\"[^\"]*)*$)"))) {
        if (!arg.isEmpty()) {
          list << arg.replace("\"", "");
        }
      }
    }
  }

  // rclone adds every include rule before every exclude rule whatever the
  // order of the flags, so with both an Include and an Exclude text the Exclude
  // text would never apply to a file that an include matches (#179). Then the
  // rules go out as ordered --filter rules, exclude first; they are followed by
  // the Filter tab and a final "- **" (rclone only implies it for the include
  // flags). Every other combination keeps the plain flags.
  const auto rules = [](const QString &text) {
    QStringList out;
    for (const QString &line : text.split('\n')) {
      if (!line.trimmed().isEmpty()) {
        out << line;
      }
    }
    return out;
  };
  const QStringList includeRules = rules(included);
  const QStringList excludeRules = rules(excluded);
  const bool ordered = !includeRules.isEmpty() && !excludeRules.isEmpty();

  if (ordered) {
    for (const QString &line : excludeRules) {
      list << "--filter" << ("- " + line);
    }
    for (const QString &line : includeRules) {
      list << "--filter" << ("+ " + line);
    }
  } else {
    if (!included.isEmpty()) {
      for (auto line : included.split('\n')) {
        list << "--include" << line;
      }
    }

    // excluded after included and extra options as they can also contain
    // included
    if (!excluded.isEmpty()) {
      for (auto line : excluded.split('\n')) {
        list << "--exclude" << line;
      }
    }
  }

  if (!filtered.isEmpty()) {
    for (auto line : filtered.split('\n')) {
      list << "--filter" << line;
    }
  }

  if (ordered) {
    list << "--filter"
         << "- **";
  }

  // get Google Drive mode option
  if (remoteType == "drive") {
    if (remoteMode == "shared") {
      list << "--drive-shared-with-me";
    } else {
      if (remoteMode == "trash") {
        list << "--drive-trashed-only";
      } else {
        if (remoteMode == "main") {
        } else {
          // older tasks dont't have googleDriveMode
          // and value from DriveSharedWithMe has to be used
          if (DriveSharedWithMe) {
            list << "--drive-shared-with-me";
          }
        }
      }
    }
  }

  if (dryRun) {
    list << "--dry-run";
  }

  if (operation == Bisync) {
    if (bisyncResync) {
      list << "--resync";
    }
    if (bisyncConflictResolve != "none" && !bisyncConflictResolve.isEmpty()) {
      list << "--conflict-resolve" << bisyncConflictResolve;
    }
    if (bisyncResilient) {
      list << "--resilient";
    }
    if (bisyncRecover) {
      list << "--recover";
    }
    if (!bisyncWorkdir.isEmpty()) {
      list << "--workdir" << bisyncWorkdir;
    }
  }

  return list;
}

SerializationException::SerializationException(QString msg)
    : QException(), Message(msg) {}
