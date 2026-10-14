/*
    SPDX-FileCopyrightText: 2014 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014, 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2 // just for desktop-styled context menus
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

import org.kde.draganddrop as DragDrop

import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

FocusScope {
    id: editorRoot
    required property Note note
    required property TabModel tabs
    required property var appletRoot
    property alias textArea: mainTextArea
    property bool initialized: false
    property bool loading: false
    property bool restoring: true

    function flush() {
        throttledTextSaver.stop();
        maximumSaveTimer.stop();
        if (!initialized || !note) return true;
        scrollview.saveScroll();
        return !note.dirty || note.flush();
    }

    function restoreView() { Qt.callLater(() => { if (visible) scrollview.restoreScroll(); }); }
    onVisibleChanged: {
        if (initialized && visible) restoreView();
    }
    Timer {
        id: throttledTextSaver
        interval: 1000
        onTriggered: editorRoot.flush()
    }
    Timer {
        id: maximumSaveTimer
        interval: 10000
        onTriggered: editorRoot.flush()
    }
    Connections {
        target: note
        function onNoteTextChanged() {
            if (!editorRoot.initialized || note.dirty || mainTextArea.text === note.noteText) return;
            editorRoot.loading = true;
            mainTextArea.text = note.noteText;
            editorRoot.loading = false;
            editorRoot.restoreView();
        }
    }
    Component.onCompleted: {
        loading = true;
        mainTextArea.text = note.noteText;
        loading = false;
        initialized = true;
        restoreView();
    }
    Component.onDestruction: flush()
    DocumentHandler {
        id: documentHandler
        target: mainTextArea
        cursorPosition: mainTextArea.cursorPosition
        selectionStart: mainTextArea.selectionStart
        selectionEnd: mainTextArea.selectionEnd
        defaultFontSize: (Plasmoid.configuration.fontSize || Kirigami.Theme.defaultFont.pointSize)
    }

    Connections {
        target: appletRoot
        function onExpandedChanged(expanded) {
            // don't autofocus when we're on the desktop
            if (expanded && (Plasmoid.formFactor === PlasmaCore.Types.Vertical || Plasmoid.formFactor === PlasmaCore.Types.Horizontal)) {
                mainTextArea.forceActiveFocus()
            }
        }
    }

    FocusScope {
        id: focusScope
        anchors {
            fill: parent
        }

        PlasmaComponents3.ScrollView {
            id: scrollview
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: fontButtons.top
                bottomMargin: Kirigami.Units.largeSpacing
            }

            clip: true

            // avoids a binding loop
            property bool scrollBarNecessary: false
            Binding on scrollBarNecessary {
                value: mainTextArea.height > scrollview.height
                delayed: true
            }
            PlasmaComponents3.ScrollBar.vertical.visible: scrollBarNecessary

            PlasmaComponents3.TextArea {
                id: mainTextArea
                objectName: "noteTextArea"
                readOnly: !note.ready
                property int cfgFontPointSize: (Plasmoid.configuration.fontSize || Kirigami.Theme.defaultFont.pointSize)

                textFormat: TextEdit.RichText
                onLinkActivated: Qt.openUrlExternally(link)
                background: null
                color: appletRoot.textIconColor
                persistentSelection: true
                wrapMode: TextEdit.Wrap

                font.pointSize: cfgFontPointSize

                Keys.onPressed: event => {
                    if (event.modifiers & Qt.ControlModifier && (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)) {
                        appletRoot.navigate((event.modifiers & Qt.ShiftModifier) || event.key === Qt.Key_Backtab ? -1 : 1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Escape) {
                        appletRoot.expanded = false;
                        event.accepted = true;
                    } else if (event.modifiers === Qt.ControlModifier) {
                        if (event.key === Qt.Key_B) {
                            documentHandler.bold = !documentHandler.bold;
                            event.accepted = true;
                        } else if (event.key === Qt.Key_I) {
                            documentHandler.italic = !documentHandler.italic;
                            event.accepted = true;
                        } else if (event.key === Qt.Key_U) {
                            documentHandler.underline = !documentHandler.underline;
                            event.accepted = true;
                        } else if (event.key === Qt.Key_S) {
                            documentHandler.strikeOut = !documentHandler.strikeOut;
                            event.accepted = true;
                        } else if (event.matches(StandardKey.Paste)) {
                            documentHandler.pasteWithoutFormatting();
                            documentHandler.reset();
                            event.accepted = true;
                        } else if (event.matches(StandardKey.Cut)) {
                            cut();
                            documentHandler.reset();
                            event.accepted = true;
                        }
                    }
                }

                // Apply the font size change to existing texts
                onCfgFontPointSizeChanged: {
                    const start = selectionStart;
                    const end = selectionEnd;

                    selectAll();
                    documentHandler.fontSize = cfgFontPointSize;
                    select(start, end);
                }

                // Save semi-continuously to prevent possible data loss
                onTextChanged: {
                    if (editorRoot.initialized && !editorRoot.loading) {
                        note.edit(text);
                        throttledTextSaver.restart();
                        if (!maximumSaveTimer.running) maximumSaveTimer.start();
                    }
                }

                onActiveFocusChanged: {
                    const window = Window.window;
                    if (activeFocus && window && (window.flags & Qt.WindowDoesNotAcceptFocus)) {
                        Plasmoid.status = PlasmaCore.Types.AcceptingInputStatus
                    } else {
                        Plasmoid.status = PlasmaCore.Types.ActiveStatus
                        editorRoot.flush();
                    }
                }

                onPressed: event => {
                    if (event.button === Qt.RightButton) {
                        event.accepted = true;
                        contextMenu.popup();
                        forceActiveFocus();
                    }
                    if (event.button === Qt.LeftButton && contextMenu.visible === true) {
                        event.accepted = true;
                        contextMenu.dismiss();
                        forceActiveFocus();
                    }
                }

                Component.onCompleted: {
                    if (!Plasmoid.configuration.fontSize) {
                        // Set fontSize to default if it is not set
                        Plasmoid.configuration.fontSize = font.pointSize
                    }
                }

                QQC2.Menu {
                    id: contextMenu
                    popupType: QQC2.Menu.Window

                    readonly property bool shortcutsEnabled: contextMenu.visible
                    ShortcutMenuItem {
                        _sequence: StandardKey.Undo
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.canUndo
                        _iconName: "edit-undo"
                        _text: i18n("Undo")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.undo())
                    }

                    ShortcutMenuItem {
                        _sequence: StandardKey.Redo
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.canRedo
                        _iconName: "edit-redo"
                        _text: i18n("Redo")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.redo())
                    }

                    QQC2.MenuSeparator {}

                    ShortcutMenuItem {
                        _sequence: StandardKey.Cut
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.selectedText.length > 0
                        _iconName: "edit-cut"
                        _text: i18n("Cut")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.cut())
                    }

                    ShortcutMenuItem {
                        _sequence: StandardKey.Copy
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.selectedText.length > 0
                        _iconName: "edit-copy"
                        _text: i18n("Copy")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.copy())
                    }

                    ShortcutMenuItem {
                        _sequence: StandardKey.Paste
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.canPaste
                        _iconName: "edit-paste"
                        _text: i18n("Paste")
                        onTriggered: contextMenu.retFocus(() => documentHandler.pasteWithoutFormatting())
                    }

                    ShortcutMenuItem {
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.canPaste
                        _text: i18n("Paste with Full Formatting")
                        _iconName: "edit-paste"
                        onTriggered: contextMenu.retFocus(() => mainTextArea.paste())
                    }

                    ShortcutMenuItem {
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.selectedText.length > 0
                        _text: i18nc("@action:inmenu", "Remove Formatting")
                        _iconName: "edit-clear-all"
                        onTriggered: {
                            var richText = mainTextArea.getFormattedText(mainTextArea.selectionStart, mainTextArea.selectionEnd)
                            var unformattedText = documentHandler.strip(richText)
                            unformattedText = unformattedText.replace(/\n/g, "<br>")
                            mainTextArea.remove(mainTextArea.selectionStart, mainTextArea.selectionEnd)
                            contextMenu.retFocus(() => mainTextArea.insert(mainTextArea.selectionStart, unformattedText))
                        }
                    }

                    ShortcutMenuItem {
                        _sequence: StandardKey.Delete
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.selectedText.length > 0
                        _iconName: "edit-delete"
                        _text: i18n("Delete")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.remove(mainTextArea.selectionStart, mainTextArea.selectionEnd))
                    }

                    ShortcutMenuItem {
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.text.length > 0
                        _iconName: "edit-clear"
                        _text: i18n("Clear")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.clear())
                    }

                    QQC2.MenuSeparator {}

                    ShortcutMenuItem {
                        _sequence: StandardKey.SelectAll
                        _enabled: contextMenu.shortcutsEnabled && mainTextArea.text.length > 0
                        _iconName: "edit-select-all"
                        _text: i18n("Select All")
                        onTriggered: contextMenu.retFocus(() => mainTextArea.selectAll())
                    }

                    function retFocus(f) {
                        f()
                        documentHandler.reset()
                        mainTextArea.forceActiveFocus()
                    }
                }
            }

            // Save scrolling position when it changes, but throttle to avoid
            // killing a storage disk.
            Connections {
                target: scrollview.contentItem
                function onContentXChanged() {
                    throttedScrollSaver.restart();
                }
                function onContentYChanged() {
                    throttedScrollSaver.restart();
                }
            }
            Connections {
                target: mainTextArea
                function onCursorPositionChanged() {
                    throttedScrollSaver.restart();
                }
            }

            Timer {
                id: throttedScrollSaver
                interval: Kirigami.Units.humanMoment
                repeat: false
                running: false
                onTriggered: scrollview.saveScroll()
            }

            function saveScroll() {
                if (!editorRoot.initialized || editorRoot.restoring || !editorRoot.visible || !note) return;
                const flickable = scrollview.contentItem;
                tabs.setViewState(note.id, mainTextArea.cursorPosition, flickable.contentX, flickable.contentY);
            }

            function restoreScroll() {
                if (!editorRoot.visible) return;
                editorRoot.restoring = true;
                const state = tabs.viewState(note.id);
                mainTextArea.cursorPosition = Math.min(Math.max(0, state.cursor || 0), mainTextArea.length);
                const flickable = scrollview.contentItem;
                flickable.contentX = Math.max(0, Math.min(state.x || 0, flickable.contentWidth - flickable.width));
                flickable.contentY = Math.max(0, Math.min(state.y || 0, flickable.contentHeight - flickable.height));
                editorRoot.restoring = false;
            }

            Component.onDestruction: saveScroll()
        }

        DragDrop.DropArea {
            id: dropArea
            anchors.fill: scrollview

            function positionOfDrop(event) {
                return mainTextArea.positionAt(event.x, event.y)
            }

            onDrop: event => {
                var mimeData = event.mimeData
                var text = ""
                if (mimeData.hasUrls) {
                    var urls = mimeData.urls
                    for (var i = 0, j = urls.length; i < j; ++i) {
                        var url = urls[i]
                        text += "<a href=\"" + url + "\">" + url + "</a><br>"
                    }
                } else {
                    text = mimeData.text.replace(/\n/g, "<br>")
                }

                mainTextArea.insert(positionOfDrop(event), text)
                event.accept(Qt.CopyAction)
            }
            onDragMove: event => {
                // there doesn't seem to be a "just move the cursor", so we move
                // the selection and then unselect so the cursor follows the mouse
                mainTextArea.moveCursorSelection(positionOfDrop(event))
                mainTextArea.deselect()
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: mainTextArea.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
                acceptedButtons: Qt.NoButton
            }
        }

        RowLayout {
            id: fontButtons
            spacing: Kirigami.Units.smallSpacing
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            height: visible ? implicitHeight : 0
            visible: opacity > 0
            opacity: (focusScope.activeFocus || appletRoot.compactInPanel) ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration } }

            readonly property int requiredWidth: spacing * (children.length - 1) + formatButtonsRow.width + removeButton.width
                                                 + spacer.implicitWidth + settingsButton.width + (appletRoot.compactInPanel ? pinButton.width : 0)
            readonly property bool showFormatButtons: width > requiredWidth

            Row {
                id: formatButtonsRow
                spacing: Kirigami.Units.smallSpacing
                // show format buttons if TextField or any of the buttons have focus
                enabled: opacity > 0
                visible: fontButtons.showFormatButtons

                PlasmaComponents3.ToolButton {
                    focusPolicy: Qt.TabFocus
                    icon.name: "format-text-bold"
                    icon.color: appletRoot.textIconColor
                    checked: documentHandler.bold
                    onClicked: documentHandler.bold = !documentHandler.bold
                    text: boldTooltip.text
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    PlasmaComponents3.ToolTip {
                        id: boldTooltip
                        text: i18nc("@info:tooltip", "Bold")
                    }
                }
                PlasmaComponents3.ToolButton {
                    focusPolicy: Qt.TabFocus
                    icon.name: "format-text-italic"
                    icon.color: appletRoot.textIconColor
                    checked: documentHandler.italic
                    onClicked: documentHandler.italic = !documentHandler.italic
                    text: italicTooltip.text
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    PlasmaComponents3.ToolTip {
                        id: italicTooltip
                        text: i18nc("@info:tooltip", "Italic")
                    }
                }
                PlasmaComponents3.ToolButton {
                    focusPolicy: Qt.TabFocus
                    icon.name: "format-text-underline"
                    icon.color: appletRoot.textIconColor
                    checked: documentHandler.underline
                    onClicked: documentHandler.underline = !documentHandler.underline
                    text: underlineTooltip.text
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    PlasmaComponents3.ToolTip {
                        id: underlineTooltip
                        text: i18nc("@info:tooltip", "Underline")
                    }
                }
                PlasmaComponents3.ToolButton {
                    focusPolicy: Qt.TabFocus
                    icon.name: "format-text-strikethrough"
                    icon.color: appletRoot.textIconColor
                    checked: documentHandler.strikeOut
                    onClicked: documentHandler.strikeOut = !documentHandler.strikeOut
                    text: strikethroughTooltip.text
                    display: PlasmaComponents3.AbstractButton.IconOnly
                    PlasmaComponents3.ToolTip {
                        id: strikethroughTooltip
                        text: i18nc("@info:tooltip", "Strikethrough")
                    }
                }
            }

            Item { // spacer
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            PlasmaComponents3.ToolButton {
                id: removeButton
                focusPolicy: Qt.TabFocus
                icon.name: "edit-delete"
                icon.color: appletRoot.textIconColor
                onClicked: {
                    if (appletRoot.flushEditors()) Plasmoid.internalAction("remove").trigger();
                }
                Accessible.name: removeTooltip.text
                PlasmaComponents3.ToolTip {
                    id: removeTooltip
                    text: Plasmoid.internalAction("remove").text
                }
            }

            Item { // spacer
                id: spacer
                implicitWidth: Kirigami.Units.largeSpacing
            }

            PlasmaComponents3.ToolButton {
                id: settingsButton
                focusPolicy: Qt.TabFocus
                icon.name: "configure"
                icon.color: appletRoot.textIconColor
                onClicked: Plasmoid.internalAction("configure").trigger()
                text: settingsTooltip.text
                display: PlasmaComponents3.AbstractButton.IconOnly
                PlasmaComponents3.ToolTip {
                    id: settingsTooltip
                    text: Plasmoid.internalAction("configure").text
                }
            }

            PlasmaComponents3.ToolButton {
                id: pinButton
                visible: appletRoot.compactInPanel
                checkable: true
                checked: Plasmoid.configuration.pinOpen
                focusPolicy: Qt.TabFocus
                icon.name: "window-pin"
                icon.color: appletRoot.textIconColor
                text: pinTooltip.text
                display: PlasmaComponents3.AbstractButton.IconOnly
                PlasmaComponents3.ToolTip {
                    id: pinTooltip
                    text: i18nc("@action:button pin popup for panel widget", "Keep Open")
                }
                Binding {
                    target: appletRoot
                    property: "hideOnWindowDeactivate"
                    value: !Plasmoid.configuration.pinOpen
                    restoreMode: Binding.RestoreNone
                }
                onToggled: Plasmoid.configuration.pinOpen = checked
            }
        }
    }

}
