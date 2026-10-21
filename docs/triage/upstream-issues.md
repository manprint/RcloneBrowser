# Upstream issue and PR triage

Source snapshot: open issues and open pull requests of kapitainsky/RcloneBrowser as of 2026-10-21 (129 issues, 10 PRs). Nothing is posted upstream: no comment, label, close or pull request is made on kapitainsky/RcloneBrowser. Dispositions refer to units of plan 001 in `docs/plans/001_plan-RcloneBrowserModernization/`. The user fills the `User OK` column for each row; it stays empty until then.

## Bug-fixed-by-plan

Count: 12

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #79 | rclone-browser should not prompt for the config password if RCLONE_PASSWORD_COMMAND is set | 2020-02-20 | fixed-by 3.3 | 3.3, 5.2 | fixed in 3.3 |  |
| #107 | Can't access folder if folder name ends with space | 2020-04-23 | fixed-by 2.2 | 2.2 | fixed in P2 (T-E2E-LIST-NAMES/UNKNOWNSIZE) |  |
| #117 | rclone runs in background and do transferring jobs even after canceling running job | 2020-05-07 | fixed-by 1.4 | 1.4 (recorded in 3.3) | fixed in P1 (T-E2E-JOB-CANCEL) |  |
| #130 | [ ] ( ) and others is not supported | 2020-08-04 | fixed-by 2.2 | 2.2 | fixed in P2 (T-E2E-LIST-NAMES/UNKNOWNSIZE) |  |
| #140 | [Bugs x2] Not detecting rclone v1.53.1 manual update, not listing Google Docs in file list view | 2020-10-07 | fixed-by 2.2 | 2.2 | fixed in P2 (T-E2E-LIST-NAMES/UNKNOWNSIZE) |  |
| #141 | Error field is always empty | 2020-10-09 | fixed-by 1.4 | 1.4 | fixed in P1 |  |
| #142 | Size, Bandwidth and Remaining Time always empty when using --log-file option | 2020-10-29 | fixed-by 1.4 | 1.4 | fixed in P1 |  |
| #160 | [macOS] Filename with forward slashes not parsed in running output | 2021-02-07 | fixed-by 1.4 | 1.4 | fixed in P1 |  |
| #162 | lockFile is global for all users | 2021-04-13 | fixed-by 3.1 | 3.1 | fixed in 3.1 |  |
| #163 | RcloneBrowser - "Config" button not working on Ubuntu 18.04 & 20.04 | 2021-04-16 | fixed-by 3.1 | 3.1 | fixed in 3.1 (terminal); native config in P6 |  |
| #172 | Clicking config opens a empty terminal | 2021-07-04 | fixed-by 3.1 | 3.1 | fixed in 3.1 (terminal); native config in P6 |  |
| #239 | Configure doesn't work | 2024-10-06 | fixed-by 6.2 | 6.2 | fixed in 3.1 (terminal); native config in P6 |  |

## Bug-verify

Count: 34

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #77 | Export's output file encoded in operating system encoding but not UTF-8 | 2020-02-18 | verify-in 2.2 | 2.2 |  |  |
| #83 | Status overlaps the breadcrumb | 2020-02-29 | needs-user | — |  |  |
| #84 | Notification title and icon | 2020-02-29 | needs-user | — |  |  |
| #104 | Config not working (v1.8.0) | 2020-04-11 | verify-in 6.2 | 6.2 |  |  |
| #115 | Glitched tooltips | 2020-05-02 | needs-user | — |  |  |
| #119 | FTP , 500 MLSD command not understood. | 2020-05-11 | needs-user | — |  |  |
| #120 | rclone Browser sometimes crashes after heavy use | 2020-06-04 | needs-user | — |  |  |
| #124 | Google drive root not deleting files as there were IO errors | 2020-06-05 | needs-user | — |  |  |
| #128 | Cannot retrieve directory listing via FTP mount | 2020-07-27 | needs-user | — |  |  |
| #136 | Not remembering Extra Option | 2020-09-18 | needs-user | — |  |  |
| #145 | Fail to configure RcloneBrowser | 2020-11-04 | verify-in 6.2 | 6.2 |  |  |
| #146 | Mount remotes automatically on launch | 2020-11-06 | verify-in P3 | P3 | already implemented: mount task option "Run saved task every time Rclone Browser starts (auto start)" (cb_taskAutostart → JobOptions::mountAutoStart, started by MainWindow::autoStartMounts) |  |
| #150 | Task is not saved | 2020-11-26 | needs-user | — |  |  |
| #156 | Minimum width is forced to 1232px | 2020-12-30 | needs-user | — |  |  |
| #158 | Mount dropped easily | 2021-01-27 | needs-user | — |  |  |
| #159 | When I open the software, it will be closed automatically and forced to exit | 2021-02-07 | needs-user | — |  |  |
| #167 | Google Photos lists only empty folders | 2021-05-07 | verify-in 2.2 | 2.2 |  |  |
| #169 | Job Widget UI issues when sending multiple small files | 2021-05-31 | verify-in 1.4 | 1.4 |  |  |
| #171 | Rclone Browser 1.8 moves files not folder on DropBox | 2021-06-14 | needs-user | — |  |  |
| #174 | Various fields are empty in Rclone v1.56.0 | 2021-07-21 | verify-in 1.4 | 1.4 |  |  |
| #179 | The exclude option is never being applied to transfers | 2021-10-05 | needs-user | — |  |  |
| #182 | Problems with remaining time and bandwidth fields while downloading | 2021-11-01 | verify-in 1.4 | 1.4 |  |  |
| #190 | Error when run git clone in mount directory | 2021-11-28 | needs-user | — |  |  |
| #198 | Various fields are empty in Rclone v1.57.0 | 2022-02-03 | verify-in 1.4 | 1.4 |  |  |
| #201 | Tasks and jobs tab are Grayed out | 2022-02-19 | needs-user | — |  |  |
| #220 | 'Create folder in' (1) confuses, (2) hangs. | 2023-02-23 | needs-user | — |  |  |
| #221 | Can No Longer Mount | 2023-04-08 | needs-user | — |  |  |
| #226 | Command copied to clipboard not working | 2023-08-11 | needs-user | — |  |  |
| #228 | Small visual bug | 2023-09-05 | needs-user | — |  |  |
| #231 | RcloneBrowser no longer seems compatible with Rclone 1.64.2 | 2023-10-29 | verify-in 1.4 | 1.4 |  |  |
| #235 | Taking longer to complete the task | 2024-06-30 | needs-user | — |  |  |
| #238 | Duplicates on Google Drive are silently both deleted | 2024-10-02 | needs-user | — |  |  |
| #250 | how can mounts be autostarted | 2026-04-19 | verify-in P3 | P3 | already implemented: mount task option "Run saved task every time Rclone Browser starts (auto start)" (cb_taskAutostart → JobOptions::mountAutoStart, started by MainWindow::autoStartMounts) |  |
| #251 | Can't View Sub Folders | 2026-06-08 | verify-in 2.2 | 2.2 |  |  |

## Feature-in-plan

Count: 15

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #16 | Manually enter path to browse | 2019-10-01 | in-plan 10.1 | 10.1 |  |  |
| #21 | Requests: Sorting Task list and Search box | 2019-10-20 | in-plan 10.2 | 10.2 |  |  |
| #28 | Move files and folders on remotes using drag and drop | 2019-10-27 | in-plan 11.1 | 11.1 |  |  |
| #47 | Suggestion: Multilanguage | 2019-12-01 | in-plan 12.1 | 12.1 |  |  |
| #71 | dual pane interface | 2020-01-31 | in-plan 11.2 | 11.2 |  |  |
| #94 | Keyboard shortcuts | 2020-03-18 | in-plan 10.3 | 10.3 |  |  |
| #112 | ETA field in transfers | 2020-04-28 | in-plan 1.4 | 1.4 |  |  |
| #129 | [Feature request] GUI context menu options for rclone "crypt"/"check"/"serve"? | 2020-07-31 | in-plan 9.1 | 9.1 |  |  |
| #134 | Option to log errors to file | 2020-09-07 | in-plan 1.4 | 1.4 |  |  |
| #138 | Really wish RcloneBrowser could support multi language, I can help translate. | 2020-09-21 | in-plan 12.1 | 12.1 |  |  |
| #148 | Tap to edit path | 2020-11-20 | in-plan 10.1 | 10.1 |  |  |
| #195 | Check for updates | 2022-01-06 | in-plan 3.2 | 3.2 |  |  |
| #199 | Need Support Remote Upload For Url | 2022-02-07 | in-plan 8.2 | 8.2 |  |  |
| #233 | Generate error log in real time? | 2024-03-14 | in-plan 1.4 | 1.4 |  |  |
| #243 | Support for bisync ? | 2025-03-05 | in-plan 8.1 | 8.1 |  |  |

## Feature-deferred

Count: 36

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #19 | Features request - Multiple Config Files and Remotes arrangement | 2019-10-05 | deferred | — |  |  |
| #42 | Select multiple files in file upload/download mode | 2019-11-25 | deferred | — |  |  |
| #52 | Extended mount functionality | 2019-12-05 | needs-user | — |  |  |
| #60 | Wishlist: System Tray icon, silent startup/auto-mount remotes | 2019-12-29 | needs-user | — |  |  |
| #61 | Sound notification on MacOS | 2019-12-29 | deferred | — |  |  |
| #62 | cryptcheck | 2020-01-01 | deferred | — |  |  |
| #73 | Task Scheduling | 2020-02-12 | needs-user | — |  |  |
| #76 | Remotes: Change List view to Tiles to fit in more clouds | 2020-02-17 | deferred | — |  |  |
| #80 | Multiselect to run multiple tasks | 2020-02-21 | deferred | — |  |  |
| #87 | Feature request: allow editing of source & destination for saved tasks | 2020-03-04 | deferred | — |  |  |
| #98 | Allow multi-selection on the tasks tab | 2020-03-28 | deferred | — |  |  |
| #108 | a bigger "show output" ? | 2020-04-25 | needs-user | — |  |  |
| #109 | Feature Request: Queue Jobs (rather than run concurrently) | 2020-04-26 | deferred | — |  |  |
| #111 | Global and/or session (up/down) counter statistics on a status bar | 2020-04-28 | deferred | — |  |  |
| #116 | Global upload limit | 2020-05-03 | deferred | — |  |  |
| #118 | Feature Request: Restart Jobs | 2020-05-07 | deferred | — |  |  |
| #121 | Multiselect for files/folders within drives | 2020-06-05 | deferred | — |  |  |
| #122 | Sort by recent in google drive. | 2020-06-05 | deferred | — |  |  |
| #123 | Option to show commands used for tasks | 2020-06-05 | deferred | — |  |  |
| #132 | Listing (cache) pre warming in mounts | 2020-08-10 | deferred | — |  |  |
| #139 | Tasks - some further ideas | 2020-09-30 | needs-user | — |  |  |
| #143 | Request: Convert the tasks.bin file into xml, text or other editable format or give more options for working with tasks. | 2020-10-29 | deferred | — |  |  |
| #144 | Request: "Are you sure you want to run task" confirmation before clicking Run button | 2020-10-30 | deferred | — |  |  |
| #176 | 'Copy command to clipboard ' button in Tasks view | 2021-09-06 | needs-user | — |  |  |
| #177 | Feature Request: Scheduled Tasks | 2021-09-18 | needs-user | — |  |  |
| #178 | Hidden Remotes | 2021-10-02 | deferred | — |  |  |
| #181 | Command Line Interface? | 2021-10-30 | deferred | — |  |  |
| #188 | not an issue - feature suggestion | 2021-11-22 | needs-user | — |  |  |
| #192 | [Feature Request] 'Copy' Button in context menu and top bar | 2021-12-12 | needs-user | — |  |  |
| #194 | Feature Request: Copy/Export Mount Command | 2021-12-29 | deferred | — |  |  |
| #200 | Scheduled Tasks | 2022-02-10 | needs-user | — |  |  |
| #204 | Queue manager | 2022-03-29 | deferred | — |  |  |
| #214 | Make running executable again open the original instance | 2022-08-28 | deferred | — |  |  |
| #215 | Feature Request: remember last used mountpoint | 2022-09-10 | deferred | — |  |  |
| #240 | Rclone Browser into ARM architecture | 2024-12-08 | deferred | — |  |  |
| #249 | Can RcloneBrowser be autoupdated? | 2026-03-29 | deferred | — |  |  |

## Packaging/platform

Count: 13

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #161 | [macOS] Build for Apple Silicon / Universal Binary | 2021-03-08 | fixed-by fork PR #1 | — |  |  |
| #168 | On Linux Mint, Appimage starts and Immediately exits | 2021-05-10 | needs-user | — |  |  |
| #175 | How to launch terminal in BSD | 2021-09-02 | needs-user | — |  |  |
| #186 | If I change the main RB app icon that appears in the Dock when the app is closed, the app icon reverts to the original one when RB is opened. | 2021-11-14 | needs-user | — |  |  |
| #193 | Compile the test2 | 2021-12-24 | needs-user | — |  |  |
| #207 | Issue on compile | 2022-04-16 | needs-user | — |  |  |
| #209 | MacOS rclone location error | 2022-05-22 | fixed-by fork 0a57f4f | — |  |  |
| #219 | make error | 2023-02-09 | needs-user | — |  |  |
| #222 | Portable version? | 2023-05-09 | needs-user | — |  |  |
| #242 | Brew intall stopped cold on "could not create work tree dir 'RcloneBrowser': Read-only file system" | 2025-01-02 | needs-user | — |  |  |
| #244 | [linux] QT6 + wayland | 2025-04-08 | needs-user | — |  |  |
| #247 | RCloneBrowser Compile for FreeBSD Core(No Desktop) | 2025-07-17 | needs-user | — |  |  |
| #253 | [Announcement] Rclone Browser Next for macOS: Apple Silicon, redesigned UI and current rclone support | 2026-10-20 | needs-user | — |  |  |

## Question/support

Count: 15

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #93 | Next release - beta testing | 2020-03-17 | needs-user | — |  |  |
| #133 | --fast-list | 2020-08-27 | needs-user | — |  |  |
| #153 | How to get outside $HOME using SFTP remote | 2020-12-18 | needs-user | — |  |  |
| #154 | What is the difference between Dry Run and Run? | 2020-12-22 | deferred | — |  |  |
| #157 | Previous issues | 2021-01-06 | needs-user | — |  |  |
| #180 | Mount multiple remotes | 2021-10-18 | needs-user | — |  |  |
| #185 | setting up configs on one of two mounts of same remote | 2021-11-11 | needs-user | — |  |  |
| #203 | Has this been abandoned? Any good forks? | 2022-03-20 | deferred | — |  |  |
| #206 | possible to hide unencrypted remote source ? | 2022-04-14 | needs-user | — |  |  |
| #208 | is it possible to start rclonebrowser in tray? | 2022-05-14 | deferred | — |  |  |
| #210 | setting as a network drive on windows // and mounting to any unused drive letter | 2022-05-26 | needs-user | — |  |  |
| #211 | How to retrieve folder listings from local storage? | 2022-07-12 | needs-user | — |  |  |
| #213 | Rclone configuration on NAS | 2022-08-21 | needs-user | — |  |  |
| #218 | Possible to quit without “job in progress” alert? Or is force quit safe? | 2023-01-08 | needs-user | — |  |  |
| #252 | Exclude files from Sync | 2026-06-10 | needs-user | — |  |  |

## Obsolete

Count: 4

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #125 | OSX make failing due to deprecated declarations? | 2020-06-25 | wontfix-obsolete | — |  |  |
| #127 | Build fail on Fedora32: requires "qt5-qtbase-devel" | 2020-07-27 | wontfix-obsolete | — |  |  |
| #223 | Build Errors on Ubuntu from Debrecations | 2023-05-16 | wontfix-obsolete | — |  |  |
| #248 | void QProcess::start(const QString&, QIODevice::OpenMode)’ is deprecated | 2026-03-14 | wontfix-obsolete | — |  |  |

## Pull requests

Count: 10

| # | Title | Created | Disposition | Plan unit | Fork status | User OK |
|---|---|---|---|---|---|---|
| #126 | Replaced Qt 5.15 deprecated method calls | 2020-07-09 | wontfix-obsolete | — |  |  |
| #152 | Add architecture ppc64le to travis build | 2020-12-10 | wontfix-obsolete | — |  |  |
| #164 | Mac brew install option | 2021-04-21 | in-plan 4.2 | 4.2 |  |  |
| #166 | added a second row | 2021-05-06 | deferred | — |  |  |
| #173 | Use separate lockfile for each user. | 2021-07-14 | in-plan 3.1 | 3.1 |  |  |
| #205 | Fix typo | 2022-04-13 | in-plan 3.1 | 3.1 |  |  |
| #212 | Update size-transferred regex for progress in Jobs tab | 2022-07-14 | superseded-by 1.1 | 1.1 |  |  |
| #225 | Update `job_widget.cpp` to support rclone 1.56 output changes | 2023-07-17 | superseded-by 1.1 | 1.1 |  |  |
| #232 | fix: Homebrew install script in README.md results in 404 | 2024-03-11 | in-plan 4.2 | 4.2 |  |  |
| #246 | fix regexes for bw, eta, etc | 2025-05-23 | superseded-by 1.1 | 1.1 |  |  |
