# Verification

Build with `BUILD_TESTING=ON` (the default), then run:

```sh
ctest --test-dir build --output-on-failure
QT_PLUGIN_PATH="$PWD/build/bin" QT_QPA_PLATFORM=wayland \
  build/bin/applet-integration -v1
python3 tools/test-plasma-session.py
```

The C++ tests use temporary XDG data/configuration directories. They load the
compiled applet through Plasma's real plugin loader and QML engine. Coverage
includes rich text, Unicode, links, both paste modes, double-click renaming,
keyboard navigation, rapid switching, undo/redo isolation, cursor restoration,
lazy editor creation, confirmation for unvisited nonempty tabs, recovery,
widget duplication with pending edits, narrow layouts, overflow, the panel
popup, baseline migration, invalid metadata, external write conflicts and
coexistence with KDE's original Notes applet. Clipboard contents are restored
after the paste test.

The storage test injects a short-write failure with a process-local file-size
limit. It verifies that the original file survives, the draft stays dirty, and
saving succeeds after the failure is removed. It also checks missing files,
invalid IDs and copying pending drafts.

The Python smoke test requires `kwin_wayland`, `plasmashell`, `gdbus` and
`dbus-run-session`. It starts a private D-Bus session and virtual Wayland
compositor with temporary configuration. It creates desktop and panel applets,
restarts the entire private session, verifies tab metadata, captures the
restored desktop, removes the widgets, and checks that all note files remain
byte-for-byte intact. Output is in `build/plasma-smoke/`. The live desktop and
its service configuration are untouched. This exercises a fresh Plasma session;
it does not log the user out of their desktop.

To test an extracted/relocated RPM, pass its Qt plugin directory with
`--plugin-root`. The integration executable can likewise use that directory
in its process-local `QT_PLUGIN_PATH`.
