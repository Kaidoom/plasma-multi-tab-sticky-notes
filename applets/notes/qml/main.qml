/*
    SPDX-FileCopyrightText: 2014 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014, 2015 Kai Uwe Broulik <kde@privat.broulik.de>

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

PlasmoidItem {
    id: root

    switchWidth: Kirigami.Units.gridUnit * 5
    switchHeight: Kirigami.Units.gridUnit * 5

    width: Kirigami.Units.gridUnit * 25 // default size for fullRepresentation
    height: Kirigami.Units.gridUnit * 25 // default size for fullRepresentation

    expandedOnDragHover: true

    Plasmoid.icon: "knotes-symbolic"
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    // this isn't a frameSVG, the default SVG margins take up around 7% of the frame size, so we use that
    readonly property int horizontalMargins: Math.round((fullRepresentationItem?.width || width) * 0.07)
    readonly property int verticalMargins: Math.round((fullRepresentationItem?.height || height) * 0.07)
    readonly property var mainTextArea: activeEditor ? activeEditor.textArea : null
    readonly property var activeEditor: fullRepresentationItem ? fullRepresentationItem.activeEditor : null
    readonly property bool inPanel: [PlasmaCore.Types.TopEdge, PlasmaCore.Types.RightEdge,PlasmaCore.Types.BottomEdge, PlasmaCore.Types.LeftEdge].includes(Plasmoid.location)
    readonly property bool compactInPanel: inPanel && !!compactRepresentationItem?.visible
    // In a panel when it is translucent, the panel background is used, so we use the normal text color, and remove any margins.
    property bool noBackground: false // binding in onCompleted to avoid binding loop

    // define colors used for icons in ToolButtons and for text in TextArea.
    // this is deliberately _NOT_ the theme color as we are over a known bright background!
    // except in a panel when it is translucent, the panel background is used, so we use the normal text color.
    // an unknown colour over a known colour is a bad move as you end up with white on yellow.
    readonly property color textIconColor: {
        if (noBackground) {
            return Kirigami.Theme.textColor;
        } else if (Plasmoid.configuration.color === "black" || Plasmoid.configuration.color === "translucent-light") {
            return "#dfdfdf";
        }
        return "#202020";
    }


    property string storageError: ""
    function updateStorageError() {
        storageError = activeEditor?.note.error || tabModel.error;
    }
    onActiveEditorChanged: Qt.callLater(updateStorageError)
    Connections {
        target: root.activeEditor?.note || null
        function onStateChanged() { Qt.callLater(root.updateStorageError); }
    }
    Connections {
        target: tabModel
        function onErrorChanged() { Qt.callLater(root.updateStorageError); }
    }

    TabModel {
        id: tabModel
        objectName: "tabModel"
        applet: Plasmoid
    }

    function prepareSwitch() {
        Qt.inputMethod.commit();
        return !activeEditor || activeEditor.flush();
    }
    function selectTab(id) {
        if (prepareSwitch() && tabModel.activate(id)) focusEditor();
    }
    function navigate(direction) {
        if (prepareSwitch() && tabModel.next(direction)) focusEditor();
    }
    function addTab() {
        if (prepareSwitch() && tabModel.addTab()) focusEditor();
    }
    function focusEditor() {
        Qt.callLater(() => { if (mainTextArea) mainTextArea.forceActiveFocus(); });
    }
    function flushEditors() {
        Qt.inputMethod.commit();
        if (activeEditor && !activeEditor.flush()) return false;
        return tabModel.flushAll();
    }
    function requestClose(id) {
        if (!prepareSwitch()) return;
        if (tabModel.hasText(id)) {
            closeDialog.noteId = id;
            closeDialog.open();
        } else {
            tabModel.closeTab(id);
            focusEditor();
        }
    }
    onExternalData: (mimetype, data) => {
        if (!mainTextArea) return;
        const contents = NotesHelper.fileContents(data) || data;
        mainTextArea.text = String(contents).replace(/\n/g, "<br>");
        mainTextArea.cursorPosition = mainTextArea.length;
        focusEditor();
    }
    Connections {
        target: Plasmoid
        function onActivated() { root.focusEditor(); }
        function onDestroyedChanged(destroyed) { if (destroyed) root.flushEditors(); }
    }
    Component.onDestruction: tabModel.flushAll()

    MessageDialog {
        id: closeDialog
        property string noteId
        title: i18n("Close this tab?")
        text: i18n("The note contains text. Its file will be kept and can be recovered from the widget menu.")
        buttons: MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: (button, role) => {
            if (button === MessageDialog.Discard) {
                tabModel.closeTab(noteId);
                root.focusEditor();
            }
        }
    }

    preloadFullRepresentation: true
    fullRepresentation: KSvg.SvgItem {
        id: backgroundItem
        readonly property QtObject appletInterface: root
        readonly property var activeEditor: {
            for (let i = 0; i < editors.count; ++i) {
                const loader = editors.itemAt(i);
                if (loader && loader.noteId === tabModel.activeId) return loader.item;
            }
            return null;
        }
        Layout.minimumWidth: root.inPanel && !root.compactInPanel ? -1 : Kirigami.Units.gridUnit * 5
        Layout.minimumHeight: root.inPanel && !root.compactInPanel ? -1 : Kirigami.Units.gridUnit * 5
        imagePath: root.noBackground ? "" : "widgets/notes"
        elementId: Plasmoid.configuration.color + "-notes"

        ColumnLayout {
            anchors {
                fill: parent
                leftMargin: root.noBackground ? 0 : root.horizontalMargins
                rightMargin: root.noBackground ? 0 : root.horizontalMargins
                topMargin: root.noBackground ? 0 : root.verticalMargins
                bottomMargin: root.noBackground ? 0 : root.verticalMargins
            }
            spacing: Kirigami.Units.smallSpacing
            TabBar {
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                tabs: tabModel
                appletRoot: root
                enabled: tabModel.ready
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.storageError
                type: Kirigami.MessageType.Error
            }
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Repeater {
                    id: editors
                    model: tabModel
                    delegate: Loader {
                        required property string noteId
                        required property bool visited
                        anchors.fill: parent
                        active: visited && tabModel.ready
                        visible: noteId === tabModel.activeId
                        sourceComponent: NoteEditor {
                            note: tabModel.noteForId(parent.noteId)
                            tabs: tabModel
                            appletRoot: root
                        }
                    }
                }
            }
        }
    }

    QQC2.Menu {
        id: recoveryMenu
        property var notes: []
        onAboutToShow: notes = tabModel.recoverableNotes()
        Instantiator {
            model: recoveryMenu.notes
            delegate: QQC2.MenuItem {
                required property var modelData
                text: modelData.preview || i18n("Empty note")
                onTriggered: {
                    if (root.prepareSwitch() && tabModel.recoverNote(modelData.id)) root.focusEditor();
                }
            }
            onObjectAdded: (index, object) => recoveryMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => recoveryMenu.removeItem(object)
        }
        QQC2.MenuItem {
            visible: recoveryMenu.notes.length === 0
            enabled: false
            text: i18n("No closed notes")
        }
    }

    PlasmaCore.ActionGroup {
        id: noteColorGroup
    }

    Instantiator {
        model: {
            let model = [
                {text: i18nc("@item:inmenu", "White"), color: "white"},
                {text: i18nc("@item:inmenu", "Black"), color: "black"},
                {text: i18nc("@item:inmenu", "Red"), color: "red"},
                {text: i18nc("@item:inmenu", "Orange"), color: "orange"},
                {text: i18nc("@item:inmenu", "Yellow"), color: "yellow"},
                {text: i18nc("@item:inmenu", "Green"), color: "green"},
                {text: i18nc("@item:inmenu", "Blue"), color: "blue"},
                {text: i18nc("@item:inmenu", "Pink"), color: "pink"},
                {text: i18nc("@item:inmenu", "Transparent"), color: "translucent"},
            ];
            // Explicit translucent light makes no sense in a panel since it will always be the popup background.
            if (!root.inPanel) {
                model.push({text: i18nc("@item:inmenu", "Transparent Light"), color: "translucent-light"});
            }
            return model;
        }

        onObjectAdded: (index, object) => {
            Plasmoid.contextualActions.push(object);
        }

        PlasmaCore.Action {
            required text
            required property string color

            icon.icon: NotesHelper.noteIcon(color)
            actionGroup: noteColorGroup
            checkable: true
            checked: (Plasmoid.configuration.color === color)
            // Pretend to be translucent if translucent light in a panel to allow roaming between panel and desktop.
                     || (root.inPanel && color === "translucent" && Plasmoid.configuration.color === "translucent-light")
            onTriggered: Plasmoid.configuration.color = color
        }
    }


    PlasmaCore.Action {
        id: recoverAction
        text: i18n("Recover Closed Note…")
        icon.name: "edit-undo"
        onTriggered: recoveryMenu.popup()
    }
    Component.onCompleted: {
        tabModel.initialize();
        Plasmoid.contextualActions.push(recoverAction);
        root.noBackground = Qt.binding(() => inPanel && (Plasmoid.configuration.color === "translucent" || Plasmoid.configuration.color === "translucent-light"));
    }
}
