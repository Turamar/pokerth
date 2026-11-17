import QtQuick

import "chipsplit.js" as ChipSplit
import "../config" as Config

// The pot as COLUMNS of lying chips in a pyramid (decision 2026-11-17, replacing
// the packed pyramid of single chips).
//
//   • a column is a stack of up to 4 chips, the biggest chip at the bottom;
//   • ONE DENOMINATION PER COLUMN while columns are left (5): 200 = 2×100 is one
//     column of two, not 100 + 100 in two columns; 900 = 500 + 4×100 is two
//     columns. A denomination that would need more than four chips continues in
//     the next column; when the denominations outnumber the columns the smallest
//     ones are mixed into the LAST column, and whatever still does not fit is not
//     drawn — the pill has the truth (`chipsplit.js::columns`);
//   • LAYOUT: three columns in the front row, two in the top row. The top row
//     nestles in the hollows between the front columns — the same equilateral
//     packing as a chip lying in a hollow — so it sits one depth step higher and
//     one row further away: sqrt(3)/2 · (d + gap) in depth, which the lying
//     asset's own projection (sin θ = faceH/d) maps to
//         0.6847 · (d + gap) = 15.75 px    at d = 22, gap = 1
//     on the screen. The top row is drawn FIRST and is covered by the front row:
//     occlusion, never intersection (the same rule as everywhere on the felt).
//   • The block: width 3·(d+gap) + d = 68 (the front row), height
//     faceH + 4·band + 15.75 + pill 13 = 54.84 in the worst case (a four-chip
//     column in the top row). CommunityCards hangs the block 6 px above the card
//     row; GamePage reserves potTop 92.84 (= 32 + 6 + 54.84) above the card row
//     centre and a 124.84-high "pot + cards" block (see there and
//     tools/table-preview/geometry.js).
Item {
    id: pot

    property int amount: 0
    property bool cash: false
    property real renderScale: 1
    property int chipSize: 22
    // Up to five columns (three front + two top); the block's width budget.
    property int maxColumns: 5
    // Up to four chips per column.
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
    // The pill tucks 1 px under the front row (invisible) — that keeps the block
    // 1 px shorter, which the tight 5-seat bands need.
    readonly property int _pillOverlap: 1
    // One depth step of the lying asset: a column standing one row further away
    // is drawn higher by exactly this much (same as a chip in the hollow).
    readonly property real _shift: 0.8660254 * _step * (_faceH / _d)

    // ── the pot's chips: columns, bottom → top ──────────────────────────────
    // Rolled when the amount changes and stored: a repaint or a resize keeps the
    // picture (the mix rule picks the same columns anyway, but this also keeps
    // the z/y evaluation cheap).
    property var _columns: []
    function _rebuild() {
        _columns = ChipSplit.columns(amount, cash, maxColumns, maxPerStack)
    }
    onAmountChanged: _rebuild()
    Component.onCompleted: _rebuild()

    readonly property int _n: _columns.length
    // Three columns stand in the front row, the rest in the top row (behind).
    readonly property int _frontN: Math.min(_n, 3)
    readonly property int _backN: Math.max(0, _n - 3)

    // Centre of a column relative to the block centre; each row is centred on the
    // block, so adding a column re-centres the pile as a whole.
    function colCentre(i) {
        if (i < _frontN)
            return (i - (_frontN - 1) / 2) * _step
        var j = i - _frontN
        return (j - (_backN - 1) / 2) * _step
    }

    readonly property real _frontW: _frontN > 0 ? (_frontN - 1) * _step + _d : 0
    readonly property real _backW: _backN > 0 ? (_backN - 1) * _step + _d : 0
    width: Math.max(_frontW, _backW)

    // Height of the chips alone: the tallest column, plus the depth step when it
    // stands in the top row.
    readonly property real _chipsH: {
        var h = 0
        for (var i = 0; i < _n; ++i)
            h = Math.max(h, _faceH + _columns[i].length * _band
                            + (i >= _frontN ? _shift : 0))
        return h
    }
    height: _chipsH + _pillH - _pillOverlap
    // Bottom line of the front row (the pill hangs under it, overlapping 1 px).
    readonly property real _baseY: height - (_pillH - _pillOverlap)

    // The chips to paint: the top row first (it lies further away), then the front
    // row; inside a column the bottom chip first, so every upper chip covers its
    // neighbour's band.
    readonly property var _chips: {
        var back = [], front = []
        for (var i = 0; i < _n; ++i) {
            var dst = i >= _frontN ? back : front
            for (var k = 0; k < _columns[i].length; ++k)
                dst.push({ "value": _columns[i][k], "cx": colCentre(i), "level": k,
                           "back": i >= _frontN })
        }
        return back.concat(front)
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
            x: pot.width / 2 + modelData.cx - width / 2
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
