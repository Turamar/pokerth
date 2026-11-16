import QtQuick

// The player's OWN current hand on the flop/turn/river – the calm counterpart of
// WinningHandBadge (PLAN.md §6, 2026-11-10): a dark pill, normal font, no gold,
// no glow and no "pop". It is a hint "what do I have", not advice: only the name
// of the combination, no strength and no percentages.
//
// The category comes from GameHandler.currentHandCategory (C++): 0 = high card …
// 9 = royal flush, the order of the handDefs in GameInfoPanel. The NAME is
// translated here, not in C++, like every other interface text.
//
// Deliberately standalone (NOT inside the community cards), so that it lies above
// the player boxes independently of their z/scale – and in the same place as the
// winning hand badge, which is never shown at the same time: the pill is hidden
// from the end of the hand (GameHandler sets -1), the golden badge appears then.
Rectangle {
    id: currentHandBadge

    // Reference element (community cards): the pill hangs below the (scaled) card row.
    property Item community: null
    property bool wide: false
    property real communityScale: 1.0

    // Category index (0 = high card … 9 = royal flush) → German source, translated
    // through the .ts like every other string. Same wording as GameInfoPanel.
    readonly property var handNames: [
        qsTr("Höchste Karte"),
        qsTr("Paar"),
        qsTr("Zwei Paare"),
        qsTr("Drilling"),
        qsTr("Straße"),
        qsTr("Flush"),
        qsTr("Full House"),
        qsTr("Vierling"),
        qsTr("Straight Flush"),
        qsTr("Royal Flush")
    ]

    // −1 = nothing to show (preflop, folded, spectator, hand over, setting off).
    readonly property int category:
        (typeof GameTable !== "undefined" && GameTable) ? GameTable.currentHandCategory : -1

    z: 49   // above the boxes (z:1), just below the winning badge (z:50)
    visible: category >= 0 && category < handNames.length
    anchors.horizontalCenter: parent.horizontalCenter
    // The same distance to the card row as the winning-hand badge (portrait 6,
    // landscape 8 – each · communityScale), so the pill never jumps when the two
    // replace each other.
    y: community
       ? community.y + community.height / 2
         + (community.height * community.scale) / 2
         + (wide ? 8 : 6) * community.scale
       : 0
    width: currentHandLabel.implicitWidth + 18
    height: Math.max(17, Math.round(22 * communityScale))
    radius: height / 2
    // Matt graphite like the player plates – deliberately no accent colour of the
    // table style: the pill is information, not decoration.
    color: Qt.rgba(0.105, 0.129, 0.161, 0.86)    // #1b2129 at 86 %
    border.color: Qt.rgba(0.2, 0.235, 0.282, 0.9) // #333c48
    border.width: 1

    AppText {
        id: currentHandLabel
        anchors.centerIn: parent
        text: (currentHandBadge.category >= 0
               && currentHandBadge.category < currentHandBadge.handNames.length)
              ? currentHandBadge.handNames[currentHandBadge.category] : ""
        color: "#d6d9de"
        font.pixelSize: Math.max(9, Math.round(12 * currentHandBadge.communityScale))
        // Normal weight on purpose: the golden winning badge is bold.
    }
}
