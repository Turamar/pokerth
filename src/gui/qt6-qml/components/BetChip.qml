import QtQuick

import "../config" as Config

// A bet (and the pot) on the felt: stacks of lying chips plus a dark pill with
// the sum underneath. Two arrangements:
//   • `pyramid` (a player's bet): two stacks at the base, one above between them;
//     the returned block is 45 × at most 51.5 base px.
//   • flat (the pot): all stacks side by side on one baseline (up to 68 × 34.7) —
//     the strip above the board cannot take the height of a pyramid with a pill,
//     and the top-seat bet needs the room between its plate and the pot.
//
// Chips are the player's set in the lying tier (`ChipImage` uses obl/obln): the
// face is squashed to 0.80 and every chip has its own 10 % side band, so a stack
// is a row of these assets offset by exactly `bandStep` and every lower chip
// shows only its edge. Faces never overlap.
//
// The split is greedy over the ladder, but a denomination is NOT cut off at the
// stack limit: its chips are spread EVENLY over the free stacks (2026-11-12, the
// player's "стопка из двух работает, а из 4 нет" — a 400 bet is four 100-chips
// and all four must show; the old `min(n, maxPerStack)` silently dropped two).
// Only when every stack is full (maxPiles × maxPerStack) is the rest not drawn —
// the pill has the truth. The height follows the highest stack, so a small pile
// does not reserve room for a big one.
//
// Each stack takes a RANDOM free slot, including the top one: a single stack can
// end up anywhere in the pyramid (player's rule, 2026-11-12). Lower chips of a
// stack are rotated a little, each differently, so the rim stripes never line up;
// the top chip stays upright for its value.
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
    // Stacks shown: 3 for a player's bet, 3 in a row for the pot.
    property int maxPiles: 3
    // Chips per stack. The greedy ladder over these denominations can repeat one
    // rung at most four times (e.g. 400 = 4x100; the next rung is always 5x or
    // less), so four is the real maximum and more would only be a safety margin.
    property int maxPerStack: 4
    // Pyramid (a bet) vs a flat row of stacks (the pot) — see the header.
    property bool pyramid: true

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
    // The top pile sits ON the two base piles: its bottom edge is faceH − 5
    // above the base line. A circle-triangle (√3/2·(d+1) = 19.9) would be taller
    // and the block then does not fit between the plates; the five pixels of
    // overlap only touch the base chips' top rim, never a face.
    readonly property real _slotDY: _faceH - 5
    readonly property int _pillH: Math.max(13, Math.round(fontSize) + 3)
    readonly property int _pillGap: 0
    readonly property int _pillPad: 7

    // Stacks actually drawn (0 while there is no amount).
    readonly property int _pileCount: _split.length
    width: pyramid ? 2 * _d + _gap
                   : Math.max(1, _pileCount) * _d + Math.max(0, _pileCount - 1) * _gap
    // Height follows the HIGHEST stack of the current amount (the pill sits at the
    // bottom, the chips grow upwards), so a one-chip bet is not as tall as a
    // four-chip one. The clearances in GamePage/geometry.js reserve the maximum.
    readonly property int _maxCount: {
        var m = 1
        for (var i = 0; i < _split.length; ++i)
            m = Math.max(m, _split[i].count)
        return m
    }
    height: (pyramid && maxPiles > 2 ? _slotDY : 0) + _faceH + _maxCount * _band
            + _pillGap + _pillH
    // Baseline (visual bottom) of the stacks.
    readonly property real _baseY: height - _pillH - _pillGap
    // In a flat row the stacks share one baseline; their centres are one chip plus
    // the gap apart. In the pyramid slot 2 is the one on TOP of the two base
    // stacks (their centres are _slotDX = half that step to each side).
    function slotCentre(slot) {
        if (!pyramid)
            return (slot - (_pileCount - 1) / 2) * (_d + _gap)
        return slot === 0 ? -_slotDX
             : slot === 1 ? _slotDX
             : 0
    }

    // ── the split ───────────────────────────────────────────────────────────
    // A PURE function of the amount. `_roll()` must call it with the current
    // amount instead of reading the `_split` binding: when `amount` changes, the
    // handler runs while that binding is still stale, so the pile lagged one
    // change behind (the player's bug of 2026-11-12: "не все фишки отображаются",
    // and the pot showed the chips of the previous round).
    //
    // Chips of one denomination are spread EVENLY over the free stacks instead of
    // being cut off at `maxPerStack` (player's bug of 2026-11-12: a 400 bet is
    // four 100-chips and only two/three were drawn). When a denomination needs
    // more stacks than are left, one chip is dropped only if the pyramid is
    // genuinely full — the pill still has the sum.
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
            var free = bet.maxPiles - out.length
            if (free > 0) {
                var shown = Math.min(n, free * bet.maxPerStack)
                var k = Math.max(1, Math.min(free,
                                             Math.ceil(shown / bet.maxPerStack)))
                var base = Math.floor(shown / k), extra = shown % k
                for (var j = 0; j < k; ++j)
                    out.push({ "value": bet._ladder[i],
                               "count": base + (j < extra ? 1 : 0) })
            }
            rest -= n * bet._ladder[i]
        }
        return out
    }
    readonly property var _split: splitOf(amount)

    // ── randomness: rolled when the bet is placed, then stored ─────────────
    property var _chips: []
    function _roll() {
        var piles = splitOf(bet.amount)
        if (!bet.pyramid) {
            // Flat row: the stacks stand next to each other without gaps, so their
            // left-to-right order is chosen randomly instead of the slot.
            for (var q = piles.length - 1; q > 0; --q) {
                var r = Math.floor(Math.random() * (q + 1))
                var pt = piles[q]; piles[q] = piles[r]; piles[r] = pt
            }
        }
        // One free slot per stack (0/1 base, 2 top — see the header).
        var slots = []
        for (var s = 0; s < Math.max(1, Math.min(3, bet.maxPiles)); ++s) slots.push(s)
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
                    "slot": bet.pyramid ? slots[p % slots.length] : p,
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
            x: bet.width / 2 + bet.slotCentre(modelData.slot) - width / 2
               + modelData.jx
            y: (bet.pyramid && modelData.slot === 2 ? bet._baseY - bet._slotDY
                                                    : bet._baseY)
               - bet._chipBottomInItem - modelData.level * bet._band
               + modelData.jy
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
