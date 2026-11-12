import QtQuick
import QtQuick.Controls

import "../config" as Config

// The plate of an opponent seat.
//
// Shape since 2026-11-10 (the player's mock-up): the two hole cards stand BEHIND
// the plate and stick out above its upper edge, the plate is a "pill" with the
// name on the first line and the stack on the second, and the round avatar sits
// on the right end of the plate, a little larger than the plate is high.
//
// The second line is the stack – except for two seconds after the player acted:
// then it shows the action (fold/check/call/bet/raise/all-in) and returns to the
// stack afterwards. A folded player's cards disappear (the muck); the plate
// itself stays dimmed.
//
// The box is 112 × 70 base pixels (before the table scale); the numbers are
// derived from `bodyH` below so that the seat style "inset" (bet base in the box)
// keeps working unchanged.
SeatBox {
    id: root

    // Show the winner badge below instead of above the box – only sensible for the
    // topmost box (player 5) in portrait, otherwise it would bump into the top.
    property bool winnerBelow: false
    // The side on which the dealer/blind button is shown (and, in the seat style
    // "inset", the bet base): "top" | "bottom" | "left" | "right". The bet itself
    // lies on the FELT in the style "classic" – see betAt below.
    property string betSide: up ? "bottom" : "top"
    // A split display: the dealer/blind button LEFT of the box, the bet
    // RIGHT of the box – both vertically centred. For the topmost box in
    // landscapeCompact, whose badge would otherwise collide below with the pot badge.
    // It overrides betSide.
    property bool betSplit: false

    // Where the bet chip lies when it is NOT shown next to the box but on the
    // felt (seat style "classic"): the centre of the chip in this item's own
    // coordinates, put on the line from the box towards the centre of the
    // table by GamePage (betFeltPoint).
    // `betOnFelt` says whether it is used at all. The coordinates must NOT
    // double as the flag: the chip lies OUTSIDE the plate, so to the left of a
    // right-hand seat its x is negative (that was the bug on 2026-11-10 – the
    // right seats kept their old chip on the outer contour).
    property bool betOnFelt: false
    property point betAt: Qt.point(0, 0)
    readonly property bool betOnFeltShown: betOnFelt && bet > 0 && !betInset

    // ── Size of the box ──────────────────────────────────────────────────────
    // Everything is derived from the body height, so that the seat style "inset"
    // (which adds the bet base below the body) does not move a single pixel:
    //   envelope 112×70  =  cards 40 (upper 26 visible) + plate 44 (14 px overlap)
    //   plate    44 high =  two text lines
    //   avatar   46      =  plate height + 2 (it stands out above and below)
    readonly property int _hMargin: 4
    readonly property int _textX: 8
    readonly property int _cardH: Math.round(bodyH * 40 / 70)
    readonly property int _cardW: Math.round(_cardH * 120 / 168)
    readonly property int _plateH: Math.round(bodyH * 44 / 70)
    // The avatar stands out of the plate only slightly (2 px larger): that keeps
    // it a round anchor while leaving the name line two characters more room, and
    // it no longer hangs 2 px below the 112×70 envelope (now 1 px).
    readonly property int _avatarD: _plateH + 2
    readonly property int _plateY: bodyH - _plateH
    // Up to the left edge of the avatar: the name line may use the full room
    // above the round profile instead of cutting a name like "Player 9" short.
    readonly property int _textW: Math.max(24, Math.round(width) - _avatarD - _textX)

    // Type scale of the table (Theme.tableFont adds 12 % in the usual 1024ish
    // window), so that the name and the stack/action stay readable.
    readonly property int _nameSize: Config.Theme.tableFont(13)
    readonly property int _stackSize: Config.Theme.tableFont(12)

    implicitWidth: 112
    implicitHeight: 70

    // Knocked out (no money left) - a default of its own as opposed to the self box.
    readonly property bool isActive: seatData ? seatData.active : false
    // Hide the avatars of ignored players (bind against the base property),
    // unless DontHideAvatarsOfIgnored switches that off.
    hideIgnoredAvatar:
        playerIgnored
        && ((typeof SettingsManager !== "undefined" && SettingsManager && SettingsManager.configRevision >= 0)
                ? SettingsManager.readConfigInt("DontHideAvatarsOfIgnored") === 0 : true)

    // ── The action instead of the stack, for two seconds ─────────────────────
    // The action itself stays in the seat data until the end of the hand; what we
    // want is only the short flash of the word, afterwards the stack is back.
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

    // ── Context actions ──────────────────────────────────────────────────────
    // A right click (desktop) or a long press (touch) on an opponent box opens
    // a context menu with "ignore player", "unignore player", "show player
    // stats" and the player note – as in the Qt widgets client (MyAvatarLabel)
    // or the lobby player list (PlayerListItem). The actions only apply in a
    // network game: only there does seatData carry a playerId (for local
    // games/CPU opponents 0 → no menu).
    readonly property bool targetIsComputer:
        seatData && seatData.isComputer !== undefined ? seatData.isComputer : false
    readonly property int targetPlayerId:
        seatData && seatData.playerId !== undefined ? seatData.playerId : 0
    readonly property bool targetIsGuest:
        seatData && seatData.isGuest !== undefined ? seatData.isGuest : false
    readonly property bool targetIsSelf:
        targetPlayerId !== 0 && typeof Lobby !== "undefined" && Lobby && targetPlayerId === Lobby.myPlayerId
    readonly property bool playerIgnored: {
        var _rev = (typeof Lobby !== "undefined" && Lobby) ? Lobby.playerIgnoreListRevision : 0
        return (typeof Lobby !== "undefined" && Lobby && targetPlayerId !== 0)
            ? Lobby.isPlayerIgnored(targetPlayerId) : false
    }
    readonly property bool canIgnore: !targetIsGuest && !targetIsSelf && !playerIgnored
    readonly property bool canUnignore: !targetIsGuest && !targetIsSelf && playerIgnored
    readonly property bool canShowStats: !targetIsGuest
    // Report an avatar: only in an internet game and only if the player has set an
    // (existing) avatar – 1:1 as in the Qt widgets client
    // (MyAvatarLabel). seatData.avatar is only set when a file is present.
    // Guests are not allowed to report (the server rejects it as well).
    readonly property bool canReportAvatar:
        !targetIsSelf
        && !(typeof Lobby !== "undefined" && Lobby && Lobby.isMyPlayerGuest)
        && (typeof GameTable !== "undefined" && GameTable && GameTable.isInternetGameRunning())
        && !!(seatData && seatData.avatar && seatData.avatar !== "")
    readonly property bool hasContextActions:
        targetPlayerId !== 0 && !targetIsComputer
        && (canIgnore || canUnignore || canShowStats || canReportAvatar || canEditNote)

    // ── Player note and rating ───────────────────────────────────────────────
    // Your own, purely local note (stars + text) about a fellow player, stored
    // in the same config entry as in the Qt widgets client (see
    // SettingsManager::setPlayerNote). As there, only in an internet game: only there
    // is there a permanently registered account behind the name that a
    // name based note can stick to at all.
    readonly property bool canEditNote:
        !targetIsGuest && !targetIsSelf && !targetIsComputer
        && (typeof GameTable !== "undefined" && GameTable && GameTable.isInternetGameRunning())
    readonly property int playerRating: {
        var _rev = (typeof SettingsManager !== "undefined" && SettingsManager)
                   ? SettingsManager.playerNotesRevision : 0
        return (canEditNote && typeof SettingsManager !== "undefined" && SettingsManager)
            ? SettingsManager.playerRating(root.targetPlayerName) : 0
    }
    readonly property string playerNote: {
        var _rev = (typeof SettingsManager !== "undefined" && SettingsManager)
                   ? SettingsManager.playerNotesRevision : 0
        return (canEditNote && typeof SettingsManager !== "undefined" && SettingsManager)
            ? SettingsManager.playerNote(root.targetPlayerName) : ""
    }

    // Only show it when the seat is occupied
    visible: root.seatData !== null && root.seatData.name !== ""

    // Information density: whoever is out (no money left → !isActive) is clearly
    // darkened, whoever has only folded is pulled back subtly.
    opacity: !root.isActive ? Config.Theme.dimmedOpacity
           : (root.folded ? 0.72 : 1.0)
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

    // ── The body: cards behind, plate in front, avatar on top ────────────────
    Item {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.bodyH + betStrip.height

        // 1. The hole cards – they stand behind the plate and only their upper
        //    part is visible. As soon as the player folds, they are gone.
        HoleCards {
            id: cards
            // 12 px left of the plate's centre: the room above the avatar belongs to
            // the dealer badge (see platePuck below), otherwise it would sit on the
            // right card's corner. The cards stay centred on the plate's visible part.
            x: Math.round((body.width - width) / 2) - 12
            y: 0
            z: 1
            cardH: root._cardH
            card0: root.card0
            card1: root.card1
            fade0: root.fade0
            fade1: root.fade1
            cardRenderScale: root.cardRenderScale
            visible: root.isActive && !root.folded
        }

        // 2. The plate: name + stack (or action), the avatar hangs on its right end.
        Rectangle {
            id: plate
            x: 0
            y: root._plateY
            width: body.width
            height: root._plateH
            radius: height / 2
            color: "transparent"
            z: 10

            // A raised card with a soft shadow, now with the radius of the pill.
            PlayerBoxBackground { radius: plate.height / 2 }

            // Highlight: the active player gets a gold frame + a soft glow.
            PlayerTurnGlow { active: root.isAtTurn; radius: plate.height / 2 }

            AppText {
                x: root._textX
                y: 6
                width: root._textW
                height: 16
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                color: "#eff1f5"
                font.pixelSize: root._nameSize
                font.weight: Font.DemiBold
                font.letterSpacing: 0
                // The plate is 112 px wide with a 46 px avatar, so 58 px are left
                // for the name – at the table type scale (15 px in the usual
                // 1024ish window) "Player 9" is right at that edge. Fit a name
                // that still overflows instead of cutting it off.
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Config.Theme.tableFont(10)
                elide: Text.ElideRight
                text: root.seatData && root.seatData.name !== "" ? root.seatData.name : "---"
            }

            // The second line: the stack – or, for two seconds, what the player did.
            AppText {
                x: root._textX
                y: 22
                width: root._textW
                height: 15
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                text: root._showAction
                      ? root.actionText
                      : (root.seatData && root.seatData.name !== ""
                            ? (GameTable ? GameTable.amount(root.seatData.stack) : "") : "")
                color: root._showAction
                       ? Config.Theme.actionEdgeColor(root.action)
                       : Config.Theme.colorAccent
                font.pixelSize: root._stackSize
                font.bold: true
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            // Action timeout: a slim progress bar inside the plate, at its lower
            // edge – it belongs to the seat, not to the cards.
            PlayerTimeoutBar {
                id: timeoutBar
                readonly property bool atTurn: (typeof GameTable !== "undefined" && GameTable)
                                               && GameTable.timeoutSeatId === root.seatIndex
                active: atTurn
                visible: atTurn && !root.isWinner && !root._showAction
                width: root._textW
                height: 4
                x: root._textX
                y: plate.height - 9
                z: 14
            }
        }

        // 3. The round avatar on the right end of the plate.
        PlayerAvatar {
            id: avatar
            x: body.width - root._avatarD
            y: root._plateY + Math.round((root._plateH - root._avatarD) / 2)
            width: root._avatarD
            height: root._avatarD
            z: 12
            source: root.avatarSource
            playerActive: root.isActive
        }

        // A crisp 1 px ring on the avatar's edge: the profile stays a clean round
        // anchor on the felt (the avatar's own rim is wider and softer).
        Rectangle {
            x: avatar.x
            y: avatar.y
            width: avatar.width
            height: avatar.height
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.30)
            z: 13
        }

        // Winner highlight: a golden frame around the plate + a "WINNER" badge.
        // The frame encloses the plate, not the cards behind it.
        Item {
            x: 0
            y: root._plateY
            width: body.width
            height: root._plateH
            z: 16
            PlayerWinnerOverlay {
                active: root.isWinner
                below: root.winnerBelow
                radius: plate.height / 2
            }
        }

        // The bet base at the lower box edge (seat style "inset"). 1 px inside
        // the frame of PlayerBoxBackground, so that its border stays visible.
        PlayerBetStrip {
            id: betStrip
            open: root.stripOpen
            amount: root.bet
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    // ── The dealer button: a small badge like a crown on the avatar ──────────
    // It sits on the avatar's upper edge, centred on the avatar. The hole cards
    // are drawn 12 px left of the plate's centre (see the cards above), so the
    // crown has clear felt above the avatar and cannot cover a card. Beats every
    // other arrangement by construction: next to the plate the puck shares the gap
    // with the neighbouring seat's bet chip, and lower on the avatar it was barely
    // visible (player report 2026-11-10).
    BlindButtonImage {
        id: platePuck
        visible: root.buttonVisible
        button: root.button
        width: Math.round(root._plateH * 0.5)
        height: width
        z: 14
        x: Math.round(body.width - root._avatarD / 2 - width / 2)
        y: Math.round(root._plateY + (root._plateH - root._avatarD) / 2
                      - height + Math.round(height * 0.35))
    }

    // The bet (chip pile + amount), grouped.
    // Top/bottom centre (betSide top/bottom): the full box width.
    // Sides (betSide left/right): vertically centred next to the box.
    Item {
        id: betGroup
        visible: betGroup.chipHere
        z: 25

        readonly property bool split: root.betSplit
        readonly property bool horizontal: !split && (root.betSide === "bottom" || root.betSide === "top")
        // In the style "inset" the bet sits in the box base and, when the bet lies
        // on the felt (betOnFelt), the chip is drawn there as well – the group then
        // carries nothing.
        readonly property bool chipHere: root.bet > 0 && !root.betInset && !root.betOnFeltShown
        readonly property real betW: chipHere ? betRow.width : 0
        readonly property real betH: chipHere ? betRow.height : 0

        width: (horizontal || split) ? body.width : betW
        height: horizontal ? betH : body.height

        x: split ? 0
         : root.betSide === "right" ? body.width + 8
         : root.betSide === "left"  ? -width - 8
         : 0
        y: split ? 0
         : root.betSide === "bottom" ? body.height + 7
         : root.betSide === "top"    ? -height - 7
         : (body.height - height) / 2

        BetRows {
            id: betRow
            visible: betGroup.chipHere
            amount: root.bet
            chipSize: 16
            fontSize: 12
            textColor: "#f0f0f0"
            cash: root.chipCash
            renderScale: root.cardRenderScale
            // Up to four chips per stack: a 400 bet is four 100-chips and all of
            // them must show (player's bug of 2026-11-12 in a plate's bet).
            maxColumns: 2
            maxPerColumn: 4
            // split: the bet to the right NEXT TO the box; otherwise centred inside.
            x: betGroup.split ? betGroup.width + 8 : (betGroup.width - width) / 2
            y: (betGroup.height - height) / 2
        }
    }

    // ── The bet ON THE FELT ──────────────────────────────────────────────────
    // The bet is part of the table, not of the player's plate: it lies on the
    // felt between the box and the pot as a pyramid of lying chips with the sum
    // in a dark pill under it (2026-11-12). The footprint and the anchor come
    // from GamePage (betFeltPoint): chipHalfX/chipHalfY are the half sizes of
    // that block, so the point returned there is its CENTRE.
    BetChip {
        id: feltBet
        visible: root.betOnFeltShown
        amount: root.bet
        chipSize: 22
        fontSize: 10
        textColor: "#f0f0f0"
        cash: root.chipCash
        renderScale: root.cardRenderScale
        z: 24
        x: root.betAt.x - width / 2
        y: root.betAt.y - height / 2
    }

    // ── Right click context menu (desktop only) ──────────────────────────────
    // It only intercepts the right mouse button; left clicks/hover fall through to the
    // elements below. It only appears when the seat carries a
    // real online fellow player (hasContextActions).
    MouseArea {
        // Only above the box body – the base height reserved below it is
        // empty table, no context menu may open there.
        id: contextArea
        anchors.fill: body
        z: 30
        enabled: root.hasContextActions
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => contextMenu.popup(mouse.x, mouse.y)

        // Touch has no right mouse button: there a long press opens
        // the same menu. Deliberately limited to touch devices, so that a
        // held left click with the mouse still triggers nothing.
        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen
            enabled: root.hasContextActions
            onLongPressed: contextMenu.popup(point.position.x, point.position.y)
        }
    }

    // A uniformly styled menu entry (a dark theme, it collapses when invisible).
    component CtxItem: MenuItem {
        height: visible ? implicitHeight : 0
        contentItem: AppText {
            text: parent.text
            color: parent.enabled
                   ? (parent.highlighted ? Config.Theme.colorAccent : Config.Theme.colorTextPrimary)
                   : Config.Theme.colorTextMuted
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }
        background: Rectangle {
            color: parent.highlighted ? Config.StaticData.palette.secondary.col600 : "transparent"
        }
    }

    Menu {
        id: contextMenu

        // Adjust the width to the widest visible entry (at least 180).
        implicitWidth: {
            var w = 180
            for (var i = 0; i < count; ++i) {
                var it = itemAt(i)
                if (it && it.visible)
                    w = Math.max(w, it.implicitWidth)
            }
            return w
        }

        // A dark theme matching the table surface.
        background: Rectangle {
            implicitWidth: 180
            color: Config.Theme.colorBox
            border.width: 1
            border.color: Config.StaticData.palette.secondary.col500
            radius: Config.Theme.radiusSmall
        }

        CtxItem {
            text: qsTr("Ignore player")
            visible: root.canIgnore
            onTriggered: root.confirmIgnore()
        }
        CtxItem {
            text: qsTr("Unignore player")
            visible: root.canUnignore
            onTriggered: root.confirmUnignore()
        }
        CtxItem {
            text: qsTr("Show player stats")
            visible: root.canShowStats
            onTriggered: { if (typeof Lobby !== "undefined" && Lobby) Lobby.showPlayerStats(root.targetPlayerId) }
        }
        CtxItem {
            text: qsTr("Report inappropriate avatar")
            visible: root.canReportAvatar
            onTriggered: root.confirmReportAvatar()
        }
        CtxItem {
            text: qsTr("Note about player ...")
            visible: root.canEditNote
            onTriggered: notePopup.openFor(root.targetPlayerName)
        }
    }

    PlayerNoteDialog { id: notePopup }

    readonly property string targetPlayerName: root.seatData ? (root.seatData.name || "") : ""

    // Confirmation before ignoring a player (an accidental click).
    function confirmIgnore() {
        ignorePopup.openWith(
            qsTr("Ignore player"),
            qsTr("Are you sure you want to ignore \"%1\"?").arg(root.targetPlayerName),
            qsTr("Ignore player"))
    }

    // Confirmation before unignoring a player.
    function confirmUnignore() {
        unignorePopup.openWith(
            qsTr("Unignore player"),
            qsTr("Are you sure you want to unignore \"%1\"?").arg(root.targetPlayerName),
            qsTr("Unignore player"))
    }

    // Confirmation before reporting an inappropriate avatar (a port of the
    // confirmation from MyAvatarLabel::reportBadAvatar).
    function confirmReportAvatar() {
        reportAvatarPopup.openWith(
            qsTr("Report inappropriate avatar"),
            qsTr("Are you sure you want to report the avatar of \"%1\" as inappropriate?").arg(root.targetPlayerName),
            qsTr("Report"))
    }

    ConfirmPopup {
        id: ignorePopup
        onConfirmed: { if (typeof Lobby !== "undefined" && Lobby) Lobby.ignorePlayer(root.targetPlayerId) }
    }

    ConfirmPopup {
        id: unignorePopup
        onConfirmed: { if (typeof Lobby !== "undefined" && Lobby) Lobby.unignorePlayer(root.targetPlayerId) }
    }

    ConfirmPopup {
        id: reportAvatarPopup
        onConfirmed: { if (typeof GameTable !== "undefined" && GameTable) GameTable.reportAvatar(root.seatIndex) }
    }
}
