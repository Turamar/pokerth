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
//
// The ladder: every limit is a step, and the balance decides which steps are
// open. A limit opens at a bankroll of 1000 BB (ten maximal buy-ins), so the
// bottom step opens at $20 - that is where the ladder starts. Moving up is a
// result of winning, not of a choice in a menu, which is why a locked limit is
// shown but not offered.
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

    // Which step of the ladder is selected; the default is the highest one the
    // balance opens.
    property int selectedLimit: -1

    readonly property var limits: Wallet ? Wallet.limits : []
    readonly property var currentLimit: (selectedLimit >= 0 && selectedLimit < limits.length)
                                        ? limits[selectedLimit] : null
    readonly property bool canSit: currentLimit !== null && currentLimit.unlocked
    readonly property int bottomGate: limits.length > 0 ? limits[0].gate : 0
    readonly property int bigBlind: currentLimit ? currentLimit.bigBlind : 0

    // The buy-in is chosen in big blinds - 20 to 100, the breadth every limit
    // has - so that the field never shows cents. The money is the stack times
    // the big blind of the limit.
    readonly property int buyInBBMin: 20
    readonly property int buyInBBMax: currentLimit
        ? Math.max(buyInBBMin,
                   Math.min(100, Math.floor(cashBalance / currentLimit.bigBlind)))
        : buyInBBMin

    // The deepest stack the balance allows is what the page offers.
    function defaultBuyInBB() {
        return localGamePagePage.buyInBBMax
    }

    function buyInChips(bigBlinds) {
        return localGamePagePage.currentLimit
                ? bigBlinds * localGamePagePage.currentLimit.bigBlind : 0
    }

    // The highest open step: that is where the player stands in the ladder.
    function bestLimit() {
        var best = -1
        for (var i = 0; i < localGamePagePage.limits.length; ++i) {
            if (localGamePagePage.limits[i].unlocked)
                best = i
        }
        return best
    }

    // Selecting a step: the property plus the radio buttons of the list. The
    // buttons are set by hand because a clicked button writes "checked" itself,
    // and a binding on it would be broken from then on.
    function selectLimit(index) {
        localGamePagePage.selectedLimit = index
        for (var i = 0; i < limitGroup.buttons.length; ++i) {
            var button = limitGroup.buttons[i]
            button.checked = (button.limitIndex === index)
        }
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
        selectLimit(bestLimit())
        buyIn.value = defaultBuyInBB()
        errorText = ""
        Qt.callLater(startGameButton.forceActiveFocus)
    }

    onSelectedLimitChanged: buyIn.value = defaultBuyInBB()

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
                            text: Wallet ? Wallet.money(localGamePagePage.cashBalance) : "–"
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

                    ButtonGroup {
                        id: limitGroup
                        exclusive: true
                    }

                    GridLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10

                        // --- The step of the ladder ---
                        Label {
                            id: limitLabel
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: qsTr("Limit (Blinds, ab wie viel Guthaben):")
                            color: Config.StaticData.palette.secondary.col200
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: localGamePagePage.limits

                            delegate: RadioButton {
                                id: limitOption
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                enabled: modelData.unlocked
                                // The index of the limit this button stands for.
                                property int limitIndex: index
                                ButtonGroup.group: limitGroup
                                onClicked: localGamePagePage.selectLimit(limitIndex)
                                Component.onCompleted: checked =
                                        (localGamePagePage.selectedLimit === limitIndex)
                                text: modelData.label
                                      + "  ·  " + qsTr("Buy-in %1–%2")
                                        .arg(Wallet.money(modelData.minBuyIn))
                                        .arg(Wallet.money(modelData.maxBuyIn))
                                      + "  ·  " + (modelData.unlocked
                                                   ? qsTr("offen")
                                                   : qsTr("ab %1").arg(Wallet.money(modelData.gate)))
                            }
                        }

                        // --- Buy-in (the stack he sits down with) ---
                        Label {
                            id: buyInLabel
                            text: qsTr("Buy-in (Big Blinds):")
                            color: Config.StaticData.palette.secondary.col200
                            Layout.fillWidth: true
                        }
                        CustomSpinBox {
                            id: buyIn
                            from: localGamePagePage.buyInBBMin
                            to: localGamePagePage.buyInBBMax
                            stepSize: 5
                            enabled: localGamePagePage.canSit
                            value: localGamePagePage.defaultBuyInBB()
                            onValueModified: localGamePagePage.errorText = ""
                        }

                        // What the chosen stack means at this table.
                        Label {
                            id: blindsNote
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: localGamePagePage.currentLimit
                                  ? qsTr("%1 Big Blinds = %2 · Blinds %3 / %4, keine Erhöhung")
                                    .arg(buyIn.value)
                                    .arg(Wallet ? Wallet.money(localGamePagePage.buyInChips(buyIn.value)) : "–")
                                    .arg(Wallet ? Wallet.money(localGamePagePage.currentLimit.smallBlind) : "–")
                                    .arg(Wallet ? Wallet.money(localGamePagePage.currentLimit.bigBlind) : "–")
                                  : ""
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

                // No money, no seat: the bottom step of the ladder needs its gate.
                AppLabel {
                    Layout.fillWidth: true
                    visible: !localGamePagePage.canSit
                    text: qsTr("Kein offenes Limit: für die unterste Stufe sind %1 nötig, dein Guthaben ist %2. Der Rebuy auf der Guthaben-Seite füllt bis dahin auf.")
                          .arg(Wallet ? Wallet.money(localGamePagePage.bottomGate) : "–")
                          .arg(Wallet ? Wallet.money(localGamePagePage.cashBalance) : "–")
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
                            if (!GameTable.startCashGame(numPlayers.value,
                                                         localGamePagePage.buyInChips(buyIn.value),
                                                         localGamePagePage.selectedLimit)) {
                                // The bank refused (balance changed meanwhile):
                                // show why, and refresh what the page displays.
                                localGamePagePage.errorText =
                                        qsTr("Der Platz konnte nicht gekauft werden: %1")
                                        .arg(Wallet.lastError)
                                localGamePagePage.cashBalance = Wallet.balance
                                localGamePagePage.selectLimit(localGamePagePage.bestLimit())
                                buyIn.value = localGamePagePage.defaultBuyInBB()
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
