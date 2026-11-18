// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page
    objectName: "homeScreen"
    required property Theme theme
    required property var library
    property string sessionState: "idle"
    property date now: new Date()
    property string profileName
    property string statusText
    property bool hasNotification
    property bool networkKnown
    property bool networkOnline
    property int batteryLevel: -1
    property bool batteryCharging
    readonly property var games: library.homeGames
    readonly property int gameCount: games.length
    readonly property var selectedGame: games[shelf.currentIndex] || ({})
    readonly property bool gameFocused: shelf.activeFocus && shelf.currentIndex >= 0
    readonly property bool hasGames: gameCount > 0
    readonly property bool hasSession: sessionState !== "idle"
    readonly property string confirmCaption: gameFocused ? (hasSession ? qsTr("Game session") : qsTr("Play"))
        : addFolderButton.activeFocus ? qsTr("Add folder") : browse.activeFocus ? qsTr("Browse") : qsTr("Open")
    property Item lastCommand

    signal selectGame(var game)
    signal showLibrary()
    signal addFolder()
    signal gameAction()
    signal optionsRequested()
    signal showControllers()
    signal showSettings()
    signal showScreenshots()
    signal showProfile()
    signal showNotifications()
    signal showPower()
    signal showSession()
    signal showDesktop()

    component CommandSlot: Item {
        required property Button control
    }

    function ownsSelectionFocus(item) {
        return item === shelf || item === addFolderButton
    }

    function enter() {
        if (hasGames) {
            if (shelf.currentIndex < 0)
                restore(0)
            shelf.forceActiveFocus()
        } else {
            addFolderButton.forceActiveFocus()
        }
    }

    function restore(index) {
        if (index >= 0 && index < gameCount && shelf.currentIndex !== index) {
            shelf.currentIndex = index
            shelf.positionViewAtIndex(index, ListView.Contain)
        }
    }

    function pick(index) {
        if (index >= 0 && index < gameCount) {
            shelf.currentIndex = index
            selectGame(games[index])
        }
    }

    function focusCommands() {
        const control = lastCommand && lastCommand.visible ? lastCommand : commandAt(0)
        if (control)
            control.forceActiveFocus()
    }

    function commandAt(index: int): Button {
        const slot = commandButtons.itemAt(index) as CommandSlot
        return slot?.visible ? slot.control : null
    }

    Label {
        anchors.left: profile.right
        anchors.leftMargin: 16
        y: 24
        height: 44
        text: qsTr("Lichii")
        color: page.theme.ink
        font.family: page.theme.displayFont
        font.pixelSize: page.theme.screenTitle
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
    }

    RowLayout {
        objectName: "homeStatus"
        anchors.right: parent.right
        anchors.rightMargin: page.theme.gutter
        anchors.verticalCenter: profile.verticalCenter
        spacing: 14

        Label {
            visible: page.hasSession
            text: page.statusText
            color: page.theme.accent
            font.pixelSize: page.theme.noteSize
        }
        Label {
            visible: page.networkKnown && !page.networkOnline
            text: qsTr("Offline")
            color: page.theme.muted
            font.pixelSize: page.theme.noteSize
        }
        Label {
            visible: page.batteryLevel >= 0
            text: page.batteryCharging ? qsTr("%1% · AC").arg(page.batteryLevel)
                : qsTr("%1%").arg(page.batteryLevel)
            color: page.theme.muted
            font.pixelSize: page.theme.noteSize
            Accessible.name: page.batteryCharging ? qsTr("Battery at %1%, on AC power").arg(page.batteryLevel)
                : qsTr("Battery at %1%").arg(page.batteryLevel)
        }
        Label {
            objectName: "homeClock"
            text: Qt.formatTime(page.now, Qt.locale().timeFormat(Locale.ShortFormat))
            color: page.theme.ink
            font.pixelSize: page.theme.compactSize
            font.weight: Font.DemiBold
        }
    }

    Button {
        id: profile
        objectName: "homeProfile"
        x: page.theme.gutter
        y: 24
        width: 44
        height: 44
        padding: 9
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: page.profileName || qsTr("Profile")
        Keys.onDownPressed: page.enter()
        Keys.onReturnPressed: event => { if (!event.isAutoRepeat) clicked() }
        Keys.onEnterPressed: event => { if (!event.isAutoRepeat) clicked() }
        onClicked: page.showProfile()

        background: Rectangle {
            radius: width / 2
            color: profile.hovered ? page.theme.hover : page.theme.surface
            border.width: profile.activeFocus ? page.theme.focusWidth : 1
            border.color: profile.activeFocus ? page.theme.accent : page.theme.line
        }
        contentItem: Image {
            source: page.theme.icon("profile")
            sourceSize.width: 32
            sourceSize.height: 32
            fillMode: Image.PreserveAspectFit
        }
    }

    Rectangle {
        x: page.theme.gutter
        y: page.theme.homeHeader
        width: parent.width - 2 * x
        height: 1
        color: page.theme.line
    }

    Label {
        x: page.theme.gutter
        y: 111
        height: 42
        text: qsTr("Games")
        color: page.theme.ink
        font.family: page.theme.displayFont
        font.pixelSize: page.theme.homeTitle
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
    }

    NavButton {
        id: browse
        objectName: "allSoftwareAction"
        x: page.theme.gutter
        y: page.theme.homeShelfY + page.theme.homeTile + 22
        compact: true
        symbol: "library"
        text: qsTr("All Software")
        theme: page.theme
        background: Rectangle {
            radius: page.theme.radius
            color: browse.hovered || browse.activeFocus ? page.theme.surface : "transparent"
            border.color: browse.activeFocus ? page.theme.accent : page.theme.line
            border.width: browse.activeFocus ? page.theme.focusWidth : 1
        }
        Keys.onUpPressed: page.enter()
        Keys.onLeftPressed: page.enter()
        Keys.onDownPressed: page.focusCommands()
        onClicked: page.showLibrary()
    }

    Label {
        objectName: "homeSelectedTitle"
        x: page.theme.gutter
        y: 164
        width: parent.width - 2 * x
        height: 34
        visible: page.gameFocused
        text: page.selectedGame.title || ""
        textFormat: Text.PlainText
        color: page.theme.accent
        font.pixelSize: page.theme.bodySize
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    ListView {
        id: shelf
        objectName: "homeShelf"
        x: page.theme.gutter - page.theme.homeFocusInset
        y: page.theme.homeShelfY - page.theme.homeFocusInset
        width: parent.width - 2 * x
        height: page.theme.homeTile + 2 * page.theme.homeFocusInset
        orientation: ListView.Horizontal
        layoutDirection: Qt.LeftToRight
        spacing: page.theme.homeTileGap - 2 * page.theme.homeFocusInset
        model: page.games
        visible: page.hasGames
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        highlightMoveDuration: page.theme.motion
        highlight: Item {}
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
            interactive: false
        }

        WheelHandler {
            target: null
            onWheel: event => {
                const delta = event.pixelDelta.x || event.pixelDelta.y
                    || (event.angleDelta.x || event.angleDelta.y) / 2
                shelf.contentX = Math.max(0, Math.min(shelf.contentWidth - shelf.width,
                    shelf.contentX - delta))
            }
        }

        Keys.onLeftPressed: if (currentIndex > 0) page.pick(currentIndex - 1)
        Keys.onRightPressed: {
            if (currentIndex + 1 < count)
                page.pick(currentIndex + 1)
            else
                browse.forceActiveFocus()
        }
        Keys.onUpPressed: profile.forceActiveFocus()
        Keys.onDownPressed: page.focusCommands()
        Keys.onReturnPressed: event => {
            if (!event.isAutoRepeat && page.gameFocused)
                page.gameAction()
        }
        Keys.onEnterPressed: event => {
            if (!event.isAutoRepeat && page.gameFocused)
                page.gameAction()
        }

        delegate: GameTile {
            id: gameTile
            objectName: "homeGameTile"
            required property int index
            required property var modelData
            width: page.theme.homeTile + 2 * page.theme.homeFocusInset
            height: width
            contentInset: page.theme.homeFocusInset
            frameInset: page.theme.homeFocusInset - 1
            theme: page.theme
            game: modelData
            selected: gameTile.ListView.isCurrentItem
            focused: selected && shelf.activeFocus
            onPicked: {
                page.pick(index)
                shelf.forceActiveFocus(Qt.MouseFocusReason)
            }
            onLaunched: {
                page.pick(index)
                page.gameAction()
            }
            onOptionsRequested: {
                page.pick(index)
                shelf.forceActiveFocus(Qt.MouseFocusReason)
                page.optionsRequested()
            }
        }
    }

    NavButton {
        id: addFolderButton
        objectName: "homeAddFolder"
        visible: !page.hasGames
        enabled: visible
        x: page.theme.gutter
        y: page.theme.homeShelfY
        width: page.theme.homeTile
        height: width
        implicitWidth: width
        theme: page.theme
        symbol: "folder"
        text: qsTr("Add game folder")
        Keys.onRightPressed: browse.forceActiveFocus()
        Keys.onUpPressed: profile.forceActiveFocus()
        Keys.onDownPressed: page.focusCommands()
        onClicked: page.addFolder()
        background: Rectangle {
            radius: page.theme.radius
            color: addFolderButton.activeFocus || addFolderButton.hovered
                ? page.theme.surface : page.theme.rail
            border.color: addFolderButton.activeFocus ? page.theme.accent : page.theme.line
            border.width: addFolderButton.activeFocus ? page.theme.focusWidth : 1
        }
        contentItem: ColumnLayout {
            spacing: 8
            Item { Layout.fillHeight: true }
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                source: page.theme.icon("folder")
                sourceSize.width: 48
                sourceSize.height: 48
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: addFolderButton.text
                color: page.theme.ink
                font.pixelSize: page.theme.compactSize
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
        }
    }

    Label {
        visible: !page.hasGames
        x: page.theme.gutter + page.theme.homeTile + 32
        y: page.theme.homeShelfY + 76
        width: 400
        text: page.library.scanning ? qsTr("Scanning game folders…")
            : qsTr("Add a folder to show your games here.")
        color: page.theme.muted
        font.pixelSize: page.theme.bodySize
        wrapMode: Text.Wrap
    }

    Rectangle {
        x: page.theme.gutter
        y: page.theme.homeCommandsY - 16
        width: parent.width - 2 * x
        height: 1
        color: page.theme.line
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        y: page.theme.homeCommandsY
        spacing: 16

        Repeater {
            id: commandButtons
            model: [
                { key: "settings", glyph: "settings", caption: qsTr("Settings"), action: page.showSettings },
                { key: "controllers", glyph: "controllers", caption: qsTr("Controllers"), action: page.showControllers },
                { key: "screenshots", glyph: "screenshots", caption: qsTr("Screenshots"), action: page.showScreenshots },
                { key: "notifications", glyph: "notifications", caption: qsTr("Notifications"), action: page.showNotifications },
                { key: "desktop", glyph: "desktop", caption: qsTr("Desktop"), action: page.showDesktop },
                { key: "power", glyph: "power", caption: qsTr("Power"), action: page.showPower },
                { key: "session", glyph: "play", caption: qsTr("Game session"), action: page.showSession }
            ]
            delegate: CommandSlot {
                id: slot
                required property int index
                required property var modelData
                control: command
                readonly property bool unread: modelData.key === "notifications" && page.hasNotification
                width: 114
                height: 104
                visible: modelData.key !== "session" || page.hasSession

                Button {
                    id: command
                    objectName: slot.modelData.key === "settings" ? "settingsNavigation" : "homeSystemControl"
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: page.theme.homeCommandDiameter
                    height: width
                    padding: 18
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: slot.modelData.caption
                    Accessible.description: slot.unread ? qsTr("Unread notification") : ""
                    KeyNavigation.left: page.commandAt(slot.index - 1)
                    KeyNavigation.right: page.commandAt(slot.index + 1)
                    Keys.onUpPressed: browse.forceActiveFocus()
                    Keys.onReturnPressed: event => { if (!event.isAutoRepeat) clicked() }
                    Keys.onEnterPressed: event => { if (!event.isAutoRepeat) clicked() }
                    onActiveFocusChanged: if (activeFocus) page.lastCommand = command
                    onClicked: slot.modelData.action()

                    background: Rectangle {
                        radius: width / 2
                        color: command.hovered || command.activeFocus ? page.theme.hover : page.theme.surface
                        border.color: command.activeFocus ? page.theme.accent : page.theme.line
                        border.width: command.activeFocus ? page.theme.focusWidth : 1
                    }
                    contentItem: Image {
                        source: page.theme.icon(slot.modelData.glyph)
                        sourceSize.width: 64
                        sourceSize.height: 64
                        fillMode: Image.PreserveAspectFit
                    }
                }
                Rectangle {
                    visible: slot.unread
                    x: command.x + command.width - 11
                    y: 4
                    width: 10
                    height: width
                    radius: width / 2
                    color: page.theme.accent
                    border.color: page.theme.background
                    border.width: 1
                }
                Label {
                    anchors.top: command.bottom
                    anchors.topMargin: 7
                    width: parent.width
                    visible: command.activeFocus || command.hovered
                    text: slot.modelData.caption
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    color: command.activeFocus ? page.theme.accent : page.theme.muted
                    font.pixelSize: page.theme.noteSize
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
