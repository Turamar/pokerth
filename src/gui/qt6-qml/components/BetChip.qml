import QtQuick

import "../config" as Config

// Bet/pot display: a PILE of chips instead of a single chip, plus the amount.
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
// Look of a single chip (2026-11-12, "chips look dated"): a coloured rim in the
// denomination colour, six ivory edge notches, a light ivory face with a thin
// ring in the rim colour and a small centre dot, a top-left glare and a soft
// drop shadow. Dark rim + light centre for the black 100 chip, so the stack
// reads even on a dark felt. No numbers anywhere.
//
// Visibility/position is set by the caller (e.g. visible: amount > 0).
Item {
    id: betChip

    property int amount: 0
    property color textColor: "#eff1f5"
    // Diameter of one chip in base pixels.
    property int iconSize: 16
    property real fontSize: Config.Theme.tableFont(11)
    // true: the amount under the pile (wide spots, e.g. on the felt);
    // false: the amount to the right of the pile (tight spots, e.g. the bet base,
    //        and the pot above the board).
    property bool labelBelow: false
    // Caps: more would eat the table, not inform anything.
    property int maxColumns: 4
    property int maxPerColumn: 4
    // Horizontal overlap of the columns (0.30 = each next column starts 30 % of
    // a chip before the previous one ends). Tighter piles fit more denominations
    // into a narrow spot.
    property real columnOverlap: 0.30
    // Gap between the pile and the amount beside it.
    property int labelGap: 3
    // A block that has to stay inside a given width (the pot strip above the
    // board): above this limit the number of columns shrinks so that
    // pile + amount never gets wider. 0 = no limit. The amount wins – the pile
    // is decoration, the number is the truth.
    property int maxWidth: 0
    // A dark 1 px offset behind the amount keeps it readable on the felt.
    property bool labelShadow: true

    // The bet ON THE FELT (labelBelow, wide spot) never renders smaller than the
    // size the player asked for: 15 px chips and a tableFont(12) amount (the
    // call sites in the player boxes still pass the old tight-plate values
    // 13/10, and those must not shrink the felt bet and its sum again).
    readonly property int _feltMinChip: 15
    readonly property int _chipD: Math.max(labelBelow ? _feltMinChip : 9,
                                           Math.max(1, Math.round(iconSize)))
    readonly property real _labelPx: labelBelow
        ? Math.max(Math.round(fontSize), Config.Theme.tableFont(12))
        : Math.round(fontSize)
    // On the felt the amount is the only readable part of the bet: brighten the
    // caller's colour towards white (the boxes pass a slightly grey one) and keep
    // a soft dark shadow behind it.
    readonly property color _labelColor: labelBelow ? Qt.lighter(textColor, 1.10)
                                                    : textColor
    // The offset of a chip in a stack: 25–30 % of the diameter, so that the rim
    // of every chip in the column stays visible instead of the column reading as
    // one shapeless blob (player report 2026-11-12 about a 4-chip bet).
    readonly property int _stackStep: Math.max(2, Math.floor(_chipD * 0.30))
    // Columns overlap a bit – a pile of chips, not a row of coins.
    readonly property int _colStep: Math.max(2, _chipD
        - Math.max(2, Math.round(_chipD * columnOverlap)))
    readonly property int _labelGap: labelGap
    // THE ladder: 5/10/25/100/500/2500/10000/50000. The cash table uses the same
    // ladder with the cent as unit (5c … $500) – there is no 1c chip: the
    // smallest chip on the felt is the 5 one, everything below is not drawn.
    readonly property var _ladder: [50000, 10000, 2500, 500, 100, 25, 10, 5]
    readonly property int _minChipValue: 5

    // Columns actually drawn: `maxColumns`, reduced when the block has a
    // `maxWidth` and the amount would not fit next to that many columns.
    // 0 = not even one chip fits beside the amount → the amount is drawn alone.
    readonly property int _labelW: label.visible ? Math.ceil(label.implicitWidth) : 0
    readonly property int _effMaxColumns: {
        var cap = betChip.maxColumns
        if (betChip.maxWidth <= 0 || betChip.labelBelow || !label.visible)
            return cap
        var avail = betChip.maxWidth - betChip._labelGap - betChip._labelW
        if (avail < betChip._chipD)
            return 0
        var n = Math.floor((avail - betChip._chipD) / betChip._colStep) + 1
        return Math.max(1, Math.min(cap, n))
    }

    // One entry per denomination that occurs: { value: .., count: n }
    readonly property var _columns: {
        var amount = Math.max(0, Math.round(betChip.amount))
        if (amount <= 0)
            return []
        // Below the smallest chip (e.g. a 4c bet at the micro cash table): one
        // neutral chip as decoration – never a stack of copies of it. Value 0 has
        // no denomination, so chipColors() draws the silver fallback (a red 5c
        // chip would claim a value the bet does not have); the number is the truth.
        if (amount < betChip._minChipValue)
            return [{ "value": 0, "count": 1 }]
        var rest = amount
        var ladder = betChip._ladder
        var out = []
        for (var i = 0; i < ladder.length && rest > 0; ++i) {
            var n = Math.floor(rest / ladder[i])
            if (n <= 0)
                continue
            if (out.length < betChip._effMaxColumns)
                out.push({ "value": ladder[i], "count": Math.min(n, betChip.maxPerColumn) })
            rest -= n * ladder[i]
        }
        // The remainder below the smallest chip (< 5) is deliberately not drawn.
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
    // No gap when there is no pile at all (the amount alone is the block).
    readonly property int _gapNow: _pileW > 0 ? _labelGap : 0

    // Rim colour and face colour per denomination. The rim is the denomination
    // colour; the face is ivory (a dark rim/light centre contrast, exactly how
    // real casino chips read). Cash uses the same ladder with the cent as unit.
    function chipColors(value) {
        if (value >= 50000) return ["#a16207", "#f7edd6"]   // gold
        if (value >= 10000) return ["#0e7490", "#eef8fa"]   // teal
        if (value >= 2500)  return ["#c2410c", "#fdf0e6"]   // orange
        if (value >= 500)   return ["#6d28d9", "#f3edff"]   // violet
        if (value >= 100)   return ["#24262d", "#f1f0ec"]   // near black
        if (value >= 25)    return ["#15803d", "#effaf1"]   // green
        if (value >= 10)    return ["#1d4ed8", "#eef3ff"]   // blue
        if (value >= 5)     return ["#b91c1c", "#fdeeee"]   // red
        return ["#8792a2", "#f4f7fb"]                        // silver fallback
    }

    width: implicitWidth
    height: implicitHeight
    implicitWidth: {
        if (!label.visible || labelBelow) return Math.max(_pileW, label.implicitWidth)
        return _pileW + _gapNow + label.implicitWidth
    }
    implicitHeight: {
        if (!label.visible) return _pileH
        return labelBelow ? _pileH + 3 + label.implicitHeight
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

                    // One chip, drawn: coloured rim with ivory notches, a light
                    // face with a thin rim ring and a centre dot, a glare and a
                    // soft shadow. The lowest chip is drawn first, every next one
                    // is offset upwards – that is the stack.
                    Canvas {
                        id: chip
                        width: betChip._chipD
                        height: width
                        y: (col.entry.count - 1 - index) * betChip._stackStep

                        readonly property var face: betChip.chipColors(col.entry.value)
                        // Precomputed shades: the rim gets a light→dark body
                        // gradient, the face a subtle ivory gradient.
                        readonly property var tone: {
                            var c = chip.face
                            return {
                                "rim": c[0],
                                "rimTop": Qt.lighter(c[0], 1.20),
                                "rimBottom": Qt.darker(c[0], 1.28),
                                "face": c[1],
                                "faceTop": Qt.lighter(c[1], 1.04),
                                "faceBottom": Qt.darker(c[1], 1.08)
                            }
                        }

                        Connections {
                            target: col
                            function onEntryChanged() { chip.requestPaint() }
                        }
                        onWidthChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var r = width / 2
                            var tone = chip.tone

                            // Soft drop shadow: the rim of a chip in the stack
                            // shadows the chip below it.
                            ctx.beginPath()
                            ctx.arc(r, r + Math.max(0.6, r * 0.10), r - 0.4, 0, 6.2832)
                            ctx.fillStyle = "rgba(0,0,0,0.38)"
                            ctx.fill()

                            // The rim (body) in the denomination colour, a bit
                            // lighter at the top for a rounded, modern look.
                            var rimGrad = ctx.createLinearGradient(0, 0, 0, width)
                            rimGrad.addColorStop(0, tone.rimTop)
                            rimGrad.addColorStop(1, tone.rimBottom)
                            ctx.beginPath()
                            ctx.arc(r, r, r - 0.5, 0, 6.2832)
                            ctx.fillStyle = rimGrad
                            ctx.fill()

                            // Six ivory edge notches – the classic casino rim.
                            ctx.strokeStyle = tone.face
                            ctx.lineWidth = Math.max(1.1, r * 0.30)
                            for (var i = 0; i < 6; ++i) {
                                var a = i * 1.0472 + 0.2618   // 60°, first notch at 15°
                                ctx.beginPath()
                                ctx.arc(r, r, r * 0.79, a - 0.20, a + 0.20)
                                ctx.stroke()
                            }

                            // The face: a light ivory disc with a subtle gradient.
                            var faceGrad = ctx.createLinearGradient(0, r * 0.35, 0, width)
                            faceGrad.addColorStop(0, tone.faceTop)
                            faceGrad.addColorStop(1, tone.faceBottom)
                            ctx.beginPath()
                            ctx.arc(r, r, r * 0.60, 0, 6.2832)
                            ctx.fillStyle = faceGrad
                            ctx.fill()

                            // A thin ring in the rim colour around the face.
                            ctx.beginPath()
                            ctx.arc(r, r, r * 0.60, 0, 6.2832)
                            ctx.strokeStyle = tone.rim
                            ctx.lineWidth = Math.max(0.9, r * 0.10)
                            ctx.stroke()

                            // Centre dot in the rim colour (a light centre with a
                            // dark rim is what makes the chip read as one).
                            if (r >= 5) {
                                ctx.beginPath()
                                ctx.arc(r, r, r * 0.20, 0, 6.2832)
                                ctx.fillStyle = tone.rim
                                ctx.globalAlpha = 0.88
                                ctx.fill()
                                ctx.globalAlpha = 1.0
                            }

                            // Top-left glare on the rim.
                            if (r >= 5) {
                                ctx.beginPath()
                                ctx.arc(r, r, r * 0.80, 3.6128, 4.5553)   // ~207°…261°
                                ctx.strokeStyle = "rgba(255,255,255,0.32)"
                                ctx.lineWidth = Math.max(0.9, r * 0.12)
                                ctx.stroke()
                            }

                            // A dark outline keeps stacked chips apart.
                            ctx.beginPath()
                            ctx.arc(r, r, r - 0.5, 0, 6.2832)
                            ctx.lineWidth = Math.max(1.1, r * 0.10)
                            ctx.strokeStyle = "rgba(0,0,0,0.62)"
                            ctx.stroke()
                        }
                    }
                }
            }
        }
    }

    // ── The amount ───────────────────────────────────────────────────────────
    // A 1 px dark copy behind the number: the felt/plate behind the pile is not
    // guaranteed to be dark, the amount must be readable in any case.
    AppText {
        id: labelShadow
        visible: betChip.labelShadow && betChip.amount > 0
        text: label.text
        color: Qt.rgba(0, 0, 0, 0.72)
        font.pixelSize: label.font.pixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        x: label.x
        y: label.y + 1
    }

    AppText {
        id: label
        visible: betChip.amount > 0
        text: (typeof GameTable !== "undefined" && GameTable) ? GameTable.amount(betChip.amount) : ""
        color: betChip._labelColor
        font.pixelSize: betChip._labelPx
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        x: betChip.labelBelow ? (betChip.width - width) / 2
         : !visible ? 0
         : pile.x + pile.width + betChip._gapNow
        // Own line under the pile with a small gap – the sum must not touch the
        // chips (player report 2026-11-12).
        y: betChip.labelBelow ? pile.height + 3
         : (betChip.height - height) / 2
    }
}
