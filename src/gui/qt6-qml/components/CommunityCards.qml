import QtQuick
import QtQuick.Effects

import "../config" as Config

// Community cards (flop/turn/river) + pot badge in the centre of the table.
// Size = the card row; position/scaling/z is set by the caller (anchors +
// scale), so that the cards stay centred depending on the table layout.
Item {
    id: root

    // Landscape? Controls the distance of the pot badge to the card row.
    property bool wide: false

    // Effective table scaling of this card row (communityScale × zoom), passed on
    // to the cards so that their SVG raster hits the real screen size
    // (see CardImage.renderScale).
    property real cardRenderScale: 1.0
    // The same effective scale for the chips of the pot (see ChipImage.renderScale).
    property real chipRenderScale: 1.0
    // Cash table: money ladder with the 1¢ rung and money labels (see ChipImage).
    property bool chipCash: false
    // Landscape: the pot sum hangs at the TOP of the felt (`PotAmount` in GamePage,
    // between the two topmost seats, player's request 2026-11-13), so the chips here
    // stay without their pill and the strip above the board holds only the pot
    // picture. Portrait keeps the pill under the chips: there the top-centre seat
    // stands exactly where the label would go.
    property bool amountAtTop: false

    // True while any board slot is currently revealing new cards
    // (stagger/backside/flip). The action bar locks the buttons on it –
    // during reveal animations no action is possible.
    readonly property bool dealing: slot0.revealing || slot1.revealing
                                    || slot2.revealing || slot3.revealing
                                    || slot4.revealing

    width: cardRow.width
    height: cardRow.height
    transformOrigin: Item.Center

    // Inline component for a single board card slot.
    // Card aspect ratio 120:168 (≈0.714).
    //
    // Deal sequence (as in the widget client):
    //   1. placeholder frame visible (initial state)
    //   2. isDealt → after the stagger pause: _displayIndex = -1  (the backside appears)
    //   3. short pause (like dealCardsSpeed in the widget client)
    //   4. _displayIndex = actualCardIndex  → CardImage.onIsBackChanged fires the flip
    component CommunitySlot: Item {
        id: slot
        property int boardIndex: 0
        width: 46; height: 64

        // Showdown spotlight: if this board card does not belong to the winning hand,
        // it is dimmed to 25 % opacity (setting "fade out animation
        // for loser cards", config key ShowFadeOutCardsAnimation).
        // Data: GameHandler.boardCardFade (5 bools per board card).
        readonly property bool fadeLosingCards:
            (typeof SettingsManager !== "undefined" && SettingsManager && SettingsManager.configRevision >= 0)
                ? SettingsManager.readConfigInt("ShowFadeOutCardsAnimation") !== 0 : true
        readonly property bool faded: {
            if (!fadeLosingCards)
                return false
            var f = (typeof GameTable !== "undefined" && GameTable) ? GameTable.boardCardFade : null
            return (f && boardIndex < f.length) ? f[boardIndex] === true : false
        }
        opacity: faded ? 0.25 : 1.0
        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

        // Is this slot currently revealing? (stagger + backside + flip). While it is
        // true, the action bar locks the buttons. With its closing pause, dealAnim
        // also covers the ~470 ms CardImage flip animation.
        readonly property bool revealing: dealAnim.running

        readonly property bool isDealt: {
            var cnt = (typeof GameTable !== "undefined" && GameTable)
                      ? GameTable.boardCardCount : 0
            return boardIndex < cnt
        }

        readonly property int actualCardIndex: {
            var cards = (typeof GameTable !== "undefined" && GameTable)
                        ? GameTable.boardCards : null
            return (cards && boardIndex < cards.length) ? cards[boardIndex] : -1
        }

        // -2 = placeholder (nothing yet), -1 = backside, 0-51 = front side
        property int _displayIndex: -2

        // Placeholder frame: only visible while no card is shown yet
        Rectangle {
            anchors.fill: parent
            visible: slot._displayIndex === -2
            radius: 4
            color: Qt.rgba(0, 0, 0, 0.30)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.38)
        }

        // Card: backside (cardIndex -1) or front side (cardIndex 0-51)
        CardImage {
            anchors.fill: parent
            visible: slot._displayIndex !== -2
            cardIndex: slot._displayIndex >= 0 ? slot._displayIndex : -1
            renderScale: root.cardRenderScale
        }

        onIsDealtChanged: {
            if (isDealt) {
                dealAnim.restart()
            } else {
                dealAnim.stop()
                slot._displayIndex = -2
            }
        }

        // Stagger → show the backside → flip to the front side (a ScriptAction sets
        // _displayIndex, which triggers CardImage.onIsBackChanged and thereby the flip).
        SequentialAnimation {
            id: dealAnim
            // Flop staggered (0 / 140 / 280 ms); turn/river without a delay
            PauseAnimation { duration: slot.boardIndex < 3 ? slot.boardIndex * 220 : 0 }
            ScriptAction   { script: { slot._displayIndex = -1 } }
            // Pause: the backside is visible (analogous to dealCardsSpeed in the widget client)
            PauseAnimation { duration: 160 }
            // Now turn to the front side → the CardImage flip animation fires
            ScriptAction   { script: { slot._displayIndex = slot.actualCardIndex } }
            // Wait for the flip duration (CardImage: 170 + 300 ms) so that `revealing`
            // stays true until the end of the reveal animation.
            PauseAnimation { duration: 480 }
        }
    }

    // Soft glow behind the community cards → focus on the
    // centre of the table (subtle, warm).
    Rectangle {
        anchors.centerIn: cardRow
        width: cardRow.width + 80
        height: cardRow.height + 54
        radius: height / 2
        color: Qt.rgba(1.0, 0.93, 0.72, 0.12)
        z: -1
        layer.enabled: Config.Theme.effectsEnabled
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 1.0
            blurMax: 48
            autoPaddingEnabled: true
        }
    }

    // 5 Slots: Flop (0-2) | Turn (3) | River (4)
    Row {
        id: cardRow
        anchors.centerIn: parent
        spacing: 3

        CommunitySlot { id: slot0; boardIndex: 0 }
        CommunitySlot { id: slot1; boardIndex: 1 }
        CommunitySlot { id: slot2; boardIndex: 2 }

        Item { width: 8; height: 1 }

        CommunitySlot { id: slot3; boardIndex: 3 }

        Item { width: 8; height: 1 }

        CommunitySlot { id: slot4; boardIndex: 4 }
    }

    // ── The pot ──────────────────────────────────────────────────────────────
    // The pot is the same chips as a player's bet (`BetChip`), but laid out as a
    // FLAT ROW of up to three stacks (two chips each) with the sum in a dark pill
    // under them. A pyramid with its top stack would be 52.13 base px high and push
    // the top-centre seat's bet into its plate on a small table; the row is 72 wide
    // and 34.7 high and still shows up to six chips.
    //
    // Since 2026-11-12 the pot shows ONLY COLLECTED chips (`GameTable.pot`, not
    // `totalPot` = pot + the current betting round): when a player bets, the
    // chips are in front of him, not in the pot yet. The pill and the chips
    // change when the engine collects the round — that is when they "arrive".
    //
    // The chips are 22 px, the same as on a bet (they were 18 px): at 18 px the
    // chip was below the digit threshold on a small table and the player reported
    // that chips "lose their digit when they get into the pot" (2026-11-12);
    // two lone discs also did not read as a pile ("стопок в банке тоже нет" — that
    // 1195 was the pot). Up to two chips per stack: four chips of one denomination
    // (e.g. a 400 pot) are spread 2+2, and a stack of four would add 2.2 px of
    // height for nothing — the ladder never gives more than four of one rung.
    //
    // Height budget (Lead, GamePage): 22 px chips + pill = at most 34.7 base
    // pixels, sits 6 above the card row → 32 (half a card) + 6 + 34.7 = 72.7
    // units above the centre of the card row for `potTop` in GamePage; the whole
    // "pot + cards" block is 104.7 high. In landscape the pill moves to the top of
    // the felt (2026-11-13) and the chips alone are 21.73 high – but the page keeps
    // the 72.7 reserve: the pot picture that goes into that strip is the next step,
    // and a reserve is cheaper than moving the cards again.
    Item {
        id: potBadge
        anchors.horizontalCenter: cardRow.horizontalCenter
        anchors.bottom: cardRow.top
        // Fixed by the vertical budget above: 6 both in landscape and portrait.
        anchors.bottomMargin: 6
        visible: (typeof GameTable !== "undefined" && GameTable) ? GameTable.pot > 0 : false
        width: potChip.width
        height: potChip.height
        transformOrigin: Item.Center

        BetChip {
            id: potChip
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            amount: (typeof GameTable !== "undefined" && GameTable) ? GameTable.pot : 0
            // Same chip size as a player's bet, flat row of three stacks: the value
            // is the readable part (`ChipImage` prints it from 22 screen px), the
            // pill below is the truth.
            chipSize: 22
            pyramid: false
            maxPiles: 3
            maxPerStack: 2
            // Same pill height as the felt bets (font 10 → 13 px): the strip above
            // the board has no room for a bigger one.
            fontSize: 10
            textColor: Config.Theme.colorAccent
            cash: root.chipCash
            renderScale: root.chipRenderScale
            // Landscape: the sum hangs at the top of the felt (PotAmount).
            showAmount: !root.amountAtTop
        }

        SequentialAnimation {
            id: potPop
            NumberAnimation { target: potBadge; property: "scale"; from: 1.0; to: 1.15; duration: 110; easing.type: Easing.OutQuad }
            NumberAnimation { target: potBadge; property: "scale"; to: 1.0; duration: 170; easing.type: Easing.OutBack }
        }
        Connections {
            target: (typeof GameTable !== "undefined") ? GameTable : null
            function onPotChanged() {
                if (GameTable && GameTable.pot > 0) potPop.restart()
            }
        }
    }
}
