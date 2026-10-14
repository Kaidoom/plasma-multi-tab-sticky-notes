/* SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3

RowLayout {
    id: bar
    required property TabModel tabs
    required property var appletRoot
    spacing: Kirigami.Units.smallSpacing
    implicitHeight: Kirigami.Units.gridUnit * 1.6
    property string draggedId: ""
    property int draggedIndex: -1
    property int insertionIndex: -1
    property real insertionX: 0
    property point dragPosition: Qt.point(0, 0)
    property string renamingId: ""
    readonly property real scrollEdge: Math.min(Kirigami.Units.gridUnit, view.width / 4)

    function updateInsertion() {
        insertionIndex = -1;
        if (!draggedId || dragPosition.y < 0 || dragPosition.y > view.height
                || dragPosition.x < -scrollEdge || dragPosition.x > view.width + scrollEdge) return;
        const x = view.contentX + Math.max(0, Math.min(view.width - 1, dragPosition.x));
        let index = view.indexAt(x, view.height / 2);
        // In the spacing between delegates, use the adjacent tab's edge.
        if (index < 0) index = view.indexAt(x - view.spacing, view.height / 2);
        if (index < 0) index = view.indexAt(x + view.spacing, view.height / 2);
        if (index < 0) {
            const first = view.itemAtIndex(0);
            const last = view.itemAtIndex(bar.tabs.count - 1);
            if (first && x < first.x) index = 0;
            else if (last && x >= last.x + last.width) index = bar.tabs.count - 1;
        }
        const item = view.itemAtIndex(index);
        if (!item) return;
        const after = x > item.x + item.width / 2;
        insertionIndex = index + (after ? 1 : 0);
        insertionX = Math.max(0, Math.min(view.width - 2,
            item.x - view.contentX + (after ? item.width + view.spacing / 2 : -view.spacing / 2)));
    }

    function cancelDrag() {
        draggedId = "";
        draggedIndex = -1;
        insertionIndex = -1;
    }

    function finishDrag() {
        if (draggedId && insertionIndex >= 0 && dragPosition.x >= 0 && dragPosition.x <= view.width) {
            const destination = insertionIndex > draggedIndex ? insertionIndex - 1 : insertionIndex;
            if (tabs.moveTab(draggedId, destination)) {
                Qt.callLater(() => view.positionViewAtIndex(destination, ListView.Contain));
            }
        }
        cancelDrag();
    }

    Connections {
        target: bar.tabs
        function onCountChanged() { bar.cancelDrag(); }
    }
    Shortcut {
        sequence: "Esc"
        enabled: bar.draggedId.length > 0
        onActivated: bar.cancelDrag()
    }
    Timer {
        interval: 40
        repeat: true
        running: bar.draggedId.length > 0 && bar.insertionIndex >= 0
                 && (bar.dragPosition.x < bar.scrollEdge || bar.dragPosition.x > view.width - bar.scrollEdge)
        onTriggered: {
            const direction = bar.dragPosition.x < bar.scrollEdge ? -1 : 1;
            view.contentX = Math.max(view.originX, Math.min(view.originX + Math.max(0, view.contentWidth - view.width),
                view.contentX + direction * Kirigami.Units.smallSpacing * 2));
            bar.updateInsertion();
        }
    }

    ListView {
        id: view
        objectName: "tabList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        orientation: ListView.Horizontal
        // Mouse title drags reorder; wheel, scrollbar and touch still scroll.
        acceptedButtons: Qt.NoButton
        boundsBehavior: Flickable.StopAtBounds
        model: bar.tabs
        currentIndex: bar.tabs.activeIndex
        onCurrentIndexChanged: if (!bar.draggedId) positionViewAtIndex(currentIndex, ListView.Contain)
        onWidthChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        spacing: Kirigami.Units.smallSpacing
        QQC2.ScrollBar.horizontal: QQC2.ScrollBar { height: 3 }
        DragHandler {
            id: reorderDrag
            objectName: "tabReorderDrag"
            parent: view // Keep the gesture attached while tab delegates scroll out of view.
            target: null
            enabled: bar.renamingId.length === 0 && bar.tabs.count > 1
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            yAxis.enabled: false
            cursorShape: active ? Qt.ClosedHandCursor : Qt.ArrowCursor
            onActiveChanged: {
                if (!active) return;
                const p = view.mapFromItem(null, centroid.scenePressPosition.x, centroid.scenePressPosition.y);
                const index = view.indexAt(p.x + view.contentX, p.y);
                const item = view.itemAtIndex(index);
                if (!item || item.mapFromItem(view, p.x, p.y).x >= item.dragWidth) return;
                bar.draggedId = item.noteId;
                bar.draggedIndex = index;
                bar.dragPosition = view.mapFromItem(null, centroid.scenePosition.x, centroid.scenePosition.y);
                bar.updateInsertion();
            }
            onCentroidChanged: {
                if (!active || !bar.draggedId) return;
                bar.dragPosition = view.mapFromItem(null, centroid.scenePosition.x, centroid.scenePosition.y);
                bar.updateInsertion();
            }
            onGrabChanged: (transition, point) => {
                if (transition === PointerDevice.UngrabExclusive) bar.finishDrag();
            }
            onCanceled: bar.cancelDrag()
        }
        Rectangle {
            objectName: "tabInsertionMarker"
            parent: view
            visible: bar.draggedId.length > 0 && bar.insertionIndex >= 0
            x: bar.insertionX
            width: 2
            height: view.height
            color: bar.appletRoot.textIconColor
            z: 2
        }
        delegate: Item {
            id: tab
            required property string noteId
            required property string title
            property bool renaming: false
            readonly property real dragWidth: titleButton.width
            opacity: noteId === bar.draggedId ? 0.5 : 1
            onRenamingChanged: {
                if (renaming) bar.renamingId = noteId;
                else if (bar.renamingId === noteId) bar.renamingId = "";
            }
            Component.onDestruction: if (bar.renamingId === noteId) bar.renamingId = ""
            width: Math.max(24, Math.min(view.width, Math.max(Kirigami.Units.gridUnit * 4, Math.min(Kirigami.Units.gridUnit * 9, label.implicitWidth + closeButton.width + Kirigami.Units.largeSpacing))))
            height: view.height
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: bar.appletRoot.textIconColor
                opacity: tab.noteId === bar.tabs.activeId ? 0.7 : 0
            }
            QQC2.AbstractButton {
                id: titleButton
                objectName: "tabTitleButton"
                anchors { left: parent.left; right: closeButton.left; top: parent.top; bottom: parent.bottom }
                visible: !tab.renaming
                Accessible.name: tab.title
                onClicked: bar.appletRoot.selectTab(tab.noteId)
                contentItem: QQC2.Label {
                    id: label
                    text: tab.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: bar.appletRoot.textIconColor
                    font.bold: tab.noteId === bar.tabs.activeId
                }
                onDoubleClicked: Qt.callLater(() => {
                    tab.renaming = true;
                    rename.text = tab.title;
                    rename.forceActiveFocus();
                    rename.selectAll();
                })
                QQC2.ToolTip.visible: hovered && !bar.draggedId
                QQC2.ToolTip.text: i18n("%1\nDrag to reorder", tab.title)
                QQC2.ToolTip.delay: 1000
            }
            PlasmaComponents3.TextField {
                id: rename
                objectName: "tabRenameField"
                anchors.fill: titleButton
                visible: tab.renaming
                onAccepted: {
                    bar.tabs.renameTab(tab.noteId, text);
                    tab.renaming = false;
                    bar.appletRoot.focusEditor();
                }
                Keys.onEscapePressed: {
                    tab.renaming = false;
                    bar.appletRoot.focusEditor();
                }
                onActiveFocusChanged: {
                    if (!activeFocus && tab.renaming) {
                        bar.tabs.renameTab(tab.noteId, text);
                        tab.renaming = false;
                    }
                }
            }
            PlasmaComponents3.ToolButton {
                id: closeButton
                objectName: "closeTabButton"
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                width: Kirigami.Units.gridUnit * 1.2
                height: Math.min(width, parent.height)
                icon.name: "window-close"
                icon.color: bar.appletRoot.textIconColor
                display: PlasmaComponents3.AbstractButton.IconOnly
                text: i18n("Close tab")
                onClicked: bar.appletRoot.requestClose(tab.noteId)
            }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => {
                view.contentX = Math.max(view.originX, Math.min(view.originX + Math.max(0, view.contentWidth - view.width),
                    view.contentX - (event.angleDelta.x || event.angleDelta.y)));
                event.accepted = true;
            }
        }
    }
    PlasmaComponents3.ToolButton {
        objectName: "addTabButton"
        Layout.preferredWidth: bar.implicitHeight
        Layout.preferredHeight: bar.implicitHeight
        icon.name: "list-add"
        icon.color: bar.appletRoot.textIconColor
        display: PlasmaComponents3.AbstractButton.IconOnly
        text: i18n("New tab")
        onClicked: bar.appletRoot.addTab()
    }
}
