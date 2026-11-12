import QtQuick

import "../config" as Config

// A bet (and the pot) on the felt: stacks of lying chips plus a dark pill with
// the sum underneath. Two arrangements:
//   • `pyramid` (a player's bet): two stacks at the base, one above between them,
//     drawn from the back; the block is 47 × 52.13 base px.
//   • flat (the pot): all stacks side by side on one baseline (up to 72 × 34.7) —
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
// the pill has the truth.
//
// The stacks are painted back to front. A stack of two or more chips always takes
// a BASE slot: in the top slot it would lift the pyramid by one band per chip and
// the block would no longer fit between the plates and the board (see _slotDY).
// Single chips share the free slots at random, so the top chip is a different
// denomination every time (player's rule of 2026-11-12, kept where it costs
// nothing). Lower chips of a stack are turned a little (±8°), each differently,
// so the rim stripes never line up and the stack does not read as one thick chip.
// The top chip stays upright for its value.
//
// The randomness is rolled when the amount changes and stored: a repaint, a
// resize or a QML re-evaluation keeps the same picture.
Item {
    id: bet

    property int amount: 0
    property color textColor: "#eff1f5"
    // Width (diameter) of one chip in base pixels.
    property int chipSize: 22
    // Cash table: the ladder gains the 1¢ rung (money, cents) and the chips
    // print money — see ChipImage and `_ladder`. Tournaments count chips.
    property bool cash: false
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
    // Print the sum in the pill under the chips. The pot's sum moved to the top of
    // the felt on 2026-11-13 (`PotAmount`, between the two topmost seats) — the
    // chips there stay without a pill, so that the strip above the board is free
    // for the pot the player wants to build there. A player's bet always keeps it.
    property bool showAmount: true

    // The ladder (top rung first, the last one is the smallest chip):
    //   • tournament — chip counts 5 … 50000, eight rungs (the smallest is 5, so
    //     an all-in below it is drawn as the neutral spare chip);
    //   • cash — money in CENTS with the 1¢ rung added (2026-11-12): every amount
    //     is then a whole number of pennies, so a cash bet has no spare chip.
    readonly property var _ladder: cash
        ? [50000, 10000, 2500, 500, 100, 25, 10, 5, 1]
        : [50000, 10000, 2500, 500, 100, 25, 10, 5]
    readonly property int _minChipValue: _ladder[_ladder.length - 1]

    // ── geometry constants (see ChipImage for the asset proportions) ────────
    readonly property real _d: chipSize
    // Gap between the stacks (3 px since 2026-11-12, was 1): a rotated lower chip
    // is wider than its upright neighbour, and with a 1 px gap the two base stacks
    // of the pyramid touched/overlapped (player: "они реально накладываются друг
    // на друга"). 3 px guarantees a visible seam between the piles.
    readonly property real _gap: 3
    readonly property real _band: 0.0988 * _d
    readonly property real _faceH: 0.7906 * _d
    // Visual bottom of a chip inside its square item.
    readonly property real _chipBottomInItem: _d / 2 + (_faceH + _band) / 2
    // Base slots sit at ±_slotDX, the top slot _slotDY above them.
    readonly property real _slotDX: (_d + _gap) / 2
    // The top chip stands ONE FULL CHIP HEIGHT higher (face + band = 19.57 at
    // 22 px), not "sitting on" the base chips (it was faceH − 5 = 12.4 until
    // 2026-11-12). With the old value its lower edge hid under the base chips and
    // the three read as merged: the player asked for the same small seam between
    // ALL pairs ("синяя и чёрная чуть слиты вместе"). At one chip height the
    // seam to a one-chip base stack is 2.9 px against the 3 px between the base
    // stacks — the same small gap. A taller base stack rises towards the top chip
    // and narrows that seam, which is what a stack in front of the back chip does.
    // The price is height: the block is 52.13 instead of 51.5, and the reserve in
    // GamePage/geometry.js follows it (52.13 is the last height with a clean
    // 216-layout sweep; 52.3 already gives two findings).
    readonly property real _slotDY: _faceH + _band
    readonly property int _pillH: Math.max(13, Math.round(fontSize) + 3)
    readonly property int _pillGap: 0
    readonly property int _pillPad: 7

    // Stacks actually drawn (0 while there is no amount).
    readonly property int _pileCount: _split.length
    width: pyramid ? 2 * _d + _gap
                   : Math.max(1, _pileCount) * _d + Math.max(0, _pileCount - 1) * _gap
    // Height. In the pyramid the reserve is CONSTANT: the top chip's top edge
    // (a whole chip height above the base line) plus the pill. Base stacks never
    // stick out above it — their chips stay below the top chip's top edge, — and
    // a stack in the top slot is capped at one chip (see _roll), so nothing above
    // the top chip is ever drawn. Constant height also keeps the bet's bottom line
    // (the pill) at one distance from the plate for every amount.
    // In a flat row (the pot) the height follows the highest stack.
    readonly property int _maxCount: {
        var m = 1
        for (var i = 0; i < _split.length; ++i)
            m = Math.max(m, _split[i].count)
        return m
    }
    height: ((pyramid && maxPiles > 2) ? _slotDY + _faceH + _band
                                       : _faceH + _maxCount * _band)
            + _pillTotal
    // Money the pill occupies at the bottom (0 when the sum is printed elsewhere).
    readonly property int _pillTotal: showAmount ? (_pillGap + _pillH) : 0
    // Baseline (visual bottom) of the stacks.
    readonly property real _baseY: height - _pillTotal
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
        // Below the smallest chip: one neutral chip as decoration, never a stack
        // of copies of it, and the pill tells the sum. Cash has the 1¢ rung, so
        // this is a tournament all-in of 1–4 chips; the rest of a cash remainder
        // (a raise that is not a whole number of rungs) is simply not drawn —
        // floor, the pill is the truth (player's rule of 2026-11-12).
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

    function _shuffle(a) {
        for (var i = a.length - 1; i > 0; --i) {
            var j = Math.floor(Math.random() * (i + 1))
            var t = a[i]; a[i] = a[j]; a[j] = t
        }
    }

    // Slot capacities: both base slots hold a stack (up to maxPerStack chips), the
    // top slot only ONE chip. The top slot stands a whole chip height above the
    // base line (see _slotDY), so a second chip there would lift the block by a
    // band and it would no longer fit the strip between the plates and the board;
    // a tall stack also reads better in front (the reference client does the same).
    readonly property int _topCap: 1

    function _roll() {
        var piles = splitOf(bet.amount)
        var entries = []
        if (!bet.pyramid) {
            // Flat row (the pot): the stacks stand next to each other, so their
            // left-to-right order is chosen randomly instead of the slot.
            _shuffle(piles)
            for (var q = 0; q < piles.length; ++q)
                entries.push({ "pile": piles[q], "slot": q })
        } else {
            // Pyramid: stacks taller than the top slot's capacity go to the base
            // slots first, single chips share whatever is left at random — so the
            // top chip is a different denomination every time.
            var multi = [], single = []
            for (var p = 0; p < piles.length; ++p)
                (piles[p].count > bet._topCap ? multi : single).push(piles[p])
            _shuffle(multi)
            _shuffle(single)
            // The BIG stacks take the base slots: with two base slots free the two
            // tallest piles go there, and if a third tall stack exists (three rungs
            // with 2+ chips — a huge bet) the smallest one is the one the top slot
            // cuts down to a single chip, which loses the fewest chips.
            for (var t = 0; t < multi.length; ++t) multi[t]._r = Math.random()
            multi.sort(function (a, b) {
                return (b.count - a.count) || (a._r - b._r)
            })
            var free = [0, 1, 2]
            _shuffle(free)
            for (var m = 0; m < multi.length; ++m) {
                var bi = -1
                for (var f = 0; f < free.length; ++f)
                    if (free[f] !== 2) { bi = f; break }
                if (bi >= 0) {
                    entries.push({ "pile": multi[m], "slot": free[bi] })
                    free.splice(bi, 1)
                } else {
                    // Both base slots are taken (only huge bets with three tall
                    // stacks): the top slot shows one chip, the pill has the sum.
                    entries.push({ "pile": { "value": multi[m].value, "count": 1 },
                                   "slot": 2 })
                    free.splice(free.indexOf(2), 1)
                }
            }
            for (var s = 0; s < single.length && free.length > 0; ++s)
                entries.push({ "pile": single[s], "slot": free.pop() })
            // Draw the pyramid from the BACK: the stack on the top slot is painted
            // FIRST, the two base stacks cover its lower edge. The other way round
            // the top chip lay on the faces of the base chips (player's report
            // 2026-11-12: "две нижние не перекрывают верхнюю").
            entries.sort(function (a, b) {
                return (a.slot === 2 ? 0 : 1) - (b.slot === 2 ? 0 : 1)
            })
        }
        var out = []
        for (var e = 0; e < entries.length; ++e) {
            var pile = entries[e].pile
            var count = pile.count
            for (var k = 0; k < count; ++k) {
                out.push({
                    "value": pile.value,
                    "slot": entries[e].slot,
                    "level": k,
                    // ±8° (was ±15°): the rim stripes still never line up, but a
                    // rotated lower chip no longer sticks out into the neighbour
                    // stack (with a 15° turn its band reached 2.2 px sideways and
                    // the piles touched).
                    "rot": (k === count - 1) ? 0 : (Math.random() * 16 - 8),
                    // No horizontal jitter — it closed the seam between the base
                    // stacks of the pyramid.
                    "jx": 0,
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
            cash: bet.cash
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
        visible: bet.amount > 0 && bet.showAmount
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
