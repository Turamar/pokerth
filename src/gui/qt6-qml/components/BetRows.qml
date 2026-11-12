import QtQuick

import "../config" as Config

// A COMPACT chip row for the tight places: the bet inside a player's plate
// (16 px) and the bet strip of the own box (22 px). Same lying chips as the felt
// pile (`BetChip`), but the stacks stand in a row and the amount sits beside
// them — there is no room for a pyramid and a pill there.
//
// One stack per denomination (greedy ladder), at most `maxColumns` stacks and
// `maxPerColumn` chips each: a stack is a row of lying chips offset by one side
// band, so every lower chip shows only its edge. The number is the truth.
Item {
    id: row

    property int amount: 0
    property color textColor: "#eff1f5"
    // Width (diameter) of one chip in base pixels.
    property int chipSize: 16
    property bool digits: false
    property real renderScale: 1
    property int fontSize: Config.Theme.tableFont(11)
    // Test hook (see BetChip): printed instead of GameTable.amount().
    property string amountText: ""
    property int maxColumns: 2
    property int maxPerColumn: 2

    readonly property var _ladder: [50000, 10000, 2500, 500, 100, 25, 10, 5]
    readonly property int _minChipValue: 5

    readonly property real _d: chipSize
    readonly property real _band: 0.0593 * _d
    readonly property real _faceH: 0.7906 * _d
    readonly property real _chipBottomInItem: _d / 2 + (_faceH + _band) / 2
    readonly property real _colStep: _d + 1

    readonly property var _columns: {
        var a = Math.max(0, Math.round(row.amount))
        if (a <= 0) return []
        if (a < row._minChipValue) return [{ "value": 0, "count": 1 }]
        var rest = a, out = []
        for (var i = 0; i < row._ladder.length && rest > 0; ++i) {
            var n = Math.floor(rest / row._ladder[i])
            if (n <= 0) continue
            if (out.length < row.maxColumns)
                out.push({ "value": row._ladder[i],
                           "count": Math.min(n, row.maxPerColumn) })
            rest -= n * row._ladder[i]
        }
        return out
    }
    readonly property real _rowW: _columns.length > 0
        ? _d + (_columns.length - 1) * _colStep : 0
    readonly property real _rowH: _columns.length > 0
        ? _faceH + Math.max(1, _maxCount()) * _band : 0
    function _maxCount() {
        var m = 1
        for (var i = 0; i < _columns.length; ++i) m = Math.max(m, _columns[i].count)
        return m
    }

    width: visible ? _rowW + 2 + label.implicitWidth : 0
    height: Math.max(_rowH, label.implicitHeight)

    Repeater {
        model: row._columns

        Item {
            required property var modelData
            required property int index
            property var entry: modelData
            width: row._d
            height: row._faceH + (entry.count - 1) * row._band
            x: index * row._colStep
            y: row.height - height

            Repeater {
                model: entry.count

                ChipImage {
                    required property int index
                    chipSize: row._d
                    value: entry.value
                    digits: row.digits
                    renderScale: row.renderScale
                    x: (row._d - width) / 2
                    y: (entry.count - 1 - index) * row._band
                }
            }
        }
    }

    AppText {
        id: label
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: row.amountText !== "" ? row.amountText
              : ((typeof GameTable !== "undefined" && GameTable)
                 ? GameTable.amount(row.amount) : "")
        color: row.textColor
        font.pixelSize: row.fontSize
        font.bold: true
    }
}
