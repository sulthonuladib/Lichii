// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

QtObject {
    id: theme
    required property var appearance
    property real viewScale: 1
    readonly property string mode: appearance.effectiveTheme
    readonly property bool dark: mode === "dark"

    readonly property color background: dark ? "#161d23" : "#eff2f4"
    readonly property color surface: dark ? "#222d35" : "#e2e9ed"
    readonly property color panel: dark ? "#202a32" : "#f8fafb"
    readonly property color rail: dark ? "#1b242c" : "#e8edf0"
    readonly property color hover: dark ? "#2a3942" : "#dae5e9"
    readonly property color line: dark ? "#3c4b55" : "#c4cfd5"
    readonly property color lineStrong: dark ? "#53646d" : "#aebfc8"
    readonly property color ink: dark ? "#edf4f7" : "#1d2c36"
    readonly property color muted: dark ? "#aabac4" : "#4e626f"
    readonly property color accent: dark ? "#8fd2de" : "#266e81"
    readonly property color link: accent
    readonly property color selectedText: accent
    readonly property color danger: dark ? "#ffb0a5" : "#a52f28"
    readonly property color slot: dark ? "#29343d" : "#d8e1e6"
    readonly property color keyBadge: dark ? "#2a3942" : "#dbe5eb"
    readonly property color keyBadgeText: ink
    readonly property color accentText: dark ? "#142931" : "#ffffff"
    readonly property color overlay: "#880b1117"

    readonly property string displayFont: bodyFont
    readonly property string bodyFont: "Noto Sans"
    readonly property int logicalWidth: 1280
    readonly property int logicalHeight: 720
    readonly property int gutter: 64
    readonly property int footerHeight: 56
    readonly property int motion: appearance.reducedMotion ? 0 : 140

    readonly property int screenTitle: 32
    readonly property int homeTitle: 28
    readonly property int sectionTitle: 23
    readonly property int bodySize: 21
    readonly property int compactSize: 18
    readonly property int noteSize: 15
    readonly property int iconSize: 23
    readonly property int focusWidth: 3
    readonly property int radius: 6

    readonly property int homeHeader: 88
    readonly property int homeShelfY: 215
    readonly property int homeTile: 216
    readonly property int homeTileGap: 24
    readonly property int homeFocusInset: 5
    readonly property int homeFocusWidth: 3
    readonly property int homeCommandsY: 528
    readonly property int homeCommandDiameter: 68
    readonly property int keyBadgeSize: 25

    property string confirmKey: "Enter"
    property string backKey: "Esc"
    property string optionsKey: "Menu"
    property string homeKey: "F1"

    function icon(name, onAccent = false) {
        const palette = onAccent ? (dark ? "light" : "dark") : mode
        return appearance.icon(name, palette, appearance.iconPack)
    }
}
