// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: dialog
    required property Theme theme
    property url coverArt
    readonly property int availableLogicalHeight: theme.logicalHeight

    parent: Overlay.overlay
    popupType: Popup.Item
    modal: true
    focus: true
    scale: theme.viewScale
    transformOrigin: Popup.TopLeft
    padding: 24
    spacing: 14
    width: 620
    height: implicitHeight
    x: parent ? (parent.width - width * scale) / 2 : 0
    y: parent ? (parent.height - height * scale) / 2 : 0
    closePolicy: Popup.CloseOnEscape

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: dialog.theme.motion }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: dialog.theme.motion }
    }

    background: Rectangle {
        color: dialog.theme.panel
        radius: dialog.theme.radius
        border.color: dialog.theme.line
        border.width: 1
    }
    Overlay.modal: Rectangle { color: dialog.theme.overlay }

    header: Item {
        visible: dialog.title.length > 0
        implicitHeight: visible ? Math.max(heading.implicitHeight, cover.visible ? cover.height : 0) + 36 : 0

        Image {
            id: cover
            visible: dialog.coverArt.toString().length > 0
            anchors.left: parent.left
            anchors.leftMargin: dialog.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            width: 56
            height: width
            source: dialog.coverArt
            sourceSize.width: 112
            sourceSize.height: 112
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }

        Label {
            id: heading
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: cover.visible ? cover.x + cover.width + 16 : dialog.leftPadding
            anchors.rightMargin: dialog.rightPadding
            text: dialog.title
            textFormat: Text.PlainText
            color: dialog.theme.ink
            font.family: dialog.theme.displayFont
            font.weight: Font.DemiBold
            font.pixelSize: dialog.theme.sectionTitle
            elide: Text.ElideRight
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: dialog.leftPadding
            anchors.rightMargin: dialog.rightPadding
            anchors.bottom: parent.bottom
            height: 1
            color: dialog.theme.line
        }
    }
}
