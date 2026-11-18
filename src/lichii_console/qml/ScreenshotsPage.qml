// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt.labs.folderlistmodel

Item {
    id: page
    objectName: "screenshotsScreen"
    required property Theme theme
    required property url folder
    readonly property bool previewVisible: imagePreview.visible
    readonly property FolderListModel screenshots: files.item as FolderListModel
    signal openFolder()
    signal backRequested()

    function enter() {
        if (grid.count)
            grid.forceActiveFocus()
        else
            open.forceActiveFocus()
    }

    function inspect(index) {
        const images = screenshots
        if (!images || index < 0 || index >= grid.count)
            return
        imagePreview.source = images.get(index, "fileUrl")
        imagePreview.title = images.get(index, "fileName")
        imagePreview.open()
    }

    Loader {
        id: files
        active: page.visible && page.folder.toString().length > 0
        sourceComponent: FolderListModel {
            folder: page.folder
            nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp"]
            showDirs: false
            sortField: FolderListModel.Time
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: page.theme.gutter
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            NavButton {
                id: back
                theme: page.theme
                compact: true
                symbol: "back"
                text: qsTr("Back")
                KeyNavigation.right: open
                onClicked: page.backRequested()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Screenshots")
                color: page.theme.ink
                font.family: page.theme.displayFont
                font.pixelSize: page.theme.screenTitle
            }
            NavButton {
                id: open
                theme: page.theme
                compact: true
                symbol: "folder"
                text: qsTr("Open folder")
                KeyNavigation.left: back
                KeyNavigation.down: grid.visible ? grid : null
                onClicked: page.openFolder()
            }
        }
        GridView {
            id: grid
            objectName: "screenshotsGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property int columns: Math.max(2, Math.floor(width / 230))
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 0.70 + 38)
            model: page.screenshots
            visible: count > 0
            keyNavigationEnabled: false
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            onCountChanged: {
                if (count > 0 && currentIndex < 0)
                    currentIndex = 0
                if (!count && activeFocus)
                    open.forceActiveFocus()
            }

            Keys.onLeftPressed: {
                if (currentIndex % columns > 0)
                    moveCurrentIndexLeft()
                else
                    back.forceActiveFocus()
            }
            Keys.onRightPressed: {
                if (currentIndex % columns < columns - 1 && currentIndex + 1 < count)
                    moveCurrentIndexRight()
                else
                    open.forceActiveFocus()
            }
            Keys.onUpPressed: {
                if (currentIndex >= columns)
                    moveCurrentIndexUp()
                else
                    open.forceActiveFocus()
            }
            Keys.onDownPressed: {
                if (currentIndex + columns < count)
                    moveCurrentIndexDown()
            }
            Keys.onReturnPressed: event => {
                if (!event.isAutoRepeat)
                    page.inspect(currentIndex)
            }
            Keys.onEnterPressed: event => {
                if (!event.isAutoRepeat)
                    page.inspect(currentIndex)
            }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: GameTile {
                required property int index
                required property url fileUrl
                required property string fileName
                theme: page.theme
                game: ({ title: fileName, artwork: fileUrl })
                widescreen: true
                width: grid.cellWidth - 12
                height: grid.cellHeight - 12
                selected: GridView.isCurrentItem
                focused: selected && grid.activeFocus
                onPicked: {
                    grid.currentIndex = index
                    grid.forceActiveFocus(Qt.MouseFocusReason)
                }
                onLaunched: page.inspect(index)
                onOptionsRequested: page.inspect(index)
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !grid.visible
            text: page.screenshots?.status === FolderListModel.Loading ? qsTr("Looking for screenshots…")
                : qsTr("Screenshots captured in Lichii will appear here.")
            color: page.theme.muted
            font.pixelSize: 16
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    PanelDialog {
        id: imagePreview
        theme: page.theme
        width: 920
        property url source
        onOpened: closePreview.forceActiveFocus()
        onClosed: page.enter()
        contentItem: ColumnLayout {
            spacing: 12
            Image {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(420, imagePreview.availableLogicalHeight * 0.59)
                source: imagePreview.visible ? imagePreview.source : ""
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 1280
                sourceSize.height: 720
                asynchronous: true
            }
            NavButton {
                id: closePreview
                Layout.alignment: Qt.AlignRight
                theme: page.theme
                compact: true
                text: qsTr("Back")
                symbol: "back"
                onClicked: imagePreview.close()
            }
        }
    }
}
