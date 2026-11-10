import QtQuick

// The two hole cards of a seat, drawn UNDER the player plate: only the upper part
// sticks out, the lower part is covered by the plate. That is enough – at the
// showdown one reads the rank and suit in the TOP LEFT corner of the card, as in
// every other client.
//
// A folded player's cards are gone entirely (not dimmed): the muck.
Item {
    id: root

    property int card0: -1
    property int card1: -1
    // Showdown spotlight: dim a hole card that does not count towards the
    // winning hand (data from the GameHandler via seatData.fade0/fade1).
    property bool fade0: false
    property bool fade1: false
    // Anti-peek (self box only): keep the cards covered until they are lifted.
    property bool antiPeek: false
    property real cardRenderScale: 1.0

    property int cardH: 40
    readonly property int cardW: Math.round(cardH * 120 / 168)

    // Setting "fade out animation for loser cards" (ShowFadeOutCardsAnimation).
    readonly property bool fadeLosingCards:
        (typeof SettingsManager !== "undefined" && SettingsManager && SettingsManager.configRevision >= 0)
            ? SettingsManager.readConfigInt("ShowFadeOutCardsAnimation") !== 0 : true

    readonly property bool _peeking: peekArea.enabled
                                     && (peekArea.containsMouse || peekArea.pressed)

    implicitWidth: cardW * 2 + 4
    implicitHeight: cardH
    width: implicitWidth
    height: implicitHeight

    // "Show" confirmation: turn over both hole cards (self box, on "show cards").
    function playShowFlip() {
        card0Img.playShowFlip()
        card1Img.playShowFlip()
    }

    // ── Card 0 ───────────────────────────────────────────────────────────────
    Item {
        x: 0
        y: 0
        width: root.cardW
        height: root.cardH
        // Showdown: dim a card not counting towards the winning hand to 25 %.
        opacity: (root.fade0 && root.fadeLosingCards) ? 0.25 : 1.0
        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

        CardImage { id: card0Img; anchors.fill: parent; cardIndex: root.card0; renderScale: root.cardRenderScale }

        // Anti-peek cover (card back) above the real front side.
        CardImage {
            anchors.fill: parent
            cardIndex: -1
            renderScale: root.cardRenderScale
            readonly property bool covering: root.antiPeek && root.card0 >= 0 && !root._peeking
            visible: opacity > 0
            opacity: covering ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
        }
    }

    // ── Card 1 ───────────────────────────────────────────────────────────────
    Item {
        x: root.cardW + 4
        y: 0
        width: root.cardW
        height: root.cardH
        opacity: (root.fade1 && root.fadeLosingCards) ? 0.25 : 1.0
        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

        // flipDelay staggers the dealing: the second card turns 80 ms later
        CardImage { id: card1Img; anchors.fill: parent; cardIndex: root.card1; flipDelay: 80; renderScale: root.cardRenderScale }

        CardImage {
            anchors.fill: parent
            cardIndex: -1
            renderScale: root.cardRenderScale
            readonly property bool covering: root.antiPeek && root.card1 >= 0 && !root._peeking
            visible: opacity > 0
            opacity: covering ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
        }
    }

    // "Lift" area above both cards (anti-peek). With anti-peek disabled it is
    // inert (enabled: false → events fall through).
    MouseArea {
        id: peekArea
        anchors.fill: parent
        enabled: root.antiPeek && (root.card0 >= 0 || root.card1 >= 0)
        hoverEnabled: enabled
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
