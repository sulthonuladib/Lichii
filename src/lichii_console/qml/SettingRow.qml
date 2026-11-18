// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: row
    required property Theme theme
    required property var setting
    property bool highlighted: false
    property bool adjusting: false
    property int displayValue: 0
    signal selected()
    signal activated()
    signal valueEdited(int value)

    readonly property bool readOnly: setting.type === "description" || setting.type === "warning"
    readonly property bool interactive: !readOnly && setting.enabled !== false
    readonly property bool isSlider: setting.type === "slider"
    readonly property bool isToggle: setting.type === "toggle"
    implicitHeight: Math.max(60, content.implicitHeight + 22)
    height: implicitHeight

    Accessible.ignored: isSlider
    Accessible.role: readOnly ? Accessible.StaticText
        : isToggle ? Accessible.CheckBox : Accessible.Button
    Accessible.name: setting.label
    Accessible.description: detail.text
    Accessible.focused: highlighted
    Accessible.checked: isToggle && setting.value
    Accessible.readOnly: !interactive
    Accessible.onPressAction: trigger()
    Accessible.onToggleAction: trigger()

    function trigger() {
        if (interactive) {
            selected()
            activated()
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 1
        color: row.highlighted ? row.theme.panel
            : hover.hovered ? row.theme.hover : "transparent"
        border.width: row.highlighted ? 2 : 0
        border.color: row.theme.accent
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: row.theme.line
    }

    HoverHandler {
        id: hover
        enabled: row.interactive
    }

    MouseArea {
        anchors.fill: parent
        enabled: row.interactive
        cursorShape: Qt.PointingHandCursor
        onClicked: row.trigger()
    }

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: row.setting.label
                textFormat: Text.PlainText
                color: row.setting.type === "warning" ? row.theme.danger
                    : row.interactive || row.readOnly ? row.theme.ink : row.theme.muted
                font.family: row.theme.bodyFont
                font.pixelSize: row.theme.bodySize
                wrapMode: Text.Wrap
            }

            Label {
                id: detail
                Layout.fillWidth: true
                visible: text.length > 0
                text: [row.setting.description,
                    row.setting.inherited ? qsTr("Using global value") : "",
                    row.setting.timing,
                    row.adjusting ? qsTr("Use Left and Right to adjust.") : ""]
                    .filter(value => value).join("  ·  ")
                textFormat: Text.PlainText
                color: row.theme.muted
                font.family: row.theme.bodyFont
                font.pixelSize: row.theme.noteSize
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            visible: !row.readOnly
            spacing: 10
            Layout.maximumWidth: row.width * 0.43

            Rectangle {
                visible: row.isToggle
                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 2
                color: row.setting.value ? row.theme.accent : "transparent"
                border.width: row.setting.value ? 0 : 1
                border.color: row.theme.muted
            }

            Label {
                Layout.maximumWidth: row.width * 0.38
                Layout.minimumWidth: row.isSlider ? 60 : 0
                text: {
                    switch (row.setting.type) {
                    case "toggle": return row.setting.value ? qsTr("On") : qsTr("Off")
                    case "choice": return row.setting.options[row.setting.value] ?? qsTr("Choose")
                    case "slider": return String(slider.pressed ? Math.round(slider.value) : row.displayValue)
                    case "reset": return row.setting.button || qsTr("Reset")
                    default: return row.setting.button || qsTr("Open")
                    }
                }
                textFormat: Text.PlainText
                font.family: row.theme.bodyFont
                font.pixelSize: row.theme.compactSize
                font.weight: Font.Medium
                color: row.interactive ? row.theme.ink : row.theme.muted
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
            }

            Slider {
                id: slider
                objectName: row.isSlider ? "settingsSlider" : ""
                visible: row.isSlider
                enabled: row.interactive && row.isSlider
                Layout.preferredWidth: 146
                Layout.preferredHeight: 36
                leftPadding: 7
                rightPadding: 7
                focusPolicy: Qt.NoFocus
                from: row.setting.min ?? 0
                to: row.setting.max ?? 100
                stepSize: row.setting.step ?? 1
                snapMode: Slider.SnapAlways
                value: row.displayValue
                Accessible.name: row.setting.label
                Accessible.focused: row.highlighted

                onPressedChanged: {
                    if (pressed) {
                        row.selected()
                    } else if (Math.round(value) !== row.setting.value) {
                        row.valueEdited(Math.round(value))
                    }
                }

                background: Rectangle {
                    x: slider.leftPadding
                    y: (slider.height - height) / 2
                    width: slider.availableWidth
                    height: 3
                    color: row.theme.lineStrong

                    Rectangle {
                        width: parent.width * slider.visualPosition
                        height: parent.height
                        color: slider.enabled ? row.theme.accent : row.theme.muted
                    }
                }

                handle: Rectangle {
                    x: slider.leftPadding + slider.visualPosition * slider.availableWidth - width / 2
                    y: (slider.height - height) / 2
                    width: 12
                    height: 16
                    radius: 2
                    color: row.adjusting || slider.pressed ? row.theme.accent
                        : slider.enabled ? row.theme.ink : row.theme.muted
                }
            }
        }
    }
}
