# Change Log

## [2.1.0] - unreleased
Changes since 2.0.0.

-   NEW: "Bisync" mode in the Transfer dialog runs `rclone bisync` between two folders (Upload and Download, saved tasks, Dry run, queue and scheduler), with "First run / resync", conflict resolution (none, newer, older, larger, smaller, path1, path2), "Resilient", "Recover" and a "Workdir"; a Bisync task leaves out the six options that bisync rejects, ignores, turns into deletions or cannot stay in sync with - #243
-   NEW: "Copy to…" in a remote's right-click and Tools menus copies one file to a name and place you type, on the same remote, on another remote or to a local path (`rclone copyto`) - #192
-   NEW: "Upload from URL…" in the same menus uploads a file from an http or https address into the selected folder (`rclone copyurl`), named from the address or by you, with an option not to overwrite an existing file - #199
-   NEW: "Serve…" in a remote's right-click and Tools menus serves the selected folder (or the top of a remote) over http, webdav, ftp, sftp, dlna, nfs, restic or s3 through the private backend (`rclone serve`), on 127.0.0.1 and read-only by default (restic has no read-only mode), and "Serves…" in the File menu opens a window that lists the running serves with Copy URL and Stop; serves are not saved and end when Rclone Browser quits - #129
-   NEW: the path field of a remote tab is editable: type a path and press Enter or "Go" to open it as a top-level row of the tab (the tab's own `remote:` prefix is accepted), Ctrl+L moves the cursor into the field and Esc puts back the selected path; an Internet Archive remote opens with the cursor in the field, ready for an item identifier, and a Google Drive tab drops the typed rows when its mode changes - #16, #148
-   NEW: a "Filter" field above the Tasks list and above the Jobs list hides the rows that do not contain every word typed, in any letter case (a task's name, source and destination; a job's title, source and destination); it only hides rows, so the count of running jobs, Stop All (running transfers) and Clean (inactive jobs) still include the hidden ones, and Ctrl+F moves the cursor into the filter of the Tasks or Jobs tab, whichever is current - #21
-   NEW: keyboard shortcuts for the main actions: Alt+I, Alt+S, Alt+T, Alt+L, Alt+M, Alt+V, F6, Shift+F5 and Alt+E on a remote tab (next to the existing F5, F7, F2, Del, Alt+U and Alt+D), Ctrl+1 to Ctrl+5 for the main tabs, Ctrl+R for the remote list, and Ctrl+Return, Ctrl+E and Ctrl+Shift+Return for Run, Edit and Dry Run in the Tasks tab; the full table is "Keyboard shortcuts" in the README - #94
-   FIXED: Ctrl+W on a remote tab only hid the tab: its widget stayed in memory until Rclone Browser quit, and closing the last remote tab did not go back to Remotes; it now closes the tab like the x button does
-   CHANGED: the tasks file (tasks.bin) has a new format version. Rclone Browser 2.0.0 and older cannot read it once this version has saved, edited, duplicated or deleted a task (a Run with a task name or "Auto task name" also saves one): they show no tasks and replace the file at their next save. To go back, copy tasks.bin before that first save, and keep the newer file too if you will return to this version

## [2.0.0] - 2026-10-22
Changes since 2.0.0-beta.1. The older `[2.0.0] - 2020-03-` section further down is upstream's development history, not this release.

-   NEW: a private `rclone rcd` backend lists folders; it listens on `127.0.0.1` with a random port and gets random credentials through its environment. `rclone lsjson` remains the fallback when the backend is unavailable or "Default rclone options" is set
-   NEW: remotes are created and edited in a native dialog: provider list with a filter, one question per page with rclone's help text, browser sign-in for OAuth backends. Cancel while creating removes the half-created remote
-   NEW: "Edit remote…" and "Advanced (terminal)…" in a remote's context menu; "Advanced (terminal)…" opens `rclone config` in a terminal
-   NEW: mounts started with a remote control port show a "Refresh cache" button (rclone's `vfs/refresh`)
-   NEW: protocol tests against isolated sftp, FTP, WebDAV and S3 (MinIO) servers in local containers (`scripts/dev.sh servers`)
-   NEW: "Copy command" in the Tasks context menu copies the command Run would start, one line per selected task; mount tasks carry no remote control credentials - #123, #176
-   NEW: "Duplicate" in the Tasks context menu copies the selected task as "<description> (copy)" - #139
-   NEW: a "Restart" button on a finished, failed or stopped transfer job starts the same transfer again as a new job - #118
-   NEW: "Ask before running a task" in Preferences (on by default) turns off the question before Run, Dry run and Restart - #144
-   NEW: "Terminal command" in Preferences (Linux and BSD) chooses the terminal for "Config" and "Advanced (terminal)…", before `$TERMINAL` and the built-in list - #175
-   NEW: README "FAQ and known limitations"; `docs/triage/upstream-issues.md` gives the fork's status for every open upstream issue, checked by `scripts/check-triage.py`
-   FIXED: copied commands of jobs, mounts and streams did not run when pasted into a shell; arguments are now quoted for the platform's shell - #226, #194
-   FIXED: the job panel changed height whenever a file started or finished - #169
-   FIXED: with both Include and Exclude patterns, excluded files were still copied because rclone applies every include rule first; the transfer now sends ordered `--filter` rules - #179
-   FIXED: moving a folder into a location that already has a folder of that name left the emptied source folder behind - #171
-   FIXED: "Create folder" asks for the name, keeps OK disabled for names rclone cannot create, can be cancelled and stops a hanging mkdir after 60 s - #220
-   FIXED: a missing or failing rclone quit the application at startup; a message with a Preferences button now says what is wrong - #159, #145, #209
-   FIXED: the main window could not be narrower than the button row of a remote tab (up to 1500 px with large buttons); the row now scrolls - #156, #228
-   FIXED: the Cancel button of the rename progress dialog stayed disabled while a rename hung
-   CHANGED: mount rc credentials reach rclone through the environment, never the command line; `--rc-user` and `--rc-pass` typed into the mount options are removed
-   CHANGED: mount scripts get `RCLONE_RC_USER`, `RCLONE_RC_PASS` and `RB_RC_PORT` in their environment; the positional arguments `$3` (rc user) and `$4` (rc password) still work but are deprecated
-   CHANGED: the main window code is split into `main_window.cpp` and `main_window_tasks.cpp`, guarded by a snapshot test of the window's widgets and actions
-   CHANGED: when both Include and Exclude are set, they reach rclone as ordered `--filter` rules (exclude, include, your Filter lines, `- **`); an `--exclude` typed into the extra options still follows rclone's include-first rule

## [2.0.0-beta.1] - 2026-10-22
Rclone Browser 2.x by the manprint fork. The [2.0.0] section below is upstream's unreleased development history that this fork builds on.

-   NEW: transfers read progress, per-file stats and errors from rclone's JSON log, so they work for any file name; `--log-file` is honoured
-   NEW: folder listings, export and the remote list use `rclone lsjson` and `rclone listremotes --json`; a failed listing shows a warning row with rclone's error
-   NEW: Docker development environment (`scripts/dev.sh`); unit and end-to-end test suites run in CI
-   NEW: macOS Apple Silicon (arm64) DMG and Windows ARM64 build
-   NEW: "Test" button in Preferences checks the selected rclone
-   NEW: configurable rclone releases repository for the update check (default `rclone/rclone`)
-   NEW: asynchronous update checks
-   NEW: a wrong config password asks again; a failing `RCLONE_PASSWORD_COMMAND` shows rclone's error
-   NEW: Nicolaius's customizations (thanks @Nicolaius): remote icon size as a multiplier (0.5 to 20, default 4.0), options to hide the Tasks, Queue and Scheduler tabs and the remote icon row, and a player chooser when the stream command lists several players separated by `|`
-   FIXED: error field always empty - #141
-   FIXED: size, bandwidth and remaining time empty with `--log-file` - #142
-   FIXED: file names with slashes and special characters in transfer output - #160 (progress is now read from rclone's JSON log)
-   FIXED: folder names ending with a space - #107
-   FIXED: names with `[ ]`, `( )` and other characters - #130
-   FIXED: sizes of Google Docs and other unknown sizes - #140 (listing via `rclone lsjson`)
-   FIXED: cancelled jobs kept running - #117
-   FIXED: lock file shared by all users; the lock is now per user on Linux - #162
-   FIXED: "Config" button opening an empty or broken terminal on Linux - #163, #172, #239
-   FIXED: config password prompt shown although `RCLONE_PASSWORD_COMMAND` is set - #79
-   CHANGED: minimum rclone is 1.75.0; older versions get a blocking dialog at startup
-   CHANGED: the update check for Rclone Browser targets manprint/RcloneBrowser
-   CHANGED: remotes are listed by name, case-sensitively (uppercase first)
-   CHANGED: tighter rows in the long list view of remotes
-   CHANGED: the remote icon size setting is a number; the old XS to XXL values convert automatically
-   CHANGED: logging and stats options in a transfer's extra options are ignored (see "Transfers and logging" in README)
-   CHANGED: Travis CI and AppVeyor removed; GitHub Actions builds every platform
-   CHANGED: no 32-bit, i386, armhf/Raspbian or Intel Mac builds are published any more

## [2.0.0][2.0.0] - 2020-03-
-   NEW: interface look and feel redisign - more space, more flexible and dynamic, more information, more cutomizations
-   NEW: added queueing functionality. Tasks can be queued for later excecution. Queue itself can be rearranged and edidted. It is persistent between application restart. And in addition user defined script can be run when all queued tasks are processed.
-   NEW: changed how tasks are handled - multiple actionable selections available now
-   NEW: added remotes tiles and list view - more remotes on one screen
-   NEWL added tasks view wrapping - more tasks on one screen
-   NEW: added configurable application font size
-   NEW: added configurable inteface buttons style and size
-   NEW: changed dark mode look on Windows and Linux
-   NEW: replaced all Qt generic icons with custom ones - more consistent look and feel
-   NEW: jobs are not deleted immediately after pressing close button. Now they enter "stopped" state
-   NEW: added "rclone about" command button - provides info about remote space (total/used/free)
-   NEW: getTree, getSize, getLink, getInfo and export dialogs are now modeless - they don't block main application window
-   NEW: remote actions e.g. getTree now have status info - "Running" and "Finished"
-   NEW: added new button for rclone check/cryptcheck
-   NEW: for Google drive added option to browse trash (--drive-trashed-only) - #82
-   NEW: added buttons to stop all running jobs and clean all innactive
-   NEW: added files deduplication option (rclone dedupe) for Google drive
-   NEW: added "copy rclone command to clipboard" for stream and mount jobs
-   FIXED: added all missing remotes icons including latest rclone 1.51 additions (SugarSync and memory)
-   FIXED: widgets layouts
-   FIXED: export files list encoding switched to UTF8 to fix characters support - issue #77 - Thank you @partment for PR #78
-   FIXED: parsing rclone options containing quotes and spaces e.g. --password-command="pass rclone/config" - issue #79 - Thank you @myelsukov for help
-   FIXED: job notificantion icon
-   FIXED: quitting the app from systray when "close to systray" is enabled
-   bunch of usual small tweaks and fixes

## [1.8.0][1.8.0] - 2020-02-17
-   NEW: http(s) proxy configuration for rclone
-   NEW: remotes icons size option selector
-   NEW: directories tree display for remotes
-   NEW: rclone extra default options for all operations (e.g. --fast-list)
-   NEW: added "Public Link" button to remote view
-   FIXED: option to show hidden files and folders was not always working as expected
-   FIXED: for sftp server default to home user directory (as normal sftp would do)
-   FIXED: an issue when on Windows local remote only allowed to browse drive C:
-   FIXED: problem using rclone and rclone.conf when path contained spaces
-   FIXED: bandwidth box on jobs tab is too small for fast connections
-   bunch of usual small tweaks and fixes

## [1.7.0][1.7.0] - 2019-11-27
-   NEW: built all releases with the latest Qt 5.13.2
-   NEW: changed Linux releases format to AppImage only
-   NEW: changed macOS release format to dmg image file
-   NEW: added installer for Windows releases - implemented using [Inno Setup](https://github.com/jrsoftware/issrc)
-   NEW: added Linux i386 release
-   NEW: changed macOS release compilation options to make it work on all macOS versions starting with 10.9
-   NEW: added portable mode for macOS and Linux
-   NEW: on Linux multiple terminals are tried for rclone config ($TERMINAL then gnome-terminal followed by xfce4-terminal, xterm, x-terminal-emulator and konsole)
-   NEW: enabled Qt HighDpiScaling - should help people with high DPI monitors
-   NEW: added dark mode - configurable via preferences or system setting (newer macOS) - thank you @noaione for initial PR
-   changed preferences window - added tabs to create more space for new options
-   fixed Windows portable mode
-   fixed mount/unmount on FreeBSD
-   disabled mount on OpenBSD and NetBSD (as not supported by rclone)
-   updated build and install for Linux - now all files will be installed in /usr/local root
-   fixed possible crashes when old rclone is used (with different version information output)
-   fixed an issue with long file names leading sometimes to inaccurate transfer progress bar display
-   added additional info to file progress bar tooltip - individual file stats
-   changed program icon
-   bunch of usual small tweaks and fixes

## [1.6.0][1.6.0] - 2019-10-27
-   fixed Windows mount/unmount (requires rclone v1.50+)
-   Rclone Browser checks now for used rclone version (mount is disabled in Windows if rclone <v1.50)
-   added default download/upload folders - configurable in settings
-   add default download/upload extra options - configurable in settings
-   added available updates' notifications for both Rclone Browser and rclone - can be turned on/off in settings
-   all mount options are configurable via settings - generic "rclone mount remote local" is used without any options specified
-   default mount option (in settings) is "--vfs-cache-mode writes"
-   Google Drive with "shared with me" option on is always mounted as read-only
-   Windows deployment includes now all required runtime files for users without MSVCR installed
-   added ftp, MS Azureblob and Google Photos remote icons
-   modified main application window status bar to save space
-   released binary for Windows 32 bits
-   released binary for armhf 32 bits - for Raspberry Pi running raspbian
-   bunch of usual small tweaks and fixes

## [1.5.3][1.5.3] - 2019-10-24
-   Windows only update - include all required runtime dll files

## [1.5.2][1.5.2] - 2019-09-27
-   code cleanup - clean compilation with -Werror enabled, GCC8 compilation fixed
-   add tooltips showing rclone options used to all transfer window options
-   Google "drive shared with me" caused multiple of issues - now all should work
-   as always small cosmetic UI improvements - still plenty to do but core functionality was first

## [1.5.1][1.5.1] - 2019-09-25
-   after task edit initiated by double click main window does not get proper focus back and subsequent Run click might lead to wrong task execution. For time being I disable double click edit - until proper fix is produced.

## [1.5][1.5] - 2019-09-25
-   tasks - jobs can be saved/edited/run/deleted. No need creating the same job again and again.
-   on Google drive DriveSharedWithMe can be mounted to local filesystem
-   DriveSharedWithMe checkbox is now disabled for non Google destinations - it is Google only feature and turning it on for other destinations does not make sense - could even crash the browser.
-   verbose option is now always on and has been removed from UI - which means that stats will be always displayed. No more wondering how long it is going to take for some long job to finish.
-   fixed an issue with local remote on Windows when local drive content was not properly displayed
-   replaced remote Amazon icon with generic S3 one. S3 became name on its own and almost de-facto standard in cloud access used by many rclone supported destinations
-   new application logo

## [1.4.1][1.4.1] - 2019-09-18
-   small GUI tweaks to make all progress fields always visible (they were too small for large transfers) and adjust some screen sizes to make all GUI elements visible
-   update all builds with latest Qt (5.13.1)

## [1.4][1.4] - 2019-08-23
-   Fix compliation errors and update all builds with latest Qt (5.13)
-   Fix Config button command
-   Further fix and tweak progress display. Add ETA and Total Size fields
-   Fix remotes icons display
-   Add sftp icon
-   Fix progress display for rclone > 1.37 (by DinCahill)
-   Add a Public Link option to the right-click menu (by DinCahill)
-   Add preference: Show hidden files and folders (by DinCahill)
-   Add Mega icon (by DinCahill)
-   Refresh when Shared is toggled (by DinCahill)
-   Disable Upload button for Shared (by DinCahill)
-   Support for shared Google Drive files. Enable the checkbox when you open a remote, and all rclone commands will be passed --drive-shared-with-me (by DinCahill)
-   Set cache mode for mounts (by DinCahill)
-   Fixed missing leading / in path (required for some SFTP servers) (by DinCahill)

## [1.2][1.2] - 2017-03-11
-   Calculate size of folders, issue #4
-   Copy transfer command to clipboard, issue #20
-   Support custom .rclone.conf location, #21
-   Export list of files, issue #27
-   Bugfix for folder refresh not working after rename, issue #30
-   Remember empty text fields in transfer dialog, issue #32
-   Error message when too old rclone version is selected
-   Support portable mode, issue #28
-   Create .deb packages, issue #26

## [1.1][1.1] - 2017-01-31
-   Added `--transfer` option in UI, issue #1
-   Supports encrypted `.rclone.conf` configuration file, issue #2
-   Fixed crash when canceling active stream
-   Added ETA tooltip for transfer progress bars
-   Allow to specify extra arguments for rclone, issue #7
-   Fix for browsing Hubic remotes, issue #10
-   Support high-dpi mode for macOS

## [1.0.0][1.0.0] - 2017-01-29
-   Allows to browse and modify any rclone remote, including encrypted ones
-   Uses same configuration file as rclone, no extra configuration required
-   Simultaneously navigate multiple repositories in separate tabs
-   Lists files hierarchically with file name, size and modify date
-   All rclone commands are executed asynchronously, no freezing GUI
-   File hierarchy is lazily cached in memory, for faster traversal of folders
-   Allows to upload, download, create new folders, rename or delete files and folders
-   Can process multiple upload or download jobs in background
-   Drag & drop support for dragging files from local file explorer for uploading
-   Streaming media files for playback in player like mpv or similar
-   Mount and unmount folders on macOS and GNU/Linux
-   Optionally minimizes to tray, with notifications when upload/download finishes

[1.9.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.9.0
[1.8.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.8.0
[1.7.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.7.0
[1.6.0]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.6.0
[1.5.3]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.3
[1.5.2]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.2
[1.5.1]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5.1
[1.5]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.5
[1.4.1]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.4.1
[1.4]: https://github.com/kapitainsky/RcloneBrowser/releases/tag/1.4
[1.2]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.2
[1.1]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.1
[1.0.0]: https://github.com/mmozeiko/RcloneBrowser/releases/tag/1.0.0
