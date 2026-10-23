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

Building from source needs Qt 6.4 or newer, CMake and a C++17 compiler. The Windows, Linux AppImage and macOS release binaries bundle Qt 6.10.3; the FreeBSD build uses the Qt 6 packages installed with `pkg`. OpenBSD and NetBSD are built from source (see [Compatibility](#compatibility)).

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
*   [Uploads: headers and S3 tags](#uploads-headers-and-s3-tags)
*   [Navigation](#navigation)
*   [Search](#search)
*   [Keyboard shortcuts](#keyboard-shortcuts)
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
*   Mount and unmount folders on macOS, GNU/Linux, FreeBSD and Windows (for Windows requires [winfsp](https://winfsp.dev/), for mac [macFUSE](https://macfuse.github.io/) and the rclone binary from rclone.org, see [How to get it](#how-to-get-it))
*   Optionally minimizes to tray, with notifications when upload/download finishes
*   Supports portable mode (an .ini file next to the executable on Windows or next to the app bundle on macOS, a `<AppImage name>.config` folder next to the Linux AppImage); rclone and rclone.conf paths can then be relative to the application (see [Portable vs standard mode](#portable-vs-standard-mode))
*   Supports drive-shared-with-me (Google Drive specific)
*   For remotes supporting public link sharing has an option (right-click menu) to fetch it
*   Supports tasks. Created jobs can be saved and run or edited later.
*   Dark mode on Windows, Linux and BSD, switchable in Preferences; on macOS the app follows the system appearance

Fork changes
------------
Compared with upstream, this fork includes the following. The full list is in [CHANGELOG.md](CHANGELOG.md).

*   Transfers read progress, per-file stats and errors from rclone's JSON log, so they work for any file name; `--log-file` is honoured (see [Transfers and logging](#transfers-and-logging))
*   Folder listings, exports and the remote list use `rclone lsjson` and `rclone listremotes --json`; a failed listing shows rclone's error in a warning row
*   Fixes for empty error and size fields, sizes of Google Docs and other unknown sizes, names with special characters or a trailing space, and cancelled jobs that kept running
*   Remotes are created and edited in a native dialog (provider list, one question per page, browser sign-in for OAuth backends) through the private backend; "Advanced (terminal)…" keeps `rclone config` in a terminal, which on Linux now opens reliably (see [rclone version, updates and config password](#rclone-version-updates-and-config-password))
*   The lock file is per user on Linux and BSD, and no password prompt is shown when `RCLONE_PASSWORD_COMMAND` is set
*   A wrong config password asks again, and a failing `RCLONE_PASSWORD_COMMAND` shows rclone's error
*   New build targets: macOS Apple Silicon DMG and Windows ARM64; Docker development environment (`scripts/dev.sh`) and unit and end-to-end tests run in CI
*   New in Preferences: a "Test" button for the selected rclone, a configurable rclone releases repository for update checks, and asynchronous update checks against manprint/RcloneBrowser
*   Nicolaius's customizations (thanks @Nicolaius): remote icon size as a multiplier (0.5 to 20, default 4.0 - the old "M" size), options to hide the Tasks, Queue and Scheduler tabs and the remote icon row, and a player chooser for stream commands that list several players separated by `|`
*   Folder listings go through a private `rclone rcd` backend (with `rclone lsjson` as the fallback), and mounts get their remote-control credentials through the environment instead of the command line, with a new "Refresh cache" button (see [Backend](#backend))
*   From the upstream issue sweep: copied rclone commands of tasks, jobs and mounts quoted for your shell, Duplicate task, an option to skip the question before Run, Restart for ended jobs, ordered include and exclude rules, a folder Move that merges into an existing folder and can remove the emptied source, a Create folder dialog with a working Cancel and a timeout, a "Terminal command" preference, a message with a Preferences button instead of a quit when rclone is missing or fails, and a remote button row that scrolls instead of widening the window (see [FAQ and known limitations](#faq-and-known-limitations))
*   New in 2.1.0 (first released as 2.1.0-beta.1): a "Bisync" mode in the Transfer dialog (`rclone bisync`: two-way sync with First run / resync, conflict resolution, Resilient, Recover and a workdir), and "Copy to…" (`rclone copyto`), "Upload from URL…" (`rclone copyurl`) and "Serve…" (`rclone serve`: a folder over http, webdav, ftp, sftp, dlna, nfs, restic or s3) in a remote's right-click and Tools menus, with the running serves in the Jobs tab (see [Bisync](#faq-bisync), [Copy to…](#faq-copy-to), [Upload from URL…](#faq-upload-from-url) and [Serve…](#faq-serve))
*   Also new in 2.1.0: the path field of a remote tab is editable (type a path and press Enter or "Go"; this is also how you open an item of an Internet Archive remote), the Tasks and Jobs tabs have a "Filter" field, and the main actions have keyboard shortcuts (see [Navigation](#navigation), [Search](#search) and [Keyboard shortcuts](#keyboard-shortcuts))
*   Also new in 2.1.0: uploads can send HTTP headers (rclone's `--header-upload`) and, to an S3 remote, up to 10 object tags, typed in the "Extra Options" tab of the Transfer dialog and saved in tasks (see [Uploads: headers and S3 tags](#uploads-headers-and-s3-tags)); teldrive remotes (from an rclone build with the teldrive backend) and the `gcs`, `googlecloudstorage`, `gphotos` and `googlephotos` type spellings get their icons
*   Also fixed in 2.1.0 (bugs of 2.0.0): Delete, Copy and Move stay off for a multi-selection that includes a top-level row, a folder Move of a top-level folder (a bucket) of a Google Cloud Storage remote no longer removes the emptied bucket, and Ctrl+W closes a remote tab
*   Changed for existing setups: logging and stats options typed into a transfer are ignored (see [Transfers and logging](#transfers-and-logging)); mount scripts get `RCLONE_RC_USER`, `RCLONE_RC_PASS` and `RB_RC_PORT` in their environment, and their arguments `$3` and `$4` (rc user and password) are deprecated (see [Backend](#backend)); 2.1.0 saves tasks.bin in a new format that 2.0.0 and older cannot read, so copy tasks.bin before you try 2.1.0 if you may go back (see [CHANGELOG](CHANGELOG.md))
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

The Windows zips do not include the Microsoft Visual C++ runtime: if `RcloneBrowser.exe` reports a missing `VCRUNTIME140.dll` or `MSVCP140.dll`, install the latest [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) (x64 or ARM64).

Tags with a suffix such as `v2.0.0-beta.1` are published as pre-releases.

**macOS first launch:** the DMG build is ad-hoc signed and not notarized, so macOS Gatekeeper may refuse to open it. After copying *Rclone Browser.app* from the image to `/Applications`, run `xattr -dr com.apple.quarantine "/Applications/Rclone Browser.app"` once. On macOS 14 and older you can instead right-click the app and choose **Open**; on macOS 15 and later, try to open it once, then click **Open Anyway** in System Settings > Privacy & Security. Install rclone with Homebrew (`brew install rclone`) or from [rclone.org](https://rclone.org/downloads/). Homebrew's rclone cannot mount (it stops with "rclone mount is not supported on MacOS when rclone is installed via Homebrew"): to use Mount, install the rclone.org binary and [macFUSE](https://macfuse.github.io/), and select that binary in Preferences, `rclone location`. While `rclone location` is empty, Rclone Browser looks for rclone in `PATH`, then in `/opt/homebrew/bin`, `/usr/local/bin` and `/opt/local/bin`.

Distribution packages of Rclone Browser are listed on [repology](https://repology.org/project/rclone-browser/packages): ArchLinux [AUR][7], Fedora (`sudo dnf install rclone-browser`), FreeBSD [freshports](https://www.freshports.org/net/rclone-browser), and a [docker image](https://github.com/romancin/rclonebrowser-docker) by [@romancin](https://github.com/romancin) for NAS devices (Synology, QNAP, etc.). They are not builds of this fork and may still ship upstream 1.8 or older, so check the version before you use one. The releases above are the builds of this fork.

Compatibility
--------------
Release binaries are built by GitHub Actions for these targets:

*   **Windows x64 and ARM64**: built with Qt 6.10.3, which needs Windows 10 or newer; the ARM64 build is for Windows on ARM devices.
*   **Linux x86_64**: an [AppImage](https://appimage.org/) built on Ubuntu 22.04 (glibc 2.35). Systems with an older glibc may not run it.
*   **macOS**: Apple Silicon (arm64) only, built with Qt 6.10.3. The lowest macOS release it runs on is the one Qt 6.10.3 supports.
*   **FreeBSD**: built on FreeBSD with the Qt 6 packages from `pkg`.
*   **OpenBSD and NetBSD**: no release binaries. CI builds both from source with their Qt 6 packages (`pkg_add qt6`, `pkgin install qt6-qtbase`) and runs the unit tests; Mount is disabled on both (see [Build instructions](#build-instructions)).

No 32-bit Windows or Linux binaries, Raspberry Pi builds or Intel Mac builds are published any more. On other systems build from source, see [Build instructions](#build-instructions); Qt 6 has no 32-bit Windows kits.

Situation with Linux is a bit fuzzier...

The whole idea with AppImage is to build it on the oldest still supported LTS distro – and it should work on all newer OS releases. AppImage contains an aplication and all the files the app needs to run. In other words, each AppImage has no dependencies other than what is included in the base operating system.

In practical terms it means that the x86_64 AppImage is expected to work on distributions with glibc 2.35 or newer, for example Ubuntu 22.04 and later. With other distributions YMMV. This is Linux. 10000 different distributions… with changes and customizations often only their authors are aware of. I would be happy to hear what distribution it does not work for.

To make life easier when using AppImages on Linux, you can use [AppImageLauncher](https://github.com/TheAssassin/AppImageLauncher) which monitors your system for downloaded AppImages and provides several useful benefits including:

*   **AppImage desktop integration** - AppImageLauncher allows you to integrate AppImages you download into your application menu or launcher to make it easier for you to launch them. It also takes care of moving them into a central location, where you can find them later if you need access to them again.

*   **Update management** - not available for this fork's AppImage: it carries no update information, so AppImageUpdate cannot update it. With update checks on in Preferences, Rclone Browser tells you when a new release is on the [releases page][3].

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

If for whatever reason you are not happy or your system is not covered with provided binaries you can easily build Rclone Browser for yourself. Especially on Unix-like systems it is very easy. Please see below step by step instructions for major operating systems. CI builds on FreeBSD, OpenBSD and NetBSD with the packages named below (on NetBSD without `qt6-qtmultimedia`), and the Docker development images (see [Development (Docker)](#development-docker)) use the Debian/Ubuntu ones; the macOS and Windows releases are built with Qt from the Qt installer, and the package names for the other Linux distributions are not tested.

Build instructions
------------------

The `cmake ..` steps below build the app and its unit tests (`RB_BUILD_TESTS`, on by default), which need the Qt Test module. Add `-DRB_BUILD_TESTS=OFF` to build only the app, and with make add `-DCMAKE_BUILD_TYPE=Release` for an optimized build (CI and `scripts/dev.sh` build Release). Warnings are errors (`-Werror`, `/WX`), so a compiler or Qt newer than the CI's can stop the build on a new warning.

### Linux
1.  Install dependencies for your particular distribution:
    *   **Debian 12, Ubuntu 24.04 or newer, and derivatives** (Ubuntu 22.04 ships Qt 6.2, which is too old): `sudo apt update && sudo apt -y install git g++ cmake make qt6-base-dev qt6-multimedia-dev libgl1-mesa-dev`
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

*Note: the `qt6-qtbase` package needs the X11 sets `xbase` and `xcomp`. On a system installed without them `pkgin` refuses it because `/usr/X11R7/lib/libxcb.so.2` is missing: add the sets with sysinst, or extract them from the NetBSD release as CI does.*

*Note: rclone for NetBSD does not support `mount` hence this feature is disabled in Rclone Browser. cgofuse guys did not manage to implement it: [#18][billziss-gh_cgofuse_i18]*

### macOS
1.  If you don't have [Homebrew](https://brew.sh/) yet install it `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
2.  You might be asked to install xcode command line tools - do it. This is actuall macOS SDK, headers, and build tools. You don't need full xcode IDE.
3.  Install dependencies `brew install git cmake rclone qt` (Homebrew's `qt` formula is Qt 6, and builds natively for Apple Silicon and Intel; Homebrew's rclone cannot mount, see [How to get it](#how-to-get-it))
4.  Clone source code from this repo `git clone https://github.com/manprint/RcloneBrowser.git`
5.  Go to source folder `cd RcloneBrowser`
6.  Create new build folder - `mkdir build && cd build`
7.  Run `cmake .. -DCMAKE_PREFIX_PATH:PATH="$(brew --prefix qt)"` from build folder to create makefile (on Apple Silicon this is `/opt/homebrew/opt/qt`, on Intel `/usr/local/opt/qt`)
8.  Run `make` from build folder to create binary
9.  Go to yet another newly created build folder `cd build`. Your binary should be here
10. Package your binary with Qt libraries to create self contained application `"$(brew --prefix qt)"/bin/macdeployqt rclone-browser.app -executable="rclone-browser.app/Contents/MacOS/rclone-browser"`, then re-sign the bundle ad-hoc as CI does: `codesign --force --deep --sign - rclone-browser.app`. Without the first step the app needs Qt installed; without the second, Apple Silicon may refuse to start it

### Windows
1.  Install [Visual Studio 2022](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-notes) or Visual Studio 2026 - you only need the **Desktop development with C++** workload. For a native Windows on ARM build also add the **ARM64 build tools** component.
2.  Install [CMake][9] 3.16 or newer (CMake 4.x is supported); the Visual Studio 2026 generator needs a CMake release that lists it in `cmake --help`
3.  Install Qt 6.4 or newer (the releases use Qt 6.10.3) from the [Qt website][10]. Select the **MSVC 2022 64-bit** prebuilt component (`msvc2022_64`), and optionally **Qt Multimedia** (only needed for the finished-transfer notification sound). For native Windows on ARM also install the **MSVC 2022 ARM64** kit (`msvc2022_arm64`). The steps below assume Qt 6.11.1 installed in `c:\Qt\6.11.1`
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

*Optional:* from the `scripts` folder, `release_windows.cmd x64` (or `arm64`) builds a release folder, a zip and an Inno Setup installer in the repository's `release` folder (windeployqt, MSVC runtime and OpenSSL DLLs). It expects Visual Studio 2022 Community, Qt 6.11.1 in `C:\Qt\6.11.1`, OpenSSL 3, 7-Zip and Inno Setup 6 at the paths written in the script. The releases on GitHub are built by GitHub Actions, not by this script.

> **Note:** Qt 6 no longer provides 32-bit Windows desktop kits, so 32-bit Windows builds are no longer supported.

Development (Docker)
-----------------------

`scripts/dev.sh` builds, tests and smoke-runs the app inside a Docker container, so no Qt toolchain is needed on the host. The containers run with `--network none` (`servers` uses its own internal network, see below), as your uid:gid, with the repository mounted at `/src`. The host `$HOME` and your rclone configuration are never mounted: inside the container rclone only sees an empty temporary config.

Every command except `image` and `server-images` needs a host `rclone` 1.75 or newer (taken from `PATH` or from `RB_RCLONE=/path/to/rclone`; on the host only `rclone version` runs). It is mounted read-only at `/opt/rclone/rclone` for the end-to-end tests and `fixtures`. The optional `RB_RCLONE_WEBUI=/path/to/rclone` (an rclone built from the webui fork) is mounted read-only at `/opt/rclone/rclone-webui` and enables the webdav-webui end-to-end test (`tst_e2e_serve_webui`); without it that test is not registered and `dev.sh e2e` prints a notice, so set it for `build` and `e2e` alike.

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

Building the images is the only step that needs network access: run `scripts/dev.sh image qt64` or `scripts/dev.sh image qt610` once, and `scripts/dev.sh server-images` for the protocol tests. All other commands work offline; they stop with exit code 2 when the host rclone is missing or older than 1.75, and with exit code 3 when the image is missing.

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
    *   lock file:  in $TMPDIR or /tmp if $TMPDIR is not defined; on Linux and BSD the name includes your user id, so several users of one machine can run Rclone Browser at the same time (in portable mode the lock file is in the portable `.config` folder)

*   Windows:
    *   preferences: in registry Computer\HKEY_CURRENT_USER\Software\rclone-browser\rclone-browser
    *   tasks file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\tasks.bin
    *   queue file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\queue.conf
    *   scheduler file:  %HOMEPATH%\AppData\Local\rclone-browser\rclone-browser\scheduler.conf
    *   lock file:   %HOMEPATH%\AppData\Local\Temp\    

Portable mode is supported on Windows, on macOS and with the Linux AppImage. On Windows, create an .ini file with the executable's name next to it: for `RcloneBrowser.exe` create `RcloneBrowser.ini`. On macOS, create it next to the app bundle and name it after the bundle: for the release's `Rclone Browser.app` create `Rclone Browser.ini` in the same folder (for a self-built `rclone-browser.app`, `rclone-browser.ini`). For Linux create a directory (not a file) with the same name as the AppImage plus the ".config" extension in the same directory as the AppImage file - e.g. if application name is `rclone-browser.AppImage` create folder `rclone-browser.AppImage.config` next to it. This is solution supported by [AppImage specification](https://docs.appimage.org/user-guide/portable-mode.html).

In portable mode all configuration files will be stored in the same folder as application (in .config folder on Linux) and rclone and rclone.conf path can be relative to executable - so if you put `rclone.exe` in Preferences, `rclone location`, Rclone Browser looks for it in the folder where the application resides. It means that you can put all required stuff including rclone binary itself and its config on e.g. memory stick and everything will be stored there.

Transfers and logging
---------------------

Rclone Browser reads the progress of every transfer (stats, per-file progress and errors) from rclone's JSON log, so it works for any file name. To do that it manages rclone's logging options itself:

*   Logging, stats and verbosity options typed into the extra options of a transfer or set in Preferences ("Default rclone options", "Default upload options", "Default download options") (`-v`, `-vv`, `--verbose`, `-q`, `-P`/`--progress`, `--log-level`, `--log-format`, `--stats`, `--stats-one-line`, `--use-json-log`, `--color`, `--syslog`, ...) are removed. The job's output panel lists them as "ignored options". Transfers always run with `--use-json-log -v --stats 1s --stats-log-level NOTICE --stats-file-name-length 0 --color NEVER`; `-vv` or `--log-level DEBUG` in the extra options switches to debug output (`-vv`).
*   `--log-file <path>` is honored: the job appends a readable text log to that file (the same lines as the output panel, followed by the final stats).
*   rclone's logging environment variables (`RCLONE_VERBOSE`, `RCLONE_LOG_LEVEL`, `RCLONE_LOG_FILE`, `RCLONE_STATS*`, `RCLONE_USE_JSON_LOG`, `RCLONE_PROGRESS`, `RCLONE_QUIET`, `RCLONE_COLOR`, ...) are not passed to the rclone processes whose output the app reads. `RCLONE_LOG_FILE` and `RCLONE_LOG_LEVEL` still count as `--log-file` and `--log-level` for transfers. All other `RCLONE_*` settings apply as usual; the `rclone config` terminal keeps the full environment.
*   Combined short flags such as `-vP` are passed to rclone unchanged; write logging flags separately.

Uploads: headers and S3 tags
----------------------------

The Transfer dialog of an upload can send HTTP headers with every file it uploads (rclone's `--header-upload`) and, to an S3 remote, object tags. Click the arrow button "Upload headers" below the extra options in the "Extra Options" tab to show the fields. A download has no such button.

*   Headers: one per line, written `Name: value`, for example `Cache-Control: no-cache`. Each line is trimmed and blank lines are skipped. The line is split at the first `:`, so a value may contain `:` and may be empty. The name may use only ASCII letters, digits and the characters ``!#$%&'*+-.^_`|~`` (spaces around it are dropped, none inside). Each header reaches rclone as one `--header-upload "Name: value"`, in the order typed, after the extra options and the default options of Preferences, for Copy, Move, Sync and Bisync uploads.
*   What a header does depends on the backend. The S3 backend of rclone 1.75.2 sets only the keys it knows: Cache-Control, Content-Disposition, Content-Encoding, Content-Language, Content-Type, If-Match, If-None-Match, X-Amz-Tagging and X-Amz-Meta-* (user metadata). For any other key rclone writes `Don't know how to set key "<key>" on upload` to the job's output as an error, still uploads the file and exits with 0, so read the output when a header seems to have no effect.
*   S3 object tags: the "S3 object tags" field is shown only when the remote is of type `s3` (Amazon S3, MinIO, Wasabi, Ceph and the other providers of rclone's S3 backend); a crypt, alias or union remote on top of a bucket has its own type and gets no tags field. Write one tag per line as `key=value`. The line is split at the first `=`, so a value may contain `=` and may be empty; the key may not be empty. At most 10 tags are accepted and a key may appear once, as S3 requires. S3 also limits a key to 128 and a value to 256 characters and limits the characters it accepts; the dialog checks neither, so S3 may refuse the upload (`InvalidTag`).
*   The tags are sent as one header, `X-Amz-Tagging: key1=value1&key2=value2`, with each key and value percent-encoded (a space becomes `%20`, `&` becomes `%26`, `=` becomes `%3D`). It comes after the typed headers, so the tags win over an `X-Amz-Tagging` line in the headers field. Storing tags needs the permission to tag objects (`s3:PutObjectTagging` on Amazon S3).
*   Text that does not parse turns off Save task, Dry run and Run (also when they read "Save and Dry run" and "Save and Run"; a task being edited shows only Save task) until it is fixed: a red message under the fields says what is wrong (it stays visible when the panel is closed), the tab title becomes "Extra Options(!)" and the three buttons say why in their tooltip. Only the fields that apply are checked. The tab title shows "(x)" when the extra options, the headers or the tags are not empty.
*   Both fields are saved in a task and shown again when it is edited (the panel opens by itself when either has text). They are not remembered by "Remember dialogs' last used options", and "Restore defaults" clears them. Rclone Browser 2.1.0 saves every task, with or without headers and tags, in a new tasks file format that 2.0.0 and older cannot read (see [CHANGELOG](CHANGELOG.md)).
*   Only an upload started from the Transfer dialog, or a task saved from one, sends these headers and tags. A download, including a Download Bisync task (bisync also writes to the remote), "Copy to…", "Upload from URL…" and the Copy and Move actions of a remote tab do not send them.
*   The tests check the request rclone sends to a local `rclone serve s3`; they do not check that a real S3 service stores the tags.

Navigation
----------

The path field above the file list of a remote tab is editable. Type a path and press Enter, or press "Go" next to the field, to open it:

*   The typed path becomes a new top-level row of the tab, named after the path, which is selected and listed. A path that already has a typed row selects that row instead of adding a second one. The file list takes the focus and the field shows `remote:path` for the selected folder. If the listing fails (for example `directory not found`), rclone's error shows in a warning row under the new row, like for any other folder. On bucket-based remotes such as S3, a folder that does not exist inside an existing bucket opens as an empty row, with no warning.
*   The `remote:` prefix of the tab's own remote is accepted and removed (`gd:Photos` and `Photos` open the same folder in a `gd` tab); any other prefix stays part of the path. Spaces before and after what you type, repeated `/` and a trailing `/` are dropped (a space right after `remote:` is kept), and an empty field does nothing. Otherwise the path goes to rclone as typed; on Windows a local path is also tidied (`\` becomes `/`, `.` and `..` are resolved). On Windows, where a local remote lists the drives, the field takes drive paths such as `C:\Users`, and a bare `C:` means the root of that drive.
*   A path with and without a leading `/` can be two different places. The first row of a tab is shown as `/` but stands for `remote:`, the default folder of the remote (the field shows `remote:`). On an SFTP or FTP remote that is the home or login folder of the user, and typing `/` adds a second row, also shown as `/`, for the root of the server (the field shows `remote:/`) ([SFTP outside the home folder](#faq-sftp-outside-home), [FTP and MLSD](#faq-ftp-mlsd)). On remotes where `remote:` and `remote:/` are the same place, typing `/` adds a second row for the same folder. On a remote of type `local` (Linux, macOS and BSD) the first row is the root `/` itself: the field shows `remote:/`, and typing `/` selects that row instead of adding one.
*   A typed row is a top-level row, like the first one: Rename, Copy, Move, Delete, Link and Copy to… are off for it, also in a selection of several rows, and work on the files and folders inside it. Mount works on it (Mount on a typed `/` row mounts `remote:/`). Typed rows exist only in their tab and are not saved. On a Google Drive tab, changing the mode (Main, Shared with me, Trash) removes them. While that change waits for running listings (and for the icons of files in typed rows), the field blinks "Please wait - finishing background jobs", is read-only, and Enter and "Go" do nothing.
*   While the cursor stays in the field, what you typed is kept when the selection of the list changes by itself (for example when a listing ends). Clicking a row moves the cursor to the list, and the field then shows the path of that row. Esc puts back the path of the selection and moves the focus to the list. Ctrl+L ("Go to path") moves the cursor into the field and selects its text, from anywhere in the tab ([Keyboard shortcuts](#keyboard-shortcuts)).
*   Internet Archive: a remote of that type is browsed by item identifier, so its tab does not list the first row when it opens. The cursor starts in the path field, with the hint "Enter an Internet Archive item identifier and press Go". Type the identifier of an item (the part right after `archive.org/details/`, up to the next `/`) and press "Go": its files are listed under a new row.

Search
------

The Tasks tab and the Jobs tab each have a "Filter" field above their list; its clear button empties it. Type to hide the rows that do not match, clear the field to show them all. Ctrl+F, on the Tasks or Jobs tab, moves the cursor into its field and selects its text; on the other tabs it does nothing ([Keyboard shortcuts](#keyboard-shortcuts)).

*   The text is split at spaces. A row is shown only when every word occurs in it, in any letter case and also inside a longer word (`pho` finds `Photos`). Different words may match different texts of the row. There are no quotes, wildcards or exclusions.
*   Tasks: the filter looks at the task's name as the list shows it (a scheduled task ends with ` (*Sch)`, a mount task started with the program with ` (*auto)`), its source and its destination. Saved, edited and deleted tasks keep following the filter.
*   Jobs: the filter looks at the job's title line, its source and its destination (a mount: title line, remote and folder; a stream: title line, remote and player command; a serve: type, source folder and URL). A title line longer than 140 characters is shown shortened in the middle, and only the shortened text is searched; source and destination are searched in full. A job that starts while the filter is set follows it.
*   The filter only hides rows: the count in the title of the Jobs tab still counts every running job, Stop All still stops every running transfer, and Clean still removes every inactive job, hidden ones included (its question counts them all). In the Tasks tab every action works on the selected tasks, and a hidden task is never selected: select all takes only the rows you see. A filter that hides everything shows an empty list.
*   Clicking the tray message about an ended transfer ("Rclone Browser - transfer finished", "... error" or "... stopped", shown when "Notify about finished transfers" is on in Preferences) clears the Jobs filter when the filter hides that job (not for macOS notifications, which cannot be clicked in this way).
*   The filter texts are not saved. The Remotes, Queue and Scheduler tabs and the file list of a remote tab have no filter. Sorting of the Tasks and Jobs lists is unchanged.

Keyboard shortcuts
------------------

| Scope | Key | Action |
|-------|-----|--------|
| Remote tab | F5 | Refresh |
| Remote tab | F7 | New folder |
| Remote tab | F2 | Rename |
| Remote tab | Del | Delete |
| Remote tab | Alt+U | Upload |
| Remote tab | Alt+D | Download |
| Remote tab | Alt+I | Info |
| Remote tab | Alt+S | Size |
| Remote tab | Alt+T | DirTree |
| Remote tab | Alt+L | Link |
| Remote tab | Alt+M | Mount |
| Remote tab | Alt+V | Stream |
| Remote tab | F6 | Move |
| Remote tab | Shift+F5 | Copy to… |
| Remote tab | Alt+E | Serve… |
| Remote tab | Ctrl+L | Go to path: focus the path field to type a path |
| Remote tab | Esc | In the path field: put back the path of the selection and move the focus to the list |
| Remote tab | Ctrl+W | Close the remote tab (the platform's standard Close key) |
| Main window | Ctrl+1 to Ctrl+5 | Show the Remotes, Jobs, Tasks, Queue or Scheduler tab (nothing for a tab hidden in Preferences) |
| Main window | Ctrl+F | Go to the filter of the Tasks or Jobs tab (nothing on the other tabs) |
| Main window | Ctrl+R | Refresh the remote list |
| Tasks tab | Ctrl+Return | Run the selected tasks |
| Tasks tab | Ctrl+E | Edit the selected task |
| Tasks tab | Ctrl+Shift+Return | Dry Run the selected tasks |
| Main window | Alt+P | Preferences |
| Main window | Ctrl+Q | Quit |
| Main window | Alt+F, Alt+H (not on macOS) | Open the File or Help menu |

*   The keys of a remote tab work only while that remote's tab is the current one, also when the row of buttons is hidden.
*   A key does nothing while its action, or on the Tasks tab its button, is disabled: Edit needs exactly one selected task, Run and Dry Run need selected tasks that are not running, and Dry Run is off for a mount task.
*   On macOS Ctrl is the Command key and Alt is the Option key; there are no menu mnemonics on macOS, and Del is Fn+Delete on a Mac keyboard. Because a remote tab's Alt keys are shortcuts, on macOS they take precedence over the Option+letter characters (accents, @, €) typed into the path field.
*   On Linux and BSD, while the cursor is in the Tasks filter, Ctrl+E moves to the end of the text instead of editing the task; press Tab first.

rclone version, updates and config password
-------------------------------------------

*   **Preferences, "Test" button**: runs the rclone selected in `rclone location` with `version` and shows its first line, followed by "OK" or "requires 1.75.0 or newer" (or the reason it cannot run). Saving Preferences checks the version again.
*   **Update checks** (enabled in Preferences, at most once a day, in the background): Rclone Browser looks for new releases of itself at [manprint/RcloneBrowser](https://github.com/manprint/RcloneBrowser/releases) and of rclone in the GitHub repository set in `rclone releases repository` (default `rclone/rclone`; any `owner/repo` with GitHub-style releases works, e.g. a fork). A failed check is only logged on stderr.
*   **Encrypted rclone.conf**: when rclone needs the configuration password, Rclone Browser asks for it; a wrong password asks again. If `RCLONE_PASSWORD_COMMAND` is set in the environment Rclone Browser is started from, rclone gets the password from that command and no prompt is shown; if the command fails, a message shows rclone's error instead of a prompt.
*   **Config button**: while the [private backend](#backend) runs, "Config" opens the remote configuration dialog instead of a terminal. Pick a provider (the filter matches its name or description) and a remote name, then answer one question per page. Each page shows the option name, rclone's help text and one field: a check box, a list, a password field or a text field. Backends that sign in through a browser (OAuth) open the sign-in page in your browser and wait until you finish there; "Cancel" stops the sign-in. Cancel while creating removes the half-created remote. Cancel while editing keeps the answers already confirmed with "Next", because rclone saves each answer as it is given. The remote list reloads afterwards.
*   **Edit remote… and Advanced (terminal)…** (right-click a remote): "Edit remote…" runs the same dialog on the remote's current settings. A password field left empty keeps the stored password; a stored password cannot be removed in the dialog. Advanced options are not asked in the dialog: set them with "Advanced (terminal)…", which opens `rclone config` in a terminal. On Linux and BSD that is the Preferences "Terminal command" when set, then `$TERMINAL`, then the first of gnome-terminal, konsole, xfce4-terminal, xterm and x-terminal-emulator found in `PATH` (see [the FAQ](#faq-terminal-command)); on macOS it is Terminal, through a temporary `.command` file. Without a running backend the Config button opens the same terminal. The remote list reloads when the terminal closes.

Backend
-------

*   **Private rclone backend**: once a supported rclone is found, Rclone Browser starts its own `rclone rcd` on `127.0.0.1` with a random port and a random user name and password. The credentials reach rclone through its environment (`RCLONE_RC_USER`, `RCLONE_RC_PASS`), never through the command line, so other users cannot read them from the process list. The backend stops when Rclone Browser quits, and restarts when the rclone location or rclone.conf path changes in Preferences. Serves started with [Serve…](#faq-serve) run inside it and end with it. If it cannot start, the status bar shows "rclone backend unavailable, using direct commands" and everything runs through rclone commands as before. With an encrypted rclone.conf the backend is unlocked with the same config password; if it is still locked when a folder is listed, the password prompt appears.
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

Issues: [#139](https://github.com/kapitainsky/RcloneBrowser/issues/139), [#143](https://github.com/kapitainsky/RcloneBrowser/issues/143)

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

Yes. The remote tab has "Copy" in its toolbar and in the right-click menu; its tooltip says "Server side copy" and it runs `rclone copy`. It copies inside the same remote and is disabled for top-level items of a remote and, when one item is selected, in the Google Drive "Shared with me" and "Trash" views (with several items selected Copy, Move and Delete stay on there; do not use them in those views). To copy between two remotes, use Download and type the other remote's path (`remote:path`) in the destination field, or use Upload and type it in the source field. The Transfer dialog passes the local-side field to rclone as typed, while the other side always gets this remote's name. For one file, [Copy to…](#faq-copy-to) takes any destination directly.

Issues: [#192](https://github.com/kapitainsky/RcloneBrowser/issues/192)

<a id="faq-dry-run"></a>**What is the difference between Dry run and Run?**

Dry run starts the same job with `--dry-run`: rclone reports in the job's output what it would copy, move, sync or delete and changes nothing. Run does it for real. Use Dry run first for Sync, Bisync ([details](#faq-bisync)), Move and "Delete excluded files from destination". The button is in the Transfer dialog, and the Tasks tab has Dry Run for saved tasks.

Issues: [#154](https://github.com/kapitainsky/RcloneBrowser/issues/154)

<a id="faq-exclude-patterns"></a>**How do I exclude a folder from a transfer?**

Each line of the Exclude tab is an rclone `--exclude` pattern, and patterns are relative to the folder the transfer starts from, not to the disk. To skip `/media/user/TERA/POWERMATE/` when you sync `/media/user/TERA/`, write `/POWERMATE/**`: the leading `/` anchors it at the top of the source and `**` matches everything inside. Without a leading `/` a pattern matches at any depth, so `file.jpg` also matches `a/b/file.jpg`. Excluded files that already exist at the destination stay there unless "Delete excluded files from destination" is ticked, so try it with Dry run (see rclone's filtering documentation).

Issues: [#252](https://github.com/kapitainsky/RcloneBrowser/issues/252)

<a id="faq-include-exclude-order"></a>**Why is a file I excluded still copied when I also use Include?**

rclone applies every `--include` rule before every `--exclude` rule whatever their order on the command line, and the first matching rule decides. When both the Include and the Exclude tab hold a pattern, Rclone Browser therefore sends ordered `--filter` rules: the exclude patterns first, then the include patterns, then your Filter tab lines and a final `- **`. A `--exclude` typed into the Extra Options tab together with an Include pattern is not reordered and the include still wins: put that pattern in the Exclude tab instead, or leave Include empty and write all rules as `+` and `-` lines in the Filter tab, ending with your own `- **`.

Issues: [#179](https://github.com/kapitainsky/RcloneBrowser/issues/179)

<a id="faq-bisync"></a>**How do I keep two folders in sync in both directions (Bisync)?**

Choose "Bisync" in the Transfer dialog, next to Copy, Move and Sync (it is offered for Upload and for Download). It runs `rclone bisync` between the source (the "From" field, Path1) and the destination (the "To" field, Path2): what is new, changed or deleted on one side is copied or deleted on the other. Bisync works on folders, not on single files: for a download of a single file the Bisync option is greyed out, an Upload whose source is a file is refused, and a Download offers to create a local destination folder that does not exist yet. The Bisync group of the Settings tab has these options:

*   "First run / resync (--resync)": see below.
*   "Conflicts": what happens to a file that is new or changed on both sides and now differs. `none` (the default) keeps both versions: they are renamed `file.txt.conflict1` (the source version) and `file.txt.conflict2` (the destination version), or the next free numbers when older conflict copies exist, and both are copied to the other side. With `newer`, `older`, `larger` or `smaller` the matching version wins and is copied over the other, whose copy is kept on both sides renamed `file.txt.conflict1`; `path1` and `path2` always let the source or the destination version win. When rclone cannot pick a winner it behaves like `none`: with equal times or sizes, with a remote without modification times, and with a Compare setting that leaves them out ("Size & checksum", "Only size" or "Only checksum" for `newer` and `older`, "Only mod-time" or "Only checksum" for `larger` and `smaller`; the output then says `WARNING: ignoring --conflict-resolve`).
*   "Resilient" (`--resilient`) lets the next run retry after a less serious error instead of demanding a resync. "Recover" (`--recover`) lets a run go back to the backup listings of the last successful run when the listings were set aside, after a critical error or an expired lock ([what now](#faq-bisync-recover)), instead of asking for a resync. Most people tick both. Neither removes the lock file of a stopped run.
*   "Workdir" (`--workdir`): the local folder for rclone's listings and lock file; empty means rclone's default, `rclone/bisync` in the user cache folder (`~/.cache/rclone/bisync` on Linux, or under `$XDG_CACHE_HOME` when that is set; `--cache-dir` does not move it). It must be absolute, because a relative folder depends on where the application was started and a scheduled run would not find the listings again; the dialog refuses a relative one. It does not shorten the names of those files ([long folder paths](#faq-limit-bisync-path)).

The first run for a pair of folders needs "First run / resync". Without it rclone has no record of the last state and stops with `cannot find prior Path1 or Path2 listings`, followed by `Bisync aborted. Must run --resync to recover.` (with "Resilient" ticked the second line says `Error is retryable without --resync due to --resilient mode`, but only a resync helps here); the job ends in Error (rclone's exit code 7). A resync only copies and never deletes: a file that exists on one side only is copied to the other, and a file that exists on both sides with different content is overwritten at the destination by the source (Path1) version, whatever its date; "Conflicts" does not apply and nothing is renamed. Use Dry run first and read its output (`Skipped copy as --dry-run is set` for each file it would copy); a Dry run keeps its listings apart (`.lst-dry` files in the Workdir, which a real run does not read), so the first real run still needs "First run / resync". After that run without "First run / resync" every time. A task saved with it ticked resyncs on every run, so a file you deleted on one side comes back from the other and a renamed file exists under both names: edit the task after its first run and untick it. Resync again after you change the Include, Exclude or Filter rules or "Create empty source directories": rclone's bisync documentation asks for a resync after changing bisync settings such as the filters, and rclone does not notice a change of these rules itself (it checks only a `--filters-file`). The dialog does not remember "First run / resync" or "Workdir" for the next task. A saved Bisync task can be run, dry-run, queued and scheduled like any other transfer task.

A Bisync task leaves out six options, and the dialog disables their check boxes while Bisync is chosen: "Delete empty source directories" (`--delete-empty-src-dirs`, which bisync rejects with `unknown flag`), "Skip files that are newer on the destination" (`--update`) and "Don't traverse destination directory" (`--no-traverse`) (bisync ignores both and only prints a warning), "Delete excluded files from destination" (`--delete-excluded`), which with bisync deletes, on each side that receives a copy (the source included), every file that is not part of the run's changes (in a test it left that side with only the newly copied file and ended without an error), and "Skip all files that exist" (`--ignore-existing`) and "Don't update mod-time if files are identical" (`--no-update-modtime`), which leave a changed file behind, so bisync stops with `path1 and path2 are out of sync` and asks for a resync. Do not add them in Extra Options either.

Issues: [#243](https://github.com/kapitainsky/RcloneBrowser/issues/243)

<a id="faq-bisync-recover"></a>**Bisync says "Must run --resync to recover", "Safety abort" or "prior lock file found". What now?**

After a critical error (for example a copy that failed) rclone no longer trusts what it knows about the two folders and sets its listings aside. With "Recover" ticked, the next run goes back to the backup listings of the last successful run (`Listings not found. Reverting to prior backup as --recover is set.`) and continues. Without "Recover", or before the first successful run after a resync, every later run stops with `cannot find prior Path1 or Path2 listings` and `Bisync aborted. Must run --resync to recover.` until a resync has been done: read the job's output for the cause, run again with "First run / resync" and Dry run, check what it would copy, then run it for real and untick "First run / resync" again ([how a resync works](#faq-bisync)). If the output also says `file name too long`, see [long folder paths](#faq-limit-bisync-path).

Stop on a running job, Stop in the Jobs tab and quitting Rclone Browser while a Bisync job runs end rclone at once, and so does a crash. rclone then leaves its lock file in the Workdir, and the next runs of the same two folders stop with `prior lock file found`, whether or not "Recover" is ticked: the job's output shows the `rclone deletefile` command that removes the lock, and after that the next run continues without a resync. Adding `--max-lock` afterwards does not release that lock, because the run that created it wrote the expiry into it. To avoid this in future, put `--max-lock 2m` (rclone's minimum) into the task's Extra Options before it runs and tick "Recover": a lock left by such a run expires after 2 minutes, rclone then sets the listings aside, and "Recover" continues from the backup listings (without "Recover" that run asks for a resync).

Bisync also stops without changing anything when more than half of the files of one side were deleted (`Safety abort: too many deletes`, for example after renaming a big folder) or when every file of one side changed (`Safety abort: all files were changed`, which happens easily in a folder with a single file). A Dry run stops at the same check, but its output lists the files bisync sees as deleted or changed (`File was deleted`, `File changed`); to see what it would then do on the other side, put `--force` into Extra Options for a Dry run. Then add `--force` (or, for deletes, a higher `--max-delete`, for example `--max-delete 75`) to Extra Options for that one real run.

Issues: [#243](https://github.com/kapitainsky/RcloneBrowser/issues/243)

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

<a id="faq-copy-to"></a>**How do I copy one file to another name, folder or remote?**

Select one file in a remote tab and choose "Copy to…" in the right-click menu or in the Tools menu. The "Copy to" dialog asks for "Destination (remote:path or local path)" and starts with the file's own path. Type the full path of the new file, for example `other:backup/report-2026.pdf` or `/home/me/report.pdf`, and OK runs `rclone copyto` as a job. The last part is the name of the new file, so this is also how you copy and rename in one step. A name or path without `remote:` (for example `report-copy.pdf` or `old/report.pdf`) is taken relative to the file's folder, a destination that ends with `/` or `:` (a folder, or the top of a remote) gets the file's own name, and a destination that is the file itself does nothing. A different file already at the destination is overwritten, an identical one (same size and modification time or checksum) is left alone, and the original stays where it is. Copy to is off for folders and for several selected items (use Copy or Download), for every top-level row of the tree (the first row and each typed path) and in the Google Drive "Shared with me" and "Trash" views (use Download), and on a row that is still loading or shows a listing error ([Is there a Copy button?](#faq-copy-action)). Between two different remotes the data normally passes through this computer.

Issues: [#192](https://github.com/kapitainsky/RcloneBrowser/issues/192)

<a id="faq-upload-from-url"></a>**Can I upload a file straight from a URL to a remote?**

Yes. Select a folder in a remote tab (a top-level row counts too: the first row, which is the remote's default folder, or a typed path such as `/`, see [Navigation](#navigation)) and choose "Upload from URL…" in the right-click menu or in the Tools menu; it is off for files, for several selected items and in the Google Drive "Shared with me" and "Trash" views. Enter an http or https address with a host name: OK stays disabled until there is one, and while "Name the file from the URL" is unticked it also needs a "File name" without `/`. The job runs `rclone copyurl` and puts the file into the selected folder. The file is downloaded by this computer and uploaded to the remote; the remote does not fetch the address itself. With "Name the file from the URL" ticked, rclone names the file after the last part of the address' path, after any redirects and without the query string (`.../f%201.txt` becomes `f 1.txt`). An address whose path, after any redirects, is empty or only `/` (such as `https://example.com/`) fails with `file name wasn't found in url`, and one that ends with a folder (such as `https://example.com/downloads/`) gives a file named after that folder (`downloads`): in both cases untick the box and type the "File name". The `Content-Disposition` name that a server may send is not used. "Do not overwrite an existing file" adds `--no-clobber`: when the file exists the job ends with an error (`CopyURL failed: file already exist`) and leaves it as it is; without it an existing file of that name is replaced. The address is shown in the job panel (its title, its source field and the tooltip of its Output button), in the command that the job's copy button copies, in the notification when the job ends (if those notifications are on), at the top of the `--log-file` if you set one, and in rclone's error messages. It is also part of the rclone command line, which other users of the machine can see in the process list: do not use an address that carries a password or a secret token.

Issues: [#199](https://github.com/kapitainsky/RcloneBrowser/issues/199)

<a id="faq-serve"></a>**How do I serve a folder over http, webdav, ftp, sftp, dlna, nfs, restic or s3 (Serve…)?**

Select a folder in a remote tab (a top-level row counts too: the first row, which is the remote's default folder, or a typed path such as `/`, see [Navigation](#navigation)) and choose "Serve…" in the right-click menu, in the Tools menu or with the "Serve" button of the remote tab's button row. It is off for files, for several selected items, in the Google Drive "Shared with me" and "Trash" views, on a row that is still loading or shows a listing error, and while the [private backend](#backend) is not running (its tooltip then says "Requires the rclone backend"): the serve runs inside that backend and works like `rclone serve` on the command line. The "Serve" dialog shows the folder and asks for the "Type", the "Address", an optional "User" and "Password", and "Read only". OK closes the dialog, starts the serve and adds a row for it to the Jobs tab: the row says "Starting" while rclone starts it, then "Serving", and the status bar shows "Serve started:" followed by the URL. When rclone refuses (a port already in use, a type your rclone does not have) its error text appears in a message box and no row is added: choose "Serve…" again and change the setting. The row shows the type and the source folder, and its buttons are "Copy URL" (copies the URL), "Open in browser" (only for http and webdav serves; opens the URL in the browser), "Stop" (ends the serve; the row then says "Finished" and the button becomes "Close", which removes the row) and "Restart" (enabled once the serve has ended: it starts the serve again with the same settings, including the same user name and password; when the address has port 0, a new port may be chosen). A serve that Rclone Browser did not start in this session, for example one started by another rc client of the same backend, is listed too, but without "Restart". The details arrow ("Show details") shows the Type, Source, URL, Address and the started and finished times. If a restart fails, the row shows "Error" with rclone's message, and the message box appears too.

The types are the ones the backend's rclone offers (`serve/types`: dlna, ftp, http, nfs, restic, s3, sftp and webdav for rclone 1.75.2; rclone builds nfs for Unix only, so Windows does not list it). Until the backend answers, or if it cannot, the dialog offers these eight types. The dialog adapts to the chosen type:

*   `http`: a web page that lists the folder; the URL opens in a browser. Clients can only read, whatever "Read only" says.
*   `webdav`: for WebDAV clients and file managers, and readable in a browser. Windows' own WebDAV client refuses a login over plain http unless a registry setting is changed (see "Access WebDAV on Windows" in rclone's `serve webdav` documentation).
*   `ftp`: needs a port of your own. The address starts as `127.0.0.1:2121`, and OK stays disabled for port 0 or no port ("FTP cannot report a random port; give one"): given port 0, an ftp serve listens on 2121 and reports port 0. A second ftp serve needs another port. Without a user name clients log in as `anonymous` with any password; with a user name and no password that user logs in with any password. Each data connection uses a passive port between 30000 and 32000. That port listens on every address of this computer, IPv6 included, whatever the "Address" field says, from the client's request until the first connection (at most 60 seconds), and rclone does not check who connects to it. So even on `127.0.0.1` an ftp serve is not strictly local: on a network you do not trust, use sftp.
*   `sftp`: needs a user name and a password; OK stays disabled until both are typed ("SFTP needs a user name and a password"): rclone accepts a password login only when both are set. Rclone Browser turns off rclone's default `~/.ssh/authorized_keys` file for the serve, so no key logs in, only that user name and password. The first sftp serve creates its host keys in the `serve-sftp` folder of rclone's cache folder.
*   `nfs`: has no login (the "User" and "Password" fields are off). The URL column shows `nfs://host:port/`, which the `mount` command does not take: mount with the host and port from it, for example `mount -t nfs -o port=PORT,mountport=PORT,tcp HOST:/ /mnt/point` (rclone's `serve nfs` documentation). rclone marks nfs as experimental. Without a VFS cache mode rclone serves nfs read-only, and Rclone Browser sets none, so an nfs serve started here is always read-only, whatever "Read only" says.
*   `dlna`: has no login (the "User" and "Password" fields are off). Players such as TVs find it by themselves through multicast on the network, so the URL is not a page to open (the server answers at `/rootDesc.xml`, not at `/`). rclone sends its regular announcements only for the address you bind, but it answers a player's search on every network interface with this computer's network address: with `127.0.0.1` a player on the network may list the server by name but cannot open it. For a TV or a phone use `0.0.0.0:PORT`. It only ever reads.
*   `restic`: for restic's REST backend; give restic the URL as `rest:http://127.0.0.1:PORT/`. A restic serve has no read-only mode, so "Read only" is off and unticked.
*   `s3`: rclone marks it as experimental. It is served without access keys, which means anonymous access: any access key and secret an S3 client sends are accepted, and the "User" and "Password" fields are off (they would add an HTTP login that S3 clients do not send). Use the URL as the endpoint with path-style addressing. The folders directly inside the served folder are the buckets; files directly in the served folder are not shown.

"Read only" is ticked by default. For webdav, ftp, sftp and s3 a client can then browse and download but not upload, change, rename or delete (rclone's `read_only` VFS option); untick it only when clients should write. http, dlna and nfs serves only ever read, whatever the box says, and a restic serve always accepts writes.

A login is optional for http, webdav, ftp and restic. OK stays disabled when a password is typed without a user name ("A password needs a user name"): rclone switches the login of http, webdav and restic on only when a user name is given, so a password alone would leave the serve open. A user name alone means an empty password for http, webdav and restic, and any password for ftp. The password is for the people who connect: it reaches the backend in the request, never on a command line, and the Jobs tab does not show it.

To expose a serve on the network: a new serve listens on `127.0.0.1` and a free port (address `127.0.0.1:0`, or `127.0.0.1:2121` for ftp; the URL shows the port rclone chose), so only programs on this computer can reach it, apart from the passive ports of ftp and the search replies of dlna (see above). To let other computers in, type `0.0.0.0:PORT` in the "Address" field, as the hint under it says; a warning then says "Anyone on your network can reach this serve". On a computer with IPv6, every type except dlna then listens on every address, IPv6 included; dlna listens on IPv4 only. The URL of the serve's row in the Jobs tab shows that bind address (`[::]`, or `0.0.0.0` for ftp and dlna): other computers use this computer's network address with the same port. Over IPv6 no router port forward is involved, so a serve may be reachable from outside your network unless your router blocks incoming IPv6. Think before you do it. Everyone who can reach that address and port can read the folder; with webdav, ftp, sftp and s3 they can also change or delete in it when "Read only" is unticked, and with restic always. nfs, dlna and s3 have no login here, sftp always has one, and http, webdav, ftp and restic have one only when you set it (ftp without a user name lets anyone in as `anonymous`). Nothing is encrypted except sftp: every other type sends the files in clear text, and http, webdav, restic and ftp also send the password in clear text (the dialog has no certificate field, and rclone has no encryption for nfs and dlna). Do not do it on a network you do not trust or through a router port forward, stop the serve when you are done, and expect your firewall to ask or to need a rule (for ftp also for the ports 30000 to 32000).

A serve is not a task. It is not saved and not restored when Rclone Browser starts again. It counts in the Jobs tab title, follows the Jobs filter and sort, and "Clean" removes it once it has ended; "Stop All" does not stop it. It ends with Stop and when Rclone Browser quits. When the backend restarts (a changed rclone location or rclone.conf path in Preferences) or stops, every running serve is marked finished with the message "Backend stopped", and "Restart" starts it again.

Issues: [#129](https://github.com/kapitainsky/RcloneBrowser/issues/129)

### Mounts

<a id="faq-several-mounts"></a>**How do I mount several remotes, or one remote twice, with different options?**

Each mount is its own rclone process, so any number can run at once; each needs its own mount point (a folder, or a drive letter on Windows). If you set a fixed RC port in the Mount dialog, each mount also needs its own port: the error "Only one usage of each socket address" (Windows) or "address already in use" (Linux, macOS) in the mount's output means the port is taken, and 0 picks a free one. The Mount dialog keeps one set of options for all remotes (see Known limitations), so save each mount with "Save task" and run it from the Tasks tab: every task keeps its own options.

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

No. `--bwlimit` (the Bandwidth field of the Transfer dialog, or Default rclone options, or Extra Options) is enforced inside one rclone process, and every job of Rclone Browser is its own rclone process. Three concurrent jobs limited to 8M each can use about 24M together. Divide the limit by the number of jobs you run at the same time, or run them one after the other through the [queue](#faq-run-tasks-one-by-one).

Issues: [#116](https://github.com/kapitainsky/RcloneBrowser/issues/116)

<a id="faq-100-percent"></a>**Why does a job show 100 % but keep running?**

A job ends when rclone exits, and rclone still checks and finishes the last files after the last bytes are transferred. The speed rclone reports is a moving average of one-second samples over about the last 16 seconds, so it decays after the last bytes instead of dropping to zero at once. Rclone Browser does not wait for the speed to reach zero: it shows what rclone reports until the process ends.

Issues: [#235](https://github.com/kapitainsky/RcloneBrowser/issues/235)

<a id="faq-remember-options"></a>**Can the dialogs remember my Extra options and mount point?**

Yes: Preferences, "Remember dialogs' last used options" (on by default) makes the Transfer, Export and Mount dialogs and the remote tab's Move and Copy dialog open with the options you used last, including Extra options and the mount point text (not the upload headers and S3 object tags, see [Uploads: headers and S3 tags](#uploads-headers-and-s3-tags)). The remembered set is one per dialog type, shared by all remotes, and the mount point's browse button does not start in it. Save a transfer or mount as a task to keep its own options. Options that every upload or download should get belong in Preferences, "Default upload options" and "Default download options".

Issues: [#136](https://github.com/kapitainsky/RcloneBrowser/issues/136), [#215](https://github.com/kapitainsky/RcloneBrowser/issues/215)

### Connecting and running

<a id="faq-ftp-mlsd"></a>**What can I do about "500 MLSD command not understood" on FTP?**

Both reporters found that rclone listed their server when the path started with `/` (`remote:/`) and failed with this error without it (`remote:`), so the server may reject an `MLSD` without a folder argument, which is what rclone sends for `remote:`; for `remote:/` it sends `MLSD /`. Rclone Browser opens a remote at its default folder, but you can type `/` into the path field of the remote's tab and press Enter or "Go": that lists `remote:/` as a new top-level row ([Navigation](#navigation)), and Mount on that row mounts `remote:/`. To have the root in the remote list, so you do not type it again in every tab, create an alias remote whose `remote` is `ftpname:/` (for example `[tnas-root]`, `type = alias`, `remote = tnas-ftp:/`) and browse or mount that, as for [SFTP](#faq-sftp-outside-home). If a server really lacks MLSD, rclone's FTP backend has the option `disable_mlsd` ("Disable using MLSD even if server advertises support") that may help: set `disable_mlsd = true` in the remote's section of rclone.conf, for example through "Advanced (terminal)…", or start Rclone Browser with `RCLONE_FTP_DISABLE_MLSD=true`, which applies to every FTP remote.

Issues: [#119](https://github.com/kapitainsky/RcloneBrowser/issues/119), [#128](https://github.com/kapitainsky/RcloneBrowser/issues/128)

<a id="faq-sftp-outside-home"></a>**How do I get outside my home folder on an SFTP remote?**

With rclone's SFTP backend a path that does not start with `/` is relative to the user's home folder, and `remote:/` is the root of the server (the machine's `/`, or the top of the account's folder when the server confines the user to it). Type `/`, or an absolute path such as `/home/samba/x`, into the path field of the remote's tab and press Enter or "Go": it opens as a new top-level row below the first one, named after the path (for `/` both rows are shown as `/`), and the path field shows `nas:` for the home folder and `nas:/` for the root ([Navigation](#navigation)). An alias remote whose `remote` is `nas:/` gives the root as a remote of its own. In the Transfer dialog, the field next to the `nas:` label (the source of a download, the destination of an upload) also takes an absolute path such as `/home/samba/x`, typed without the `nas:` prefix.

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

Yes, on Windows, on macOS and with the Linux AppImage. Create the `.ini` file or `.config` folder next to the program as described in [Portable vs standard mode](#portable-vs-standard-mode), and settings, tasks and the rclone and rclone.conf locations stay with the program, so Rclone Browser, rclone and a config fit on a memory stick.

Issues: [#222](https://github.com/kapitainsky/RcloneBrowser/issues/222)

<a id="faq-release-assets"></a>**Is there a build for Apple Silicon, Windows ARM64 or a NAS?**

The releases have a macOS Apple Silicon DMG and a Windows ARM64 zip, and rclone installs with Homebrew (`brew install rclone`; for Mount use the rclone.org binary, see [How to get it](#how-to-get-it)); a third-party docker image for NAS systems is linked from [How to get it](#how-to-get-it) (it may ship an older version). There is no Linux ARM build: on those systems build from source.

Issues: [#161](https://github.com/kapitainsky/RcloneBrowser/issues/161), [#240](https://github.com/kapitainsky/RcloneBrowser/issues/240), [#164](https://github.com/kapitainsky/RcloneBrowser/issues/164)

### Known limitations

*   <a id="faq-limit-command-preview"></a>The command preview of the Transfer dialog and of the remote tab's Move and Copy dialog puts plain double quotes around paths and some other arguments that contain a space (the Move and Copy dialog does not quote its extra or default options) and escapes nothing, so it is not always valid in a shell. It is display only: the command that runs is correct. For a job, use its copy button or Copy command, which quote for your shell ([Copy command](#faq-copy-command)). Issues: [#226](https://github.com/kapitainsky/RcloneBrowser/issues/226)
*   <a id="faq-limit-mount-options"></a>The Mount dialog remembers one set of options for all remotes, so the options last used show up for any remote. Save mounts that need their own options as tasks ([several mounts](#faq-several-mounts)). Issues: [#185](https://github.com/kapitainsky/RcloneBrowser/issues/185), [#215](https://github.com/kapitainsky/RcloneBrowser/issues/215)
*   <a id="faq-limit-eta"></a>A job shows the remaining time as a duration, as rclone reports it, plus its Started and Finished times; it does not show the clock time at which it will finish. Issues: [#112](https://github.com/kapitainsky/RcloneBrowser/issues/112)
*   <a id="faq-limit-bwlimit"></a>`--bwlimit` is per job, not for the whole application ([details](#faq-bandwidth-limit)). Issues: [#116](https://github.com/kapitainsky/RcloneBrowser/issues/116)
*   <a id="faq-limit-bisync"></a>Bisync works on folders, not on single files, and a Bisync task leaves out six options ("Delete empty source directories", "Skip files that are newer on the destination", "Don't traverse destination directory", "Delete excluded files from destination", "Skip all files that exist" and "Don't update mod-time if files are identical"), which the dialog disables while Bisync is chosen ([details](#faq-bisync)). Issues: [#243](https://github.com/kapitainsky/RcloneBrowser/issues/243)
*   <a id="faq-limit-bisync-path"></a>rclone names the listing and lock files of a Bisync after the full paths of both folders (`/`, `\`, `:`, `?`, `*` and spaces become `_` and the leading `/` is dropped, for example `tmp_a1..tmp_a2.path1.lst`), so two long folder paths together can give a file name longer than the 255 bytes most file systems allow. The job's output then shows `file name too long`, followed either by `Lock file exists, but contents are unreadable` and `prior lock file found` (deleting the lock does not help) or by `Bisync aborted. Must run --resync to recover.` (a resync fails the same way). The Workdir only chooses the folder of these files, not their names: use shorter folder paths (a short symbolic link to the folder, or an rclone alias remote, also works). Issues: [#243](https://github.com/kapitainsky/RcloneBrowser/issues/243)
*   <a id="faq-limit-url-name"></a>Upload from URL names the file from the address or from the name you type, never from the server's `Content-Disposition` header, and has no field for headers, a user name or a password ([details](#faq-upload-from-url)). Issues: [#199](https://github.com/kapitainsky/RcloneBrowser/issues/199)
*   <a id="faq-limit-serve"></a>The Serve dialog sets only the type, the address, a user name and password and "Read only", and the backend gets none of the "Default rclone options" of Preferences. TLS (https), an htpasswd file, S3 access keys, a VFS cache mode and the other `rclone serve` options are not available: run `rclone serve` in a terminal for them ([details](#faq-serve)). Serves are not saved and not restored at start. Issues: [#129](https://github.com/kapitainsky/RcloneBrowser/issues/129)
*   <a id="faq-limit-serve-orphan"></a>Serves end with the backend, and the backend ends when Rclone Browser quits. If Rclone Browser crashes or is killed, Linux ends the backend with it; on Windows, macOS and the BSDs the `rclone rcd` process may keep running and keep serving, even on a network address, and the Jobs tab of the next start does not list those serves (each backend has its own list): end that `rclone rcd` process yourself. Issues: [#129](https://github.com/kapitainsky/RcloneBrowser/issues/129)

History
--------
*The story of the project up to 2020, in the words of kapitainsky, the upstream maintainer. What this fork changed is in [Fork changes](#fork-changes).*

I have been using rclone-browser for long time and being annoyed by small not working bits and pieces I decided for DYI approach and this is how this repo was created. Original mmozeiko’s [repository](https://github.com/mmozeiko/RcloneBrowser) was abandoned and in the meantime rclone changed few things breaking rclone-browser functionality.

I looked around but could not find anything fully working. Some github users made progress in fixing and adding stuff so I built upon it.

I used DinCahill's [fork](https://github.com/DinCahill/RcloneBrowser) as a base for my version.

I fixed whatever I found not working and added various tweaks enhancing functionality. I recompiled and repackaged everything using latest Qt (5.13.1) and latest platforms' compilers. This on its own fixed some issues and added new features like support for dark mode in macOS. Then followed with more fixes and more features. Rclone Browser was great again:) and is getting better.

[3]: https://github.com/manprint/RcloneBrowser/releases
[4]: https://github.com/manprint/RcloneBrowser/releases/latest
[5]: https://github.com/manprint/RcloneBrowser/blob/master/LICENSE
[6]: https://www.videolan.org
[7]: https://aur.archlinux.org/packages/rclone-browser
[9]: https://cmake.org/
[10]: https://www.qt.io/download-open-source/
[img3]: https://img.shields.io/github/downloads/manprint/RcloneBrowser/total.svg?maxAge=3600
[img4]: https://img.shields.io/github/release/manprint/RcloneBrowser.svg?maxAge=3600
[img5]: https://img.shields.io/github/license/manprint/RcloneBrowser.svg?maxAge=3600
[billziss-gh_cgofuse_i18]: https://github.com/billziss-gh/cgofuse/issues/18
