import QtQuick

import "../config" as Config

// The pot sum on the felt – at the top centre between the two topmost seats.
//
// Player's request 2026-11-13: "Перенести число денег в банке так же наверх между
// p4/p5 … чтобы освободить центр над картами для красивого банка фишек". The sum
// used to sit in the pill UNDER the pot chips (BetChip), and that pill took 13 base
// px of the strip above the board – exactly the strip the pot picture is going
// into next. Now the chips keep the strip and the number hangs above, between the
// two top plates (the reference client prints "POT 480" there in the same way).
//
// The page owns the layout: it computes where the label goes (it knows the seats,
// so it can put the pill between them – see `tableZone.potLabelTop`), this
// component only draws the pill and prints the value. That way no layout number is
// duplicated in two files.
//
// The value is the SAME one the chips show: `GameTable.pot` – only the collected
// part of the round (a bet still lying in front of a player is not in the pot yet,
// decision 2026-11-12), formatted by `GameTable.amount` (money at a cash table,
// chip counts in a tournament).
Item {
    id: root

    property int amount: 0
    property color textColor: Config.Theme.colorAccent
    property int fontSize: Config.Theme.tableFont(12)
    // Test hook (see BetChip.amountText): printed instead of GameTable.amount().
    property string amountText: ""

    // A little larger than the bet pill (which is 10 px): this is the number of
    // the whole hand, read from the whole table.
    readonly property int pillHeight: Math.max(18, Math.round(fontSize) + 6)
    implicitWidth: label.implicitWidth + 18
    implicitHeight: pillHeight
    visible: amount > 0

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.62)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.12)
    }

    AppText {
        id: label
        anchors.centerIn: parent
        text: root.amountText !== ""
              ? root.amountText
              : ((typeof GameTable !== "undefined" && GameTable)
                 ? GameTable.amount(root.amount) : "")
        color: root.textColor
        font.pixelSize: root.fontSize
        font.bold: true
    }

    // A short pop when the pot grows (the chips of this pot keep their own pop):
    // the number is noticed when the round is collected.
    transformOrigin: Item.Center
    onAmountChanged: if (visible) potPop.restart()
    SequentialAnimation {
        id: potPop
        NumberAnimation { target: root; property: "scale"; from: 1.0; to: 1.12; duration: 110; easing.type: Easing.OutQuad }
        NumberAnimation { target: root; property: "scale"; to: 1.0; duration: 170; easing.type: Easing.OutBack }
    }
}
