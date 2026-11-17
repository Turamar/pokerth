import QtQuick

import "chipsplit.js" as ChipSplit
import "../config" as Config

// The pot as COLUMNS of lying chips in a pyramid (decision 2026-11-17, late pass:
// the creation ORDER and the chip mix were fixed by the player).
//
// Creation order (the columns are filled in exactly this order):
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
//
// A column is a stack of up to 4 chips. The chips are NOT sorted by denomination:
// the chip multiset is shuffled (`chipsplit.js::columns`), so the pot reads as a
// real mix. Whatever still does not fit is not drawn — the pill has the truth.
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

    // ── the piles: columns in slot order, bottom → top ──────────────────────
    // Rolled when the amount changes and stored: a repaint or a resize keeps the
    // picture (and the random mix).
    property var _columns: []
    function _rebuild() {
        _columns = ChipSplit.columns(amount, cash, maxColumns, maxPerStack)
    }
    onAmountChanged: _rebuild()
    Component.onCompleted: _rebuild()

    readonly property int _n: _columns.length

    // ONE atomic snapshot of the layout: the slot of every column (its centre and
    // whether it stands in the top row), the chips to paint, the bounding box and
    // the height of the chip block. Everything is computed in ONE binding from a
    // copy of `_columns`: reading separate `_slots`/`_chips` bindings used to hit
    // a stale array on an amount change ("Cannot read property 'back' of
    // undefined").
    readonly property var _layout: {
        var cols = pot._columns
        var n = cols.length
        // The front row holds two stacks; the fifth column closes it.
        var frontN = n >= 5 ? 3 : Math.min(n, 2)
        var d = pot._d, step = pot._step, shift = pot._shift
        var chips = [], backChips = [], frontChips = []
        var minX = 0, maxX = 0, chipsH = 0
        for (var i = 0; i < n; ++i) {
            var back = i >= frontN
            var cx = 0
            if (!back) {
                // front slots in creation order: left, right, then the closing one
                if (frontN === 3)
                    cx = i === 2 ? 0 : (i === 0 ? -step : step)
                else if (frontN === 2)
                    cx = i === 0 ? -step / 2 : step / 2
            } else {
                var j = i - frontN
                if (frontN === 3)
                    // the classic pair in the two hollows of the closed front row
                    cx = j === 0 ? -step / 2 : step / 2
                else if (frontN === 2)
                    // above the hollow, then next to it on the right
                    cx = j === 0 ? 0 : step
            }
            minX = Math.min(minX, cx - d / 2)
            maxX = Math.max(maxX, cx + d / 2)
            var col = cols[i]
            var h = pot._faceH + col.length * pot._band + (back ? shift : 0)
            if (h > chipsH)
                chipsH = h
            for (var k = 0; k < col.length; ++k) {
                var item = { "value": col[k], "cx": cx, "level": k, "back": back }
                if (back)
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

    // The pile is centred on its OWN bounding box: the four-column state is not
    // symmetric (the top row is shifted right), so the item cannot just centre the
    // slot origin.
    width: _layout.maxX - _layout.minX
    height: _layout.chipsH + _pillH - _pillOverlap
    // Bottom line of the front row (the pill hangs under it, overlapping 1 px).
    readonly property real _baseY: height - (_pillH - _pillOverlap)
    readonly property var _chips: _layout.chips

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
