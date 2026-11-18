// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: tile
    required property Theme theme
    required property var game
    property bool selected
    property bool listStyle
    property bool widescreen
    property bool showCaption: false
    property bool focused
    property int contentInset: 0
    property int frameInset: theme.homeFocusInset
    readonly property bool hovered: pointer.hovered
    signal picked()
    signal launched()
    signal optionsRequested()

    Accessible.role: Accessible.Button
    Accessible.name: game.title || qsTr("Untitled game")
    Accessible.focusable: true
    Accessible.focused: focused
    Accessible.onPressAction: launched()

    Rectangle {
        id: art
        objectName: "gameTileCover"
        x: tile.contentInset
        y: tile.contentInset
        width: tile.listStyle ? height : parent.width - tile.contentInset * 2
        height: tile.listStyle ? parent.height - tile.contentInset * 2
            : tile.widescreen ? parent.height - 34 - tile.contentInset * 2 : width
        color: tile.theme.slot

        Image {
            id: cover
            anchors.fill: parent
            source: tile.game.artwork || ""
            sourceSize.width: 512
            sourceSize.height: 512
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }
        Image {
            anchors.fill: parent
            visible: cover.status !== Image.Ready
            source: tile.theme.icon("cover")
            sourceSize.width: 256
            sourceSize.height: 256
            fillMode: Image.PreserveAspectCrop
        }
    }

    Rectangle {
        id: frame
        anchors.fill: art
        anchors.margins: -tile.frameInset
        visible: tile.focused
        color: "transparent"
        border.width: tile.theme.homeFocusWidth
        border.color: tile.theme.accent
    }

    Label {
        id: caption
        visible: tile.showCaption
        textFormat: Text.PlainText
        text: tile.game.title || qsTr("Untitled game")
        color: tile.focused ? tile.theme.link : tile.theme.ink
        font.family: tile.theme.bodyFont
        font.pixelSize: tile.listStyle ? tile.theme.bodySize : tile.theme.noteSize
        font.weight: tile.selected ? Font.DemiBold : Font.Normal
        elide: Text.ElideRight
        x: tile.listStyle ? art.x + art.width + 16 : art.x
        y: tile.listStyle ? (heightAvailable - height) / 2 : art.y + art.height + 6
        width: parent.width - x - 3 - (favorite.visible ? 25 : 0)
        readonly property real heightAvailable: parent.height
    }
    Image {
        id: favorite
        visible: tile.showCaption && !!tile.game.favorite
        source: visible ? tile.theme.icon("favorite") : ""
        anchors.right: parent.right
        anchors.verticalCenter: caption.verticalCenter
        anchors.rightMargin: 5
        width: 17
        height: 17
        sourceSize.width: 20
        sourceSize.height: 20
    }

    HoverHandler {
        id: pointer
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: tile.picked()
        onDoubleTapped: tile.launched()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: tile.optionsRequested()
    }
}
