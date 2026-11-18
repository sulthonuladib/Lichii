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
    signal activated()
    spacing: 10
    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: caption
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
        radius: legend.theme.radius
        color: legend.theme.keyBadge
        Label {
            id: keyText
            anchors.centerIn: parent
            text: legend.button
            color: legend.theme.keyBadgeText
            font.family: legend.theme.bodyFont
            font.pixelSize: 13
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
