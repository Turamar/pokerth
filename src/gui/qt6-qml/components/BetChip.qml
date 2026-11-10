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
    // The offset of a chip in a stack: a quarter of the diameter, otherwise a
    // column of two does not read as a stack at all.
    readonly property int _stackStep: Math.max(2, Math.round(_chipD * 0.25))
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

    // Two colours per denomination: the body and the lighter face. No digits on
    // the chip – the colour is the denomination. Cash uses the same ladder in cents.
    function chipColors(value) {
        if (value >= 50000) return ["#0f766e", "#2dd4bf"]   // teal
        if (value >= 10000) return ["#1d4ed8", "#60a5fa"]   // blue
        if (value >= 2500)  return ["#c2410c", "#fb923c"]   // orange
        if (value >= 500)   return ["#6d28d9", "#c084fc"]   // purple
        if (value >= 100)   return ["#1f2937", "#6b7280"]   // near black
        if (value >= 25)    return ["#15803d", "#4ade80"]   // green
        if (value >= 10)    return ["#b91c1c", "#f87171"]   // red
        return ["#9ca3af", "#e5e7eb"]                        // silver: 1 and 5
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

                    // One chip, drawn: coloured body, white marks on the rim, a
                    // lighter face with a thin ring. The lowest chip is drawn
                    // first, every next one is offset upwards – that is the stack.
                    Canvas {
                        id: chip
                        width: betChip._chipD
                        height: width
                        y: (col.entry.count - 1 - index) * betChip._stackStep

                        readonly property var face: betChip.chipColors(col.entry.value)

                        Connections {
                            target: col
                            function onEntryChanged() { chip.requestPaint() }
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var r = width / 2
                            var body = chip.face[0]
                            var inner = chip.face[1]

                            // The body.
                            ctx.beginPath()
                            ctx.arc(r, r, r - 0.6, 0, 6.2832)
                            ctx.fillStyle = body
                            ctx.fill()

                            // The rim marks: eight white segments, like a real chip.
                            ctx.lineWidth = Math.max(1, r * 0.26)
                            ctx.strokeStyle = "#f8fafc"
                            for (var i = 0; i < 8; ++i) {
                                var a0 = i * 0.7854 + 0.10
                                ctx.beginPath()
                                ctx.arc(r, r, r * 0.80, a0, a0 + 0.30)
                                ctx.stroke()
                            }

                            // The face: a lighter disc with a thin light ring.
                            ctx.beginPath()
                            ctx.arc(r, r, r * 0.52, 0, 6.2832)
                            ctx.fillStyle = inner
                            ctx.fill()
                            ctx.lineWidth = Math.max(0.7, r * 0.09)
                            ctx.strokeStyle = "rgba(255,255,255,0.72)"
                            ctx.stroke()

                            // A dark outline keeps stacked chips apart.
                            ctx.beginPath()
                            ctx.arc(r, r, r - 0.6, 0, 6.2832)
                            ctx.lineWidth = 1
                            ctx.strokeStyle = "rgba(0,0,0,0.55)"
                            ctx.stroke()
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
