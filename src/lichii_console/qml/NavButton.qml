// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: control
    required property Theme theme
    property string symbol
    property string detail
    property bool selected
    property bool emphasized
    property bool compact
    property bool danger
    property int horizontalInset: 14

    focusPolicy: Qt.StrongFocus
    implicitHeight: compact ? 40 : 52
    implicitWidth: Math.max(92, label.implicitWidth + 2 * horizontalInset
                             + (symbol.length ? 35 : 0) + (detail.length ? value.implicitWidth + 18 : 0))
    leftPadding: horizontalInset
    rightPadding: horizontalInset
    Accessible.name: text
    Accessible.description: detail
    Keys.onReturnPressed: event => {
        if (!event.isAutoRepeat)
            control.clicked()
    }
    Keys.onEnterPressed: event => {
        if (!event.isAutoRepeat)
            control.clicked()
    }

    background: Rectangle {
        color: control.activeFocus ? control.theme.panel
            : control.hovered || control.down || control.selected ? control.theme.hover : "transparent"
        border.width: control.activeFocus ? control.theme.focusWidth : 0
        border.color: control.theme.accent
        opacity: control.enabled ? 1 : 0.48

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            visible: !control.activeFocus
            height: 1
            color: control.theme.line
        }
    }

    contentItem: RowLayout {
        spacing: control.compact ? 9 : 12
        Image {
            visible: control.symbol.length > 0
            source: visible ? control.theme.icon(control.symbol) : ""
            sourceSize.width: control.theme.iconSize
            sourceSize.height: control.theme.iconSize
            Layout.preferredWidth: control.theme.iconSize
            Layout.preferredHeight: control.theme.iconSize
            fillMode: Image.PreserveAspectFit
        }
        Label {
            id: label
            Layout.fillWidth: true
            text: control.text
            textFormat: Text.PlainText
            color: !control.enabled ? control.theme.muted
                : control.danger ? control.theme.danger : control.theme.ink
            font.family: control.theme.bodyFont
            font.pixelSize: control.compact ? control.theme.compactSize : control.theme.bodySize
            font.weight: control.emphasized || control.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }
        Label {
            id: value
            visible: control.detail.length > 0
            color: control.theme.muted
            text: control.detail
            textFormat: Text.PlainText
            font.pixelSize: control.theme.noteSize
            Layout.maximumWidth: control.width * 0.36
            elide: Text.ElideRight
        }
    }
}
