# Upstream issue and PR triage

Source snapshot: open issues and open pull requests of kapitainsky/RcloneBrowser as of 2026-10-21 (129 issues, 10 PRs). Nothing is posted upstream: no comment, label, close or pull request is made on kapitainsky/RcloneBrowser. Dispositions refer to units of plan 001 in `docs/plans/001_plan-RcloneBrowserModernization/`. The user fills the `User OK` column for each row; it stays empty until then.

Fork status says what the fork does about each row. It is one of the following forms, and `scripts/check-triage.py` checks them (run it with this file and the issue snapshot of the plan folder):

*   `fixed in <unit> (<test IDs>)`: the unit that delivered the fix and the `T_...` test functions of `tests/` that pin it (no fix without a test ID or a recorded reproduction)
*   `not reproduced (<test IDs>)`: the reproducing test passes on the base and stays as a regression guard
*   `answered`, followed in parentheses by the word README, a hash sign and the explicit `faq-...` anchor of the FAQ entry in README.md: the README answers the question
*   `planned in <unit>`: owned by a later unit of the plan
*   `deferred: <rationale>`: not done, with the reason; the rest of the cell is the rationale, and it says what already exists but is not covered by a test
*   `obsolete` and `no action (announcement)` (only #253)

Every form except `deferred` can be followed by `; <note>`. A note or rationale may point to the FAQ with the same README anchor syntax; the checker resolves every such anchor in README.md. Test IDs are written with underscores. A cell never contains a vertical bar. The `## Pull requests` rows use the same forms, and the checker validates them but does not count them.

## Bug-fixed-by-plan

Count: 12

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #79 | rclone-browser should not prompt for the config password if RCLONE_PASSWORD_COMMAND is set | 2020-02-20 | fixed-by 3.3 | 3.3, 5.2 | fixed in 3.3 (T_E2E_CFGPASS_COMMAND) |  |
| #107 | Can't access folder if folder name ends with space | 2020-04-23 | fixed-by 2.2 | 2.2 | fixed in 2.2 (T_E2E_LIST_NAMES) |  |
| #117 | rclone runs in background and do transferring jobs even after canceling running job | 2020-05-07 | fixed-by 1.4 | 1.4 (recorded in 3.3) | fixed in 1.4 (T_E2E_JOB_CANCEL) |  |
| #130 | [ ] ( ) and others is not supported | 2020-08-04 | fixed-by 2.2 | 2.2 | fixed in 2.2 (T_E2E_LIST_NAMES) |  |
| #140 | [Bugs x2] Not detecting rclone v1.53.1 manual update, not listing Google Docs in file list view | 2020-10-07 | fixed-by 2.2 | 2.2 | fixed in 2.2 (T_E2E_LIST_UNKNOWNSIZE); the test covers the Google Docs listing half of the report only |  |
| #141 | Error field is always empty | 2020-10-09 | fixed-by 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_ERRORS) |  |
| #142 | Size, Bandwidth and Remaining Time always empty when using --log-file option | 2020-10-29 | fixed-by 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_LOGFILE); the test checks the size field only |  |
| #160 | [macOS] Filename with forward slashes not parsed in running output | 2021-02-07 | fixed-by 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_STATS, T_LOG_COPY); the parser keeps names with ':' and '/', the label text itself is not asserted |  |
| #162 | lockFile is global for all users | 2021-04-13 | fixed-by 3.1 | 3.1 | fixed in 3.1 (T_LOCK_NAME) |  |
| #163 | RcloneBrowser - "Config" button not working on Ubuntu 18.04 & 20.04 | 2021-04-16 | fixed-by 3.1 | 3.1 | fixed in 3.1 (T_TERM_BUILD, T_E2E_TERMINAL_FAKE); the native Config dialog of 6.2 (T_RCD_FLOW) replaces the terminal when the backend runs |  |
| #172 | Clicking config opens a empty terminal | 2021-07-04 | fixed-by 3.1 | 3.1 | fixed in 3.1 (T_TERM_BUILD, T_E2E_TERMINAL_FAKE); the native Config dialog of 6.2 (T_RCD_FLOW) replaces the terminal when the backend runs |  |
| #239 | Configure doesn't work | 2024-10-06 | fixed-by 6.2 | 6.2 | fixed in 6.2 (T_RCD_FLOW, T_E2E_CONFIG_SFTP); Config opens the native dialog while the backend runs; the WinFsp case of the report was not reproduced and the e2e test does not run on Windows |  |

## Bug-verify

Count: 34

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #77 | Export's output file encoded in operating system encoding but not UTF-8 | 2020-02-18 | verify-in 2.2 | 2.2 | fixed in 2.2 (T_E2E_EXPORT); the test reads the exported file back as UTF-8 |  |
| #83 | Status overlaps the breadcrumb | 2020-02-29 | needs-user | — | not reproduced (T_JW_OVERLAP); the job widget has no breadcrumb, the test checks the status label and Show output against the info fields |  |
| #84 | Notification title and icon | 2020-02-29 | needs-user | — | deferred: the finished-transfer notification is titled "Rclone Browser - transfer <status>" and the Windows path passes the program icon; not covered by a test and not checked on macOS or Linux |  |
| #104 | Config not working (v1.8.0) | 2020-04-11 | verify-in 6.2 | 6.2 | fixed in 3.1 (T_TERM_BUILD, T_E2E_TERMINAL_FAKE); the Plan unit column says 6.2, whose native Config dialog (T_RCD_FLOW) replaces the terminal when the backend runs |  |
| #115 | Glitched tooltips | 2020-05-02 | needs-user | — | deferred: a tooltip animation glitch on Windows with no repro steps; no test can show it |  |
| #119 | FTP , 500 MLSD command not understood. | 2020-05-11 | needs-user | — | answered (README#faq-ftp-mlsd) |  |
| #120 | rclone Browser sometimes crashes after heavy use | 2020-06-04 | needs-user | — | deferred: a crash after a day of heavy mount use with no reproducer; no crash fix is claimed |  |
| #124 | Google drive root not deleting files as there were IO errors | 2020-06-05 | needs-user | — | deferred: rclone reported a missing source directory and IO errors on Google Drive, which points at the task or the backend; no repro here |  |
| #128 | Cannot retrieve directory listing via FTP mount | 2020-07-27 | needs-user | — | deferred: FTP server behaviour; the reporter got a listing with a path starting with `/` (as did the one of #119), see README#faq-ftp-mlsd for an alias remote and the disable_mlsd option |  |
| #136 | Not remembering Extra Option | 2020-09-18 | needs-user | — | answered (README#faq-remember-options) |  |
| #145 | Fail to configure RcloneBrowser | 2020-11-04 | verify-in 6.2 | 6.2 | fixed in 14.4 (T_START_RCLONEPATH); see README#faq-rclone-path |  |
| #146 | Mount remotes automatically on launch | 2020-11-06 | verify-in P3 | P3 | answered (README#faq-auto-mount) |  |
| #150 | Task is not saved | 2020-11-26 | needs-user | — | deferred: the report is a Save task button that does nothing; saving and reloading tasks is exercised by T_TASK_DUP but the button itself has no UI test, so no fix is claimed |  |
| #156 | Minimum width is forced to 1232px | 2020-12-30 | needs-user | — | fixed in 14.4 (T_START_MINWIDTH); see README#faq-narrow-window |  |
| #158 | Mount dropped easily | 2021-01-27 | needs-user | — | deferred: a mount that drops is rclone or network behaviour and an automatic remount is not implemented; no repro without a real remote (I-9) |  |
| #159 | When I open the software, it will be closed automatically and forced to exit | 2021-02-07 | needs-user | — | fixed in 14.4 (T_E2E_START_BADRCLONE); see README#faq-rclone-path |  |
| #167 | Google Photos lists only empty folders | 2021-05-07 | verify-in 2.2 | 2.2 | deferred: Google Photos listing is rclone backend behaviour and works on the command line of the reporter; no local repro |  |
| #169 | Job Widget UI issues when sending multiple small files | 2021-05-31 | verify-in 1.4 | 1.4 | fixed in 14.1 (T_JW_STABLE) |  |
| #171 | Rclone Browser 1.8 moves files not folder on DropBox | 2021-06-14 | needs-user | — | fixed in 14.3 (T_E2E_OPS_MOVEFOLDER, T_PROTO_MOVEFOLDER); see README#faq-folder-move |  |
| #174 | Various fields are empty in Rclone v1.56.0 | 2021-07-21 | verify-in 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_STATS) |  |
| #179 | The exclude option is never being applied to transfers | 2021-10-05 | needs-user | — | fixed in 14.2 (T_E2E_EXCLUDE); an --exclude typed into Extra options together with an Include text stays rclone's own rule, see README#faq-include-exclude-order |  |
| #182 | Problems with remaining time and bandwidth fields while downloading | 2021-11-01 | verify-in 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_STATS); the test checks the size and bandwidth fields, not the remaining time |  |
| #190 | Error when run git clone in mount directory | 2021-11-28 | needs-user | — | answered (README#faq-git-in-mount) |  |
| #198 | Various fields are empty in Rclone v1.57.0 | 2022-02-03 | verify-in 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_STATS) |  |
| #201 | Tasks and jobs tab are Grayed out | 2022-02-19 | needs-user | — | not reproduced (T_START_TABS) |  |
| #220 | 'Create folder in' (1) confuses, (2) hangs. | 2023-02-23 | needs-user | — | fixed in 14.3 (T_OPS_MKDIRARGS); the Cancel button and the 60 s timeout have no automated test, see README#faq-new-folder |  |
| #221 | Can No Longer Mount | 2023-04-08 | needs-user | — | deferred: the report has no rclone message; mounting needs FUSE or WinFsp, which the Docker test setup does not have, so only the mount arguments are pinned (T_MOUNT_ARGV) |  |
| #226 | Command copied to clipboard not working | 2023-08-11 | needs-user | — | fixed in 14.1 (T_CMD_ISSUE226); the command preview of the Transfer dialog puts double quotes around arguments with a space and escapes nothing (display only, see README#faq-limit-command-preview) |  |
| #228 | Small visual bug | 2023-09-05 | needs-user | — | fixed in 14.4 (T_START_MINWIDTH); see README#faq-narrow-window |  |
| #231 | RcloneBrowser no longer seems compatible with Rclone 1.64.2 | 2023-10-29 | verify-in 1.4 | 1.4 | fixed in 2.2 (T_E2E_REMOTES); the remote list comes from rclone listremotes --json, which needs rclone 1.68 or newer; the fork requires rclone 1.75.0, so 1.64.2 itself gets the unsupported-version dialog |  |
| #235 | Taking longer to complete the task | 2024-06-30 | needs-user | — | answered (README#faq-100-percent) |  |
| #238 | Duplicates on Google Drive are silently both deleted | 2024-10-02 | needs-user | — | answered (README#faq-drive-duplicates) |  |
| #250 | how can mounts be autostarted | 2026-04-19 | verify-in P3 | P3 | answered (README#faq-auto-mount) |  |
| #251 | Can't View Sub Folders | 2026-06-08 | verify-in 2.2 | 2.2 | deferred: the report names no rclone error and Mega needs a real account (I-9); listing of folders is exercised on local folders only (T_E2E_LIST_NAMES) |  |

## Feature-in-plan

Count: 15

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #16 | Manually enter path to browse | 2019-10-01 | in-plan 10.1 | 10.1 | planned in 10.1 |  |
| #21 | Requests: Sorting Task list and Search box | 2019-10-20 | in-plan 10.2 | 10.2 | planned in 10.2; the search filter is planned there, sorting stays as it is |  |
| #28 | Move files and folders on remotes using drag and drop | 2019-10-27 | in-plan 11.1 | 11.1 | planned in 11.1 |  |
| #47 | Suggestion: Multilanguage | 2019-12-01 | in-plan 12.1 | 12.1 | planned in 12.1 |  |
| #71 | dual pane interface | 2020-01-31 | in-plan 11.2 | 11.2 | planned in 11.2 |  |
| #94 | Keyboard shortcuts | 2020-03-18 | in-plan 10.3 | 10.3 | planned in 10.3 |  |
| #112 | ETA field in transfers | 2020-04-28 | in-plan 1.4 | 1.4 | deferred: 1.4 shows the remaining duration and the Started and Finished times, not the clock time of the expected finish (README#faq-limit-eta) |  |
| #129 | [Feature request] GUI context menu options for rclone "crypt"/"check"/"serve"? | 2020-07-31 | in-plan 9.1 | 9.1 | planned in 9.1 |  |
| #134 | Option to log errors to file | 2020-09-07 | in-plan 1.4 | 1.4 | fixed in 1.4 (T_E2E_JOB_LOGFILE); the output goes to the --log-file; that the file is written while the job runs, and that it holds error lines, is code reading and not asserted by a test |  |
| #138 | Really wish RcloneBrowser could support multi language, I can help translate. | 2020-09-21 | in-plan 12.1 | 12.1 | planned in 12.1 |  |
| #148 | Tap to edit path | 2020-11-20 | in-plan 10.1 | 10.1 | planned in 10.1 |  |
| #195 | Check for updates | 2022-01-06 | in-plan 3.2 | 3.2 | deferred: background update checks exist (T_E2E_UPD_NEWER), switched in Preferences and run at most once a day; an on-demand menu entry is not offered |  |
| #199 | Need Support Remote Upload For Url | 2022-02-07 | in-plan 8.2 | 8.2 | fixed in 8.2 (T_ACT_URL, T_ACT_COPYURL, T_E2E_COPYURL); the action runs rclone copyurl, named from the address path (after redirects) or by the name typed, see README#faq-upload-from-url and README#faq-limit-url-name |  |
| #233 | Generate error log in real time? | 2024-03-14 | in-plan 1.4 | 1.4 | fixed in 1.4 (T_JOB_RENDER_ERR, T_E2E_JOB_LOGFILE); errors and their file show in the job panel's Errors tooltip and the output goes to the --log-file; that the file is written while the job runs, and that it holds error lines, is code reading and not asserted by a test |  |
| #243 | Support for bisync ? | 2025-03-05 | in-plan 8.1 | 8.1 | fixed in 8.1 (T_E2E_BISYNC_RESYNC, T_E2E_BISYNC_UPDATE, T_JO_BISYNC_ARGS); see README#faq-bisync and README#faq-limit-bisync |  |

## Feature-deferred

Count: 36

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #19 | Features request - Multiple Config Files and Remotes arrangement | 2019-10-05 | deferred | — | deferred: Rclone Browser works with one rclone.conf at a time (set in Preferences); several config files and grouped remotes are a larger feature, not planned in this phase |  |
| #42 | Select multiple files in file upload/download mode | 2019-11-25 | deferred | — | deferred: the remote tab's file list allows selecting several items and keeps Copy, Move, Download and Delete enabled for them, and the Transfer dialog's "Choose items to upload" takes several items through --filter rules (code reading only, no test, so not claimed as done) |  |
| #52 | Extended mount functionality | 2019-12-05 | needs-user | — | deferred: the Refresh cache button (T_E2E_VFS_REFRESH) and the vfs/refresh recipe cover the directory cache part, see README#faq-mount-prewarm; a tree-style output like rclone tree is not offered |  |
| #60 | Wishlist: System Tray icon, silent startup/auto-mount remotes | 2019-12-29 | needs-user | — | answered (README#faq-start-in-tray) |  |
| #61 | Sound notification on MacOS | 2019-12-29 | deferred | — | deferred: Preferences has "Play sound when transfer finishes" (only in builds with Qt Multimedia); not covered by a test and not checked on macOS |  |
| #62 | cryptcheck | 2020-01-01 | deferred | — | deferred: the Check dialog of a remote already has a Cryptcheck option (code reading only, no test); a cryptcheck run automatically after every transfer is not offered |  |
| #73 | Task Scheduling | 2020-02-12 | needs-user | — | answered (README#faq-scheduler) |  |
| #76 | Remotes: Change List view to Tiles to fit in more clouds | 2020-02-17 | deferred | — | deferred: Preferences has a "Tiles" layout for the remote list (code reading only, no test, so not claimed as done) |  |
| #80 | Multiselect to run multiple tasks | 2020-02-21 | deferred | — | not reproduced (T_TASK_MULTI); Run already starts all selected tasks, for one after the other see README#faq-run-tasks-one-by-one |  |
| #87 | Feature request: allow editing of source & destination for saved tasks | 2020-03-04 | deferred | — | deferred: the Edit dialog of a saved task shows the source and destination text fields and nothing in the code makes them read-only (code reading only, no test, so not claimed as done) |  |
| #98 | Allow multi-selection on the tasks tab | 2020-03-28 | deferred | — | not reproduced (T_TASK_MULTI); see README#faq-run-tasks-one-by-one |  |
| #108 | a bigger "show output" ? | 2020-04-25 | needs-user | — | deferred: a layout wish for a larger Show output area; not planned in this phase and not testable with the layout tests |  |
| #109 | Feature Request: Queue Jobs (rather than run concurrently) | 2020-04-26 | deferred | — | answered (README#faq-run-tasks-one-by-one) |  |
| #111 | Global and/or session (up/down) counter statistics on a status bar | 2020-04-28 | deferred | — | deferred: there are no application-wide transfer counters and none are planned; each job shows its own totals |  |
| #116 | Global upload limit | 2020-05-03 | deferred | — | deferred: rclone --bwlimit is enforced per rclone process and every job is its own process, so there is no app-wide limit (README#faq-bandwidth-limit) |  |
| #118 | Feature Request: Restart Jobs | 2020-05-07 | deferred | — | fixed in 14.2 (T_E2E_JOB_RESTART); see README#faq-restart-job |  |
| #121 | Multiselect for files/folders within drives | 2020-06-05 | deferred | — | deferred: the remote tab's file list allows selecting several items and keeps Copy, Move, Download and Delete enabled for them, and the Transfer dialog's "Choose items to upload" takes several items through --filter rules (code reading only, no test, so not claimed as done) |  |
| #122 | Sort by recent in google drive. | 2020-06-05 | deferred | — | deferred: sorting a Google Drive listing by recent access is not planned; the search filter of #21 does not cover it |  |
| #123 | Option to show commands used for tasks | 2020-06-05 | deferred | — | fixed in 14.1 (T_CMD_TASKCOPY, T_E2E_CMD_ROUNDTRIP); the Tasks tab "Copy command" gives the command of a saved task and a job's copy button that of a running job, see README#faq-copy-command |  |
| #132 | Listing (cache) pre warming in mounts | 2020-08-10 | deferred | — | answered (README#faq-mount-prewarm) |  |
| #139 | Tasks - some further ideas | 2020-09-30 | needs-user | — | fixed in 14.2 (T_TASK_DUP); only cloning is done, the other ideas of the issue are not implemented, see README#faq-duplicate-task |  |
| #143 | Request: Convert the tasks.bin file into xml, text or other editable format or give more options for working with tasks. | 2020-10-29 | deferred | — | deferred: tasks.bin stays a binary file; a text format for tasks is not implemented, Duplicate and Copy command cover part of the need (README#faq-copy-command) |  |
| #144 | Request: "Are you sure you want to run task" confirmation before clicking Run button | 2020-10-30 | deferred | — | fixed in 14.2 (T_TASK_CONFIRM); the question existed, 14.2 made it an option (README#faq-ask-before-run) |  |
| #176 | 'Copy command to clipboard ' button in Tasks view | 2021-09-06 | needs-user | — | fixed in 14.1 (T_CMD_TASKCOPY); there is no Export all tasks button, select all tasks and use Copy command, see README#faq-copy-command |  |
| #177 | Feature Request: Scheduled Tasks | 2021-09-18 | needs-user | — | answered (README#faq-scheduler) |  |
| #178 | Hidden Remotes | 2021-10-02 | deferred | — | deferred: no setting hides individual remotes; not planned |  |
| #181 | Command Line Interface? | 2021-10-30 | deferred | — | deferred: the program takes no command line arguments, so a task cannot be started from a script; not planned |  |
| #188 | not an issue - feature suggestion | 2021-11-22 | needs-user | — | deferred: the remote tab's file list allows selecting several items and keeps Copy, Move, Download and Delete enabled for them, and the Transfer dialog's "Choose items to upload" takes several items through --filter rules (code reading only, no test, so not claimed as done) |  |
| #192 | [Feature Request] 'Copy' Button in context menu and top bar | 2021-12-12 | needs-user | — | answered (README#faq-copy-action); see also README#faq-copy-to |  |
| #194 | Feature Request: Copy/Export Mount Command | 2021-12-29 | deferred | — | fixed in 14.1 (T_CMD_MOUNTCOPY); see README#faq-copy-command |  |
| #200 | Scheduled Tasks | 2022-02-10 | needs-user | — | answered (README#faq-scheduler) |  |
| #204 | Queue manager | 2022-03-29 | deferred | — | deferred: the Queue tab has Start, Stop, Up, Down, Remove and Purge for the order of jobs (code reading only, no test), but the order of files inside a job cannot be changed |  |
| #214 | Make running executable again open the original instance | 2022-08-28 | deferred | — | deferred: a second start still shows "Rclone Browser is already running"; bringing the running window to the front is not implemented |  |
| #215 | Feature Request: remember last used mountpoint | 2022-09-10 | deferred | — | answered (README#faq-remember-options) |  |
| #240 | Rclone Browser into ARM architecture | 2024-12-08 | deferred | — | answered (README#faq-release-assets) |  |
| #249 | Can RcloneBrowser be autoupdated? | 2026-03-29 | deferred | — | deferred: update checks exist (T_E2E_UPD_NEWER) but only report that a newer version exists; installing an update by itself is not implemented |  |

## Packaging/platform

Count: 13

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #161 | [macOS] Build for Apple Silicon / Universal Binary | 2021-03-08 | fixed-by fork PR #1 | — | answered (README#faq-release-assets) |  |
| #168 | On Linux Mint, Appimage starts and Immediately exits | 2021-05-10 | needs-user | — | deferred: the AppImage exits at start on Linux Mint; the report has only a screenshot of the terminal output and no repro environment is available here |  |
| #175 | How to launch terminal in BSD | 2021-09-02 | needs-user | — | fixed in 14.4 (T_START_TERMINAL); see README#faq-terminal-command |  |
| #186 | If I change the main RB app icon that appears in the Dock when the app is closed, the app icon reverts to the original one when RB is opened. | 2021-11-14 | needs-user | — | deferred: macOS Dock icon behaviour cannot be verified here (no macOS test); not planned |  |
| #193 | Compile the test2 | 2021-12-24 | needs-user | — | obsolete; a request to build a test branch of the old upstream project |  |
| #207 | Issue on compile | 2022-04-16 | needs-user | — | obsolete; a build error from deprecated Qt 5 calls on the pre-Qt 6 code, which the fork no longer has |  |
| #209 | MacOS rclone location error | 2022-05-22 | fixed-by fork 0a57f4f | — | fixed in 14.4 (T_START_RCLONEPATH); see README#faq-rclone-path |  |
| #219 | make error | 2023-02-09 | needs-user | — | answered (README#faq-build-anaconda) |  |
| #222 | Portable version? | 2023-05-09 | needs-user | — | answered (README#faq-portable-mode) |  |
| #242 | Brew intall stopped cold on "could not create work tree dir 'RcloneBrowser': Read-only file system" | 2025-01-02 | needs-user | — | obsolete; the reporter solved it with the Cask install and the fork publishes a macOS DMG |  |
| #244 | [linux] QT6 + wayland | 2025-04-08 | needs-user | — | answered (README#faq-wayland) |  |
| #247 | RCloneBrowser Compile for FreeBSD Core(No Desktop) | 2025-07-17 | needs-user | — | answered (README#faq-headless) |  |
| #253 | [Announcement] Rclone Browser Next for macOS: Apple Silicon, redesigned UI and current rclone support | 2026-10-20 | needs-user | — | no action (announcement) |  |

## Question/support

Count: 15

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #93 | Next release - beta testing | 2020-03-17 | needs-user | — | obsolete; the beta-testing thread of an old upstream release |  |
| #133 | --fast-list | 2020-08-27 | needs-user | — | answered (README#faq-fast-list) |  |
| #153 | How to get outside $HOME using SFTP remote | 2020-12-18 | needs-user | — | answered (README#faq-sftp-outside-home) |  |
| #154 | What is the difference between Dry Run and Run? | 2020-12-22 | deferred | — | answered (README#faq-dry-run) |  |
| #157 | Previous issues | 2021-01-06 | needs-user | — | deferred: a list of suggestions with no reproducer; the path bar idea is planned in 10.1 (#16) |  |
| #180 | Mount multiple remotes | 2021-10-18 | needs-user | — | answered (README#faq-several-mounts) |  |
| #185 | setting up configs on one of two mounts of same remote | 2021-11-11 | needs-user | — | answered (README#faq-several-mounts) |  |
| #203 | Has this been abandoned? Any good forks? | 2022-03-20 | deferred | — | answered (README#faq-maintained) |  |
| #206 | possible to hide unencrypted remote source ? | 2022-04-14 | needs-user | — | deferred: no setting hides a remote; not planned (the remote's settings stay readable in rclone.conf in any case) |  |
| #208 | is it possible to start rclonebrowser in tray? | 2022-05-14 | deferred | — | answered (README#faq-start-in-tray) |  |
| #210 | setting as a network drive on windows // and mounting to any unused drive letter | 2022-05-26 | needs-user | — | answered (README#faq-windows-network-drive) |  |
| #211 | How to retrieve folder listings from local storage? | 2022-07-12 | needs-user | — | deferred: listings live in memory only, so there is no cache file to read; Export saves a listing but needs the remote to be reachable |  |
| #213 | Rclone configuration on NAS | 2022-08-21 | needs-user | — | answered (README#faq-headless) |  |
| #218 | Possible to quit without “job in progress” alert? Or is force quit safe? | 2023-01-08 | needs-user | — | deferred: closing with jobs running asks "Do you want to stop them and quit?" and has no setting to skip it, while Close to system tray keeps the jobs running (code reading only, no test) |  |
| #252 | Exclude files from Sync | 2026-06-10 | needs-user | — | answered (README#faq-exclude-patterns) |  |

## Obsolete

Count: 4

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #125 | OSX make failing due to deprecated declarations? | 2020-06-25 | wontfix-obsolete | — | obsolete; a build error from deprecated Qt 5 declarations on macOS, code base moved to Qt 6 |  |
| #127 | Build fail on Fedora32: requires "qt5-qtbase-devel" | 2020-07-27 | wontfix-obsolete | — | obsolete; a build dependency of the Qt 5 code base, the fork builds with Qt 6 |  |
| #223 | Build Errors on Ubuntu from Debrecations | 2023-05-16 | wontfix-obsolete | — | obsolete; a build error from deprecated Qt 5 calls, code base moved to Qt 6 |  |
| #248 | void QProcess::start(const QString&, QIODevice::OpenMode)’ is deprecated | 2026-03-14 | wontfix-obsolete | — | obsolete; a build error from deprecated Qt 5 calls, code base moved to Qt 6 |  |

## Pull requests

Count: 10

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #126 | Replaced Qt 5.15 deprecated method calls | 2020-07-09 | wontfix-obsolete | — | obsolete; the fork builds with Qt 6 and no longer uses the Qt 5.15 calls |  |
| #152 | Add architecture ppc64le to travis build | 2020-12-10 | wontfix-obsolete | — | obsolete; Travis CI is removed, GitHub Actions builds every target |  |
| #164 | Mac brew install option | 2021-04-21 | in-plan 4.2 | 4.2 | answered (README#faq-release-assets); the README documents Homebrew for rclone and a DMG for the app |  |
| #166 | added a second row | 2021-05-06 | deferred | — | deferred: the snapshot has only the title "added a second row" and no description; not evaluated, no test |  |
| #173 | Use separate lockfile for each user. | 2021-07-14 | in-plan 3.1 | 3.1 | fixed in 3.1 (T_LOCK_NAME); the fork made the Linux lock file per user itself (issue #162) |  |
| #205 | Fix typo | 2022-04-13 | in-plan 3.1 | 3.1 | obsolete; the plan maps it to 3.1, which fixed the appdata summary typo; the snapshot has no description, so the identity of the typo is the plan's mapping |  |
| #212 | Update size-transferred regex for progress in Jobs tab | 2022-07-14 | superseded-by 1.1 | 1.1 | obsolete; superseded by the JSON log parsing of 1.1 (T_LOG_VARIANTS) |  |
| #225 | Update `job_widget.cpp` to support rclone 1.56 output changes | 2023-07-17 | superseded-by 1.1 | 1.1 | obsolete; superseded by the JSON log parsing of 1.1 (T_LOG_VARIANTS) |  |
| #232 | fix: Homebrew install script in README.md results in 404 | 2024-03-11 | in-plan 4.2 | 4.2 | obsolete; the old Homebrew install script URL returns 404 (checked 2026-10-22) and 14.5 changed the macOS build step to Homebrew's current install.sh command |  |
| #246 | fix regexes for bw, eta, etc | 2025-05-23 | superseded-by 1.1 | 1.1 | obsolete; superseded by the JSON log parsing of 1.1 (T_LOG_VARIANTS) |  |
