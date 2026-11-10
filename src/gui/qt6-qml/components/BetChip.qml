import QtQuick

// Bet display: a PILE of chips instead of a single chip, plus the amount.
//
// The amount is split greedily over the chip ladder of the mode (chips in a
// tournament, cents in a cash game). Every denomination gets its own COLUMN and
// the chips of one denomination lie on top of each other – exactly how a player
// would stack them. No digits on the chips: at 15 px they are not readable, and
// the sum is printed next to (or below) the pile anyway.
//
// The ladder guarantees at most four chips per column (the next denomination is
// always at least twice as large), the remaining rest is clamped to `maxColumns`
// columns. The pile is therefore decoration – the number is the truth.
//
// Visibility/position is set by the caller (e.g. visible: amount > 0).
Item {
    id: betChip

    property int amount: 0
    property color textColor: "#eff1f5"
    // Diameter of one chip in base pixels.
    property int iconSize: 20
    property int fontSize: 14
    // true: the amount under the pile (wide spots, e.g. on the felt);
    // false: the amount to the right of the pile (tight spots, e.g. the bet base).
    property bool labelBelow: false
    // Caps: more would eat the table, not inform anything.
    property int maxColumns: 4
    property int maxPerColumn: 4

    readonly property int _chipD: Math.max(9, iconSize)
    readonly property int _stackStep: Math.max(2, Math.round(_chipD * 0.18))
    readonly property int _colStep: Math.max(1, _chipD - Math.round(_chipD * 0.30))
    readonly property bool _tournament: (typeof GameTable !== "undefined" && GameTable)
        ? GameTable.tournamentMode : true
    // Highest denomination first – the greedy split wants them that way.
    readonly property var _ladder: _tournament
        ? [50000, 10000, 2500, 500, 100, 25, 10, 5]
        : [10000, 2500, 500, 100, 25, 5, 1]

    // One entry per denomination that occurs: { value: .., count: n }
    readonly property var _columns: {
        var rest = Math.max(0, Math.round(betChip.amount))
        var ladder = betChip._ladder
        var out = []
        for (var i = 0; i < ladder.length && rest > 0; ++i) {
            var n = Math.floor(rest / ladder[i])
            if (n <= 0)
                continue
            if (out.length < betChip.maxColumns)
                out.push({ "value": ladder[i], "count": Math.min(n, betChip.maxPerColumn) })
            rest -= n * ladder[i]
        }
        return out
    }
    readonly property int _maxCount: {
        var m = 1
        for (var i = 0; i < _columns.length; ++i)
            m = Math.max(m, _columns[i].count)
        return m
    }
    readonly property int _pileW: _columns.length > 0
        ? _chipD + (_columns.length - 1) * _colStep : 0
    readonly property int _pileH: _columns.length > 0
        ? _chipD + (_maxCount - 1) * _stackStep : 0

    // A colour per denomination – no text on the chip, so the colour is the only
    // thing that has to tell them apart. Cash works on the same ladder in cents.
    function chipColor(value) {
        if (value >= 50000) return "#0f766e"   // teal
        if (value >= 10000) return "#1d4ed8"   // blue
        if (value >= 2500)  return "#c2410c"   // orange
        if (value >= 500)   return "#7e22ce"   // purple
        if (value >= 100)   return "#26303d"   // near black
        if (value >= 25)    return "#15803d"   // green
        if (value >= 10)    return "#b91c1c"   // red
        return "#dfe3ea"                        // white: 1 and 5
    }

    width: implicitWidth
    height: implicitHeight
    implicitWidth: {
        if (!label.visible || labelBelow) return Math.max(_pileW, label.implicitWidth)
        return _pileW + 3 + label.implicitWidth
    }
    implicitHeight: {
        if (!label.visible) return _pileH
        return labelBelow ? _pileH + 2 + label.implicitHeight
                          : Math.max(_pileH, label.implicitHeight)
    }

    transformOrigin: Item.Center
    onVisibleChanged: if (visible) betPop.restart()
    onAmountChanged: if (visible) betPop.restart()
    SequentialAnimation {
        id: betPop
        NumberAnimation { target: betChip; property: "scale"; from: 0.6; to: 1.12; duration: 110; easing.type: Easing.OutQuad }
        NumberAnimation { target: betChip; property: "scale"; to: 1.0; duration: 130; easing.type: Easing.OutBack }
    }

    // ── The pile ─────────────────────────────────────────────────────────────
    Item {
        id: pile
        width: betChip._pileW
        height: betChip._pileH
        x: (betChip.labelBelow || !label.visible) ? (betChip.width - width) / 2 : 0
        y: betChip.labelBelow ? 0
          : label.visible ? (betChip.height - height) / 2
          : 0

        Repeater {
            model: betChip._columns.length

            // One column = one denomination.
            Item {
                id: col
                property var entry: betChip._columns[index] || { "value": 5, "count": 1 }
                x: index * betChip._colStep
                // The whole column rests on the bottom edge of the pile.
                y: pile.height - height
                width: betChip._chipD
                height: betChip._chipD + (entry.count - 1) * betChip._stackStep

                Repeater {
                    model: col.entry.count

                    // The lowest chip is drawn first, every next one is offset upwards.
                    Rectangle {
                        width: betChip._chipD
                        height: width
                        radius: width / 2
                        x: 0
                        y: (col.entry.count - 1 - index) * betChip._stackStep
                        color: betChip.chipColor(col.entry.value)
                        border.width: 1
                        border.color: Qt.lighter(color, 1.7)

                        // The face of the chip: a thin light ring, no digits.
                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(parent.width * 0.46)
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.55)
                        }
                    }
                }
            }
        }
    }

    // ── The amount ───────────────────────────────────────────────────────────
    AppText {
        id: label
        visible: betChip.amount > 0
        text: (typeof GameTable !== "undefined" && GameTable) ? GameTable.amount(betChip.amount) : ""
        color: betChip.textColor
        font.pixelSize: betChip.fontSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        x: betChip.labelBelow ? (betChip.width - width) / 2
         : !visible ? 0
         : pile.x + pile.width + 3
        y: betChip.labelBelow ? pile.height + 2
         : (betChip.height - height) / 2
    }
}
