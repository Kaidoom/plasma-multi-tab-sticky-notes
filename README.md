<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Plasma Tabbed Notes

A KDE Plasma 6 desktop sticky note widget with support for multiple tabs.

![Plasma Tabbed Notes preview](res/Preview.png)

An independent, unofficial fork of KDE Plasma's Sticky Note widget. It keeps
the familiar note appearance, rich-text editor and desktop/panel integration,
with separate notes in a compact tab bar.

## Features

- Create, rename and close tabs; drag titles to reorder them.
- Scroll overflowing tabs, including by holding a dragged tab near an edge.
- Switch with Ctrl+Tab and Ctrl+Shift+Tab.
- Keep independent editing and undo histories for each opened tab.
- Restore notes, tab order and the active tab after restarting Plasma.
- Recover closed notes; retain note files when a widget or the package is removed.

Double-click a title to rename it. Closing a nonempty tab asks for confirmation
inside the widget. Hiding or closing the widget cancels a pending tab closure.
The fork uses its own plugin ID, `dev.kaidoom.tabbednotes`, and does not replace
KDE's original Sticky Note.

## Requirements and compatibility

Install the development files for these upstream components:

| Component | Minimum |
| --- | --- |
| Plasma desktop and libplasma | 6.7 |
| Qt: Core, Gui, Qml, Quick, Quick Controls and Quick Dialogs | 6.10 |
| KDE Frameworks: KConfig, KCoreAddons, KI18n and KSvg | 6.26 |
| Extra CMake Modules (ECM) | 6.26 |
| CMake | 3.22 |
| C++ compiler | C++20 support |

Also needed: Git, Ninja (or another CMake build tool), GNU Gettext (`msgfmt`),
and the KF6 Kirigami, KDeclarative and KCMUtils QML runtime modules.

- **Desktop tested:** Fedora KDE 44 with Plasma 6.7.5, Qt 6.11.2 and Frameworks
  6.31.0 on Wayland.
- **Build/install tested:** Arch container with libplasma 6.7.5, Qt 6.12.0 and
  Frameworks 6.31.0, including Qt plugin discovery and uninstall. Desktop runtime
  is untested.
- **Other distributions:** Source builds may work on distributions providing
  Plasma 6.7+, Qt 6.10+ and KDE Frameworks 6.26+. Compatibility depends on the
  distribution release and has not been verified outside the environments listed
  above.

## Build from source

Use your distribution's Qt/KDE development packages matching its Plasma desktop.

```sh
git clone https://github.com/Kaidoom/plasma-multi-tab-sticky-notes.git
cd plasma-multi-tab-sticky-notes
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
  -DBUILD_TESTING=OFF
cmake --build build
```

The Qt-system-path option makes [KDEInstallDirs](https://api.kde.org/ecm/kde-module/KDEInstallDirs6.html)
resolve `KDE_INSTALL_PLUGINDIR` from the Qt 6 installation found by CMake. The
applet goes into `<Qt plugin directory>/plasma/applets/dev.kaidoom.tabbednotes.so`;
docs and translations use `/usr/local/share`. An explicit
`-DKDE_INSTALL_PLUGINDIR=/absolute/qt6/plugin/directory` must point to a directory
searched by Plasma. An arbitrary user prefix alone does not ensure discovery.
No persistent environment or Plasma service changes are needed.

## Install, update and uninstall from source

Use either a source installation or a distribution package of this fork;
uninstall the previous method before switching to avoid overwriting its files.

```sh
sudo cmake --install build
```

Log out and back in, then add **Plasma Tabbed Notes** from the widget chooser.
This is a compiled applet with embedded QML, not a QML-only widget installable
with `kpackagetool6`.

Keep the build directory and its `install_manifest.txt`. To uninstall:

```sh
sudo cmake --build build --target uninstall
```

This removes only the recorded installation files, preserving user notes,
configuration and shared directories. For source updates, uninstall with the
old build directory first, then pull, rebuild and install again. Log out and
back in after updating or removing a loaded plugin.

For development tests and a build-tree preview, see [tests/README.md](tests/README.md).

## Distribution packages

### Fedora

Build and runtime dependencies on an existing KDE installation:

```sh
sudo dnf install git gcc-c++ cmake ninja-build gettext extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel libplasma-devel \
  kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-ksvg-devel \
  kf6-kirigami kf6-kdeclarative kf6-kcmutils
```

Alternatively to source installation, build an RPM from the current Git commit:

```sh
sudo dnf install rpm-build
./tools/build-rpm.sh
sudo dnf install ./dist/plasma-sticky-note-tabs-0.3.1-1.*.rpm
```

Use the same RPM steps for updates. To uninstall the RPM:

```sh
sudo dnf remove plasma-sticky-note-tabs
```

The RPM uses Fedora's standard Qt plugin directory. The optional preview tool
is in `plasma-sdk`.

### Arch Linux

These [official repository packages](https://archlinux.org/packages/extra/x86_64/libplasma/)
were verified in an Arch container. On an existing Plasma desktop, install them
and follow the source build/install steps above:

```sh
sudo pacman -Syu --needed base-devel git cmake ninja gettext extra-cmake-modules \
  qt6-base qt6-declarative libplasma kconfig kcoreaddons ki18n ksvg \
  kirigami kdeclarative kcmutils
```

## Storage and recovery

Notes are UTF-8 rich-text files in `$XDG_DATA_HOME/plasma_tabbed_notes/`, normally
`~/.local/share/plasma_tabbed_notes/`. KDE's original `plasma_notes` directory is
unaffected. Tab titles, order, active selection and cursor/scroll positions are
stored in the widget's `tabState` entry in Plasma's configuration, normally
`~/.config/plasma-org.kde.plasma.desktop-appletsrc`.

Closing a tab, removing a widget or uninstalling the package retains the note
files. Use **Recover Closed Note…** in the widget's context menu to copy a retained
note into a new tab. To view a retained file elsewhere, copy it to a filename
ending in `.html` and open it in a browser.

Back up both the note directory and Plasma's configuration while Plasma is
stopped. Edits save automatically and before switching tabs; forced termination
can still lose edits that have not yet been saved.

## Attribution and licensing

Derived from [`KDE/kdeplasma-addons`](https://github.com/KDE/kdeplasma-addons),
`applets/notes/`, release **v6.7.5**, commit
[`fa98db42c07adb9f459deecc00a9e0ae0206ab56`](https://github.com/KDE/kdeplasma-addons/commit/fa98db42c07adb9f459deecc00a9e0ae0206ab56).
Thanks to the KDE Sticky Note authors, contributors and translation teams.
This project is not an official KDE product or endorsed by KDE. Report issues
in [this repository](https://github.com/Kaidoom/plasma-multi-tab-sticky-notes/issues).

The applet is distributed under GPL-2.0-or-later, with components under their
own GPL, LGPL and BSD terms. Original copyright and license notices are
preserved. See [LICENSE.md](LICENSE.md) for the file-level overview,
[LICENSES/](LICENSES/) for the license texts, and [UPSTREAM](UPSTREAM) for provenance.
