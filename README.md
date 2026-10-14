# Sticky Note Tabs

A standalone fork of KDE Plasma's Sticky Note for Plasma 6.7, based on
kdeplasma-addons 6.7.5. The plugin ID is `dev.kaidoom.tabbednotes`.

## Build on Fedora

Requires Qt 6.10+, KDE Frameworks 6.26+, Plasma 6.7+, CMake and a C++20 compiler.

```sh
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel libplasma-devel \
  kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel \
  kf6-ksvg-devel plasma-sdk rpm-build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
```

## Run from the build directory

```sh
QT_PLUGIN_PATH="$PWD/build/bin${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmoidviewer -a dev.kaidoom.tabbednotes
```

The QML and configuration are embedded in a compiled plugin. Installing a QML
directory with `kpackagetool6` is insufficient. No persistent environment or
Plasma service changes are required for development.

## Install, update and remove

Build an RPM from the committed source:

```sh
./tools/build-rpm.sh
sudo dnf install ./dist/plasma-sticky-note-tabs-0.3.0-*.rpm
```

The RPM installs the fork's plugin into Fedora's standard Qt plugin
directory and its own translations, documentation and licences. After installing
or updating, log out and back in, then add **Sticky Note Tabs** from the widget
chooser. Updates use the same build and install commands.

```sh
sudo dnf remove plasma-sticky-note-tabs
```

Removing the RPM leaves note files in your home directory. KDE's original
Sticky Note plugin is not replaced.

## Note storage

The fork stores notes in `$XDG_DATA_HOME/plasma_tabbed_notes/`, normally
`~/.local/share/plasma_tabbed_notes/`. It does not read or change KDE's original
`plasma_notes` directory. Contents are UTF-8 rich text.

## Tabs and recovery

Use **+** to add a note, click a title to switch, double-click to rename, and
use the tab's **×** to close it. Nonempty tabs ask for confirmation. Ctrl+Tab and
Ctrl+Shift+Tab cycle through tabs. Drag a tab title left or right to reorder it;
hold it at an edge to scroll through overflowing tabs. Press Escape or drop
outside the strip to cancel. Tab order is saved automatically.

Each opened tab keeps its own editor and undo history. New edits save after
one second of inactivity, at least every ten seconds while typing, and before
switching. Save failures keep the current tab and draft available and show an
error. As with any editor, forced termination can lose edits not yet saved.

Closing tabs, removing widgets and uninstalling the RPM retain note files.
Use **Recover Closed Note…** in a widget's context menu to copy retained notes
into new tabs. The recovery list shows text previews; recovered notes receive
a new title. Recovery copies files, so it also works after removing a widget.

Tab titles, order, active selection and cursor/scroll positions are stored in
the widget's `tabState` entry in Plasma's normal configuration, usually
`~/.config/plasma-org.kde.plasma.desktop-appletsrc`. Note contents are stored
separately as UUID-named UTF-8 HTML files. Back up both the note directory and
Plasma's configuration while Plasma is stopped. Uninstalling does not purge
either. To inspect a retained note outside Plasma, copy it to a filename ending
in `.html` and open it in a browser.

## Attribution

Based on KDE's Sticky Note applet. The exact upstream revision is recorded in
`UPSTREAM`. Original author credits and licence headers are retained; see
`LICENSES/` for the GPL, LGPL and BSD terms applying to the source files.
