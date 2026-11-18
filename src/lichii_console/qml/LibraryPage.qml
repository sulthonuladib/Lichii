// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page
    objectName: "libraryScreen"
    required property Theme theme
    required property var library
    property var selection: ({})
    property string selectionKey
    property string optionsKey: "Menu"
    property bool gamepadHints
    property bool resetting
    property bool selectionHadFocus
    readonly property bool gameFocused: visible && grid.activeFocus && !!selection?.gameKey
    readonly property bool controlsVisible: filters.visible
    readonly property string confirmCaption: gameFocused ? qsTr("Play")
        : controls.activeFocus ? qsTr("Browse") : emptyAction.text
    readonly property bool hasFilters: library.search.trim().length > 0 || library.favoritesOnly
    readonly property var sortKeys: ["title", "recent", "favorites"]
    readonly property var sortLabels: [qsTr("Title"), qsTr("Recently played"), qsTr("Favorites first")]
    readonly property int sortIndex: Math.max(0, sortKeys.indexOf(library.sortOrder))
    signal optionsRequested(var game)
    signal launchRequested(var game)
    signal addFolder()
    signal backRequested()

    function enter() {
        restoreSelection()
        if (grid.count)
            grid.forceActiveFocus()
        else
            emptyAction.forceActiveFocus()
    }

    function openLibraryControls() {
        if (filters.visible)
            return
        filters.returnToControls = controls.activeFocus
        theme.appearance.play("open")
        filters.open()
    }

    function clearFilters() {
        library.search = ""
        library.favoritesOnly = false
    }

    function restoreSelection() {
        if (!grid.count) {
            selection = {}
            return
        }
        if (selectionKey) {
            for (let i = 0; i < library.count; ++i) {
                if (library.get(i)?.gameKey === selectionKey) {
                    grid.currentIndex = i
                    updateSelection(true)
                    return
                }
            }
        }
        grid.currentIndex = Math.max(0, Math.min(grid.currentIndex, grid.count - 1))
        updateSelection(true)
    }

    function finishReset() {
        restoreSelection()
        resetting = false
        if (selectionHadFocus && visible && !filters.visible)
            (grid.count ? grid : emptyAction).forceActiveFocus()
    }

    function updateSelection(scrollToSelection) {
        if (grid.currentIndex < 0 || grid.currentIndex >= library.count)
            return
        selection = library.get(grid.currentIndex) || {}
        selectionKey = selection?.gameKey || ""
        if (scrollToSelection)
            grid.positionViewAtIndex(grid.currentIndex, GridView.Contain)
    }

    Connections {
        target: page.library
        function onModelAboutToBeReset() {
            if (!page.resetting)
                page.selectionHadFocus = grid.activeFocus || emptyAction.activeFocus
            page.resetting = true
        }
        function onModelReset() {
            Qt.callLater(page.finishReset)
        }
        function onDataChanged() {
            if (!page.resetting)
                page.updateSelection()
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        enabled: page.visible && !filters.visible
            && (grid.activeFocus || emptyAction.activeFocus || controls.activeFocus)
        autoRepeat: false
        onActivated: page.openLibraryControls()
    }

    Shortcut {
        sequences: page.optionsKey === "Menu" ? ["Menu"] : ["Menu", page.optionsKey]
        enabled: page.gameFocused && !filters.visible
        autoRepeat: false
        onActivated: page.optionsRequested(page.selection)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: page.theme.gutter
        anchors.rightMargin: page.theme.gutter
        anchors.topMargin: 32
        anchors.bottomMargin: 14
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Image {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                source: page.theme.icon("library")
                sourceSize.width: 30
                sourceSize.height: 30
                fillMode: Image.PreserveAspectFit
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("All Software")
                color: page.theme.ink
                font.family: page.theme.displayFont
                font.pixelSize: page.theme.screenTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            NavButton {
                id: controls
                objectName: "libraryControlsAction"
                theme: page.theme
                symbol: "search"
                text: qsTr("Search and filter")
                detail: page.gamepadHints ? "" : qsTr("Ctrl+F")
                compact: true
                onClicked: page.openLibraryControls()
                Keys.onDownPressed: page.enter()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: page.theme.line
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            spacing: 18

            Label {
                text: page.library.count === 1 ? qsTr("1 title")
                    : qsTr("%1 titles").arg(page.library.count)
                color: page.theme.muted
                font.family: page.theme.bodyFont
                font.pixelSize: page.theme.noteSize
            }
            Label {
                Layout.fillWidth: true
                text: page.library.search.trim().length
                    ? qsTr("Search: %1").arg(page.library.search.trim()) : ""
                textFormat: Text.PlainText
                color: page.theme.ink
                font.family: page.theme.bodyFont
                font.pixelSize: page.theme.noteSize
                elide: Text.ElideRight
            }
            Label {
                visible: page.library.favoritesOnly
                text: qsTr("Favorites only")
                color: page.theme.link
                font.family: page.theme.bodyFont
                font.pixelSize: page.theme.noteSize
            }
            Label {
                text: qsTr("Sort: %1").arg(page.sortLabels[page.sortIndex])
                color: page.theme.muted
                font.family: page.theme.bodyFont
                font.pixelSize: page.theme.noteSize
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: grid
                objectName: "allSoftwareGrid"
                readonly property int columns: 5
                anchors.fill: parent
                visible: count > 0
                cellWidth: Math.floor(width / columns)
                cellHeight: cellWidth + 8
                model: page.library
                clip: true
                keyNavigationEnabled: false
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: cellHeight

                onCurrentIndexChanged: {
                    if (page.resetting)
                        return
                    page.updateSelection(true)
                    if (activeFocus && currentIndex >= 0)
                        page.theme.appearance.play("focus")
                }
                Keys.onLeftPressed: {
                    if (currentIndex % columns > 0)
                        --currentIndex
                }
                Keys.onRightPressed: {
                    if (currentIndex % columns < columns - 1 && currentIndex + 1 < count)
                        ++currentIndex
                }
                Keys.onUpPressed: {
                    if (currentIndex >= columns)
                        currentIndex -= columns
                    else
                        controls.forceActiveFocus()
                }
                Keys.onDownPressed: {
                    if ((Math.floor(currentIndex / columns) + 1) * columns < count)
                        currentIndex = Math.min(count - 1, currentIndex + columns)
                }
                Keys.onReturnPressed: event => {
                    if (!event.isAutoRepeat && page.selection?.gameKey)
                        page.launchRequested(page.selection)
                }
                Keys.onEnterPressed: event => {
                    if (!event.isAutoRepeat && page.selection?.gameKey)
                        page.launchRequested(page.selection)
                }
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 6
                }

                delegate: GameTile {
                    required property int index
                    required property var model
                    theme: page.theme
                    game: model
                    width: grid.cellWidth - 14
                    height: grid.cellHeight
                    contentInset: 6
                    frameInset: 5
                    showCaption: true
                    selected: GridView.isCurrentItem
                    focused: selected && grid.activeFocus

                    onPicked: {
                        grid.currentIndex = index
                        grid.forceActiveFocus(Qt.MouseFocusReason)
                    }
                    onLaunched: page.launchRequested(page.library.get(index))
                    onOptionsRequested: {
                        grid.currentIndex = index
                        grid.forceActiveFocus(Qt.MouseFocusReason)
                        page.optionsRequested(page.library.get(index))
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width, 560)
                spacing: 16
                visible: grid.count === 0

                Label {
                    width: parent.width
                    text: page.library.scanning ? qsTr("Looking for games…")
                        : page.hasFilters ? qsTr("No games match your filters") : qsTr("No games found")
                    color: page.theme.ink
                    font.family: page.theme.displayFont
                    font.pixelSize: page.theme.sectionTitle
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                NavButton {
                    id: emptyAction
                    objectName: "emptyLibraryAction"
                    anchors.horizontalCenter: parent.horizontalCenter
                    theme: page.theme
                    text: page.hasFilters ? qsTr("Clear filters") : qsTr("Add a folder")
                    symbol: page.hasFilters ? "search" : "folder"
                    compact: true
                    Keys.onUpPressed: controls.forceActiveFocus()
                    onClicked: {
                        if (page.hasFilters) {
                            page.clearFilters()
                            Qt.callLater(page.enter)
                        } else {
                            page.addFolder()
                        }
                    }
                }
            }
        }
    }

    PanelDialog {
        id: filters
        objectName: "libraryControlsDialog"
        theme: page.theme
        title: qsTr("Browse library")
        property bool addFolderPending
        property bool finishing
        property bool returnToControls

        function finish() {
            finishing = true
            theme.appearance.play("confirm")
            close()
        }

        onOpened: search.forceActiveFocus()
        onClosed: {
            if (!finishing)
                theme.appearance.play("cancel")
            finishing = false
            if (page.visible) {
                if (returnToControls)
                    controls.forceActiveFocus()
                else
                    page.enter()
            }
            if (addFolderPending) {
                addFolderPending = false
                Qt.callLater(page.addFolder)
            }
        }

        contentItem: ColumnLayout {
            spacing: 6

            TextField {
                id: search
                objectName: "searchGamesField"
                Layout.fillWidth: true
                implicitHeight: 48
                leftPadding: 14
                rightPadding: 14
                color: page.theme.ink
                placeholderTextColor: page.theme.muted
                placeholderText: qsTr("Search titles, publishers, or file names")
                Accessible.name: placeholderText
                selectionColor: page.theme.accent
                selectedTextColor: page.theme.accentText
                font.family: page.theme.bodyFont
                font.pixelSize: page.theme.bodySize
                text: page.library.search
                onTextEdited: page.library.search = text
                KeyNavigation.up: done
                KeyNavigation.down: favorites
                Keys.onReturnPressed: filters.finish()
                Keys.onEnterPressed: filters.finish()
                background: Rectangle {
                    radius: page.theme.radius
                    color: page.theme.background
                    border.color: search.activeFocus ? page.theme.accent : page.theme.line
                    border.width: search.activeFocus ? page.theme.focusWidth : 1
                }
            }

            NavButton {
                id: favorites
                objectName: "filterFavoritesAction"
                Layout.fillWidth: true
                theme: page.theme
                selected: page.library.favoritesOnly
                text: qsTr("Favorites only")
                detail: selected ? qsTr("On") : qsTr("Off")
                KeyNavigation.up: search
                KeyNavigation.down: sort
                onClicked: {
                    page.theme.appearance.play("confirm")
                    page.library.favoritesOnly = !page.library.favoritesOnly
                }
            }

            NavButton {
                id: sort
                objectName: "sortLibraryAction"
                Layout.fillWidth: true
                theme: page.theme
                text: qsTr("Sort by")
                detail: page.sortLabels[page.sortIndex]
                KeyNavigation.up: favorites
                KeyNavigation.down: addFolder
                onClicked: {
                    page.theme.appearance.play("confirm")
                    page.library.sortOrder = page.sortKeys[(page.sortIndex + 1) % page.sortKeys.length]
                }
            }

            NavButton {
                id: addFolder
                Layout.fillWidth: true
                theme: page.theme
                text: qsTr("Add a folder")
                KeyNavigation.up: sort
                KeyNavigation.down: refresh.enabled ? refresh : done
                onClicked: {
                    filters.addFolderPending = true
                    filters.finish()
                }
            }

            NavButton {
                id: refresh
                Layout.fillWidth: true
                theme: page.theme
                text: qsTr("Refresh library")
                enabled: !page.library.scanning
                KeyNavigation.up: addFolder
                KeyNavigation.down: done
                onClicked: {
                    page.library.refresh()
                    filters.finish()
                }
            }

            NavButton {
                id: done
                Layout.fillWidth: true
                theme: page.theme
                emphasized: true
                text: qsTr("Done")
                KeyNavigation.up: refresh.enabled ? refresh : addFolder
                KeyNavigation.down: search
                onClicked: filters.finish()
            }
        }
    }
}
