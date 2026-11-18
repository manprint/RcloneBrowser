#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>

// The rclone command lines of the remote-browser operations "move", "rename",
// "delete" and "create folder" (the lambdas in RemoteWidget), kept free of QSettings, QDir and
// widgets so they can be tested. Paths are joined as plain strings: the root
// item of the model has the path "", and QDir("") would turn it into ".".
// `common` is what the lambdas add after the verb today: the --config option,
// the remote-mode options and the default rclone options, in that order.
namespace RemoteOps {

// Milliseconds the "create folder" process may run before it is killed (the
// reported hang, upstream #220).
inline constexpr int kMkdirTimeoutMs = 60000;

// parent + "/" + name; just name for an empty parent; no doubled '/' after a
// parent that ends with one.
QString joinPath(const QString &parent, const QString &name);

// The text after the last '/', ignoring trailing '/' ("p/F/" gives "F").
QString lastComponent(const QString &path);

// Where folder `srcPath` lands when it is moved into `destParent`: the folder
// keeps its name, so the whole folder is moved, not only its contents.
QString folderMoveDest(const QString &srcPath, const QString &destParent);

// --s3-directory-markers, --gcs-directory-markers and
// --azureblob-directory-markers. A bucket based remote (S3, GCS, Azure Blob)
// has no empty folders: with them rclone keeps an empty folder as a
// "<name>/" marker object (mkdir, move) and removes the marker of a folder it
// deletes (rmdir, purge, move). The other backends ignore them. On a bucket
// based remote they cost a HEAD request for every file uploaded.
QStringList dirMarkerFlags();

// dirMarkerFlags() when `extra` asks for empty folders to be created or
// removed (--create-empty-src-dirs, --delete-empty-src-dirs), else nothing.
QStringList emptyDirMarkerFlags(const QStringList &extra);

// move <common> <emptyDirMarkerFlags(extra)> <remote>:<srcPath>
// <remote>:<destPath> <extra>: a file, or the contents of a folder, as the
// move action ran before (the verb stays `move`, the only one that takes
// --delete-empty-src-dirs and --create-empty-src-dirs).
QStringList moveArgs(const QString &remote, const QString &srcPath,
                     const QString &destPath, const QStringList &common,
                     const QStringList &extra);

// moveArgs for folder `srcPath` into `destParent`: destination
// folderMoveDest(srcPath, destParent).
QStringList folderMoveArgs(const QString &remote, const QString &srcPath,
                           const QString &destParent,
                           const QStringList &common,
                           const QStringList &extra);

// rmdir <common> <dirMarkerFlags()> <remote>:<srcPath>: removes the source
// folder of a finished folder move when rclone left it empty; it fails
// harmlessly when the folder is gone or not empty (never rmdirs or purge,
// which delete what is in it). On a bucket based remote it removes the marker
// of the folder, whose files (if any) stay.
QStringList sourceCleanupArgs(const QString &remote, const QString &srcPath,
                              const QStringList &common);

// True when the options ask for empty source directories to be removed
// (--delete-empty-src-dirs) and are not a dry run or an interactive run, in
// any spelling rclone accepts (--dry-run=true, -n, -vn, ...).
bool wantsSourceCleanup(const QStringList &extra);

// True when both paths name the same folder (leading and trailing '/' do not
// count): a move onto itself, where rclone does nothing and exits 0.
bool isSameLocation(const QString &src, const QString &dest);

// True for the remote types whose top-level folders are buckets (s3, b2, ...):
// rmdir on one of them deletes the bucket with its settings.
bool isBucketRemoteType(const QString &remoteType);

// Whether the source folder of a finished folder move gets the rmdir: only
// for a folder move that ran to the end (the process finished, no crash or
// start failure, exit code 0), whose options ask for it
// (wantsSourceCleanup), that moved the folder somewhere else
// (isSameLocation), and that did not empty a whole bucket (a top-level folder
// of a bucket-based remote is kept).
bool shouldCleanSource(bool isFolderMove, const QString &remoteType,
                       const QString &srcPath, const QString &destPath,
                       QProcess::ProcessState state,
                       QProcess::ProcessError error,
                       QProcess::ExitStatus status, int exitCode,
                       const QStringList &extra);

// False for an empty or blank name, a name with '/', "." and "..". An
// accepted name is used as typed (no trimming).
bool isValidFolderName(const QString &name);

// mkdir <common> <dirMarkerFlags()> <remote>:<parent>/<name>: without the
// marker a bucket based remote does nothing and the new folder never shows.
QStringList mkdirArgs(const QString &remote, const QString &parent,
                      const QString &name, const QStringList &common);

// The "delete" action: purge <common> <dirMarkerFlags()> <remote>:<path> for a
// folder (the flags let purge remove the marker of an empty folder of a bucket
// based remote), delete <common> <remote>:<path> for a file.
QStringList deleteArgs(const QString &remote, const QString &path,
                       bool isFolder, const QStringList &common);

// The "rename" action: moveto <common> <remote>:<srcPath> <remote>:<destPath>
// for a file; for a folder move <common> <dirMarkerFlags()> <remote>:<srcPath>
// <remote>:<destPath> --create-empty-src-dirs --delete-empty-src-dirs, so the
// markers and the empty subfolders of a bucket based remote go along (moveto
// leaves them behind). Backends with a server side directory move rename the
// folder in one step either way.
QStringList renameArgs(const QString &remote, const QString &srcPath,
                       const QString &destPath, bool isFolder,
                       const QStringList &common);

// The rclone runs that complete a folder rename that ran to the end (see
// shouldCleanSource for the process checks), in order: mkdir of `destPath`
// (rclone move does not create the folder when the source is an empty folder
// of a bucket based remote) and, when shouldCleanSource allows it, rmdir of
// `srcPath`. Nothing for a rename onto itself, a dry run or an interactive
// run.
QList<QStringList> folderRenameFollowUps(
    const QString &remote, const QString &remoteType, const QString &srcPath,
    const QString &destPath, QProcess::ProcessState state,
    QProcess::ProcessError error, QProcess::ExitStatus status, int exitCode,
    const QStringList &common);

} // namespace RemoteOps
