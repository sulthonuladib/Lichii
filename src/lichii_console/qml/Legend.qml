// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: legend
    required property Theme theme
    required property string button
    required property string caption
    property bool interactive
    readonly property string family: theme.gamepadFamily
    readonly property bool faceButton: family.length > 0 && /^[ABXY×○□△+]$/.test(button)
    readonly property color buttonInk: {
        if (!faceButton)
            return theme.keyBadgeText
        if (family === "xbox")
            return ({ A: "#a8eab0", B: "#ffc3bd", X: "#9bceff", Y: "#ffe099" })[button] ?? "#ffffff"
        if (family === "playstation")
            return ({ "×": "#add0ff", "○": "#ffb8bf", "□": "#e8bbfa", "△": "#97e0ce" })[button] ?? "#ffffff"
        return theme.ink
    }
    signal activated()
    spacing: 10
    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: caption
    Accessible.description: button
    Accessible.onPressAction: if (interactive) activated()

    HoverHandler {
        id: pointer
        enabled: legend.interactive
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        enabled: legend.interactive
        onTapped: legend.activated()
    }

    Rectangle {
        Layout.preferredWidth: Math.max(legend.theme.keyBadgeSize, keyText.implicitWidth + 10)
        Layout.preferredHeight: legend.theme.keyBadgeSize
        radius: legend.faceButton ? height / 2 : legend.theme.radius
        color: legend.faceButton && (legend.family === "xbox" || legend.family === "playstation")
            ? "#303334" : legend.theme.keyBadge
        border.width: legend.faceButton ? 1 : 0
        border.color: legend.family === "nintendo" ? legend.theme.lineStrong : "#525757"
        Label {
            id: keyText
            anchors.centerIn: parent
            text: legend.button
            color: legend.buttonInk
            font.family: legend.theme.bodyFont
            font.pixelSize: legend.faceButton ? 16 : 13
            font.weight: Font.DemiBold
        }
    }
    Label {
        text: legend.caption
        color: pointer.hovered ? legend.theme.ink : legend.theme.muted
        font.family: legend.theme.bodyFont
        font.pixelSize: legend.theme.noteSize
        verticalAlignment: Text.AlignVCenter
    }
}
