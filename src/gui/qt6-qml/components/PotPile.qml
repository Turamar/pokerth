import QtQuick

import "chipsplit.js" as ChipSplit
import "../config" as Config

// The pot as a PACKED PYRAMID of lying chips (decision 2026-11-17, replacing the
// flat row of three stacks of two).
//
// All chips lie on the felt. The front row keeps a real 1 px gap (the asset is
// 0.988·d wide, so nothing intersects), and the chips of the second row nestle in
// the HOLLOWS between them — they are NOT on top of the front chips. The second
// row is only drawn one depth step higher, because the lying asset is an oblique
// projection: a chip in the hollow sits
//     sqrt(3)/2 · (d + gap)          (equilateral pack — its centre is one
//                                     hollow depth behind the front centres)
// further away, and the asset's own tilt (sin θ = faceH/d = 0.7906) maps that to
//     0.6847 · (d + gap)  = 15.75 px at d = 22, gap = 1
// on the screen. With this step the front chips cover only the lower rim of the
// second row (the pack reads as a pyramid); with a smaller step they cut into its
// face and with a bigger one the chip appears to float above the hollow.
//
// The deeper row is drawn FIRST, so no chip is ever painted on top of a chip of
// the other row: the front row simply covers the rim of the second.
Item {
    id: pot

    property int amount: 0
    property bool cash: false
    property real renderScale: 1
    property int chipSize: 22
    // Front row: at most this many chips (4·d + 3·gap = 91 px, the width budget).
    property int maxBase: 4
    // Chips in total; the rest is not drawn — the pill has the truth.
    property int maxChips: 7
    // Test hook (see BetChip): when set, printed instead of GameTable.amount().
    property string amountText: ""

    readonly property real _d: chipSize
    readonly property real _gap: 1
    readonly property real _band: 0.0988 * _d
    readonly property real _faceH: 0.7906 * _d
    readonly property real _chipH: _faceH + _band
    readonly property real _chipBottomInItem: _d / 2 + _chipH / 2
    readonly property int _pillH: 13
    // Depth step of the hollow, derived from the asset (see the header).
    readonly property real _shift: 0.8660254 * (_d + _gap) * (_faceH / _d)

    // The chips actually drawn (values, biggest first before the shuffle).
    property var _chips: []
    readonly property int _baseCount: Math.min(_chips.length, maxBase)
    readonly property int _secondCount: Math.min(Math.max(0, _chips.length - _baseCount),
                                                 Math.max(0, _baseCount - 1))
    readonly property real _baseW: _baseCount > 0 ? _baseCount * (_d + _gap) - _gap : 0

    width: _baseW
    height: _chipH + (_secondCount > 0 ? _shift : 0) + _pillH
    // Bottom line of the chips (the pill hangs under it).
    readonly property real _baseY: height - _pillH

    function _rebuild() {
        var piles = ChipSplit.split(amount, cash, maxBase, 4)
        var list = []
        for (var i = 0; i < piles.length && list.length < maxChips; ++i)
            for (var k = 0; k < piles[i].count && list.length < maxChips; ++k)
                list.push(piles[i].value)
        // No order at all: the chips are shuffled, so the same pot looks
        // different every time. Rolled when the amount changes and kept while it
        // stays the same (a repaint or a resize does not reshuffle).
        for (i = list.length - 1; i > 0; --i) {
            var j = Math.floor(Math.random() * (i + 1))
            var t = list[i]; list[i] = list[j]; list[j] = t
        }
        _chips = list
    }

    onAmountChanged: _rebuild()
    Component.onCompleted: _rebuild()

    // ── the second row (drawn first: it lies deeper) ────────────────────────
    Repeater {
        model: pot._secondCount

        ChipImage {
            required property int index
            chipSize: pot._d
            value: pot._chips[pot._baseCount + index]
            cash: pot.cash
            renderScale: pot.renderScale
            x: (index + 0.5) * (pot._d + pot._gap)
            y: pot._baseY - pot._chipBottomInItem - pot._shift
            z: 1
        }
    }

    // ── the front row: on the felt ──────────────────────────────────────────
    Repeater {
        model: pot._baseCount

        ChipImage {
            required property int index
            chipSize: pot._d
            value: pot._chips[index]
            cash: pot.cash
            renderScale: pot.renderScale
            x: index * (pot._d + pot._gap)
            y: pot._baseY - pot._chipBottomInItem
            z: 2
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
