// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.folderlistmodel

PanelDialog {
    id: picker
    required property var backend
    property string profileId
    property bool showFolder
    property url fileSelection
    property string saveError
    readonly property url selection: {
        if (fileSelection.toString().length)
            return fileSelection
        if (grid.currentIndex < 0)
            return ""
        return showFolder ? images.get(grid.currentIndex, "fileUrl") ?? ""
            : presets[grid.currentIndex]?.source ?? ""
    }
    readonly property var presets: [
        { label: qsTr("Ridge"), source: "qrc:/horizon/assets/avatars/ridge.svg" },
        { label: qsTr("Orbit"), source: "qrc:/horizon/assets/avatars/orbit.svg" },
        { label: qsTr("Tide"), source: "qrc:/horizon/assets/avatars/tide.svg" },
        { label: qsTr("Sprout"), source: "qrc:/horizon/assets/avatars/sprout.svg" }
    ]
    title: qsTr("Choose avatar")
    width: 848
    height: 608
    onSelectionChanged: saveError = ""

    function save() {
        if (preview.status !== Image.Ready)
            return
        if (backend.saveProfileAvatar(profileId, selection)) {
            theme.appearance.play("confirm")
            close()
        } else {
            saveError = qsTr("Could not save this image. Try a PNG or JPEG smaller than 16 MB.")
        }
    }

    function focusGrid() {
        grid.currentIndex = grid.count > 0 ? 0 : -1
        if (grid.count > 0)
            grid.forceActiveFocus()
        else
            folderButton.forceActiveFocus()
    }

    onOpened: {
        showFolder = false
        fileSelection = ""
        saveError = ""
        focusGrid()
    }

    FolderListModel {
        id: images
        folder: picker.backend.avatarFolder
        nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp", "*.gif"]
        showDirs: false
    }
    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a local avatar folder")
        onAccepted: {
            picker.backend.avatarFolder = selectedFolder
            picker.showFolder = true
            picker.fileSelection = ""
            grid.positionViewAtBeginning()
            picker.focusGrid()
        }
        onRejected: folderButton.forceActiveFocus()
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Choose an avatar image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)")]
        onAccepted: {
            picker.fileSelection = selectedFile
            picker.saveError = ""
            applyButton.forceActiveFocus()
        }
        onRejected: fileButton.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        objectName: "avatarPickerContent"
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            NavButton {
                id: presetsButton
                theme: picker.theme
                compact: true
                text: qsTr("Built-in avatars")
                selected: !picker.showFolder
                Keys.onDownPressed: picker.focusGrid()
                KeyNavigation.right: folderButton
                onClicked: {
                    picker.showFolder = false
                    picker.fileSelection = ""
                    picker.focusGrid()
                }
            }
            NavButton {
                id: folderButton
                objectName: "profileLocalAvatars"
                theme: picker.theme
                compact: true
                symbol: "folder"
                text: qsTr("Local folder")
                selected: picker.showFolder
                Keys.onDownPressed: picker.focusGrid()
                KeyNavigation.left: presetsButton
                KeyNavigation.right: fileButton
                onClicked: {
                    if (picker.imagesFolderExists && !picker.showFolder) {
                        picker.showFolder = true
                        picker.fileSelection = ""
                        picker.focusGrid()
                    } else {
                        folderDialog.open()
                    }
                }
            }
            NavButton {
                id: fileButton
                theme: picker.theme
                compact: true
                symbol: "artwork"
                text: qsTr("Choose file")
                KeyNavigation.left: folderButton
                Keys.onDownPressed: applyButton.forceActiveFocus()
                onClicked: fileDialog.open()
            }
            Item { Layout.fillWidth: true }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: previewPane.left
                anchors.rightMargin: 24
                GridView {
                    id: grid
                    objectName: "profileAvatarGrid"
                    anchors.fill: parent
                    readonly property int columns: 4
                    cellWidth: Math.floor(width / columns)
                    cellHeight: 112
                    model: picker.showFolder ? images : picker.presets
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    keyNavigationEnabled: false
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Avatars")
                    Keys.onLeftPressed: if (currentIndex % columns > 0) moveCurrentIndexLeft()
                    Keys.onRightPressed: {
                        if (currentIndex % columns < columns - 1 && currentIndex + 1 < count)
                            moveCurrentIndexRight()
                        else
                            applyButton.forceActiveFocus()
                    }
                    Keys.onUpPressed: {
                        if (currentIndex >= columns)
                            moveCurrentIndexUp()
                        else
                            (picker.showFolder ? folderButton : presetsButton).forceActiveFocus()
                    }
                    Keys.onDownPressed: {
                        if (currentIndex + columns < count)
                            moveCurrentIndexDown()
                        else
                            applyButton.forceActiveFocus()
                    }
                    Keys.onReturnPressed: event => {
                        if (!event.isAutoRepeat)
                            picker.save()
                    }
                    Keys.onEnterPressed: event => {
                        if (!event.isAutoRepeat)
                            picker.save()
                    }
                    onCurrentIndexChanged: picker.fileSelection = ""
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: cell
                        required property int index
                        width: grid.cellWidth
                        height: grid.cellHeight
                        Accessible.role: Accessible.Button
                        Accessible.name: picker.showFolder
                            ? images.get(index, "fileBaseName") ?? ""
                            : picker.presets[index]?.label ?? ""
                        Accessible.focusable: true
                        Accessible.focused: GridView.isCurrentItem && grid.activeFocus
                        Accessible.onPressAction: {
                            grid.currentIndex = index
                            picker.save()
                        }

                        Image {
                            id: tileImage
                            anchors.centerIn: parent
                            width: 90
                            height: 90
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                            sourceSize: Qt.size(128, 128)
                            source: picker.showFolder ? images.get(cell.index, "fileUrl") ?? ""
                                : picker.presets[cell.index]?.source ?? ""
                        }
                        Rectangle {
                            anchors.fill: tileImage
                            anchors.margins: -4
                            radius: picker.theme.radius
                            color: "transparent"
                            border.width: cell.GridView.isCurrentItem ? 3 : 0
                            border.color: grid.activeFocus ? picker.theme.accent : picker.theme.lineStrong
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: {
                                picker.fileSelection = ""
                                grid.currentIndex = cell.index
                                grid.forceActiveFocus(Qt.MouseFocusReason)
                            }
                            onDoubleTapped: picker.save()
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width - 16
                    visible: grid.count === 0
                    text: qsTr("No images in this folder")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: picker.theme.muted
                }
            }

            ColumnLayout {
                id: previewPane
                anchors.right: parent.right
                anchors.top: parent.top
                width: 205
                spacing: 12
                Image {
                    id: preview
                    Layout.fillWidth: true
                    Layout.preferredHeight: 205
                    source: picker.selection
                    sourceSize: Qt.size(256, 256)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
                NavButton {
                    id: applyButton
                    objectName: "applyProfileAvatar"
                    theme: picker.theme
                    Layout.fillWidth: true
                    enabled: preview.status === Image.Ready
                    text: qsTr("Save avatar")
                    Keys.onUpPressed: fileButton.forceActiveFocus()
                    Keys.onLeftPressed: picker.focusGrid()
                    onClicked: picker.save()
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: picker.saveError || (preview.status === Image.Error ? qsTr("Image unavailable") : "")
                    color: picker.theme.danger
                    wrapMode: Text.WordWrap
                    font.pixelSize: picker.theme.noteSize
                }
            }
        }

        Legend {
            Layout.alignment: Qt.AlignRight
            theme: picker.theme
            button: picker.theme.backKey
            caption: qsTr("Back")
            interactive: true
            onActivated: picker.reject()
        }
    }
    readonly property bool imagesFolderExists: backend.avatarFolder.toString().length > 0
}
