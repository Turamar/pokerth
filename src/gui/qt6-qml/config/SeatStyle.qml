pragma Singleton
import QtQuick

// Seat style of the player boxes at the table – the QML counterpart to the "seat
// packs" of the web client (html[data-seat="…"] there: classic/plate/card/pokerth).
//
// Since the settings audit of 2026-11-18 (PLAN.md §7) there is only ONE variant
// left: the bet lies on the felt beside the avatar (the "classic" layout the
// player chose). The old switchable "inset" variant (bet base inside the box)
// was removed together with its radio button – the chip collect/payout
// animations aim at the felt point, so a chip inside the box would have left
// every flight pointing at empty felt.
//
// The values below are kept as the single source for the player boxes AND the
// space calculation of the tableZone (GamePage) / seatlayout.js; the names are
// historical. With betInset == false the box needs no bet base of its own and
// reserves the room for the chip group NEXT TO the box.
QtObject {
    id: root

    // The bet sits on the felt beside the avatar (see GamePlayerBox.betOnFelt);
    // no base inside the box any more.
    readonly property bool betInset: false

    // Room that the bet + dealer/blind puck need NEXT TO the box (base pixels,
    // cf. GamePlayerBox.betGroup: 8 px spacing + group width): 8 + 60 = 68.
    readonly property int betSideOutset: 68

    // Height of the bet base inside the box – not used any more (betInset is
    // always false); kept as the value of the now-inert PlayerBetStrip.
    readonly property int betStripHeight: 20
    // Additional height a player box needs for that base (0 for the felt bet).
    readonly property int betStripExtra: 0
}
