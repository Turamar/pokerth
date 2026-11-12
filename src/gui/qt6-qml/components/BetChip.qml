import QtQuick

import "../config" as Config

// A bet (and the pot) on the felt: up to three STACKS of lying chips arranged as
// a PYRAMID — two stacks at the base, one above between them — plus a dark pill
// with the sum underneath.
//
// Chips are the player's set in the lying tier (`ChipImage` uses obl/obln): the
// face is squashed to 0.80 and every chip has its own 6 % side band, so a stack
// is a row of these assets offset by exactly `bandStep` and every lower chip
// shows only its edge. Faces never overlap.
//
// The split is greedy over the ladder (one stack per denomination, at most
// `maxPerStack` chips each, at most `maxPiles` stacks — the rest is not drawn,
// the pill has the truth). Each stack takes a RANDOM free slot, including the
// top one: a single stack can end up anywhere in the pyramid (player's rule,
// 2026-11-12). Lower chips of a stack are rotated a little, each differently, so
// the rim stripes never line up; the top chip stays upright for its value.
//
// The randomness is rolled when the amount changes and stored: a repaint, a
// resize or a QML re-evaluation keeps the same picture.
Item {
    id: bet

    property int amount: 0
    property color textColor: "#eff1f5"
    // Width (diameter) of one chip in base pixels.
    property int chipSize: 22
    // Print the denomination (tournament only — see ChipImage).
    property bool digits: false
    // Effective screen scale of the parents, passed to ChipImage.
    property real renderScale: 1
    property int fontSize: Config.Theme.tableFont(11)
    // Test hook: when set, printed instead of GameTable.amount() — the asset/dev
    // test page has no running game to format the sum.
    property string amountText: ""
    // Stacks shown: 3 for a player's bet, 2 for the pot (narrow strip).
    property int maxPiles: 3
    // Chips per stack: more would only grow the pile, the pill has the sum.
    property int maxPerStack: 3

    readonly property var _ladder: [50000, 10000, 2500, 500, 100, 25, 10, 5]
    readonly property int _minChipValue: 5

    // ── geometry constants (see ChipImage for the asset proportions) ────────
    readonly property real _d: chipSize
    readonly property real _gap: 1
    readonly property real _band: 0.0988 * _d
    readonly property real _faceH: 0.7906 * _d
    // Visual bottom of a chip inside its square item.
    readonly property real _chipBottomInItem: _d / 2 + (_faceH + _band) / 2
    // Base slots sit at ±_slotDX, the top slot _slotDY above them (tangent
    // circles, no overlap).
    readonly property real _slotDX: (_d + _gap) / 2
    // The top pile sits ON the two base piles: its bottom edge is aceH - 2
    // above the base line. A circle-triangle (√3/2·(d+1) = 19.9) would be taller
    // and the block then does not fit between the plates; the two pixels of
    // overlap only touch the base chips' top rim, never a face.
    readonly property real _slotDY: _faceH - 5
    readonly property int _pillH: Math.max(13, Math.round(fontSize) + 3)
    readonly property int _pillGap: 0
    readonly property int _pillPad: 7

    width: 2 * _d + _gap
    height: (maxPiles > 2 ? _slotDY : 0) + _faceH + maxPerStack * _band
            + _pillGap + _pillH
    // Baseline (visual bottom) of the base stacks.
    readonly property real _baseY: height - _pillH - _pillGap

    // ── the split ───────────────────────────────────────────────────────────
    // A PURE function of the amount. `_roll()` must call it with the current
    // amount instead of reading the `_split` binding: when `amount` changes, the
    // handler runs while that binding is still stale, so the pile lagged one
    // change behind (the player's bug of 2026-11-12: "не все фишки отображаются",
    // and the pot showed the chips of the previous round).
    function splitOf(a) {
        a = Math.max(0, Math.round(a))
        if (a <= 0) return []
        // Below the smallest chip (e.g. a 4c bet): one neutral chip as
        // decoration, never a stack of copies of it, and the pill tells the sum.
        if (a < bet._minChipValue) return [{ "value": 0, "count": 1 }]
        var rest = a, out = []
        for (var i = 0; i < bet._ladder.length && rest > 0; ++i) {
            var n = Math.floor(rest / bet._ladder[i])
            if (n <= 0) continue
            if (out.length < bet.maxPiles)
                out.push({ "value": bet._ladder[i],
                           "count": Math.min(n, bet.maxPerStack) })
            rest -= n * bet._ladder[i]
        }
        return out
    }
    readonly property var _split: splitOf(amount)

    // ── randomness: rolled when the bet is placed, then stored ─────────────
    property var _chips: []
    function _roll() {
        var piles = splitOf(bet.amount)
        var slots = [0, 1, 2]
        for (var i = slots.length - 1; i > 0; --i) {
            var j = Math.floor(Math.random() * (i + 1))
            var t = slots[i]; slots[i] = slots[j]; slots[j] = t
        }
        var out = []
        for (var p = 0; p < piles.length; ++p) {
            var count = piles[p].count
            for (var k = 0; k < count; ++k) {
                out.push({
                    "value": piles[p].value,
                    "slot": slots[p % slots.length],
                    "level": k,
                    "rot": (k === count - 1) ? 0 : (Math.random() * 30 - 15),
                    "jx": Math.random() * 1.2 - 0.6,
                    "jy": Math.random() * 1.2 - 0.6
                })
            }
        }
        bet._chips = out
    }

    // ── the stacks ──────────────────────────────────────────────────────────
    Repeater {
        model: bet._chips

        ChipImage {
            required property var modelData
            chipSize: bet._d
            value: modelData.value
            digits: bet.digits
            renderScale: bet.renderScale
            rotation: modelData.rot
            x: bet.width / 2 + slotX(modelData.slot) - width / 2 + modelData.jx
            y: (modelData.slot === 2 ? bet._baseY - bet._slotDY : bet._baseY)
               - bet._chipBottomInItem - modelData.level * bet._band
               + modelData.jy

            function slotX(slot) {
                return slot === 0 ? -bet._slotDX
                     : slot === 1 ? bet._slotDX
                     : 0
            }
        }
    }

    // ── the sum (the truth, the pile is decoration) ─────────────────────────
    Rectangle {
        id: pill
        visible: bet.amount > 0
        anchors.horizontalCenter: parent.horizontalCenter
        y: bet.height - bet._pillH
        width: pillLabel.implicitWidth + 2 * bet._pillPad
        height: bet._pillH
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.72)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.10)

        AppText {
            id: pillLabel
            anchors.centerIn: parent
            text: bet.amountText !== "" ? bet.amountText
                  : ((typeof GameTable !== "undefined" && GameTable)
                     ? GameTable.amount(bet.amount) : "")
            color: bet.textColor
            font.pixelSize: bet.fontSize
            font.bold: true
        }
    }

    // The randomness is rolled when the amount changes and stored: a repaint, a
    // resize or a QML re-evaluation keeps the same picture.
    // (One handler only: QML forbids two `onAmountChanged` in the same object.)
    onAmountChanged: {
        _roll()
        if (visible) betPop.restart()
    }
    Component.onCompleted: _roll()

    // A short pop when the bet appears or grows — it is placed, not switched on.
    transformOrigin: Item.Center
    onVisibleChanged: if (visible) betPop.restart()
    SequentialAnimation {
        id: betPop
        NumberAnimation { target: bet; property: "scale"; from: 0.75; to: 1.08; duration: 110; easing.type: Easing.OutQuad }
        NumberAnimation { target: bet; property: "scale"; to: 1.0; duration: 140; easing.type: Easing.OutBack }
    }
}
