// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PanelDialog {
    id: panel
    objectName: "actionDialog"
    property string description
    property var actions: []
    property bool initialLast
    property var pendingAction: null
    readonly property bool confirmation: initialLast && actions.length === 2
    signal dismissed()

    function focusAction(index, direction) {
        for (let i = index; i >= 0 && i < actionButtons.count; i += direction) {
            const button = actionButtons.itemAt(i)
            if (button && button.enabled) {
                button.forceActiveFocus()
                return
            }
        }
    }

    onOpened: Qt.callLater(() => {
        if (panel.opened) {
            list.contentY = 0
            const selected = actions.findIndex(action => action.selected && action.enabled !== false)
            focusAction(initialLast ? actionButtons.count - 1 : Math.max(0, selected),
                initialLast ? -1 : 1)
        }
    })
    onClosed: {
        const callback = pendingAction
        pendingAction = null
        dismissed()
        if (callback)
            Qt.callLater(callback)
    }

    contentItem: ColumnLayout {
        objectName: "actionDialogContent"
        spacing: 16
        Keys.onEscapePressed: panel.reject()

        Label {
            visible: panel.description.length > 0
            Layout.fillWidth: true
            text: panel.description
            textFormat: Text.PlainText
            color: panel.theme.muted
            wrapMode: Text.Wrap
            font.pixelSize: panel.theme.compactSize
            maximumLineCount: 5
            elide: Text.ElideRight
        }

        Flickable {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(buttons.implicitHeight,
                Math.max(130, panel.availableLogicalHeight * 0.56))
            clip: true
            contentWidth: width
            contentHeight: buttons.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            GridLayout {
                id: buttons
                width: list.width - (list.contentHeight > list.height ? 12 : 0)
                columns: panel.confirmation ? 2 : 1
                columnSpacing: panel.confirmation ? 10 : 0
                rowSpacing: 0
                Repeater {
                    id: actionButtons
                    model: panel.actions
                    Button {
                        id: row
                        required property int index
                        required property var modelData
                        objectName: "dialogAction_" + index
                        Layout.fillWidth: true
                        Layout.preferredWidth: panel.confirmation ? (buttons.width - buttons.columnSpacing) / 2
                            : implicitWidth
                        Layout.preferredHeight: panel.confirmation ? 65 : 56
                        leftPadding: 14
                        rightPadding: 14
                        text: modelData.label
                        enabled: modelData.enabled !== false
                        Accessible.description: modelData.reason || modelData.detail || ""
                        focusPolicy: Qt.StrongFocus

                        Keys.onUpPressed: panel.focusAction(index - 1, -1)
                        Keys.onDownPressed: panel.focusAction(index + 1, 1)
                        Keys.onLeftPressed: if (panel.confirmation) panel.focusAction(index - 1, -1)
                        Keys.onRightPressed: if (panel.confirmation) panel.focusAction(index + 1, 1)
                        Keys.onTabPressed: {
                            panel.focusAction(index + 1, 1)
                            if (activeFocus)
                                panel.focusAction(0, 1)
                        }
                        Keys.onBacktabPressed: {
                            panel.focusAction(index - 1, -1)
                            if (activeFocus)
                                panel.focusAction(actionButtons.count - 1, -1)
                        }
                        Keys.onReturnPressed: event => {
                            if (!event.isAutoRepeat)
                                clicked()
                        }
                        Keys.onEnterPressed: event => {
                            if (!event.isAutoRepeat)
                                clicked()
                        }
                        onActiveFocusChanged: {
                            if (!activeFocus || list.height <= 0)
                                return
                            if (y < list.contentY)
                                list.contentY = y
                            else if (y + height > list.contentY + list.height)
                                list.contentY = y + height - list.height
                        }
                        onClicked: {
                            panel.pendingAction = modelData.action || null
                            if (modelData.sound !== false)
                                panel.theme.appearance.play(panel.pendingAction ? "confirm" : "cancel")
                            panel.close()
                        }

                        background: Rectangle {
                            color: row.activeFocus ? panel.theme.panel
                                : row.hovered || row.down ? panel.theme.hover : "transparent"
                            border.color: panel.theme.accent
                            border.width: row.activeFocus ? panel.theme.focusWidth : 0

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: !panel.confirmation && !row.activeFocus
                                    && row.index + 1 < actionButtons.count
                                height: 1
                                color: panel.theme.line
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                text: row.text
                                textFormat: Text.PlainText
                                color: !row.enabled ? panel.theme.muted
                                    : row.modelData.danger ? panel.theme.danger : panel.theme.ink
                                opacity: row.enabled ? 1 : 0.68
                                horizontalAlignment: panel.confirmation ? Text.AlignHCenter : Text.AlignLeft
                                font.family: panel.theme.bodyFont
                                font.pixelSize: panel.theme.bodySize
                                elide: Text.ElideRight
                            }
                            Label {
                                visible: text.length > 0
                                Layout.maximumWidth: row.width * 0.34
                                text: row.modelData.detail || ""
                                textFormat: Text.PlainText
                                color: panel.theme.muted
                                font.pixelSize: panel.theme.noteSize
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
