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

    ListView {
        id: view
        objectName: "tabList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        model: bar.tabs
        currentIndex: bar.tabs.activeIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        onWidthChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        spacing: Kirigami.Units.smallSpacing
        QQC2.ScrollBar.horizontal: QQC2.ScrollBar { height: 3 }
        delegate: Item {
            id: tab
            required property string noteId
            required property string title
            property bool renaming: false
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
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: tab.title
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
                view.contentX = Math.max(0, Math.min(view.contentWidth - view.width,
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
