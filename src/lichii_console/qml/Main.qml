// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: app
    objectName: "consoleWindow"
    title: qsTr("Lichii Console")
    width: 1280
    height: 720
    minimumWidth: 700
    minimumHeight: 500
    visible: true
    color: tokens.background
    Behavior on color { ColorAnimation { duration: tokens.motion } }
    font.family: tokens.bodyFont

    required property var backend
    readonly property var library: backend.library
    readonly property var appearance: backend.appearance
    readonly property bool hasSession: backend.state !== "idle"
    readonly property bool canStop: backend.state === "running" || backend.state === "paused"
    property string activePage: "home"
    property var history: []
    property string homeGameKey
    property var contextGame: ({})
    property Item suspendedFocus
    property Item optionsReturnFocus
    property Item profileReturnFocus
    property bool editingArtwork
    property Item lastFocus
    property string folderPurpose
    property date now: new Date()
    property string lastError
    property string launchingTitle
    readonly property bool booting: backend.state === "loading" || backend.state === "stopping"
    readonly property bool gameFocused: activePage === "home" ? home.gameFocused
        : activePage === "library" && allSoftware.gameFocused
    readonly property bool gamepadHints: backend.gamepadHints
    readonly property bool modalVisible: booting || actionPanel.visible || softwareOptions.visible
        || preview.visible || captures.previewVisible || allSoftware.controlsVisible
        || folderPicker.visible || artworkPicker.visible || avatarPicker.visible || editProfile.visible
    readonly property var stateLabels: ({
        idle: qsTr("Ready"),
        loading: qsTr("Starting"),
        running: qsTr("In game"),
        paused: qsTr("Paused"),
        stopping: qsTr("Stopping")
    })

    Theme {
        id: tokens
        appearance: app.appearance
        viewScale: Math.min(app.width / logicalWidth, app.height / logicalHeight)
        confirmKey: app.gamepadHints ? app.backend.gamepadPrompts.confirm : "Enter"
        backKey: app.gamepadHints ? app.backend.gamepadPrompts.back : "Esc"
        optionsKey: app.gamepadHints ? app.backend.gamepadPrompts.options
            : app.backend.optionsKey === "Menu" ? qsTr("Context Menu") : app.backend.optionsKey
        libraryKey: app.gamepadHints ? app.backend.gamepadPrompts.library : "F2"
        gamepadFamily: app.gamepadHints ? app.backend.gamepadPrompts.family : ""
        homeKey: app.gamepadHints ? app.backend.gamepadPrompts.home : "F1"
    }
    Timer { interval: 1000; running: true; repeat: true; onTriggered: app.now = new Date() }

    function go(target, category, game, returnFocus) {
        if (target !== activePage || (target === "settings" && settings.programId !== (game?.programId || ""))) {
            history.push({
                activePage: activePage, focus: returnFocus || activeFocusItem, category: settings.category,
                programId: settings.programId, gameTitle: settings.gameTitle
            })
            activePage = target
            appearance.play("open")
        }
        if (target === "settings") {
            settings.programId = game?.programId || ""
            settings.gameTitle = game?.title || ""
            settings.enter(category)
        } else if (target === "library") {
            allSoftware.enter()
        } else if (target === "screenshots") {
            captures.enter()
        } else {
            home.enter()
        }
    }

    function back() {
        appearance.play("cancel")
        const previous = history.pop()
        if (!previous) {
            if (activePage !== "home") {
                activePage = "home"
                home.enter()
            } else {
                home.enter()
            }
            return
        }
        activePage = previous.activePage
        if (activePage === "settings") {
            settings.programId = previous.programId
            settings.gameTitle = previous.gameTitle
            settings.enter(previous.category)
        }
        if (previous.focus && previous.focus.visible)
            previous.focus.forceActiveFocus()
        else if (activePage === "home")
            home.enter()
        else if (activePage === "library")
            allSoftware.enter()
        else if (activePage === "screenshots")
            captures.enter()
    }

    function showHome() {
        if (actionPanel.opened)
            actionPanel.close()
        history = []
        activePage = "home"
        home.enter()
        appearance.play("home")
    }

    function ask(title, description, actions, initialLast, sound, cover) {
        if (!actionPanel.opened)
            suspendedFocus = activeFocusItem
        actionPanel.coverArt = cover || ""
        if (sound !== "silent")
            appearance.play(sound || "open")
        actionPanel.title = title
        actionPanel.description = description
        actionPanel.actions = actions.concat([{ label: qsTr("Back") }])
        actionPanel.initialLast = !!initialLast
        if (actionPanel.opened)
            actionPanel.focusAction(initialLast ? actionPanel.actions.length - 1 : 0, initialLast ? -1 : 1)
        else
            actionPanel.open()
    }

    function launchGame(game) {
        if (hasSession) {
            showSession()
            return
        }
        if (game && game.gamePath) {
            launchingTitle = game.title || ""
            appearance.play("confirm")
            backend.launch(game.gamePath, game.programId || "")
        }
    }

    function showOptions(game, sound, keepReturnFocus) {
        if (!game?.gameKey)
            return
        contextGame = game
        if (!softwareOptions.opened) {
            if (!keepReturnFocus)
                optionsReturnFocus = activeFocusItem
            if (sound !== "silent")
                appearance.play(sound || "options")
        }
        softwareOptions.game = game
        softwareOptions.hasSession = hasSession
        softwareOptions.canConfigure = !!game.programId && !hasSession
        softwareOptions.open()
    }

    function refreshOptions() {
        if (!contextGame?.gameKey)
            return
        const updated = library.getByKey(contextGame.gameKey)
        if (updated?.gameKey) {
            contextGame = updated
            softwareOptions.game = updated
        }
        softwareOptions.hasSession = hasSession
        softwareOptions.canConfigure = !!contextGame.programId && !hasSession
        if (softwareOptions.opened && !actionPanel.visible)
            Qt.callLater(softwareOptions.refocus)
    }

    function runOption(command) {
        const game = contextGame
        if (!game?.gameKey)
            return
        switch (command) {
        case "play":
            softwareOptions.close()
            launchGame(game)
            break
        case "graphics":
            go("settings", "graphics", game, optionsReturnFocus)
            softwareOptions.close()
            break
        case "cpu":
            go("settings", "cpu", game, optionsReturnFocus)
            softwareOptions.close()
            break
        case "controllers":
            softwareOptions.close()
            backend.configureGame(game.programId, game.gamePath)
            break
        case "open-saves":
            softwareOptions.close()
            backend.openSaves(game.programId, game.gamePath)
            break
        case "favorite":
            library.setFavorite(game.gameKey, !game.favorite)
            refreshOptions()
            break
        case "artwork":
            editingArtwork = true
            softwareOptions.close()
            appearance.play("open")
            artworkPicker.open()
            break
        case "restore-artwork":
            library.resetArtwork(game.gameKey)
            break
        case "reset-settings":
            confirmReset(game.programId)
            break
        }
    }

    function confirmReset(programId) {
        ask(qsTr("Reset game settings?"), qsTr("This game will use your global settings."),
            [{ label: qsTr("Reset settings"), danger: true, action: () => {
                backend.resetGameSettings(programId)
                settings.settingsRevision++
            }}], true)
    }

    function showSession() {
        if (!hasSession)
            return
        ask(qsTr("Game session"), stateLabels[backend.state] || "", [
            { label: qsTr("Resume game"), enabled: backend.state === "paused",
                action: () => backend.resume() },
            { label: qsTr("Pause game"), enabled: backend.state === "running",
                action: () => backend.pause() },
            { label: qsTr("Return to Home"), action: () => showHome() },
            { label: qsTr("Stop game"), enabled: canStop,
                danger: true, action: () => stopSession() }
        ])
    }

    function stopSession() {
        if (!backend.stopRequiresConfirmation()) {
            backend.stop()
            return
        }
        ask(qsTr("Close game?"), qsTr("Unsaved progress may be lost."),
            [{ label: qsTr("Close game"), danger: true, action: () => backend.stop(true) }], true)
    }

    function showProfiles() {
        profileReturnFocus = activeFocusItem
        const users = backend.profiles
        const actions = users.map(user => ({
            label: user.name, detail: user.active ? qsTr("Active") : "",
            action: () => showProfile(user)
        }))
        if (!hasSession && users.length < 8)
            actions.push({ label: qsTr("Add profile"), action: () => showProfileName("", "") })
        ask(qsTr("User profiles"), "", actions)
    }

    function showProfile(user) {
        if (!user)
            return
        const actions = [
            { label: qsTr("Edit name"), enabled: !hasSession,
                action: () => showProfileName(user.id, user.name) },
            { label: qsTr("Change avatar"), enabled: !hasSession, action: () => {
                avatarPicker.profileId = user.id
                avatarPicker.open()
            }},
            { label: qsTr("Use this profile"), enabled: !hasSession && !user.active,
                action: () => backend.activateProfile(user.id) },
            { label: qsTr("All profiles"), action: () => showProfiles() }
        ]
        ask(user.name, user.active ? qsTr("Active profile") : "", actions, false, "open",
            user.avatar.toString().length ? user.avatar : tokens.icon("profile"))
    }

    function showProfileName(id, name) {
        editProfile.profileId = id
        editProfile.validationMessage = ""
        profileNameField.text = name
        editProfile.open()
    }

    function showNotifications() {
        ask(qsTr("Notifications"), lastError || qsTr("You're all caught up."),
            lastError ? [{ label: qsTr("Clear notification"), action: () => { lastError = "" } }] : [])
    }

    function showAbout() {
        ask(qsTr("About Lichii"), qsTr("Version %1").arg(backend.version), [])
    }

    function showPower() {
        ask(qsTr("Lichii"), qsTr("Console edition  ·  %1").arg(backend.version), [
            { label: visibility === Window.FullScreen ? qsTr("Leave fullscreen") : qsTr("Enter fullscreen"),
              action: toggleFullscreen },
            { label: qsTr("Open Desktop"), enabled: !hasSession, action: () => backend.openDesktop() },
            { label: qsTr("About Lichii"), action: () => showAbout() },
            { label: qsTr("Quit Lichii"), danger: true, action: () => app.requestQuit() }
        ])
    }

    function requestQuit() {
        if (!hasSession || canStop)
            backend.quit()
        else
            ask(qsTr("Lichii"), qsTr("Wait for the game to finish starting or stopping before quitting."), [])
    }

    function chooseFolder(purpose) {
        appearance.play("open")
        folderPurpose = purpose
        folderPicker.title = purpose === "icon-pack" ? qsTr("Select icon folder")
            : purpose === "sound-pack" ? qsTr("Select sound folder") : qsTr("Add game folder")
        folderPicker.open()
    }

    function toggleFullscreen() {
        visibility = visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }

    function restorePageFocus(item) {
        if (item && item.visible && item.enabled)
            item.forceActiveFocus()
        else if (activePage === "home")
            home.enter()
        else if (activePage === "library")
            allSoftware.enter()
        else if (activePage === "screenshots")
            captures.enter()
        else
            settings.enter(settings.category)
    }

    function returnFromArtwork() {
        editingArtwork = false
        const updated = library.getByKey(contextGame.gameKey)
        showOptions(updated.gameKey ? updated : contextGame, "silent", true)
    }

    function syncHomeSelection() {
        const games = library.homeGames
        if (games.length) {
            const index = Math.max(0, games.findIndex(game => game.gameKey === homeGameKey))
            homeGameKey = games[index].gameKey
            home.restore(index)
        }
        if (activePage === "home" && !modalVisible && home.ownsSelectionFocus(lastFocus))
            home.enter()
    }

    Shortcut {
        sequence: "Esc"
        autoRepeat: false
        enabled: !app.modalVisible
        onActivated: app.back()
    }
    Shortcut {
        sequence: "F1"
        autoRepeat: false
        enabled: !app.modalVisible
        onActivated: {
            if (app.gamepadHints && app.activePage === "home" && app.backend.state === "paused")
                app.showSession()
            else
                app.showHome()
        }
    }
    Shortcut { sequence: "F11"; autoRepeat: false; onActivated: app.toggleFullscreen() }
    Shortcut {
        sequences: app.backend.optionsKey === "Menu" ? ["Menu"] : [app.backend.optionsKey, "Menu"]
        autoRepeat: false
        enabled: app.activePage === "home" && app.gameFocused && !app.modalVisible
        onActivated: app.showOptions(home.selectedGame)
    }
    Shortcut {
        sequence: "F2"
        autoRepeat: false
        enabled: app.activePage === "home" && !app.modalVisible
        onActivated: app.go("library")
    }

    onActiveFocusItemChanged: {
        if (!activeFocusItem || !activeFocusItem.visible || lastFocus === activeFocusItem)
            return
        if (lastFocus)
            appearance.play("focus")
        lastFocus = activeFocusItem
    }
    onClosing: close => {
        close.accepted = false
        requestQuit()
    }

    Connections {
        target: app.backend
        function onSettingsChanged() {
            settings.settingsRevision++
        }
        function onStateChanged() {
            settings.settingsRevision++
        }
        function onError(message) {
            app.lastError = message
            app.appearance.play("warning")
            app.ask(qsTr("Lichii"), message, [], false, "silent")
        }
    }
    Connections {
        target: app.library
        function onHomeGamesChanged() {
            Qt.callLater(app.syncHomeSelection)
        }
        function onArtworkBusyChanged() {
            if (!app.library.artworkBusy && softwareOptions.opened)
                app.refreshOptions()
        }
    }

    FolderDialog {
        id: folderPicker
        onRejected: app.appearance.play("cancel")
        onAccepted: {
            app.appearance.play("confirm")
            switch (app.folderPurpose) {
            case "icon-pack": app.appearance.iconPack = selectedFolder; break
            case "sound-pack": app.appearance.soundPack = selectedFolder; break
            default: app.library.addFolder(selectedFolder); break
            }
        }
    }
    FileDialog {
        id: artworkPicker
        title: qsTr("Choose game artwork")
        onRejected: {
            app.appearance.play("cancel")
            app.returnFromArtwork()
        }
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"), qsTr("All files (*)")]
        onAccepted: {
            preview.source = selectedFile
            app.appearance.play("confirm")
            preview.open()
        }
    }
    PanelDialog {
        id: editProfile
        theme: tokens
        title: profileId.length ? qsTr("Edit profile name") : qsTr("Add profile")
        property string profileId
        property string validationMessage

        function save() {
            if (app.backend.saveProfileName(profileId, profileNameField.text)) {
                app.appearance.play("confirm")
                close()
            } else {
                validationMessage = qsTr("Enter a name that fits in 32 UTF-8 bytes.")
            }
        }

        onOpened: {
            profileNameField.forceActiveFocus()
            profileNameField.selectAll()
        }
        onClosed: app.restorePageFocus(app.profileReturnFocus)

        contentItem: ColumnLayout {
            spacing: 14
            TextField {
                id: profileNameField
                objectName: "profileNameField"
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                placeholderText: qsTr("Profile name")
                Accessible.name: placeholderText
                maximumLength: 32
                color: tokens.ink
                placeholderTextColor: tokens.muted
                selectionColor: tokens.accent
                selectedTextColor: tokens.accentText
                font.pixelSize: tokens.bodySize
                background: Rectangle {
                    color: tokens.surface
                    radius: tokens.radius
                    border.color: profileNameField.activeFocus ? tokens.accent : tokens.line
                    border.width: profileNameField.activeFocus ? tokens.focusWidth : 1
                }
                Keys.onDownPressed: saveProfileName.forceActiveFocus()
                onAccepted: editProfile.save()
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: editProfile.validationMessage
                color: tokens.danger
                font.pixelSize: tokens.noteSize
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                NavButton {
                    theme: tokens
                    text: qsTr("Cancel")
                    KeyNavigation.right: saveProfileName
                    KeyNavigation.up: profileNameField
                    onClicked: editProfile.reject()
                }
                NavButton {
                    id: saveProfileName
                    theme: tokens
                    text: qsTr("Save")
                    emphasized: true
                    KeyNavigation.up: profileNameField
                    onClicked: editProfile.save()
                }
            }
        }
    }

    AvatarPicker {
        id: avatarPicker
        theme: tokens
        backend: app.backend
        onClosed: app.restorePageFocus(app.profileReturnFocus)
    }

    PanelDialog {
        id: preview
        title: qsTr("Preview game artwork")
        theme: tokens
        property url source
        onOpened: {
            if (saveArtwork.enabled)
                saveArtwork.forceActiveFocus()
            else
                cancelArtwork.forceActiveFocus()
        }
        onClosed: app.returnFromArtwork()
        contentItem: ColumnLayout {
            spacing: 12
            Image {
                id: previewImage
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                source: preview.source
                sourceSize.width: 600
                sourceSize.height: 600
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Label {
                Layout.fillWidth: true
                text: previewImage.status === Image.Error ? qsTr("Lichii couldn't open this image.")
                    : previewImage.status === Image.Loading ? qsTr("Loading image…") : app.contextGame.title || ""
                textFormat: Text.PlainText
                color: tokens.muted
                elide: Text.ElideRight
            }
            RowLayout {
                NavButton {
                    id: saveArtwork
                    theme: tokens
                    text: qsTr("Apply artwork")
                    emphasized: true
                    enabled: previewImage.status === Image.Ready && !app.library.artworkBusy
                    Keys.onRightPressed: cancelArtwork.forceActiveFocus()
                    onClicked: {
                        app.library.setArtwork(app.contextGame.gameKey, preview.source)
                        preview.close()
                    }
                }
                NavButton {
                    id: cancelArtwork
                    theme: tokens
                    text: qsTr("Cancel")
                    Keys.onLeftPressed: saveArtwork.forceActiveFocus()
                    onClicked: preview.close()
                }
            }
        }
    }

    ActionDialog {
        id: actionPanel
        theme: tokens
        onRejected: app.appearance.play("cancel")
        onDismissed: {
            if (softwareOptions.opened)
                softwareOptions.refocus()
            else
                app.restorePageFocus(app.suspendedFocus)
        }
    }

    SoftwareOptionsDialog {
        id: softwareOptions
        theme: tokens
        artworkBusy: app.library.artworkBusy
        onRejected: app.appearance.play("cancel")
        onCommandRequested: command => app.runOption(command)
        onClosed: {
            if (!app.editingArtwork && !actionPanel.visible)
                app.restorePageFocus(app.optionsReturnFocus)
        }
    }

    Item {
        id: stage
        objectName: "consoleStage"
        width: tokens.logicalWidth
        height: tokens.logicalHeight
        anchors.centerIn: parent
        scale: tokens.viewScale

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: footer.top

            HomePage {
                id: home
                anchors.fill: parent
                visible: app.activePage === "home"
                theme: tokens
                library: app.library
                sessionState: app.backend.state
                now: app.now
                profileName: app.backend.profileName
                profileAvatar: app.backend.profileAvatar
                statusText: app.hasSession ? app.stateLabels[app.backend.state] : ""
                hasNotification: app.lastError.length > 0
                networkKnown: app.backend.networkKnown
                networkOnline: app.backend.networkOnline
                batteryLevel: app.backend.batteryLevel
                batteryCharging: app.backend.batteryCharging
                onSelectGame: game => {
                    if (app.homeGameKey && app.homeGameKey !== game.gameKey && game.gameKey && home.gameFocused)
                        app.appearance.play("focus")
                    app.homeGameKey = game.gameKey || ""
                }
                onAddFolder: app.chooseFolder("add-folder")
                onGameAction: app.launchGame(home.selectedGame)
                onOptionsRequested: app.showOptions(home.selectedGame)
                onShowControllers: app.go("settings", "controllers")
                onShowDesktop: app.backend.openDesktop()
                onShowSettings: app.go("settings")
                onShowScreenshots: app.go("screenshots")
                onShowProfile: {
                    app.profileReturnFocus = app.activeFocusItem
                    const active = app.backend.profiles.find(user => user.active)
                    if (active)
                        app.showProfile(active)
                    else
                        app.showProfiles()
                }
                onShowNotifications: app.showNotifications()
                onShowPower: app.showPower()
                onShowSession: app.showSession()
            }
            LibraryPage {
                id: allSoftware
                anchors.fill: parent
                visible: app.activePage === "library"
                theme: tokens
                library: app.library
                optionsKey: app.backend.optionsKey
                gamepadHints: app.gamepadHints
                onOptionsRequested: game => app.showOptions(game)
                onLaunchRequested: game => app.launchGame(game)
                onAddFolder: app.chooseFolder("add-folder")
                onBackRequested: app.back()
            }
            ScreenshotsPage {
                id: captures
                anchors.fill: parent
                visible: app.activePage === "screenshots"
                theme: tokens
                folder: visible ? app.backend.dataFolder("screenshots") : ""
                onOpenFolder: app.backend.openData("screenshots")
                onBackRequested: app.back()
            }
            SettingsPage {
                id: settings
                anchors.fill: parent
                visible: app.activePage === "settings"
                theme: tokens
                backend: app.backend
                onFolderRequested: purpose => app.chooseFolder(purpose)
                onResetRequested: app.confirmReset(settings.programId)
                onManageProfilesRequested: app.showProfiles()
                onBackRequested: app.back()
                onChoiceRequested: row => app.ask(row.label, row.description || "",
                    Array.from(row.options, (name, index) => ({
                        label: name, detail: row.value === index ? qsTr("Selected") : "",
                        selected: row.value === index,
                        action: () => settings.edit(row, index)
                    })))
            }
        }

        Rectangle {
            id: footer
            objectName: "consoleFooter"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: tokens.footerHeight
            color: tokens.background

            Rectangle {
                x: tokens.gutter
                width: parent.width - tokens.gutter * 2
                height: 1
                color: tokens.lineStrong
            }
            RowLayout {
                visible: !app.modalVisible
                anchors.right: parent.right
                anchors.rightMargin: tokens.gutter
                anchors.verticalCenter: parent.verticalCenter
                spacing: 26
                Legend {
                    theme: tokens
                    visible: app.gameFocused
                    button: tokens.optionsKey
                    caption: qsTr("Options")
                    interactive: true
                    onActivated: app.showOptions(app.activePage === "home" ? home.selectedGame : allSoftware.selection)
                }
                Legend {
                    objectName: "backPrompt"
                    theme: tokens
                    visible: app.activePage !== "home"
                    button: tokens.backKey
                    caption: app.activePage === "settings" && settings.adjustingSlider
                        ? qsTr("Cancel") : qsTr("Back")
                    interactive: true
                    onActivated: {
                        if (app.activePage === "settings" && settings.adjustingSlider)
                            settings.sliderEditing = false
                        else
                            app.back()
                    }
                }
                Legend {
                    objectName: "confirmPrompt"
                    theme: tokens
                    visible: caption.length > 0
                    button: tokens.confirmKey
                    caption: app.activePage === "home" ? home.confirmCaption
                        : app.activePage === "settings" ? settings.confirmCaption
                        : app.activePage === "library"
                            ? (app.hasSession && app.gameFocused ? qsTr("Game session") : allSoftware.confirmCaption)
                            : qsTr("Open")
                }
            }
            Legend {
                visible: !app.modalVisible && button.length > 0
                anchors.left: parent.left
                anchors.leftMargin: tokens.gutter
                anchors.verticalCenter: parent.verticalCenter
                theme: tokens
                button: app.activePage === "home" ? tokens.libraryKey : tokens.homeKey
                caption: app.activePage === "home" ? qsTr("All Software") : qsTr("Home")
                interactive: true
                onActivated: {
                    if (app.activePage === "home")
                        app.go("library")
                    else
                        app.showHome()
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: app.booting
            color: tokens.background

            MouseArea { anchors.fill: parent }
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 2 * tokens.gutter
                spacing: 22
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: app.booting
                    palette.dark: tokens.accent
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: app.backend.state === "stopping" ? qsTr("Closing game…")
                        : app.launchingTitle.length ? qsTr("Starting %1…").arg(app.launchingTitle)
                        : qsTr("Starting game…")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: tokens.ink
                    font.pixelSize: tokens.screenTitle
                }
            }
        }
    }

    Component.onCompleted: {
        syncHomeSelection()
        home.enter()
    }
}
