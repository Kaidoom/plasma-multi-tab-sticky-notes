<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Licensing and attribution

Plasma Tabbed Notes is an independent, unofficial derivative of KDE Plasma's
Sticky Note applet. The applet's declared license is **GPL-2.0-or-later**.
Individual components retain the licenses and copyright notices shown below;
this document does not relicense upstream files or transfer their ownership.

## Upstream

- Repository: https://github.com/KDE/kdeplasma-addons
- Release: v6.7.5
- Commit: `fa98db42c07adb9f459deecc00a9e0ae0206ab56`
- Applet: `applets/notes/`
- Translations: `po/*/plasma_applet_org.kde.plasma.notes.po`, renamed here to
  `plasma_applet_dev.kaidoom.tabbednotes.po` without changing their contents.

The original applet credits Davide Bettio and Lukas Kropatschek. Source notices
also credit David Edmundson, Kai Uwe Broulik, Luca Carlon and Digia Plc and/or
its subsidiaries. Translator credits remain in each translation catalogue.
These notices and the upstream authors listed in `metadata.json` are preserved.
`qml/NoteEditor.qml` derives from upstream `qml/main.qml` and retains its notices.

## File licenses

Paths in the first six rows are relative to `applets/notes/`.

| Files | License declaration |
| --- | --- |
| `noteshelper.cpp`, `noteshelper.h`, `qml/main.qml`, `qml/NoteEditor.qml` | GPL-2.0-or-later |
| `tabmodel.cpp`, `tabmodel.h`, `qml/TabBar.qml` (fork additions) | GPL-2.0-or-later |
| `abstractnoteloader.*`, `filesystemnoteloader.*`, `note.cpp`, `note.h`, `notemanager.*` | LGPL-2.1-or-later |
| `documenthandler.cpp`, `documenthandler.h` (Qt-derived code) | BSD-3-Clause |
| `noteiconengine.cpp`, `noteiconengine.h` | LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL |
| `qml/config.qml`, `qml/configAppearance.qml` | GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL |
| `applets/notes/qml/ShortcutMenuItem.qml` | LGPL-2.0-or-later |
| Root `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/*.cpp`, `tools/*`, `packaging/*.spec` | BSD-2-Clause |
| `README.md`, `LICENSE.md`, `tests/README.md`, `res/Preview.png` | GPL-2.0-or-later for the project's contributions; the screenshot depicts upstream KDE/Qt interface elements |
| `.clang-format`, `.gitignore`, `UPSTREAM` | CC0-1.0 |

The license texts, including KDE's accepted-version provisions, are retained
under [`LICENSES/`](LICENSES/). Source headers and the screenshot's adjacent
`.license` file provide the applicable declarations. Dependencies such as Qt,
Plasma and KDE Frameworks are supplied by the system and retain their own
licenses; their source code and artwork are not bundled separately here.

## Inherited declarations

The pinned upstream `applets/notes/CMakeLists.txt`, `Messages.sh`, `main.xml`
and `metadata.json` do not contain file-level SPDX headers. They are retained
as upstream applet support files; `metadata.json` declares the applet's license
as `GPL-2.0+`. No new license or copyright owner has been invented for these
files. KDE's [licensing policy](https://community.kde.org/Policies/Licensing_Policy)
provides upstream guidance, including BSD licensing for CMake code.

Translation catalogues retain their original notices. Catalan and Valencian
explicitly state LGPL version 2 or later; Ukrainian states LGPL version 2.1,
version 3, or later versions approved by KDE e.V. Most others refer to the
upstream package's license. The `de`, `hr`, `nb`, `nn`, `pt`, `sk`, `sr`,
`sr@ijekavian`, `sr@ijekavianlatin`, `sr@latin`, `tr` and `zh_CN` catalogues do not
state an individual license. These inherited gaps are recorded rather than
replaced with guessed SPDX identifiers. Consult upstream and the credited
contributors if separate licensing of those files is required.

This inventory describes the retained declarations; it is not a claim that
every upstream file has complete REUSE metadata.
