import QtQuick

import "chipsplit.js" as ChipSplit
import "../config" as Config

// The pot as COLUMNS of lying chips in a pyramid (decision 2026-11-17, late pass:
// the creation ORDER and the chip mix were fixed by the player).
//
// Creation order — a column takes the next slot when it is created:
//   1. front left        — the first chip of the pot
//   2. front right       — "two chips as now, a row"
//   3. top centre        — in the HOLLOW between the two front columns, one depth
//                          step higher (0.6847·(d+gap) = 15.75 px at d = 22,
//                          gap = 1): a pyramid of two stacks under one
//   4. top right         — next to the third, on the same top row: two below, two
//                          above. It is NOT placed behind the right front stack
//                          (that read as a chip stuck on it) but one step further
//                          right, on the top row
//   5. front centre      — it CLOSES the front row (three stacks) and takes the
//                          chips that did not fit anywhere else (leftovers).
// When the fifth column appears, the front row spreads from ±step/2 to ±step and the
// top pair moves from (0, +step) into the two hollows (±step/2) — the only
// arrangement with three stacks in front and two in the hollows. It happens once per
// hand, together with the chips that close the row.
//
// The pot GROWS (2026-11-18): between streets only the chips that arrived are added
// (chipsplit.js::grow) — a column that is already on the table keeps its chips, its
// slot and its height. The pyramid is rebuilt and the POSITIONS are re-rolled only
// when the pot is empty again (a new hand). Before, every collect re-split the total
// and re-shuffled the slots, so the columns jumped at every street, and the chips
// that flew in had nothing to do with the column that appeared.
//
// A column is a stack of up to 4 chips of ONE denomination; the closing column may
// become mixed (it takes the leftovers). The chips are NOT sorted by denomination:
// when the first pot of a hand appears the columns are shuffled
// (chipsplit.js::columns), so the biggest chip does not always take slot 1. Whatever
// does not fit is not drawn — the pill has the truth.
//
// The top row is drawn FIRST (it lies further away) and is covered by the front
// row: occlusion, never intersection.
//
// Geometry: with five columns the block is 3·(d+gap) + d = 68 wide and
// faceH + 4·band + 15.75 + pill 13 − 1 (the pill tucks under the chips)
// = 53.84 high. CommunityCards hangs the block 3 px above the card row; GamePage
// reserves potTop 88.84 above the card row centre and a 120.84-high "pot + cards"
// block (see there and tools/table-preview/geometry.js).
Item {
    id: pot

    property int amount: 0
    property bool cash: false
    property real renderScale: 1
    property int chipSize: 22
    // Slot order below is fixed: three front slots + two top slots.
    property int maxColumns: 5
    property int maxPerStack: 4
    // Test hook (see BetChip): when set, printed instead of GameTable.amount().
    property string amountText: ""

    readonly property real _d: chipSize
    readonly property real _gap: 1
    readonly property real _step: _d + _gap
    readonly property real _band: 0.0988 * _d
    readonly property real _faceH: 0.7906 * _d
    readonly property real _chipBottom: _d / 2 + (_faceH + _band) / 2
    readonly property int _pillH: 13
    // The pill tucks 1 px under the front row (invisible) — the block is 1 px
    // shorter, which the tight 5-seat bands need.
    readonly property int _pillOverlap: 1
    // One depth step of the lying asset: a column standing one row further away
    // is drawn higher by exactly this much (the same as a chip in the hollow).
    readonly property real _shift: 0.8660254 * _step * (_faceH / _d)

    // ── the piles: columns in slot (creation) order, bottom → top ───────────
    // State, not a binding: the pot grows. `_seed` is the seed of the random
    // POSITIONS — rolled when the pot empties, so a hand keeps its layout and the
    // collect animation can predict the next one (`arrivalPoints`: the same seed
    // gives the same columns twice).
    property var _stateColumns: []
    property int _seed: 1
    property int _lastAmount: 0

    // A pile drawn from an EXPLICIT chip list instead of a sum (2026-11-18, the pot
    // payout): a winner's share is a subset of the chips that lay in the pot, and the
    // animation shows exactly those chips instead of a fresh split of the money.
    // `amount` still feeds the pill (the share's money) and is ignored for the columns.
    property var chipsOverride: []
    // Pill fade of the payout: a share shows its sum while the pot divides, the number
    // fades away before the chips travel (the player: the pill goes out, the chips move).
    property real pillOpacity: 1

    function _rollSeed() {
        _seed = 1 + Math.floor(Math.random() * 2147483646)
    }
    function _rng() {
        return ChipSplit.seeded(_seed)
    }
    function _rebuild() {
        // An explicit chip list (a payout share) owns the columns — the sum is only
        // the pill's text there.
        if (chipsOverride.length > 0)
            return
        _stateColumns = ChipSplit.columns(amount, cash, maxColumns, maxPerStack, _rng())
    }

    onAmountChanged: {
        var prev = _lastAmount
        _lastAmount = amount
        if (chipsOverride.length > 0)
            return
        if (amount <= 0) {
            // The hand is over: drop the chips and roll the positions of the next pot.
            _stateColumns = []
            _rollSeed()
        } else if (_stateColumns.length === 0 || amount <= prev) {
            _rebuild()
        } else {
            _stateColumns = ChipSplit.grow(_stateColumns, amount - prev, cash,
                                           maxColumns, maxPerStack)
        }
    }
    Component.onCompleted: {
        _rollSeed()
        _lastAmount = amount
        if (amount > 0)
            _rebuild()
    }

    // The columns actually drawn: the explicit list (a payout share) or the grown pot.
    readonly property var _columns: chipsOverride.length > 0
        ? ChipSplit.fromChips(chipsOverride, maxColumns, maxPerStack)
        : _stateColumns

    readonly property int _n: _columns.length

    // The slot of the i-th CREATED column: 0 front left, 1 front right, 2 top
    // centre, 3 top right, 4 front centre (closes the front row). `back` = the top
    // row (one depth step further away). Before five columns the front pair stands
    // half a step apart, so that a single column above it sits in the hollow.
    function _slotAt(i, n) {
        var closed = n >= 5
        if (i === 0)
            return { "cx": closed ? -_step : (n >= 2 ? -_step / 2 : 0), "back": false }
        if (i === 1)
            return { "cx": closed ? _step : _step / 2, "back": false }
        if (i === 2)
            return { "cx": closed ? -_step / 2 : 0, "back": true }
        if (i === 3)
            return { "cx": closed ? _step / 2 : _step, "back": true }
        return { "cx": 0, "back": false }
    }

    // ONE atomic snapshot of a layout: the slot of every column (its centre and
    // whether it stands in the top row), the chips to paint, the bounding box and
    // the height of the chip block. Computed from a COPY of the columns, so the
    // flight can ask for the layout of a pot that has not appeared yet
    // (`arrivalPoints`); reading separate bindings used to hit a stale array on an
    // amount change ("Cannot read property 'back' of undefined").
    function _layoutOf(cols) {
        var n = cols.length
        var chips = [], backChips = [], frontChips = []
        var minX = 0, maxX = 0, chipsH = 0
        for (var i = 0; i < n; ++i) {
            var s = _slotAt(i, n)
            minX = Math.min(minX, s.cx - _d / 2)
            maxX = Math.max(maxX, s.cx + _d / 2)
            var col = cols[i]
            var h = _faceH + col.length * _band + (s.back ? _shift : 0)
            if (h > chipsH)
                chipsH = h
            for (var k = 0; k < col.length; ++k) {
                var item = { "value": col[k], "cx": s.cx, "level": k, "back": s.back }
                if (s.back)
                    backChips.push(item)
                else
                    frontChips.push(item)
            }
        }
        // the top row first (it lies further away), then the front row
        return { "chips": backChips.concat(frontChips),
                 "minX": minX, "maxX": maxX, "midX": (minX + maxX) / 2,
                 "chipsH": chipsH }
    }

    readonly property var _layout: _layoutOf(_columns)

    // The pile is centred on its OWN bounding box: the four-column state is not
    // symmetric (the top row is shifted right), so the item cannot just centre the
    // slot origin.
    width: _layout.maxX - _layout.minX
    height: _layout.chipsH + _pillH - _pillOverlap
    // Bottom line of the front row (the pill hangs under it, overlapping 1 px).
    readonly property real _baseY: height - (_pillH - _pillOverlap)
    readonly property var _chips: _layout.chips
    // Distance from the pile's BOTTOM line to the centre of the chip block: the payout
    // puts a share on the pot's bottom line and then aims the chips at the winner, so
    // this is the offset between the two (see GamePage, the payout layer).
    readonly property real chipBlockUp: (_pillH - _pillOverlap) + _layout.chipsH / 2

    // The chips the pile actually draws, column by column (bottom → top): the payout
    // hands the pot's own chips out to the winners, no re-split of the money.
    function drawnChipList() {
        var out = []
        for (var i = 0; i < _columns.length; ++i)
            for (var k = 0; k < _columns[i].length; ++k)
                out.push(_columns[i][k])
        return out
    }

    // ── where the arriving chips will land ──────────────────────────────────
    // The collect animation holds the pot sum back while the chips fly
    // (GamePage.potShown), so this pile still shows the OLD columns. The flight is
    // aimed at the position the new chips WILL take, which is why the same
    // grow/roll is computed here ahead of time (the seed makes it deterministic).
    //
    // One point per column that grows, at the centre of its new top chip. The point
    // is given relative to the two things that do not depend on the height of the
    // block: `dx` from the pile's centre (the middle of the block, where the badge
    // hangs it) and `up` from the pile's BOTTOM line (the pill's bottom, fixed 3 px
    // above the card row) — see CommunityCards.potArrivalPoints.
    function arrivalPoints(added) {
        if (added <= 0)
            return []
        var fut = (_stateColumns.length === 0)
                ? ChipSplit.columns(amount + added, cash, maxColumns, maxPerStack, _rng())
                : ChipSplit.grow(_stateColumns, added, cash, maxColumns, maxPerStack)
        if (fut.length === 0)
            return []
        var lay = _layoutOf(fut)
        var out = []
        for (var i = 0; i < fut.length; ++i) {
            var wasN = i < _stateColumns.length ? _stateColumns[i].length : 0
            var nowN = fut[i].length
            if (nowN <= wasN)
                continue
            var s = _slotAt(i, fut.length)
            out.push({ "dx": s.cx - lay.midX,
                       "up": (_pillH - _pillOverlap) + (s.back ? _shift : 0)
                             + (_faceH + _band) / 2 + (nowN - 1) * _band })
        }
        return out
    }

    Repeater {
        model: pot._chips

        ChipImage {
            required property var modelData
            z: modelData.back ? 1 : 2
            chipSize: pot._d
            value: modelData.value
            cash: pot.cash
            renderScale: pot.renderScale
            x: pot.width / 2 + (modelData.cx - pot._layout.midX) - width / 2
            y: pot._baseY - (modelData.back ? pot._shift : 0)
               - pot._chipBottom - modelData.level * pot._band
        }
    }

    // ── the sum: a dark pill under the chips, as before ─────────────────────
    Rectangle {
        id: pill
        visible: pot.amount > 0
        opacity: pot.pillOpacity
        y: pot.height - pot._pillH
        x: (pot.width - width) / 2
        width: pillLabel.implicitWidth + 14
        height: pot._pillH
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.72)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.10)

        AppText {
            id: pillLabel
            anchors.centerIn: parent
            text: pot.amountText !== "" ? pot.amountText
                  : ((typeof GameTable !== "undefined" && GameTable)
                     ? GameTable.amount(pot.amount) : "")
            color: Config.Theme.colorAccent
            font.pixelSize: 10
            font.bold: true
        }
    }
}
