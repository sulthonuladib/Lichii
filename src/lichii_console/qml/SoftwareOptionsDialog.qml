// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

PanelDialog {
    id: panel
    objectName: "softwareOptionsDialog"
    property var game: ({})
    property bool hasSession: false
    property bool canConfigure: false
    property bool artworkBusy: false
    property bool showingInformation: false
    property string lastGameKey
    property string focusedCommand: "play"
    readonly property string configurationDetail: game.programId
        ? qsTr("Stop the game to edit") : qsTr("Requires a program ID")
    readonly property var actions: [
        { command: "play", label: hasSession ? qsTr("Game session") : qsTr("Play"),
            symbol: "play", emphasized: true },
        { command: "information", label: qsTr("Game Information"), symbol: "about" },
        { command: "favorite", label: game.favorite
                ? qsTr("Remove from favorites") : qsTr("Add to favorites"),
            symbol: "favorite" },
        { command: "artwork", label: qsTr("Change Game Icon"), symbol: "artwork",
            enabled: !artworkBusy },
        { command: "restore-artwork", label: qsTr("Restore Game Icon"), symbol: "artwork",
            enabled: !!game.customArtwork && !artworkBusy },
        { command: "open-saves", label: qsTr("Open Save Data"), symbol: "folder",
            enabled: !!game.programId, detail: game.programId ? "" : qsTr("Requires a program ID") },
        { command: "graphics", label: qsTr("Graphics Settings"), symbol: "settings",
            enabled: canConfigure, detail: canConfigure ? "" : configurationDetail },
        { command: "cpu", label: qsTr("CPU Settings"), symbol: "settings", enabled: canConfigure,
            detail: canConfigure ? "" : configurationDetail },
        { command: "controllers", label: qsTr("Controller Profile"), symbol: "controllers",
            enabled: canConfigure, detail: canConfigure ? "" : configurationDetail },
        { command: "reset-settings", label: qsTr("Reset Game Settings"), symbol: "settings", danger: true,
            enabled: canConfigure, detail: canConfigure ? "" : configurationDetail },
        { command: "close", label: qsTr("Back"), symbol: "back" }
    ]
    readonly property var metadata: [
        { label: qsTr("Title"), value: game.title },
        { label: qsTr("Publisher"), value: game.publisher },
        { label: qsTr("Version"), value: game.version },
        { label: qsTr("Program ID"), value: game.programId },
        { label: qsTr("Format"), value: game.format },
        { label: qsTr("Location"), value: game.gamePath }
    ]

    signal commandRequested(string command)

    function focusAction(index, direction) {
        for (let i = index; i >= 0 && i < actionRows.count; i += direction) {
            const action = actionRows.itemAt(i)
            if (!action?.enabled)
                continue
            action.forceActiveFocus()
            const top = list.contentY
            if (action.y < top)
                list.contentY = action.y
            else if (action.y + action.height > top + list.height)
                list.contentY = action.y + action.height - list.height
            return true
        }
        return false
    }

    function refocus() {
        if (!opened)
            return
        if (showingInformation) {
            informationBack.forceActiveFocus()
            return
        }
        const index = Math.max(0, actions.findIndex(action => action.command === focusedCommand))
        if (!focusAction(index, -1))
            focusAction(index, 1)
    }

    function showInformation(show) {
        showingInformation = show
        if (show)
            theme.appearance.play("open")
        else
            theme.appearance.play("cancel")
        Qt.callLater(panel.refocus)
    }

    function activate(action) {
        switch (action.command) {
        case "information":
            showInformation(true)
            break
        case "close":
            reject()
            break
        default:
            theme.appearance.play(action.danger ? "warning" : "confirm")
            commandRequested(action.command)
            break
        }
    }

    title: showingInformation ? qsTr("Game Information") : (game.title || qsTr("Software Options"))
    coverArt: game.artwork?.toString() || theme.icon("cover")
    width: 640
    height: Math.min(availableLogicalHeight - 32, 642)
    closePolicy: showingInformation ? Popup.NoAutoClose : Popup.CloseOnEscape

    onOpened: {
        showingInformation = false
        if (lastGameKey !== (game.gameKey || "")) {
            lastGameKey = game.gameKey || ""
            focusedCommand = "play"
            list.contentY = 0
        }
        Qt.callLater(panel.refocus)
    }

    Shortcut {
        sequence: "Esc"
        autoRepeat: false
        enabled: panel.opened && panel.showingInformation && informationBack.activeFocus
        onActivated: panel.showInformation(false)
    }

    contentItem: ColumnLayout {
        objectName: "softwareOptionsDialog"
        spacing: 10

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: panel.showingInformation ? 1 : 0

            Flickable {
                id: list
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: menu.implicitHeight

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 6
                }

                Column {
                    id: menu
                    width: list.width

                    Repeater {
                        id: actionRows
                        model: panel.actions

                        delegate: NavButton {
                            id: row
                            required property int index
                            required property var modelData
                            objectName: "dialogAction_" + index
                            theme: panel.theme
                            width: menu.width
                            compact: true
                            text: modelData.label
                            symbol: modelData.symbol || ""
                            detail: modelData.detail || ""
                            enabled: modelData.enabled !== false
                            emphasized: !!modelData.emphasized
                            danger: !!modelData.danger
                            Keys.onUpPressed: panel.focusAction(index - 1, -1)
                            Keys.onDownPressed: panel.focusAction(index + 1, 1)
                            onActiveFocusChanged: {
                                if (activeFocus)
                                    panel.focusedCommand = modelData.command
                            }
                            onClicked: panel.activate(modelData)
                        }
                    }
                }
            }

            ColumnLayout {
                objectName: "softwareOptionDetails"
                spacing: 8

                Repeater {
                    model: panel.metadata
                    delegate: Item {
                        id: metadataRow
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(40, detailsValue.implicitHeight + 12)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("%1: %2").arg(modelData.label)
                            .arg(modelData.value || qsTr("Unavailable"))

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 16

                            Label {
                                Layout.preferredWidth: 124
                                text: metadataRow.modelData.label
                                color: panel.theme.muted
                                font.family: panel.theme.bodyFont
                                font.pixelSize: panel.theme.noteSize
                                elide: Text.ElideRight
                            }
                            Label {
                                id: detailsValue
                                Layout.fillWidth: true
                                text: metadataRow.modelData.value || qsTr("Unavailable")
                                textFormat: Text.PlainText
                                color: panel.theme.ink
                                font.family: panel.theme.bodyFont
                                font.pixelSize: panel.theme.compactSize
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: panel.theme.line
                        }
                    }
                }

                Item { Layout.fillHeight: true }
                NavButton {
                    id: informationBack
                    Layout.fillWidth: true
                    theme: panel.theme
                    compact: true
                    text: qsTr("Back to options")
                    symbol: "back"
                    onClicked: panel.showInformation(false)
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 20

            Legend {
                theme: panel.theme
                button: panel.theme.backKey
                caption: qsTr("Back")
                interactive: true
                onActivated: {
                    if (panel.showingInformation)
                        panel.showInformation(false)
                    else
                        panel.reject()
                }
            }
            Legend {
                theme: panel.theme
                button: panel.theme.confirmKey
                caption: qsTr("Select")
            }
        }
    }
}
