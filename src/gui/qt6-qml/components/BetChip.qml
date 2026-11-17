import QtQuick

import "chipsplit.js" as ChipSplit
import "../config" as Config

// A player's bet on the felt: stacks of lying chips plus a dark pill with the sum
// underneath. Two stacks at the base (1 px apart) and one chip in the HOLLOW
// between them, drawn from the back; the block is 45 × 48.32 base px.
//
// The hollow chip is NOT on top of the base chips: all three lie on the felt, and
// the chip in the hollow is drawn one depth step higher, which for the lying
// asset is 0.6847·(d + gap) = 15.75 px (2026-11-17). With a smaller step the base
// chips cut through its face; with "one chip height" (19.57, the old value) it
// floats above the hollow. The pot uses its own packed pile (PotPile).
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
// nothing).
//
// Chips are TURNED, and only in the honest way (2026-11-17): a chip lying on the
// felt can turn about the vertical axis only, so the whole chip is an asset frame
// (face and side band turn together, the light stays put — see ChipImage.yaw).
// Rotating the picture instead tilts the projection and the chip reads as standing
// on its edge; on a real chip the face cannot turn without its rim. Only the
// COVERED chips of a stack turn (their faces are hidden — what moves is the rim
// stripes on their band, a small deviation); the top chip stays upright so its
// value is straight (player's rule 2026-11-17: random turns of the value chip are
// not wanted). `yawSpread` / `topYawSpread` are the knobs.
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
    // Yaw of the chips on the felt, degrees (2026-11-17, see the header): the
    // covered chips of a stack turn (only their band and rim stripes show); the top
    // chip stays upright — its value is read, and random turns of it were rejected
    // by the player.
    property real yawSpread: 30
    property real topYawSpread: 0

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
    // Gap between the two base stacks: 1 px (2026-11-17, was 3). The chips are
    // lying flat and 0.988·d wide, so 1 px is enough to keep them apart — the
    // player asked for them "почти в упор", and the pyramid reads tighter.
    readonly property real _gap: 1
    readonly property real _band: 0.0988 * _d
    readonly property real _faceH: 0.7906 * _d
    // Visual bottom of a chip inside its square item.
    readonly property real _chipBottomInItem: _d / 2 + (_faceH + _band) / 2
    // Base slots sit at ±_slotDX, the top slot _slotDY above them.
    readonly property real _slotDX: (_d + _gap) / 2
    // The chip in the hollow is drawn one DEPTH step higher — not on top of the
    // base chips and not "one chip height" (19.57 was the value until 2026-11-17).
    // All three chips lie on the felt, so the step is the projection of the hollow:
    // a chip nestled between two neighbours sits sqrt(3)/2·(d + gap) further away,
    // and the lying asset maps depth to the screen with sin θ = faceH/d:
    //     0.8660 · (d + gap) · (faceH / d) = 0.6847 · (d + gap)
    //   = 15.75 px at d = 22, gap = 1.
    // Smaller → the base chips cut through its face; larger → it floats. The
    // block is 48.32 instead of 52.13, so the layout gets 4 px back.
    readonly property real _slotDY: 0.8660254 * (_d + _gap) * (_faceH / _d)
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
    // A PURE function of the amount (the shared ChipSplit); `_roll()` must call it
    // with the current amount instead of reading the `_split` binding: when
    // `amount` changes, the handler runs while that binding is still stale, so the
    // pile lagged one change behind (the player's bug of 2026-11-12: "не все фишки
    // отображаются").
    //
    // Chips of one denomination are spread EVENLY over the free stacks instead of
    // being cut off at `maxPerStack` (player's bug of 2026-11-12: a 400 bet is
    // four 100-chips and only two/three were drawn). When a denomination needs
    // more stacks than are left, one chip is dropped only if the pyramid is
    // genuinely full — the pill still has the sum.
    function splitOf(a) {
        return ChipSplit.split(a, bet.cash, bet.maxPiles, bet.maxPerStack)
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
            // Pyramid: the tall stacks take the two base slots first, then single
            // chips. A chip goes into the HOLLOW (slot 2) only when BOTH base
            // stacks are exactly one chip high — that is the only support a chip
            // lying in the hollow can have (2026-11-17: on a taller base stack it
            // would be cut through its face, on purpose of the player's report).
            // Anything that does not fit is simply not drawn: the pill has the
            // truth.
            var multi = [], single = []
            for (var p = 0; p < piles.length; ++p)
                (piles[p].count > bet._topCap ? multi : single).push(piles[p])
            _shuffle(multi)
            _shuffle(single)
            // The BIG stacks take the base slots: with two base slots free the two
            // tallest piles go there; a third tall stack cannot be shown (the
            // hollow takes one chip only), so it is dropped — the sum stays.
            for (var t = 0; t < multi.length; ++t) multi[t]._r = Math.random()
            multi.sort(function (a, b) {
                return (b.count - a.count) || (a._r - b._r)
            })
            var free = [0, 1]
            _shuffle(free)
            for (var m = 0; m < multi.length && free.length > 0; ++m)
                entries.push({ "pile": multi[m], "slot": free.pop() })
            var usedSingles = 0
            while (usedSingles < single.length && free.length > 0) {
                entries.push({ "pile": single[usedSingles], "slot": free.pop() })
                ++usedSingles
            }
            var hollowOk = entries.length === 2
            for (var e = 0; e < entries.length; ++e)
                if (entries[e].pile.count !== 1)
                    hollowOk = false
            if (hollowOk && usedSingles < single.length)
                entries.push({ "pile": single[usedSingles], "slot": 2 })
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
                    // Yaw about the vertical axis (2026-11-17): a frame of the
                    // whole chip in the asset, never a rotation of the picture
                    // (see ChipImage.yaw). The top chip turns less — its value
                    // must stay readable; the covered ones turn freely.
                    "yaw": (k === count - 1)
                           ? (Math.random() * 2 - 1) * bet.topYawSpread
                           : (Math.random() * 2 - 1) * bet.yawSpread,
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
            // A turn is a frame of the whole chip, never `rotation` (see ChipImage).
            yaw: modelData.yaw
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
    // The yaw spreads are a look-and-feel knob (the asset test page tunes them).
    onYawSpreadChanged: _roll()
    onTopYawSpreadChanged: _roll()
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
