// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: page
    objectName: "settingsScreen"
    required property Theme theme
    required property var backend
    property string category: "general"
    property string programId
    property string gameTitle
    property string focusedKey
    property string rememberedCategory: "general"
    property int settingsRevision: 0
    property bool sliderEditing: false
    property int sliderDraft: 0
    signal backRequested()
    signal folderRequested(string purpose)
    signal resetRequested()
    signal choiceRequested(var row)
    signal actionRequested(string action)
    readonly property var categories: programId.length
        ? [
            { id: "general", label: qsTr("Properties"), group: 0 },
            { id: "graphics", label: qsTr("Graphics"), group: 0 },
            { id: "cpu", label: qsTr("CPU"), group: 0 }
        ] : [
            { id: "general", label: qsTr("General"), group: 0 },
            { id: "directories", label: qsTr("Game directories"), group: 0 },
            { id: "graphics", label: qsTr("Graphics"), group: 1 },
            { id: "cpu", label: qsTr("CPU and emulation"), group: 1 },
            { id: "audio", label: qsTr("Audio"), group: 1 },
            { id: "controllers", label: qsTr("Controllers"), group: 1 },
            { id: "system", label: qsTr("System"), group: 2 },
            { id: "users", label: qsTr("User profiles"), group: 2 },
            { id: "storage", label: qsTr("Storage and saves"), group: 2 },
            { id: "interface", label: qsTr("Interface"), group: 2 },
            { id: "appearance", label: qsTr("Appearance"), group: 2 },
            { id: "advanced", label: qsTr("Advanced"), group: 2 }
        ]
    readonly property var selectedCategory: categories.find(entry => entry.id === category)
    readonly property var appearance: backend.appearance
    readonly property var library: backend.library
    readonly property var settingRows: buildRows(category, programId, settingsRevision)
    readonly property var selectedValue: settingRows[entries.currentIndex] ?? null
    readonly property bool adjustingSlider: entries.activeFocus && sliderEditing
    readonly property string confirmCaption: {
        if (adjustingSlider)
            return qsTr("Apply")
        if (categoryList.activeFocus)
            return settingRows.some(canEdit) ? qsTr("Open") : ""
        if (!entries.activeFocus || !canEdit(selectedValue))
            return ""
        if (selectedValue.type === "slider")
            return qsTr("Adjust")
        return qsTr("Select")
    }

    function buildRows(section, titleId, revision) {
        if (section === "directories") {
            let items = [
                { key: "add-folder", type: "action", label: qsTr("Game folders"),
                    description: qsTr("Lichii scans these folders for software."), button: qsTr("Add folder") },
                { key: "refresh", type: "action", label: qsTr("Scan game folders"),
                    button: library.scanning ? qsTr("Scanning…") : qsTr("Refresh"),
                    enabled: !library.scanning }
            ]
            for (const path of library.folders) {
                items.push({ key: "remove-folder", type: "action", label: path, folder: path,
                    button: qsTr("Remove") })
                items.push({ key: "deep-scan", type: "toggle", label: qsTr("Scan subfolders"),
                    folder: path, value: library.deepScan(path) })
            }
            return items
        }
        if (section === "appearance") {
            return [
                { key: "theme", type: "choice", label: qsTr("Color theme"),
                    options: [qsTr("Basic Black"), qsTr("Basic White"), qsTr("Follow system")],
                    value: ["dark", "light", "system"].indexOf(appearance.theme) },
                { key: "reduced-motion", type: "toggle", label: qsTr("Reduce motion"),
                    value: appearance.reducedMotion },
                { key: "ui-mute", type: "toggle", label: qsTr("Mute interface sounds"),
                    value: appearance.muted },
                { key: "ui-volume", type: "slider", label: qsTr("Interface sound volume"),
                    value: Math.round(appearance.soundVolume * 100) },
                { key: "icon-pack", type: "action", label: qsTr("Icon folder"),
                    description: appearance.iconPack.toString(), button: qsTr("Choose folder") },
                { key: "sound-pack", type: "action", label: qsTr("Sound folder"),
                    description: appearance.soundPack.toString(), button: qsTr("Choose folder") },
                { key: "reset-packs", type: "reset", label: qsTr("Restore built-in icons and sounds"),
                    button: qsTr("Restore"), enabled: !!(appearance.iconPack.toString() ||
                                                        appearance.soundPack.toString()) }
            ]
        }
        let items = backend.settings(section, titleId)
        if (section === "controllers" && !titleId)
            items = items.concat([{ key: "configure-controller", type: "action",
                label: qsTr("Controller assignment and button mapping"),
                description: backend.state === "idle" ? qsTr("Edit devices, player assignments, and input profiles.")
                    : qsTr("Stop the game to configure controllers."),
                enabled: backend.state === "idle", button: qsTr("Configure") }])
        if (section === "storage")
            items = items.concat([{ key: "open-user-data", type: "action",
                label: qsTr("Lichii data folder"), button: qsTr("Open folder") },
                { key: "open-save-folder", type: "action", label: qsTr("Save data folder"),
                    button: qsTr("Open folder") }])
        if (section === "advanced")
            items = items.concat([{ key: "open-logs", type: "action", label: qsTr("Log folder"),
                button: qsTr("Open folder") }])
        if (section === "interface")
            items = items.concat([{ key: "open-appearance", type: "submenu",
                label: qsTr("Console appearance"), button: qsTr("Open") }])
        if (section === "users")
            items = items.concat([{ key: "current-profile", type: "description",
                label: qsTr("Active profile"), description: backend.profileName },
                { key: "manage-profiles", type: "action", label: qsTr("Manage user profiles"),
                    description: backend.state === "idle" ? "" : qsTr("Stop the game to manage profiles."),
                    enabled: backend.state === "idle", button: qsTr("Manage") }])
        if (titleId && section === "general")
            items = items.concat([{ key: "reset-game", type: "reset",
                label: qsTr("Reset per-game settings"), enabled: backend.state === "idle",
                button: qsTr("Reset") }])
        if (!items.length)
            return [{ key: "empty", type: "description",
                label: titleId && backend.state !== "idle"
                    ? qsTr("Stop the game to change its settings.") : qsTr("No settings in this category") }]
        return items
    }

    function canEdit(row) {
        return !!row && row.type !== "description" && row.type !== "warning" && row.enabled !== false
    }

    function rowKey(row) {
        return row.key + "/" + (row.folder || "")
    }

    function choose(section, feedback = false) {
        const index = categories.findIndex(entry => entry.id === section)
        if (index < 0)
            return
        if (feedback && category !== section)
            appearance.play("focus")
        if (category !== section) {
            category = section
            if (!programId)
                rememberedCategory = section
        }
        if (categoryList.currentIndex !== index)
            categoryList.currentIndex = index
        if (index < categoryList.count)
            categoryList.positionViewAtIndex(index, ListView.Contain)
    }

    function enter(section) {
        const destination = section || (programId ? "general" : rememberedCategory)
        choose(categories.some(entry => entry.id === destination) ? destination : "general")
        focusCategory()
    }

    function moveCategory(direction) {
        const next = Math.min(categories.length - 1, Math.max(0, categoryList.currentIndex + direction))
        choose(categories[next].id, true)
    }

    function handleAction(row) {
        if (!canEdit(row))
            return
        if (row.type === "choice") {
            choiceRequested(row)
            return
        }
        switch (row.key) {
        case "add-folder":
        case "icon-pack":
        case "sound-pack": folderRequested(row.key); break
        case "remove-folder": library.removeFolder(row.folder); break
        case "refresh": library.refresh(); break
        case "reset-packs":
            appearance.iconPack = ""
            appearance.soundPack = ""
            break
        case "configure-controller": backend.configureController(); break
        case "manage-profiles": backend.manageProfiles(); break
        case "open-user-data": backend.openData("user"); break
        case "open-save-folder": backend.openData("saves"); break
        case "open-logs": backend.openData("log"); break
        case "open-appearance": choose("appearance", true); enter("appearance"); break
        case "reset-game": resetRequested(); break
        default: actionRequested(row.key); break
        }
    }

    function edit(row, value) {
        let changed = true
        switch (row.key) {
        case "theme": appearance.theme = ["dark", "light", "system"][value]; break
        case "reduced-motion": appearance.reducedMotion = value; break
        case "ui-mute": appearance.muted = value; break
        case "ui-volume": appearance.soundVolume = value / 100; break
        case "deep-scan":
            changed = library.setDeepScan(row.folder, value)
            break
        default: changed = backend.setSetting(row.key, value, programId); break
        }
        if (changed && row.type === "toggle")
            appearance.play("confirm")
        settingsRevision++
    }

    function focusCategory() {
        sliderEditing = false
        categoryList.forceActiveFocus()
    }

    function selectRow(index, feedback = false) {
        const row = settingRows[index]
        if (!canEdit(row))
            return
        const key = rowKey(row)
        if (focusedKey !== key) {
            sliderEditing = false
            if (feedback)
                appearance.play("focus")
        }
        focusedKey = key
        entries.currentIndex = index
        if (index < entries.count)
            entries.positionViewAtIndex(index, ListView.Contain)
    }

    function nextEditableRow(start, direction) {
        for (let i = start; i >= 0 && i < settingRows.length; i += direction) {
            if (canEdit(settingRows[i]))
                return i
        }
        return -1
    }

    function enterValues() {
        const remembered = settingRows.findIndex(row => rowKey(row) === focusedKey && canEdit(row))
        const index = remembered >= 0 ? remembered : nextEditableRow(0, 1)
        if (index < 0)
            return
        selectRow(index)
        entries.forceActiveFocus()
    }

    function moveValue(direction) {
        if (sliderEditing)
            return
        const index = nextEditableRow(entries.currentIndex + direction, direction)
        if (index >= 0)
            selectRow(index, true)
    }

    function restoreRowSelection() {
        if (!focusedKey)
            return
        const remembered = settingRows.findIndex(row => rowKey(row) === focusedKey && canEdit(row))
        const index = remembered >= 0 ? remembered : nextEditableRow(0, 1)
        if (index >= 0) {
            selectRow(index)
        } else {
            focusedKey = ""
            entries.currentIndex = -1
            if (entries.activeFocus)
                focusCategory()
        }
    }

    function activateValue() {
        const row = selectedValue
        if (!canEdit(row))
            return
        if (row.type === "toggle") {
            edit(row, !row.value)
        } else if (row.type === "slider") {
            if (sliderEditing) {
                sliderEditing = false
                if (sliderDraft !== row.value)
                    edit(row, sliderDraft)
            } else {
                sliderDraft = row.value
                sliderEditing = true
            }
        } else {
            handleAction(row)
        }
    }

    function adjustSlider(direction) {
        const row = selectedValue
        if (row?.type === "slider")
            sliderDraft = Math.max(row.min ?? 0, Math.min(row.max ?? 100,
                sliderDraft + direction * (row.step ?? 1)))
    }

    function resetRowSelection() {
        sliderEditing = false
        focusedKey = ""
        entries.currentIndex = -1
        entries.positionViewAtBeginning()
    }

    onCategoryChanged: resetRowSelection()
    onProgramIdChanged: resetRowSelection()
    onSettingRowsChanged: Qt.callLater(restoreRowSelection)
    onVisibleChanged: {
        if (!visible)
            sliderEditing = false
    }

    Item {
        id: settingsHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: page.theme.gutter
        anchors.rightMargin: page.theme.gutter
        height: 86

        Image {
            id: headerIcon
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: width
            source: page.theme.icon(page.programId ? "game" : "settings")
            sourceSize.width: width
            sourceSize.height: height
            fillMode: Image.PreserveAspectFit
        }
        Label {
            anchors.left: headerIcon.right
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: page.programId && page.gameTitle.length
                ? page.gameTitle : qsTr("System Settings")
            textFormat: Text.PlainText
            color: page.theme.ink
            font.family: page.theme.displayFont
            font.pixelSize: page.theme.screenTitle
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: page.theme.line
        }
    }

    Rectangle {
        anchors.left: categoryList.right
        anchors.leftMargin: 18
        anchors.top: categoryList.top
        anchors.bottom: categoryList.bottom
        width: 1
        color: page.theme.line
    }

    ListView {
        id: categoryList
        objectName: "settingsCategories"
        anchors.left: settingsHeader.left
        anchors.top: settingsHeader.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: 264
        clip: true
        model: page.categories
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Settings categories")

        Keys.onRightPressed: page.enterValues()
        Keys.onUpPressed: page.moveCategory(-1)
        Keys.onDownPressed: page.moveCategory(1)
        Keys.onReturnPressed: page.enterValues()
        Keys.onEnterPressed: page.enterValues()

        delegate: Item {
            id: categoryRow
            objectName: "settingsCategory"
            required property int index
            required property var modelData
            readonly property bool chosen: page.category === modelData.id
            readonly property bool highlighted: chosen && categoryList.activeFocus
            readonly property bool groupStart: index > 0
                && page.categories[index - 1].group !== modelData.group
            width: categoryList.width
            height: 42 + (groupStart ? 10 : 0)
            Accessible.role: Accessible.ListItem
            Accessible.name: modelData.label
            Accessible.selected: chosen
            Accessible.focused: highlighted
            Accessible.onPressAction: activate()

            function activate() {
                page.choose(modelData.id, true)
                page.focusCategory()
            }

            Rectangle {
                visible: categoryRow.groupStart
                x: 12
                y: 4
                width: parent.width - 30
                height: 1
                color: page.theme.line
            }

            Rectangle {
                id: categorySurface
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom
                height: 40
                color: categoryRow.chosen ? page.theme.surface
                    : hover.hovered ? page.theme.hover : "transparent"
                border.width: categoryRow.highlighted ? 2 : 0
                border.color: page.theme.accent

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    text: categoryRow.modelData.label
                    textFormat: Text.PlainText
                    color: categoryRow.chosen ? page.theme.selectedText : page.theme.ink
                    font.family: page.theme.bodyFont
                    font.pixelSize: page.theme.compactSize
                    font.weight: categoryRow.chosen ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            HoverHandler { id: hover }
            TapHandler { onTapped: categoryRow.activate() }
        }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
    }

    Label {
        id: sectionTitle
        anchors.left: categoryList.right
        anchors.leftMargin: 44
        anchors.right: settingsHeader.right
        anchors.top: categoryList.top
        text: page.selectedCategory?.label ?? ""
        color: page.theme.ink
        font.family: page.theme.displayFont
        font.pixelSize: page.theme.sectionTitle
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    ListView {
        id: entries
        objectName: "settingsValues"
        anchors.left: sectionTitle.left
        anchors.right: settingsHeader.right
        anchors.top: sectionTitle.bottom
        anchors.topMargin: 16
        anchors.bottom: categoryList.bottom
        currentIndex: -1
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        keyNavigationEnabled: false
        model: page.settingRows
        Accessible.role: Accessible.List
        Accessible.name: sectionTitle.text

        Keys.onUpPressed: page.moveValue(-1)
        Keys.onDownPressed: page.moveValue(1)
        Keys.onLeftPressed: {
            if (page.sliderEditing)
                page.adjustSlider(-1)
            else
                page.focusCategory()
        }
        Keys.onRightPressed: {
            if (page.sliderEditing)
                page.adjustSlider(1)
        }
        Keys.onReturnPressed: event => {
            if (!event.isAutoRepeat)
                page.activateValue()
        }
        Keys.onEnterPressed: event => {
            if (!event.isAutoRepeat)
                page.activateValue()
        }
        Keys.onShortcutOverride: event => {
            if (event.key === Qt.Key_Escape && page.sliderEditing)
                event.accepted = true
        }
        Keys.onEscapePressed: event => {
            event.accepted = page.sliderEditing
            page.sliderEditing = false
        }

        delegate: SettingRow {
            objectName: "settingsValue"
            required property int index
            required property var modelData
            theme: page.theme
            setting: modelData
            width: entries.width - 8
            highlighted: ListView.isCurrentItem && entries.activeFocus
            adjusting: highlighted && page.sliderEditing
            displayValue: adjusting ? page.sliderDraft : (modelData.value ?? 0)
            onSelected: {
                page.selectRow(index)
                entries.forceActiveFocus()
            }
            onActivated: page.activateValue()
            onValueEdited: value => {
                page.sliderEditing = false
                page.edit(modelData, value)
            }
        }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
    }
}
