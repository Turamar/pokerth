import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config
import "../components"

// Sitting down at a cash table (PLAN.md section 2).
//
// Money and chips are 1:1, so the table is paid for with the balance: the
// buy-in leaves the wallet when the table starts and comes back - as the stack
// that is left - when it is left. The blinds are fixed and never raised, which
// is what makes it a cash table and not a tournament.
Rectangle {
    id: localGamePagePage
    objectName: "localGamePage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: Config.StaticData.palette.secondary.col700

    // The balance as it was when the page was opened: while the table is
    // running the buy-in is already gone from the wallet, and the fields must
    // not follow that.
    property int cashBalance: Wallet ? Wallet.balance : 0
    property string errorText: ""

    readonly property int bigBlind: Wallet ? Wallet.bigBlind : 2
    readonly property int bbMin: Wallet ? Wallet.minBuyIn : 40
    readonly property int bbMax: Wallet ? Wallet.maxBuyIn : 200
    readonly property int bbShort: Wallet ? Wallet.shortBuyIn : 10
    // Below the regular minimum the rest of the balance is a legal (short) buy-in.
    readonly property int buyInMin: cashBalance >= bbMin ? bbMin : bbShort
    readonly property int buyInMax: Math.max(buyInMin, Math.min(bbMax, cashBalance))
    readonly property bool canSit: cashBalance >= buyInMin

    function defaultBuyIn() {
        if (cashBalance <= 0)
            return buyInMin
        var wanted = Math.min(bbMax, cashBalance)
        // Whole big blinds, never below the minimum this balance allows.
        wanted = wanted - (wanted % bigBlind)
        return Math.max(buyInMin, wanted)
    }

    function stackInBB(chips) {
        return bigBlind > 0 ? Math.round(chips / bigBlind) : 0
    }

    // Default button of the page: Enter sits down, no matter which field
    // has the focus. A focused button consumes Return itself and keeps
    // precedence (Enter on "cancel" therefore cancels).
    Keys.onReturnPressed: startGameButton.clicked()
    Keys.onEnterPressed: startGameButton.clicked()

    // Initial focus on the main action – as when opening a dialog. Qt.callLater,
    // because the focus would otherwise fizzle out during the stack animation.
    StackView.onActivated: {
        cashBalance = Wallet ? Wallet.balance : 0
        buyIn.value = localGamePagePage.defaultBuyIn()
        errorText = ""
        Qt.callLater(startGameButton.forceActiveFocus)
    }

    // Ace's Help (GuideOverlay): the fields a tour step outlines (label + control).
    function guideTarget(key) {
        switch (key) {
        case "players": return [numPlayersLabel, numPlayers]
        case "stack":   return [buyInLabel, buyIn]
        case "blinds":  return blindsNote
        case "speed":   return [gameSpeedLabel, gameSpeed]
        case "actions": return startGameButton.parent
        }
        return null
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: scroller.width
        contentHeight: scrollContent.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {
            // AlwaysOff instead of AsNeeded: AsNeeded shows the bar transiently on
            // every contentHeight change (a fade of several seconds) –
            // visible as a scrollbar flashing up briefly while the page builds,
            // although there is nothing to scroll.
            policy: scroller.contentHeight > scroller.height + 1
                    ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        Item {
            id: scrollContent
            width: scroller.width
            // Minimum height = viewport (so that the fillHeight spacers keep the
            // vertical centring), but grow as soon as the layout content needs
            // more room → then the Flickable scrolls.
            height: Math.max(scroller.height,
                             pageColumn.implicitHeight + Config.Theme.margin * 2)

            ColumnLayout {
                id: pageColumn
                anchors.fill: parent
                anchors.margins: Config.Theme.margin
                spacing: Config.Theme.spacing

                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("Cash-Tisch")
                    font.bold: true
                    font.pixelSize: 16
                    color: Config.StaticData.palette.secondary.col200
                }

                SectionDivider {}

                Item { Layout.fillHeight: true }

                // What is in the wallet - the table is paid from here.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 8
                    color: Config.StaticData.palette.secondary.col600

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Config.Theme.margin
                        anchors.rightMargin: Config.Theme.margin
                        spacing: 8

                        AppLabel {
                            text: qsTr("Guthaben")
                            color: Config.StaticData.palette.secondary.col300
                        }
                        Item { Layout.fillWidth: true }
                        AppLabel {
                            text: "$" + localGamePagePage.cashBalance
                            color: Config.StaticData.palette.secondary.col100
                            font.bold: true
                            font.pixelSize: 18
                        }
                    }
                }

                // Settings card (corresponds to the Qt widgets newGameDialog)
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Tisch-Einstellungen")

                    GridLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10

                        // --- Buy-in (the stack he sits down with) ---
                        Label {
                            id: buyInLabel
                            text: qsTr("Buy-in:")
                            color: Config.StaticData.palette.secondary.col200
                            Layout.fillWidth: true
                        }
                        CustomSpinBox {
                            id: buyIn
                            from: localGamePagePage.buyInMin
                            to: localGamePagePage.buyInMax
                            stepSize: 10
                            value: localGamePagePage.defaultBuyIn()
                            onValueModified: localGamePagePage.errorText = ""
                        }

                        // What the chosen stack means at this table.
                        Label {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: qsTr("%1 Big Blinds ($%2 Blind, $%3)")
                                  .arg(localGamePagePage.stackInBB(buyIn.value))
                                  .arg(Wallet ? Wallet.smallBlind : 1)
                                  .arg(localGamePagePage.bigBlind)
                            color: Config.StaticData.palette.secondary.col400
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                            visible: localGamePagePage.canSit
                        }

                        // --- Number of players ---
                        Label {
                            id: numPlayersLabel
                            text: qsTr("Anzahl der Spieler:")
                            color: Config.StaticData.palette.secondary.col200
                            Layout.fillWidth: true
                        }
                        CustomSpinBox {
                            id: numPlayers
                            from: 2
                            to: 10
                            value: SettingsManager ? SettingsManager.readConfigInt("NumberOfPlayers") : 6
                            onValueModified: {
                                if (SettingsManager) SettingsManager.writeConfigInt("NumberOfPlayers", value)
                            }
                        }

                        // --- Blinds (fixed, as on every cash table) ---
                        Label {
                            id: blindsNote
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: qsTr("Blinds: $%1 / $%2, keine Erhöhung - wie an einem Cash-Tisch üblich.")
                                  .arg(Wallet ? Wallet.smallBlind : 1)
                                  .arg(localGamePagePage.bigBlind)
                            color: Config.StaticData.palette.secondary.col300
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }

                        // --- Spielgeschwindigkeit ---
                        Label {
                            id: gameSpeedLabel
                            text: qsTr("Spielgeschwindigkeit\n(1=langsam, 11=schnell):")
                            color: Config.StaticData.palette.secondary.col200
                            Layout.fillWidth: true
                        }
                        CustomSpinBox {
                            id: gameSpeed
                            from: 1
                            to: 11
                            value: SettingsManager ? SettingsManager.readConfigInt("GameSpeed") : 4
                            onValueModified: {
                                if (SettingsManager) SettingsManager.writeConfigInt("GameSpeed", value)
                            }
                        }
                    }
                }

                // No money, no seat.
                AppLabel {
                    Layout.fillWidth: true
                    visible: !localGamePagePage.canSit
                    text: qsTr("Nicht genug Guthaben für einen Platz: mindestens $%1 nötig.")
                          .arg(localGamePagePage.buyInMin)
                    color: Config.Theme.colorDanger
                    font.pixelSize: Config.Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                // Whatever stopped the last attempt (the bank refuses silently
                // otherwise - the reason stays English in the log).
                AppLabel {
                    Layout.fillWidth: true
                    visible: localGamePagePage.errorText !== ""
                    text: localGamePagePage.errorText
                    color: Config.Theme.colorDanger
                    font.pixelSize: Config.Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Config.Theme.spacing

                    CustomButton {
                        text: qsTr("Abbrechen")
                        Layout.fillWidth: true
                        onClicked: mainStackView.pop()
                    }

                    CustomButton {
                        id: startGameButton
                        text: qsTr("Spiel starten")
                        Layout.fillWidth: true
                        enabled: localGamePagePage.canSit
                        onClicked: {
                            // Double click protection: otherwise two GamePages end up in the stack.
                            if (mainStackView.currentItem
                                    && mainStackView.currentItem.objectName === "gamePage")
                                return
                            if (!GameTable.startCashGame(numPlayers.value, buyIn.value,
                                                         Wallet.smallBlind)) {
                                // The bank refused (balance changed meanwhile):
                                // show why, and refresh what the page displays.
                                localGamePagePage.errorText =
                                        qsTr("Der Platz konnte nicht gekauft werden: %1")
                                        .arg(Wallet.lastError)
                                localGamePagePage.cashBalance = Wallet.balance
                                buyIn.value = localGamePagePage.defaultBuyIn()
                                return
                            }
                            mainStackView.push("GamePage.qml")
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
