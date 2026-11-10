import QtQuick

import "../config" as Config

// Your own seat – the same plate as the opponent boxes (cards behind the plate,
// round avatar on its right end, stack on the second line), with the one
// difference that your hole cards are face up and stay visible.
//
// Your own box is deliberately a little larger than the opponent boxes and
// carries what only concerns you: anti-peek, the network status dot and the
// "show cards" flip.
SeatBox {
    id: root

    // Your own seat is seat 0 - all derivations from it are delivered by SeatBox.
    seatIndex: 0

    // Anti-peek (config key AntiPeekMode): keep your own hole cards covered,
    // uncover them briefly only by hovering/pressing. readConfigInt is not reactive –
    // it takes effect from the next instantiation of the self box (game start).
    readonly property bool antiPeek:
        (typeof SettingsManager !== "undefined" && SettingsManager && SettingsManager.configRevision >= 0)
            ? SettingsManager.readConfigInt("AntiPeekMode") !== 0 : false

    // Network status light (config key ShowPingStateInAvatar): only at your own
    // avatar and only once real ping data is available (pingState > 0).
    readonly property bool showPingState:
        (typeof SettingsManager !== "undefined" && SettingsManager && SettingsManager.configRevision >= 0)
            ? SettingsManager.readConfigInt("ShowPingStateInAvatar") !== 0 : false
    readonly property int pingState:
        (typeof GameTable !== "undefined" && GameTable) ? GameTable.pingState : 0
    readonly property color pingColor: pingState === 1 ? "#43a047"   // green
                                     : pingState === 2 ? "#fbc02d"   // yellow
                                     : pingState === 3 ? "#e53935"   // red
                                     : "transparent"

    // Player in the game? Whoever has no money left for the next hand is inactive.
    // A default of its own (true) as opposed to the opponent box.
    readonly property bool playerActive: seatData && seatData.active !== undefined ? seatData.active : true

    // Bet base (betInset/betStripH) and unfolding the base: see SeatBox.
    readonly property int hMargin: 4
    readonly property int vMargin: 4

    // ── Size of the box ──────────────────────────────────────────────────────
    // Same shape as the opponent plate, only larger: 148×80, cards 46 high,
    // plate 50, avatar 54 (it stands out above and below the plate).
    readonly property int _textX: 9
    readonly property int _cardH: Math.round(bodyH * 46 / 80)
    readonly property int _plateH: Math.round(bodyH * 50 / 80)
    readonly property int _avatarD: _plateH + 4
    readonly property int _plateY: bodyH - _plateH
    readonly property int _textW: Math.max(24, Math.round(width) - _avatarD - _textX - 4)
    readonly property int _nameSize: 14
    readonly property int _stackSize: 13

    implicitWidth: 148
    implicitHeight: 80

    // ── The action instead of the stack, for two seconds ─────────────────────
    property bool _flashAction: false
    readonly property bool _showAction: _flashAction && actionText !== "" && !isWinner
    onActionChanged: {
        if (root.action > 0 && !root.isWinner) {
            root._flashAction = true
            actionFlash.restart()
        } else if (root.action === 0) {
            root._flashAction = false
            actionFlash.stop()
        }
    }
    Timer {
        id: actionFlash
        interval: 2000
        onTriggered: root._flashAction = false
    }

    // Information density: folded → pull back subtly, out of the game →
    // darken clearly (analogous to the opponent boxes).
    opacity: !root.playerActive ? 0.4 : (root.folded ? 0.78 : 1.0)
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

    // Slightly "raised" when to act (depth/focus, a gentle transition).
    scale: root.isAtTurn ? 1.03 : 1.0
    transformOrigin: Item.Center
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }

    // ── The body: cards behind, plate in front, avatar on top ────────────────
    Item {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.bodyH + betStrip.height

        HoleCards {
            id: cards
            x: root.hMargin
            y: 0
            z: 1
            cardH: root._cardH
            card0: root.card0
            card1: root.card1
            fade0: root.fade0
            fade1: root.fade1
            antiPeek: root.antiPeek
            cardRenderScale: root.cardRenderScale
            visible: root.playerActive
        }

        // Click on "show cards" → turn over your own hole cards as a confirmation
        // that you really do show them (analogous to the widget client: showHoleCards → flip).
        Connections {
            target: (typeof GameTable !== "undefined") ? GameTable : null
            function onMyCardsShown() { cards.playShowFlip() }
        }

        Rectangle {
            id: plate
            x: 0
            y: root._plateY
            width: body.width
            height: root._plateH
            radius: height / 2
            color: "transparent"
            z: 10

            // Opaque on purpose: the hole cards lie BEHIND the plate.
            PlayerBoxBackground { radius: plate.height / 2; opacity: 1.0 }

            // Highlight: a gold frame + a soft glow when I am to act (the frame a bit
            // stronger than with the opponent boxes).
            PlayerTurnGlow { active: root.isAtTurn; borderWidth: 2; radius: plate.height / 2 }

            AppText {
                x: root._textX
                y: 8
                width: root._textW
                height: 17
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                color: "#eff1f5"
                font.pixelSize: root._nameSize
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
                elide: Text.ElideRight
                text: root.seatData && root.seatData.name !== "" ? root.seatData.name : qsTr("Du")
            }

            AppText {
                x: root._textX
                y: 26
                width: root._textW
                height: 16
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                text: root._showAction
                      ? root.actionText
                      : (root.seatData ? (GameTable ? GameTable.amount(root.seatData.stack) : "") : "")
                color: root._showAction
                       ? Config.Theme.actionEdgeColor(root.action)
                       : Config.Theme.colorAccent
                font.pixelSize: root._stackSize
                font.bold: true
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            PlayerTimeoutBar {
                id: timeoutBar
                readonly property bool atTurn: (typeof GameTable !== "undefined" && GameTable)
                                               && GameTable.timeoutSeatId === root.seatIndex
                active: atTurn
                visible: atTurn && !root.isWinner && !root._showAction
                fillColor: Config.Theme.colorTimeoutSelf
                width: root._textW
                height: 4
                x: root._textX
                y: plate.height - 10
                z: 14
            }
        }

        PlayerAvatar {
            id: avatar
            x: body.width - root._avatarD
            y: root._plateY + Math.round((root._plateH - root._avatarD) / 2)
            width: root._avatarD
            height: root._avatarD
            z: 12
            source: root.avatarSource
            playerActive: root.playerActive
            showNetworkStatus: root.showPingState && root.pingState > 0
            networkStatusColor: root.pingColor
            networkPingAvg: (typeof GameTable !== "undefined" && GameTable) ? GameTable.pingAvg : -1
            networkPingMin: (typeof GameTable !== "undefined" && GameTable) ? GameTable.pingMin : -1
            networkPingMax: (typeof GameTable !== "undefined" && GameTable) ? GameTable.pingMax : -1
        }

        // Winner highlight: a golden frame + a "WINNER" badge around the plate.
        Item {
            x: 0
            y: root._plateY
            width: body.width
            height: root._plateH
            z: 16
            PlayerWinnerOverlay {
                active: root.isWinner
                gap: 3
                badgeHeight: 18
                badgeFontSize: 10
                hPadding: 14
                radius: plate.height / 2
            }
        }

        // Bet base at the lower box edge (seat style "inset"). It only unfolds
        // while there really is a bet; in the style "classic" it stays permanently
        // 0 px high.
        PlayerBetStrip {
            id: betStrip
            open: root.stripOpen
            amount: root.bet
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    // ── Strip ABOVE the box: the bet (as a pile of chips) ────────────────────
    Item {
        id: topStrip
        z: 26
        width: root.width
        height: 22
        x: 0
        y: -height - 5

        // Right-aligned at the box edge (your own bet, next to your own plate).
        BetChip {
            id: betRow
            visible: root.bet > 0 && !root.betInset
            amount: root.bet
            iconSize: 16
            fontSize: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 2
        }
    }

    // Dealer/small/big blind button: at the top right NEXT TO the box (outside).
    BlindButtonImage {
        id: buttonImg
        visible: root.buttonVisible
        button: root.button
        anchors.left: parent.right
        anchors.leftMargin: 6
        anchors.top: parent.top
        z: 25
    }
}
