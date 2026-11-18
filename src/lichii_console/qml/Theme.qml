// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

QtObject {
    id: theme
    required property var appearance
    property real viewScale: 1
    readonly property string mode: appearance.effectiveTheme
    readonly property bool dark: mode === "dark"

    readonly property color background: dark ? "#242525" : "#f0f0f0"
    readonly property color surface: dark ? "#333434" : "#e5e5e5"
    readonly property color panel: dark ? "#303131" : "#fafafa"
    readonly property color rail: dark ? "#2b2c2c" : "#e9e9e9"
    readonly property color hover: dark ? "#404242" : "#dfdfdf"
    readonly property color line: dark ? "#505252" : "#cacaca"
    readonly property color lineStrong: dark ? "#676969" : "#b8b9b9"
    readonly property color ink: dark ? "#f3f3f3" : "#252727"
    readonly property color muted: dark ? "#b5b8b8" : "#505858"
    readonly property color accent: dark ? "#91d3da" : "#22687a"
    readonly property color link: accent
    readonly property color selectedText: accent
    readonly property color danger: dark ? "#ffb0a5" : "#a52f28"
    readonly property color slot: dark ? "#353636" : "#dddddd"
    readonly property color keyBadge: dark ? "#404242" : "#e3e3e3"
    readonly property color keyBadgeText: ink
    readonly property color accentText: dark ? "#142931" : "#ffffff"
    readonly property color overlay: "#880f1010"

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
    readonly property int homeCommandsY: 505
    readonly property int homeCommandDiameter: 68
    readonly property int keyBadgeSize: 25

    property string confirmKey: "Enter"
    property string backKey: "Esc"
    property string optionsKey: "F3"
    property string libraryKey: "F2"
    property string gamepadFamily
    property string homeKey: "F1"

    function icon(name, onAccent = false) {
        const palette = onAccent ? (dark ? "light" : "dark") : mode
        return appearance.icon(name, palette, appearance.iconPack)
    }
}
