<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/RcloneBrowserLongLogo1.png" width="80%" />

[![Downloads][img3]][3] [![Release][img4]][4] <img src="https://img.shields.io/badge/Qt-cmake-green.svg"> [![License][img5]][5]

Rclone Browser (manprint fork)
==============================
Maintained fork of [kapitainsky/RcloneBrowser](https://github.com/kapitainsky/RcloneBrowser), modernized for current rclone and Qt 6. Rclone Browser was originally created by [Martins Mozeiko](https://github.com/mmozeiko/RcloneBrowser). Releases of this fork are on the [manprint releases page][3].

Simple cross platform GUI for [rclone](https://rclone.org/) command line tool.

Supports macOS, Windows, GNU/Linux and BSD family.

Requirements
------------
**rclone 1.75.0 or newer.** At startup Rclone Browser runs `rclone version`; with an older rclone (or output it cannot recognize) it shows a dialog that leads to Preferences, the rclone downloads page or Quit, and does not continue. See [rclone version, updates and config password](#rclone-version-updates-and-config-password).

Building from source needs Qt 6.4 or newer, CMake and a C++17 compiler. The Windows, Linux AppImage and macOS release binaries bundle Qt 6.10.3; the FreeBSD build uses the Qt 6 packages installed with `pkg`.

<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/readme/osLogos.png" width="50%" />

Table of contents
-------------------
*   [Requirements](#requirements)
*   [Features](#features)
*   [Fork changes](#fork-changes)
*   [Sample screenshots](#sample-screenshots)
*   [How to get it](#how-to-get-it)
*   [Compatibility](#compatibility)
*   [Why AppImage only for Linux](#why-appimage-only-for-linux)
*   [Build instructions](#build-instructions)
    *   [Linux](#linux)
    *   [FreeBSD](#freebsd)
    *   [OpenBSD](#openbsd)
    *   [NetBSD](#netbsd)
    *   [macOS](#macos)
    *   [Windows](#windows)
*   [Development (Docker)](#development-docker)
*   [Portable vs standard mode](#portable-vs-standard-mode)
*   [Transfers and logging](#transfers-and-logging)
*   [rclone version, updates and config password](#rclone-version-updates-and-config-password)
*   [Backend](#backend)
*   [FAQ and known limitations](#faq-and-known-limitations)
*   [History](#history)

Features
--------
*   Allows to browse and modify any rclone remote, including encrypted ones
*   Uses same configuration file as rclone, no extra configuration required
*   Supports custom location and encryption for rclone.conf configuration file
*   Simultaneously navigate multiple repositories in separate tabs
*   Lists files hierarchically with file name, size and modify date
*   All rclone commands are executed asynchronously, no freezing GUI
*   File hierarchy is lazily cached in memory, for faster traversal of folders
*   Allows to upload, download, create new folders, rename or delete files and folders
*   Allows to calculate size of folder, export list of files and copy rclone command to clipboard
*   Can process multiple upload or download jobs in background
*   Drag & drop support for dragging files from local file explorer for uploading
*   Streaming media files for playback in player like [vlc][6] or similar
*   Mount and unmount folders on macOS, GNU/Linux and Windows (for Windows requires [winfsp](https://winfsp.dev/) and for mac [fuse for macOS](https://osxfuse.github.io/))
*   Optionally minimizes to tray, with notifications when upload/download finishes
*   Supports portable mode (create .ini file next to executable with same name), rclone and rclone.conf path now can be relative to executable
*   Supports drive-shared-with-me (Google Drive specific)
*   For remotes supporting public link sharing has an option (right-click menu) to fetch it
*   Supports tasks. Created jobs can be saved and run or edited later.
*   Configurable dark mode for all systems

Fork changes
------------
Compared with upstream, this fork includes the following. The full list is in [CHANGELOG.md](CHANGELOG.md).

*   Transfers read progress, per-file stats and errors from rclone's JSON log, so they work for any file name; `--log-file` is honoured (see [Transfers and logging](#transfers-and-logging))
*   Folder listings, exports and the remote list use `rclone lsjson` and `rclone listremotes --json`; a failed listing shows rclone's error in a warning row
*   Fixes for empty error and size fields, sizes of Google Docs and other unknown sizes, names with special characters or a trailing space, and cancelled jobs that kept running
*   Remotes are created and edited in a native dialog (provider list, one question per page, browser sign-in for OAuth backends) through the private backend; "Advanced (terminal)…" keeps `rclone config` in a terminal, which on Linux now opens reliably (see [rclone version, updates and config password](#rclone-version-updates-and-config-password))
*   The lock file is per user on Linux, and no password prompt is shown when `RCLONE_PASSWORD_COMMAND` is set
*   A wrong config password asks again, and a failing `RCLONE_PASSWORD_COMMAND` shows rclone's error
*   New build targets: macOS Apple Silicon DMG and Windows ARM64; Docker development environment (`scripts/dev.sh`) and unit and end-to-end tests run in CI
*   New in Preferences: a "Test" button for the selected rclone, a configurable rclone releases repository for update checks, and asynchronous update checks against manprint/RcloneBrowser
*   Nicolaius's customizations (thanks @Nicolaius): remote icon size as a multiplier (0.5 to 20, default 4.0 - the old "M" size), options to hide the Tasks, Queue and Scheduler tabs and the remote icon row, and a player chooser for stream commands that list several players separated by `|`
*   Folder listings go through a private `rclone rcd` backend (with `rclone lsjson` as the fallback), and mounts get their remote-control credentials through the environment instead of the command line, with a new "Refresh cache" button (see [Backend](#backend))
*   From the upstream issue sweep: copied rclone commands of tasks, jobs and mounts quoted for your shell, Duplicate task, an option to skip the question before Run, Restart for ended jobs, ordered include and exclude rules, a folder Move that merges into an existing folder and can remove the emptied source, a Create folder dialog with a working Cancel and a timeout, a "Terminal command" preference, a message with a Preferences button instead of a quit when rclone is missing or fails, and a remote button row that scrolls instead of widening the window (see [FAQ and known limitations](#faq-and-known-limitations))
*   Changed: minimum rclone is 1.75.0; remotes are listed case-sensitively (uppercase first); Travis CI and AppVeyor are removed and GitHub Actions builds every platform

Sample screenshots
-------------------
**macOS**
<p align="center">
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot24.png" width="100%" />
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot23.png" width="75%" />
</p>

**Linux**
<p align="center">
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot21.png" width="65%" />
</p>
&nbsp;
<p align="center">
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot22.png" width="65%" />
</p>

&nbsp;
**Windows**
<p align="center">
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot25.PNG" width="100%" />
</p>
&nbsp;
<p align="center">
<img src="https://github.com/kapitainsky/RcloneBrowser/wiki/images/screenshot26.PNG" width="65%" />
</p>

&nbsp;

How to get it
--------------
Get binaries of this fork on the [manprint releases][3] page. Each release has:

*   `windows.zip` - Windows x64
*   `windows-arm64.zip` - Windows ARM64
*   `rclone-browser-<version>-<commit>-linux-x86_64.AppImage` - Linux x86_64
*   `rclone-browser-<version>-<commit>-macos-arm64.dmg` - macOS on Apple Silicon
*   `freebsd.zip` - FreeBSD (Qt 6 is not bundled; install `qt6-base` and `qt6-multimedia` with `pkg`)

Tags with a suffix such as `v2.0.0-beta.1` are published as pre-releases.

**macOS first launch:** the DMG build is ad-hoc signed and not notarized, so macOS Gatekeeper may refuse to open it. After copying *Rclone Browser.app* from the image to `/Applications`, run `xattr -dr com.apple.quarantine "/Applications/Rclone Browser.app"` once, or right-click the app and choose **Open**. Install rclone with Homebrew (`brew install rclone`); on first launch Rclone Browser finds it at `/opt/homebrew/bin/rclone`.

Distribution packages (ArchLinux AUR, Fedora, FreeBSD ports, the packages listed on repology and the docker image by [@romancin](https://github.com/romancin)) may still ship upstream 1.8 or older, so check the version before you use one. The releases above are the builds of this fork.

More and more operating systems include Rclone Browser in their offical distribution channels. You can check availibility for example [here](https://repology.org/project/rclone-browser/packages) or directly in your distribution repository. If not there yet give a shout to your system maintainers.

ArchLinux users can install latest release from AUR repository: [rclone-browser][7].

Fedora package is now available from [Fedora packages](https://apps.fedoraproject.org/packages/rclone-browser) - simply run `sudo dnf install rclone-browser`

FreeBSD has its version available from [freshports](https://www.freshports.org/net/rclone-browser) website.

There is docker version provided by [@romancin](https://github.com/romancin) - https://github.com/romancin/rclonebrowser-docker - perfect solution if you would like to run it directly on your NAS (Synology, QNAP, etc.)

Compatibility
--------------
Release binaries are built by GitHub Actions for these targets:

*   **Windows x64 and ARM64**: built with Qt 6.10.3, which needs Windows 10 or newer; the ARM64 build is for Windows on ARM devices.
*   **Linux x86_64**: an [AppImage](https://appimage.org/) built on Ubuntu 22.04 (glibc 2.35). Systems with an older glibc may not run it.
*   **macOS**: Apple Silicon (arm64) only, built with Qt 6.10.3. The lowest macOS release it runs on is the one Qt 6.10.3 supports.
*   **FreeBSD**: built on FreeBSD with the Qt 6 packages from `pkg`.

No 32-bit Windows or Linux binaries, Raspberry Pi builds or Intel Mac builds are published any more. On other systems build from source, see [Build instructions](#build-instructions); Qt 6 has no 32-bit Windows kits.

Situation with Linux is a bit fuzzier...

The whole idea with AppImage is to build it on the oldest still supported LTS distro – and it should work on all newer OS releases. AppImage contains an aplication and all the files the app needs to run. In other words, each AppImage has no dependencies other than what is included in the base operating system.

In practical terms it means that the x86_64 AppImage is expected to work on distributions with glibc 2.35 or newer, for example Ubuntu 22.04 and later. With other distributions YMMV. This is Linux. 10000 different distributions… with changes and customizations often only their authors are aware of. I would be happy to hear what distribution it does not work for.

To make life easier when using AppImages on Linux, you can use [AppImageLauncher](https://github.com/TheAssassin/AppImageLauncher) which monitors your system for downloaded AppImages and provides several useful benefits including:

*   **AppImage desktop integration** - AppImageLauncher allows you to integrate AppImages you download into your application menu or launcher to make it easier for you to launch them. It also takes care of moving them into a central location, where you can find them later if you need access to them again.

*   **Update management** - AppImageLauncher provides a simple to use update mechanism. After desktop integration, the context menu of the AppImage's entry in the application launcher will have an entry for updating that launches a little helper tool that uses AppImageUpdate internally. Just click the entry and have the tool search and apply updates.

*   **Easy removal of AppImages from system** - Removing integrated AppImages is pretty simple, too. Similar to updating AppImages, you will find an entry in the context menu in the application launcher that triggers a removal tool. You will be asked to confirm the removal. If you choose to do so, the desktop integration is undone, and the file is removed from your system.

Why AppImage only for Linux
----------------------------
Starting with version 1.7.0 Linux binaries are only available in [AppImage](https://appimage.org/) format. Some explanation on this... 

Binaries for Linux desktop applications is a major f*ing pain in the ass... as Linus Torvalds said - [DebConf 14_ QA](https://www.youtube.com/watch?v=5PmHRSeA2c8) at 05:40:

> I'm talking about actual application writers that want to make a package of their application for Linux. And I've seen this firsthand with the other project I've been involved with, which is my divelog application.
> We make binaries for Windows and OS X. 

He is talking about [Subsurface](https://subsurface-divelog.org). His small side project.

> We basically don't make binaries for Linux. Why? Because binaries for Linux desktop applications is a major f*ing pain in the ass. Right. You don't make binaries for Linux. You make binaries for Fedora 19, Fedora 20, maybe there's even like RHEL 5 from ten years ago, you make binaries for debian stable.
> Or actually you don't make binaries for debian stable because debian stable has libraries that are so old that anything that was built in the last century doesn't work. But you might make binaries for debian... whatever the codename is for unstable. And even that is a major pain because (...) debian has those rules that you are supposed to use shared libraries. Right.
>
> And if you don't use shared libraries, getting your package in, like, is just painful. 
> But using shared libraries is not an option when the libraries are experimental and the libraries are used by two people and one of them is crazy, so every other day some ABI breaks. 
> So you actually want to just compile one binary and have it work. Preferably forever. And preferably across all Linux distributions. 
> And I actually think distributions have done a horribly, horribly bad job.
>
> One of the things that I do on the kernel - and I have to fight this every single release and I think it's sad - we have one rule in the kernel, one rule: we don't break userspace. (...) People break userspace, I get really, really angry. (...) 
> And then all the distributions come in and they screw it all up. Because they break binary compatibility left and right. 
> They update glibc and everything breaks. (...) 
> So that's my rant. And that's what I really fundamentally think needs to change for Linux to work on the desktop because you can't have applications writers to do fifteen billion different versions.

And I totally agree with above. I want to provide binary which works across as many Linux distributions as possible and I dont have time to fight with all mess with different dependencies etc. There are other similar ditribution formats e.g. flatpak but I had to choose one and I decided that AppImage is my best choice. I am not saying that AppImage is the best one but it nicely fits my objectives. You can see comparison of different solutions [here](https://github.com/AppImage/AppImageKit/wiki/Similar-projects#comparison).

If for whatever reason you are not happy or your system is not covered with provided binaries you can easily build Rclone Browser for yourself. Especially on Unix-like systems it is very easy. Please see below step by step instructions for major operating systems. I have tested all of them and you can have your own Linux distribution Rclone Browser running in no time  - it takes 8 min on Raspberry Pi 3B+, on modern desktop it can be less than a minute.

Build instructions
------------------

### Linux
1.  Install dependencies for your particular distribution:
    *   **Debian/Ubuntu and derivatives**: `sudo apt update && sudo apt -y install git g++ cmake make qt6-base-dev qt6-multimedia-dev libgl1-mesa-dev`
    *   **Suse/OpenSuse**: `sudo zypper ref && sudo zypper --non-interactive install git cmake make gcc-c++ qt6-base-devel qt6-multimedia-devel`
    *   **RHEL/CentOS**: `sudo yum -y install git gcc-c++ cmake make qt6-qtbase-devel qt6-qtmultimedia-devel`
    *   **Fedora**: `sudo dnf -y install git g++ cmake make qt6-qtbase-devel qt6-qtmultimedia-devel`
    *   **Arch/Manjaro**: `sudo pacman -Sy --noconfirm --needed git gcc cmake make qt6-base qt6-multimedia`
2.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
3.  Go to source folder `cd RcloneBrowser`
4.  Create new build folder - `mkdir build && cd build`
5.  Run `cmake ..` from build folder to create makefile
6.  Run `make` from build folder to create binary
7.  Install `sudo make install`

### FreeBSD
1.  Install dependencies `sudo pkg install git cmake qt6-base qt6-multimedia`
2.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
3.  Go to source folder `cd RcloneBrowser`
4.  Create new build folder - `mkdir build && cd build`
5.  Run `cmake ..` from build folder to create makefile
6.  Run `make` from build folder to create binary
7.  Install `sudo make install`

*Note: For rclone remotes mount to work please see this forum [thread](https://forum.rclone.org/t/failed-to-mount-fuse-fs-freebsd/7723/9). For me it was enough to run `sudo sysctl vfs.usermount=1`*

### OpenBSD
1.  Install dependencies `sudo pkg_add git cmake qt6`
2.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
3.  Go to source folder `cd RcloneBrowser`
4.  Create new build folder - `mkdir build && cd build`
5.  Run `cmake .. -DCMAKE_PREFIX_PATH:PATH=/usr/local/lib/qt6/cmake` from build folder to create makefile
6.  Run `make` from build folder to create binary
7.  Install `sudo make install`

*Note: rclone for openBSD does not support `mount` hence this feature is disabled in Rclone Browser. cgofuse guys did not manage to implement it: [#18][billziss-gh_cgofuse_i18]*

### NetBSD
1.  Install dependencies `sudo pkgin install git cmake qt6-qtbase qt6-qtmultimedia`
2.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
3.  Go to source folder `cd RcloneBrowser`
4.  Create new build folder - `mkdir build && cd build`
5.  Run `cmake .. -DCMAKE_PREFIX_PATH:PATH=/usr/pkg/qt6` from build folder to create makefile
6.  Run `make` from build folder to create binary
7.  Install `sudo make install`

*Note: rclone for NetBSD does not support `mount` hence this feature is disabled in Rclone Browser. cgofuse guys did not manage to implement it: [#18][billziss-gh_cgofuse_i18]*

### macOS
1.  If you don't have [Homebrew](https://brew.sh/) yet install it `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
2.  You might be asked to install xcode command line tools - do it. This is actuall macOS SDK, headers, and build tools. You don't need full xcode IDE.
3.  Install dependencies `brew install git cmake rclone qt` (Homebrew's `qt` formula is Qt 6, and builds natively for Apple Silicon and Intel)
4.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
5.  Go to source folder `cd RcloneBrowser`
6.  Create new build folder - `mkdir build && cd build`
7.  Run `cmake .. -DCMAKE_PREFIX_PATH:PATH="$(brew --prefix qt)"` from build folder to create makefile (on Apple Silicon this is `/opt/homebrew/opt/qt`, on Intel `/usr/local/opt/qt`)
8.  Run `make` from build folder to create binary
9.  Go to yet another newly created build folder `cd build`. Your binary should be here
10. Package your binary with Qt libraries to create self contained application `"$(brew --prefix qt)"/bin/macdeployqt rclone-browser.app -executable="rclone-browser.app/Contents/MacOS/rclone-browser"`. Without this step binary won't work without Qt installed

### Windows
1.  Install [Visual Studio 2022](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-notes) or Visual Studio 2026 - you only need the **Desktop development with C++** workload. For a native Windows on ARM build also add the **ARM64 build tools** component.
2.  Install [CMake][9] 3.16 or newer (CMake 4.x is supported)
3.  Install Qt 6.11.x from the [Qt website][10]. Select the **MSVC 2022 64-bit** prebuilt component (`msvc2022_64`), and optionally **Qt Multimedia** (only needed for the finished-transfer notification sound). For native Windows on ARM also install the **MSVC 2022 ARM64** kit (`msvc2022_arm64`). The steps below assume Qt is installed in `c:\Qt\6.11.1`
4.  Get Rclone Browser source code - clone this repo with git or download a zip from [releases][3]
5.  Open a **Developer Command Prompt for VS** (or any shell where `cmake` and the MSVC compiler are on `PATH`), then go to the source folder: `cd RcloneBrowser`
6.  Create a build folder: `mkdir build` and `cd build`
7.  Configure and build. Replace the `-G` value with the generator matching your Visual Studio version (run `cmake --help` to list installed generators):

    * Visual Studio 2022: `"Visual Studio 17 2022"`
    * Visual Studio 2026: `"Visual Studio 18 2026"`

    Example for 64-bit Release with Visual Studio 2022:

    `cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -DCMAKE_PREFIX_PATH=c:\Qt\6.11.1\msvc2022_64 .. && cmake --build . --config Release`

    For a native Windows on ARM (ARM64) build, use the ARM64 kit and architecture:

    `cmake -G "Visual Studio 17 2022" -A ARM64 -DCMAKE_CONFIGURATION_TYPES="Release" -DCMAKE_PREFIX_PATH=c:\Qt\6.11.1\msvc2022_arm64 .. && cmake --build . --config Release`

8.  Bundle Qt libraries with [windeployqt](https://doc.qt.io/qt-6/windows-deployment.html). Run this from the **build** folder (note the double `build` in the path - that is intentional):

    `c:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe --no-translations --no-compiler-runtime --no-svg ".\build\Release\RcloneBrowser.exe"`

    windeployqt should copy release DLLs such as `Qt6Core.dll` (not `Qt6Cored.dll`). If you see debug DLL names, the executable path or build configuration is wrong - do not rename debug DLLs to work around this. (The old `--no-angle` flag is gone in Qt 6.)

9.  The runnable application is in `build\build\Release\` - `RcloneBrowser.exe` plus the Qt DLLs and plugin folders copied by windeployqt

10. Because `--no-compiler-runtime` is used, target machines also need the matching [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) for the Visual Studio version you built with.

*Optional:* `scripts\release_windows.cmd x64` (or `arm64`) automates a full release build (windeployqt, MSVC runtime DLLs, zip, and installer) when run from a configured release environment.

> **Note:** Qt 6 no longer provides 32-bit Windows desktop kits, so 32-bit Windows builds are no longer supported.

Development (Docker)
-----------------------

`scripts/dev.sh` builds, tests and smoke-runs the app inside a Docker container, so no Qt toolchain is needed on the host. The containers run with `--network none` (`servers` uses its own internal network, see below), as your uid:gid, with the repository mounted at `/src`. The host `$HOME` and your rclone configuration are never mounted: inside the container rclone only sees an empty temporary config.

The host `rclone` (version 1.75 or newer, taken from `PATH` or from `RB_RCLONE=/path/to/rclone`) is mounted read-only at `/opt/rclone/rclone` for the end-to-end tests.

```
scripts/dev.sh build  [qt64|qt610]                 # configure and build into build-docker-<flavor>
scripts/dev.sh test   [qt64|qt610] [-- <ctest args>]   # unit tests (ctest -L unit)
scripts/dev.sh e2e    [qt64|qt610] [-- <ctest args>]   # end-to-end tests (ctest -L e2e)
scripts/dev.sh smoke  [qt64|qt610]                 # start the app offscreen for 10 s
scripts/dev.sh fixtures [qt64|qt610]               # record tests/fixtures/rclone-1.75/ with the host rclone (offline)
scripts/dev.sh shell  [qt64|qt610]                 # interactive shell in the container
scripts/dev.sh image  [qt64|qt610]                 # build the Docker image (needs network)
scripts/dev.sh servers [qt64|qt610] [-- <ctest args>]  # protocol tests against isolated sftp/ftp/WebDAV/S3 servers
scripts/dev.sh server-images                       # pull and build the server images (needs network)
```

Flavors: `qt64` (default) uses the image `rb-build:24.04` (Ubuntu 24.04, distro Qt 6.4); `qt610` uses `rb-build:qt610` (Ubuntu 22.04, Qt 6.10.3, as in CI). Example: `scripts/dev.sh test qt64 -- -R 'tst_rclone_(log|format)'`.

Building the images is the only step that needs network access: run `scripts/dev.sh image qt64` or `scripts/dev.sh image qt610` once, and `scripts/dev.sh server-images` for the protocol tests. All other commands work offline and report a missing image with exit code 3.

`scripts/dev.sh servers` runs the protocol tests (`ctest -L servers`) against real sftp, FTP, WebDAV and S3 (MinIO) servers in local containers. Each run creates its own Docker network with `--internal`, starts the four servers and the test container on it, and removes all of them when the run ends, whatever the outcome. No port is published, so nothing is reachable from the host or the LAN, and the containers cannot reach any other host. The servers use fixed test credentials; your rclone configuration is never mounted. Outside this command the protocol tests skip themselves, and CI never runs them.

Source layout:

*   `src/`: the application. Everything except `main.cpp` and the resources is built as the static library `rclone-browser-core`, which the app and the tests link. The main window is split into `main_window.cpp` (construction, actions, remotes, jobs, tray) and `main_window_tasks.cpp` (tasks, queue and scheduler).
*   `tests/`: Qt Test programs. `tst_*.cpp` are unit tests (`ctest -L unit`); `tst_e2e_*.cpp` are end-to-end tests (`ctest -L e2e`), most of which run the real rclone against temporary local folders. `tests/servers/` holds the protocol tests (`ctest -L servers`) and the WebDAV server's Dockerfile, and `tests/support/` the shared test helpers.
*   `tests/fixtures/`: `rclone-1.75/` holds outputs recorded from rclone 1.75 (`scripts/dev.sh fixtures`); `synthetic/` holds hand-written inputs and the golden main window snapshot.

Portable vs standard mode
-----------------------

In standard operations mode all configurations files are stored in the following locations:

*   macOS:
    *   preferences: ~/Library/Preferences/com.rclone-browser.rclone-browser.plist
    *   tasks file:  ~/Library/Application Support/rclone-browser/rclone-browser/tasks.bin
    *   queue file:  ~/Library/Application Support/rclone-browser/rclone-browser/queue.conf
    *   scheduler file:  ~/Library/Application Support/rclone-browser/rclone-browser/scheduler.conf
    *   lock file:  in $TMPDIR assigned by OS
    
*   Linux/BSD:
    *   preferences: ~/.config/rclone-browser/rclone-browser.conf
    *   tasks file:  ~/.local/share/rclone-browser/rclone-browser/tasks.bin
    *   queue file:  ~/.local/share/rclone-browser/rclone-browser/queue.conf
    *   scheduler file:  ~/.local/share/rclone-browser/rclone-browser/scheduler.conf
    *   lock file:  in $TMPDIR or /tmp if $TMPDIR is not defined; on Linux the name includes your user id, so several users of one machine can run Rclone Browser at the same time

*   Windows:
    *   preferences: in registry Computer\HKEY_CURRENT_USER\Software\rclone-browser\rclone-browser
    *   tasks file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\tasks.bin
    *   queue file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\queue.conf
    *   scheduler file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\scheduler.conf
    *   lock file:   %HOMEPATH%\AppData\Local\Temp\    

Starting with version 1.7.0 of Rclone Browser portable mode is supported on all operating systems. To enable it you have to create .ini file (for Windows and macOS) next to executable with same name - e.g. if application name is `RcloneBrowser.exe` or `RcloneBrowser.app` create `RcloneBrowser.ini`. For Linux create a directory (not a file) with the same name as the AppImage plus the ".config" extension in the same directory as the AppImage file - e.g. if application name is `rclone-browser.AppImage` create folder `rclone-browser.AppImage.config` next to it. This is solution supported by [AppImage specification](https://docs.appimage.org/user-guide/portable-mode.html).

In portable mode all configuration files will be stored in the same folder as application (in .config folder on Linux) and rclone and rclone.conf path can be relative to executable - so if in preferences in `rclone location` you put `rclone.exe` browser with look for it in folder where application resides. It means that you can put all required stuff including rclone binary itself and its config on e.g. memory stick and everything will be stored there.

Transfers and logging
---------------------

Rclone Browser reads the progress of every transfer (stats, per-file progress and errors) from rclone's JSON log, so it works for any file name. To do that it manages rclone's logging options itself:

*   Logging, stats and verbosity options typed into the extra options of a transfer (`-v`, `-vv`, `--verbose`, `-q`, `-P`/`--progress`, `--log-level`, `--log-format`, `--stats`, `--stats-one-line`, `--use-json-log`, `--color`, `--syslog`, ...) are removed. The job's output panel lists them as "ignored options". Transfers always run with `--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER`; `-vv` or `--log-level DEBUG` in the extra options switches to debug output (`-vv`).
*   `--log-file <path>` is honored: the job appends a readable text log to that file (the same lines as the output panel, followed by the final stats).
*   rclone's logging environment variables (`RCLONE_VERBOSE`, `RCLONE_LOG_LEVEL`, `RCLONE_LOG_FILE`, `RCLONE_STATS*`, `RCLONE_USE_JSON_LOG`, `RCLONE_PROGRESS`, `RCLONE_QUIET`, `RCLONE_COLOR`, ...) are not passed to the rclone processes whose output the app reads. `RCLONE_LOG_FILE` and `RCLONE_LOG_LEVEL` still count as `--log-file` and `--log-level` for transfers. All other `RCLONE_*` settings apply as usual; the `rclone config` terminal keeps the full environment.
*   Combined short flags such as `-vP` are passed to rclone unchanged; write logging flags separately.

rclone version, updates and config password
-------------------------------------------

*   **Preferences, "Test" button**: runs the rclone selected in `rclone location` with `version` and shows its first line, followed by "OK" or "requires 1.75.0 or newer" (or the reason it cannot run). Saving Preferences checks the version again.
*   **Update checks** (enabled in Preferences, at most once a day, in the background): Rclone Browser looks for new releases of itself at [manprint/RcloneBrowser](https://github.com/manprint/RcloneBrowser/releases) and of rclone in the GitHub repository set in `rclone releases repository` (default `rclone/rclone`; any `owner/repo` with GitHub-style releases works, e.g. a fork). A failed check is only logged on stderr.
*   **Encrypted rclone.conf**: when rclone needs the configuration password, Rclone Browser asks for it; a wrong password asks again. If `RCLONE_PASSWORD_COMMAND` is set in the environment Rclone Browser is started from, rclone gets the password from that command and no prompt is shown; if the command fails, a message shows rclone's error instead of a prompt.
*   **Config button**: while the [private backend](#backend) runs, "Config" opens the remote configuration dialog instead of a terminal. Pick a provider (the filter matches its name or description) and a remote name, then answer one question per page. Each page shows the option name, rclone's help text and one field: a check box, a list, a password field or a text field. Backends that sign in through a browser (OAuth) open the sign-in page in your browser and wait until you finish there; "Cancel" stops the sign-in. Cancel while creating removes the half-created remote. Cancel while editing keeps the answers already confirmed with "Next", because rclone saves each answer as it is given. The remote list reloads afterwards.
*   **Edit remote… and Advanced (terminal)…** (right-click a remote): "Edit remote…" runs the same dialog on the remote's current settings. A password field left empty keeps the stored password; a stored password cannot be removed in the dialog. Advanced options are not asked in the dialog: set them with "Advanced (terminal)…", which opens `rclone config` in a terminal. On Linux and BSD that is the Preferences "Terminal command" when set, then `$TERMINAL`, then the first of gnome-terminal, konsole, xfce4-terminal, xterm and x-terminal-emulator found in `PATH` (see [the FAQ](#faq-terminal-command)); on macOS it is Terminal, through a temporary `.command` file. Without a running backend the Config button opens the same terminal. The remote list reloads when the terminal closes.

Backend
-------

*   **Private rclone backend**: once a supported rclone is found, Rclone Browser starts its own `rclone rcd` on `127.0.0.1` with a random port and a random user name and password. The credentials reach rclone through its environment (`RCLONE_RC_USER`, `RCLONE_RC_PASS`), never through the command line, so other users cannot read them from the process list. The backend stops when Rclone Browser quits, and restarts when the rclone location or rclone.conf path changes in Preferences. If it cannot start, the status bar shows "rclone backend unavailable, using direct commands" and everything runs through rclone commands as before. With an encrypted rclone.conf the backend is unlocked with the same config password; if it is still locked when a folder is listed, the password prompt appears.
*   **Listing fallback**: folder listings use the backend only while it runs and "Default rclone options" in Preferences is empty. Arbitrary rclone flags cannot be passed to the backend, so with default options set every listing runs `rclone lsjson` instead. Both ways give the same result.
*   **Backend cache**: the backend keeps each listed remote location for 5 minutes (rclone's `--fs-cache-expire-duration`). On backends that look folders up by ID, such as Google Drive, a folder that was renamed or deleted outside Rclone Browser within that time can still be listed under its old path. Listings with "Default rclone options" set always run a fresh `rclone lsjson`.
*   **Mount "Refresh cache"**: a mount started with a remote control port (the mount dialog's "Remote control port (RC)", 0 for automatic, the default on Windows) shows a "Refresh cache" button in its job panel. It calls rclone's `vfs/refresh` so the mount re-reads the remote's directory tree, and prints "Refresh cache: OK" or the error in the job output. Each mount gets its own random rc credentials through the environment; `--rc-user` and `--rc-pass` typed into the mount options are removed. On Windows, Unmount asks rclone to quit through the same remote control.
*   **Mount script**: the script set in the mount dialog starts once rclone reports its remote control port. It still gets the positional arguments `$1` rclone executable, `$2` rc port, `$3` rc user, `$4` rc password and `$5` mount point, so existing scripts keep working, but `$3` and `$4` are deprecated because arguments are visible in the process list. New scripts should read the environment variables `RCLONE_RC_USER`, `RCLONE_RC_PASS` and `RB_RC_PORT` instead; with them `rclone rc --rc-addr localhost:$RB_RC_PORT vfs/stats` works without passing credentials on the command line.

FAQ and known limitations
-------------------------
Short answers to questions asked in the upstream issues, and what changed in this fork for them. Each entry ends with the issue numbers.

### Tasks, queue and jobs

<a id="faq-ask-before-run"></a>**Can I turn off the question before a task runs?**

Run, Dry Run and a job's Restart button ask "Are you sure ...?" before they start anything. Untick Preferences, "Ask before running a task" to turn the question off; it is on by default.

Issues: [#144](https://github.com/kapitainsky/RcloneBrowser/issues/144)

<a id="faq-restart-job"></a>**Can I restart a job that stopped or failed?**

Every transfer job that is finished, failed or stopped has a Restart button (tooltip "Run this transfer again") in its header. It starts the same transfer again as a new job, with the same arguments, source and destination, and asks first like Run does ([see above](#faq-ask-before-run)). While another job of the same task is still running, Restart is refused with a message. Mounts have no Restart button, and each job has its own button: there is no "restart all".

Issues: [#118](https://github.com/kapitainsky/RcloneBrowser/issues/118)

<a id="faq-duplicate-task"></a>**How do I clone a saved task?**

Select one task in the Tasks tab, right-click and choose Duplicate. The copy gets a new id and the description `<description> (copy)`, is saved with the other tasks and is selected. A duplicated mount task does not start automatically at launch. Changing the remote of a task, a config file path per task and a text format for the tasks file (tasks.bin stays binary) are not implemented.

Issues: [#139](https://github.com/kapitainsky/RcloneBrowser/issues/139)

<a id="faq-run-tasks-one-by-one"></a>**How do I run several tasks one after the other?**

In the Tasks tab you can select several tasks and press Run: every selected task starts at once, in list order, each as its own job. To run them one by one, select them, choose "Add to the queue" and press Start in the Queue tab. The queue runs one task at a time from the top, starts the next one when the running one finishes, and Up and Down change the order. A queued task also waits while any other transfer job is running. Mount tasks cannot be added to the queue.

Issues: [#80](https://github.com/kapitainsky/RcloneBrowser/issues/80), [#98](https://github.com/kapitainsky/RcloneBrowser/issues/98), [#109](https://github.com/kapitainsky/RcloneBrowser/issues/109)

<a id="faq-scheduler"></a>**Can a task run on a schedule?**

Select transfer tasks in the Tasks tab and choose "Add to the scheduler". In the Scheduler tab give each one a Daily schedule (weekdays and time) or a Cron schedule (for example `30 6,18 * * MON-FRI`), press "Save scheduler", activate it and press Start in the Scheduler tab to start the scheduler. A schedule that is edited but not saved is not used. Scheduled tasks only run while Rclone Browser is running, which can be in the system tray ([see below](#faq-start-in-tray)).

Issues: [#73](https://github.com/kapitainsky/RcloneBrowser/issues/73), [#177](https://github.com/kapitainsky/RcloneBrowser/issues/177), [#200](https://github.com/kapitainsky/RcloneBrowser/issues/200)

<a id="faq-auto-mount"></a>**How do I mount a remote automatically when Rclone Browser starts?**

Give the mount a task name in the Mount dialog, tick "Run saved task every time Rclone Browser starts (auto start)" and press "Save task". Rclone Browser then starts every such mount task about one second after it has started. To start Rclone Browser itself at login, add it to your desktop's autostart and combine it with [Start minimised to the tray](#faq-start-in-tray).

Issues: [#146](https://github.com/kapitainsky/RcloneBrowser/issues/146), [#250](https://github.com/kapitainsky/RcloneBrowser/issues/250)

<a id="faq-start-in-tray"></a>**Can Rclone Browser start in the system tray?**

In Preferences, System Tray, tick "Close to system tray" and then "Start minimised to system tray"; the second box is enabled only when the first is ticked and your system has a tray. The next start hides the main window and shows the tray icon. Mount tasks with [auto start](#faq-auto-mount) start as usual.

Issues: [#208](https://github.com/kapitainsky/RcloneBrowser/issues/208), [#60](https://github.com/kapitainsky/RcloneBrowser/issues/60)

<a id="faq-copy-command"></a>**How do I get the rclone command a job or task runs?**

A job's header and a mount job have a button that copies the rclone command to the clipboard, and the Tasks tab context menu has "Copy command": one line per selected task, the command Run would start (without dry run, and without remote control credentials for a mount task). The arguments are quoted for the shell of your platform, in the sh style on Linux, macOS and BSD and by the Windows command line rules on Windows, so a pasted line runs the same arguments; PowerShell can treat a few characters differently from cmd.exe. There is no export of all tasks at once: select them all and use Copy command.

Issues: [#123](https://github.com/kapitainsky/RcloneBrowser/issues/123), [#176](https://github.com/kapitainsky/RcloneBrowser/issues/176), [#194](https://github.com/kapitainsky/RcloneBrowser/issues/194), [#226](https://github.com/kapitainsky/RcloneBrowser/issues/226)

### Transfers and filters

<a id="faq-copy-action"></a>**Is there a Copy button for files and folders?**

Yes. The remote tab has "Copy" in its toolbar and in the right-click menu; its tooltip says "Server side copy" and it runs `rclone copy`. It copies inside the same remote and is disabled for top-level items of a remote and in some Google Drive modes. To copy between two remotes, use Download and type the other remote's path (`remote:path`) in the destination field, or use Upload and type it in the source field. The Transfer dialog passes the local-side field to rclone as typed, while the other side always gets this remote's name.

Issues: [#192](https://github.com/kapitainsky/RcloneBrowser/issues/192)

<a id="faq-dry-run"></a>**What is the difference between Dry run and Run?**

Dry run starts the same job with `--dry-run`: rclone reports in the job's output what it would copy, move, sync or delete and changes nothing. Run does it for real. Use Dry run first for Sync, Move and "Delete excluded files from destination". The button is in the Transfer dialog, and the Tasks tab has Dry Run for saved tasks.

Issues: [#154](https://github.com/kapitainsky/RcloneBrowser/issues/154)

<a id="faq-exclude-patterns"></a>**How do I exclude a folder from a transfer?**

Each line of the Exclude tab is an rclone `--exclude` pattern, and patterns are relative to the folder the transfer starts from, not to the disk. To skip `/media/user/TERA/POWERMATE/` when you sync `/media/user/TERA/`, write `/POWERMATE/**`: the leading `/` anchors it at the top of the source and `**` matches everything inside. Without a leading `/` a pattern matches at any depth, so `file.jpg` also matches `a/b/file.jpg`. Excluded files that already exist at the destination stay there unless "Delete excluded files from destination" is ticked, so try it with Dry run (see rclone's filtering documentation).

Issues: [#252](https://github.com/kapitainsky/RcloneBrowser/issues/252)

<a id="faq-include-exclude-order"></a>**Why is a file I excluded still copied when I also use Include?**

rclone applies every `--include` rule before every `--exclude` rule whatever their order on the command line, and the first matching rule decides. When both the Include and the Exclude tab hold a pattern, Rclone Browser therefore sends ordered `--filter` rules: the exclude patterns first, then the include patterns, then your Filter tab lines and a final `- **`. A `--exclude` typed into Extra options together with an Include pattern is not reordered and the include still wins: put that pattern in the Exclude tab instead, or leave Include empty and write all rules as `+` and `-` lines in the Filter tab, ending with your own `- **`.

Issues: [#179](https://github.com/kapitainsky/RcloneBrowser/issues/179)

### Remote operations

<a id="faq-folder-move"></a>**What does Move do with a folder?**

Moving the folder `p/F` into the location `L` gives `L/F` with the whole tree; if `L/F` already exists, the contents are merged into it. With "Delete empty source directories" ticked and no Dry run, Rclone Browser removes the leftover source folder `p/F` with `rmdir` after a successful move, because rclone can leave it behind when the destination exists; that step is skipped for a top-level folder of a bucket-based remote, where removing it would remove the bucket. Moving a folder onto its own location does nothing, and moving files works as before.

Issues: [#171](https://github.com/kapitainsky/RcloneBrowser/issues/171)

<a id="faq-new-folder"></a>**What does "Create folder" ask for, and what if it hangs?**

The dialog is titled `Create folder in '<parent>'` and asks for the name of the new folder; OK stays disabled while the name is empty, only blanks, contains `/`, or is `.` or `..`. The folder is created as `<parent>/<name>`. Cancel works while the progress dialog is open, and a mkdir that does not answer within 60 seconds is stopped with an error message. The rename dialog's progress window has a working Cancel too.

Issues: [#220](https://github.com/kapitainsky/RcloneBrowser/issues/220)

<a id="faq-drive-duplicates"></a>**Why were both copies of a duplicate file on Google Drive deleted?**

Google Drive allows several files with the same name in a folder, and Delete in the remote tab sends the path of the file to rclone, so with duplicate names rclone can remove every copy. Use Dedupe first (Tools menu or right-click menu of a Google Drive tab, enabled when a folder is selected): it runs `rclone dedupe` with the mode you pick (`--dedupe-mode`), and the rclone Google Drive documentation recommends `rclone dedupe` for duplicated files. The Dedupe dialog has a Dry run button; use it before a real run.

Issues: [#238](https://github.com/kapitainsky/RcloneBrowser/issues/238)

### Mounts

<a id="faq-several-mounts"></a>**How do I mount several remotes, or one remote twice, with different options?**

Each mount is its own rclone process, so any number can run at once; each needs its own mount point (a folder, or a drive letter on Windows). If you set a fixed RC port in the Mount dialog, each mount also needs its own port: the rclone error "Only one usage of each socket address" means the port is taken, and 0 picks a free one. The Mount dialog keeps one set of options for all remotes (see Known limitations), so save each mount with "Save task" and run it from the Tasks tab: every task keeps its own options.

Issues: [#180](https://github.com/kapitainsky/RcloneBrowser/issues/180), [#185](https://github.com/kapitainsky/RcloneBrowser/issues/185)

<a id="faq-mount-prewarm"></a>**Can I load the directory listing of a mount in advance?**

A mount started with a remote control port has a "Refresh cache" button in its job panel that calls rclone's `vfs/refresh` for the whole tree. To do it after every start, put a retry loop in the mount script: `until "$1" rc --rc-addr localhost:$RB_RC_PORT vfs/refresh recursive=true; do sleep 1; done` (`$1` is the rclone the app runs; in a Windows .bat file use `%1` and `%RB_RC_PORT%` and loop with `timeout /t 1`). The script starts as soon as rclone's remote control answers, which can be before the mount is ready, so it has to retry until the call succeeds. The script gets `RB_RC_PORT`, `RCLONE_RC_USER` and `RCLONE_RC_PASS` in its environment and `rclone rc` reads the credentials from there, so none go on the command line (see [Backend](#backend), "Mount script"). Write it as `recursive=true`: rclone expects the value as a string.

Issues: [#132](https://github.com/kapitainsky/RcloneBrowser/issues/132), [#52](https://github.com/kapitainsky/RcloneBrowser/issues/52)

<a id="faq-git-in-mount"></a>**Why does git clone fail inside a mount with an input/output error?**

git appends to files it has just written. In rclone's VFS documentation the `off` cache mode ignores O_APPEND and O_TRUNC and `minimal` ignores them for write-only files, while `writes` and `full` "should support all normal file system operations". Check "Cache Level" in the Mount dialog and that the options do not set a second `--vfs-cache-mode`. The reporter already used `writes`, so the cache mode is not a promise: if it still fails, report it to rclone with a `-vv` log from the mount job.

Issues: [#190](https://github.com/kapitainsky/RcloneBrowser/issues/190)

<a id="faq-windows-network-drive"></a>**Can I mount as a network drive, or to any free drive letter on Windows?**

In the Mount dialog tick the drive letter box (tooltip "Mount to drive letter") and choose a letter in "Drive letter:"; letters already in use are disabled. For a network drive add `--network-mode` to the mount options (the Extra options tab of the Mount dialog, or Preferences, "Default mount options"), as rclone's `mount` documentation describes. Mounting on Windows needs [WinFsp](https://winfsp.dev/).

Issues: [#210](https://github.com/kapitainsky/RcloneBrowser/issues/210)

### Preferences and options

<a id="faq-fast-list"></a>**Can I turn off --fast-list?**

`--fast-list` lets rclone list a tree with fewer requests at the cost of memory; rclone's Google Drive documentation warns that with it, recently uploaded files can be missing from a listing on team drives. A fresh Rclone Browser install puts `--fast-list` into Preferences, "Default rclone options": delete it there to turn it off. That field is global for the application (there is no per-remote switch) and applies to transfers and listings, but not to mounts, which use "Default mount options". While the field is not empty, folder listings run `rclone lsjson` instead of the private backend (see [Backend](#backend)); with an empty field the backend is used again.

Issues: [#133](https://github.com/kapitainsky/RcloneBrowser/issues/133)

<a id="faq-bandwidth-limit"></a>**Is there a global bandwidth limit?**

No. `--bwlimit` (the Bandwidth field of the Transfer dialog, or Default rclone options, or Extra options) is enforced inside one rclone process, and every job of Rclone Browser is its own rclone process. Three concurrent jobs limited to 8M each can use about 24M together. Divide the limit by the number of jobs you run at the same time, or run them one after the other through the [queue](#faq-run-tasks-one-by-one).

Issues: [#116](https://github.com/kapitainsky/RcloneBrowser/issues/116)

<a id="faq-100-percent"></a>**Why does a job show 100 % but keep running?**

A job ends when rclone exits, and rclone still checks and finishes the last files after the last bytes are transferred. The speed rclone reports is a moving average of one-second samples over about the last 16 seconds, so it decays after the last bytes instead of dropping to zero at once. Rclone Browser does not wait for the speed to reach zero: it shows what rclone reports until the process ends.

Issues: [#235](https://github.com/kapitainsky/RcloneBrowser/issues/235)

<a id="faq-remember-options"></a>**Can the dialogs remember my Extra options and mount point?**

Yes: Preferences, "Remember dialogs' last used options" (on by default) makes the Transfer, Export and Mount dialogs open with the options you used last, including Extra options and the mount point text. The remembered set is one per dialog type, shared by all remotes, and the mount point's browse button does not start in it. Save a transfer or mount as a task to keep its own options. Options that every upload or download should get belong in Preferences, "Default upload options" and "Default download options".

Issues: [#136](https://github.com/kapitainsky/RcloneBrowser/issues/136), [#215](https://github.com/kapitainsky/RcloneBrowser/issues/215)

### Connecting and running

<a id="faq-ftp-mlsd"></a>**What can I do about "500 MLSD command not understood" on FTP?**

Both reporters found that rclone listed their server when the path started with `/` (`remote:/`) and failed with this error without it (`remote:`), so the server may accept MLSD for `/` only. Rclone Browser opens a remote at its default folder and cannot be pointed at `/` yet, so create an alias remote whose `remote` is `ftpname:/` (for example `[tnas-root]`, `type = alias`, `remote = tnas-ftp:/`) and browse or mount that, as for [SFTP](#faq-sftp-outside-home). If a server really lacks MLSD, rclone's FTP backend has the option `disable_mlsd` ("Disable using MLSD even if server advertises support") that may help: set `disable_mlsd = true` in the remote's section of rclone.conf, for example through "Advanced (terminal)…", or start Rclone Browser with `RCLONE_FTP_DISABLE_MLSD=true`, which applies to every FTP remote.

Issues: [#119](https://github.com/kapitainsky/RcloneBrowser/issues/119), [#128](https://github.com/kapitainsky/RcloneBrowser/issues/128)

<a id="faq-sftp-outside-home"></a>**How do I get outside my home folder on an SFTP remote?**

With rclone's SFTP backend a path that does not start with `/` is relative to the user's home folder, and `remote:/` is the root of the machine. Rclone Browser cannot be pointed at `/` yet (a path field is planned), so either create an alias remote whose `remote` is `nas:/` and browse that, or type an absolute path such as `/home/samba/x` into the source or destination field of the Transfer dialog.

Issues: [#153](https://github.com/kapitainsky/RcloneBrowser/issues/153)

<a id="faq-headless"></a>**Can I use Rclone Browser on a NAS or a machine without a desktop?**

Rclone Browser is a desktop Qt application and has no web interface. rclone itself has one: `rclone rcd --rc-web-gui` serves it on localhost by default, and for access from another computer add `--rc-addr :5572` together with `--rc-user` and `--rc-pass`. For a NAS, [How to get it](#how-to-get-it) lists a third-party docker image (check which Rclone Browser version it ships); to work with a rclone.conf from another machine, copy it or set its location in Preferences, "rclone.conf location".

Issues: [#213](https://github.com/kapitainsky/RcloneBrowser/issues/213), [#247](https://github.com/kapitainsky/RcloneBrowser/issues/247)

<a id="faq-rclone-path"></a>**What happens when rclone is missing or broken, and can I change its path?**

If the rclone path in Preferences is missing, a directory, not executable, not found in PATH or exits with an error, Rclone Browser logs "rclone problem: ..." and shows a message with a Preferences button instead of closing; the version check and the Config buttons check the path first. The path field ("rclone location") can be changed at any time, and accepting Preferences checks it again. An rclone that is too old still shows the version dialog, and a cancelled config password prompt still quits.

Issues: [#145](https://github.com/kapitainsky/RcloneBrowser/issues/145), [#209](https://github.com/kapitainsky/RcloneBrowser/issues/209), [#159](https://github.com/kapitainsky/RcloneBrowser/issues/159)

<a id="faq-terminal-command"></a>**Which terminal does "Advanced (terminal)…" use on Linux and BSD?**

In this order: Preferences, "Terminal command" (a terminal emulator, not a shell: a program name or full path without arguments), then `$TERMINAL`, then the first of gnome-terminal, konsole, xfce4-terminal, xterm and x-terminal-emulator found in `PATH`. A value that is not found is logged as a warning and skipped, and the error message names the Preferences field. The field is hidden on Windows and macOS. `$TERM` is a terminal type, not a program, and is not used.

Issues: [#175](https://github.com/kapitainsky/RcloneBrowser/issues/175)

<a id="faq-narrow-window"></a>**Can the window be narrower than the row of remote buttons?**

Yes. The button row of a remote tab sits in a horizontal scroll area, so on a narrow window or with a large font it scrolls sideways instead of making the window wider. The minimum width of the main window is tested to stay at or below 1024 px with the default font.

Issues: [#156](https://github.com/kapitainsky/RcloneBrowser/issues/156), [#228](https://github.com/kapitainsky/RcloneBrowser/issues/228)

### Getting and building

<a id="faq-wayland"></a>**Does the Linux AppImage run on Wayland?**

As built today, the AppImage is made with linuxdeploy's Qt plugin and no extra platform plugins, so it is expected to run through X11 (XWayland). For native Wayland use a distribution package or a source build with the Qt 6 Wayland plugin installed (for example `qt6-wayland`) and start it with `QT_QPA_PLATFORM=wayland`.

Issues: [#244](https://github.com/kapitainsky/RcloneBrowser/issues/244)

<a id="faq-build-anaconda"></a>**The build fails with Anaconda's Qt ("GL/gl.h: No such file").**

The error shows cmake using the Qt headers under `anaconda3/include/qt`, not the system Qt 6. Run `conda deactivate` in the shell you build in, install the distribution packages from [Build instructions](#build-instructions) (on Debian and Ubuntu including `libgl1-mesa-dev`) and build in an empty build folder.

Issues: [#219](https://github.com/kapitainsky/RcloneBrowser/issues/219)

<a id="faq-maintained"></a>**Is Rclone Browser still maintained?**

Yes: this fork, manprint/RcloneBrowser, is the maintained continuation, updated for current rclone and Qt 6. New builds are on the [releases page][3]. [History](#history) is the story of the project before this fork, in the words of its previous maintainer.

Issues: [#203](https://github.com/kapitainsky/RcloneBrowser/issues/203)

<a id="faq-portable-mode"></a>**Is there a portable version?**

Yes, every platform supports it. Create the `.ini` file or `.config` folder next to the program as described in [Portable vs standard mode](#portable-vs-standard-mode), and settings, tasks and the rclone and rclone.conf locations stay with the program, so Rclone Browser, rclone and a config fit on a memory stick.

Issues: [#222](https://github.com/kapitainsky/RcloneBrowser/issues/222)

<a id="faq-release-assets"></a>**Is there a build for Apple Silicon, Windows ARM64 or a NAS?**

The releases have a macOS Apple Silicon DMG and a Windows ARM64 zip, and rclone installs with Homebrew (`brew install rclone`); a third-party docker image for NAS systems is linked from [How to get it](#how-to-get-it) (it may ship an older version). There is no Linux ARM build: on those systems build from source.

Issues: [#161](https://github.com/kapitainsky/RcloneBrowser/issues/161), [#240](https://github.com/kapitainsky/RcloneBrowser/issues/240), [#164](https://github.com/kapitainsky/RcloneBrowser/issues/164)

### Known limitations

*   <a id="faq-limit-command-preview"></a>The command preview of the Transfer dialog and of the remote tab's Move and Copy dialog puts plain double quotes around paths and some other arguments that contain a space (the Move and Copy dialog does not quote its extra or default options) and escapes nothing, so it is not always valid in a shell. It is display only: the command that runs is correct. For a job, use its copy button or Copy command, which quote for your shell ([Copy command](#faq-copy-command)). Issues: [#226](https://github.com/kapitainsky/RcloneBrowser/issues/226)
*   <a id="faq-limit-mount-options"></a>The Mount dialog remembers one set of options for all remotes, so the options last used show up for any remote. Save mounts that need their own options as tasks ([several mounts](#faq-several-mounts)). Issues: [#185](https://github.com/kapitainsky/RcloneBrowser/issues/185), [#215](https://github.com/kapitainsky/RcloneBrowser/issues/215)
*   <a id="faq-limit-eta"></a>A job shows the remaining time as a duration, as rclone reports it, plus its Started and Finished times; it does not show the clock time at which it will finish. Issues: [#112](https://github.com/kapitainsky/RcloneBrowser/issues/112)
*   <a id="faq-limit-bwlimit"></a>`--bwlimit` is per job, not for the whole application ([details](#faq-bandwidth-limit)). Issues: [#116](https://github.com/kapitainsky/RcloneBrowser/issues/116)

History
--------
I have been using rclone-browser for long time and being annoyed by small not working bits and pieces I decided for DYI approach and this is how this repo was created. Original mmozeiko’s [repository](https://github.com/mmozeiko/RcloneBrowser) was abandoned and in the meantime rclone changed few things breaking rclone-browser functionality.

I looked around but could not find anything fully working. Some github users made progress in fixing and adding stuff so I built upon it.

I used DinCahill's [fork](https://github.com/DinCahill/RcloneBrowser) as a base for my version.

I fixed whatever I found not working and added various tweaks enhancing functionality. I recompiled and repackaged everything using latest Qt (5.13.1) and latest platforms' compilers. This on its own fixed some issues and added new features like support for dark mode in macOS. Then followed with more fixes and more features. Rclone Browser was great again:) and is getting better.

[3]: https://github.com/manprint/RcloneBrowser/releases
[4]: https://github.com/manprint/RcloneBrowser/releases/latest
[5]: https://github.com/manprint/RcloneBrowser/blob/master/LICENSE
[6]: https://www.videolan.org
[7]: https://aur.archlinux.org/packages/rclone-browser
[8]: https://docs.microsoft.com/en-us/visualstudio/releases/2019/release-notes
[9]: http://www.cmake.org/
[10]: https://www.qt.io/download-open-source/
[img3]: https://img.shields.io/github/downloads/manprint/RcloneBrowser/total.svg?maxAge=3600
[img4]: https://img.shields.io/github/release/manprint/RcloneBrowser.svg?maxAge=3600
[img5]: https://img.shields.io/github/license/manprint/RcloneBrowser.svg?maxAge=3600
[billziss-gh_cgofuse_i18]: https://github.com/billziss-gh/cgofuse/issues/18
