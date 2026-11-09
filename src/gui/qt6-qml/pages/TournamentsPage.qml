import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config
import "../components"

// The tournament catalogue (PLAN.md section 3).
//
// A tournament is not a cash table with bigger blinds: the buy-in buys chips
// that never come back as chips, the blinds rise, and only the first places are
// paid. Both events here are single tables - what differs is the size and the
// payout, i.e. the question they ask: "the money is close" or "a full table and
// a long bubble".
Rectangle {
    id: tournamentsPage
    objectName: "tournamentsPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: Config.StaticData.palette.secondary.col700

    property string errorText: ""
    readonly property var events: Wallet ? Wallet.tournaments : []

    // "Platz 1: $1.56 · Platz 2: $0.84" - the paid places of one event.
    function prizeLine(prizes) {
        var parts = []
        for (var i = 0; i < prizes.length; ++i) {
            parts.push(qsTr("Platz %1: %2")
                       .arg(prizes[i].place)
                       .arg(Wallet.money(prizes[i].amount)))
        }
        return parts.join("  ·  ")
    }

    StackView.onActivated: {
        errorText = ""
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: scroller.width
        contentHeight: pageColumn.implicitHeight + Config.Theme.margin * 2
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: scroller.contentHeight > scroller.height + 1
                    ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        ColumnLayout {
            id: pageColumn
            width: scroller.width
            anchors.margins: Config.Theme.margin
            spacing: Config.Theme.spacing

            AppLabel {
                Layout.fillWidth: true
                text: qsTr("Turniere")
                font.bold: true
                font.pixelSize: 16
                color: Config.StaticData.palette.secondary.col200
            }

            SectionDivider {}

            // What is in the wallet - the buy-in is paid from here.
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
                        text: Wallet ? Wallet.money(Wallet.balance) : "–"
                        color: Config.StaticData.palette.secondary.col100
                        font.bold: true
                        font.pixelSize: 18
                    }
                }
            }

            Repeater {
                model: tournamentsPage.events

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: cardColumn.implicitHeight + Config.Theme.margin * 2
                    radius: 8
                    color: Config.StaticData.palette.secondary.col600

                    ColumnLayout {
                        id: cardColumn
                        anchors.fill: parent
                        anchors.margins: Config.Theme.margin
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AppLabel {
                                Layout.fillWidth: true
                                text: modelData.label + "  ·  " + modelData.seats + " " + qsTr("Plätze")
                                color: Config.StaticData.palette.secondary.col100
                                font.bold: true
                                font.pixelSize: 15
                            }

                            CustomButton {
                                text: qsTr("Spielen")
                                enabled: modelData.unlocked
                                onClicked: {
                                    // Double click protection: otherwise two GamePages end up in the stack.
                                    if (mainStackView.currentItem
                                            && mainStackView.currentItem.objectName === "gamePage")
                                        return
                                    if (!GameTable.startTournament(modelData.key)) {
                                        // The bank refused: the reason is English
                                        // (engine diagnostics, "Wallet: ..." in the
                                        // log), so the player gets the plain fact
                                        // instead of a second language on screen.
                                        tournamentsPage.errorText =
                                                qsTr("Der Platz konnte nicht gekauft werden.")
                                        return
                                    }
                                    mainStackView.push("GamePage.qml")
                                }
                            }
                        }

                        AppLabel {
                            Layout.fillWidth: true
                            text: modelData.note
                            color: Config.StaticData.palette.secondary.col300
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }

                        AppLabel {
                            Layout.fillWidth: true
                            text: qsTr("Buy-in %1  ·  %2 BB  ·  Level alle %3 Hände  ·  Feld: %4")
                                  .arg(Wallet.money(modelData.buyIn))
                                  .arg(modelData.stackInBB)
                                  .arg(modelData.levelHands)
                                  .arg(modelData.field)
                            color: Config.StaticData.palette.secondary.col400
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }

                        AppLabel {
                            Layout.fillWidth: true
                            text: tournamentsPage.prizeLine(modelData.prizes)
                            color: Config.StaticData.palette.secondary.col300
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }

                        AppLabel {
                            Layout.fillWidth: true
                            visible: !modelData.unlocked
                            text: qsTr("Nicht genug Guthaben: nötig sind %1.")
                                  .arg(Wallet.money(modelData.buyIn))
                            color: Config.Theme.colorDanger
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Whatever stopped the last attempt (the bank refuses silently
            // otherwise - the reason stays in the log).
            AppLabel {
                Layout.fillWidth: true
                visible: tournamentsPage.errorText !== ""
                text: tournamentsPage.errorText
                color: Config.Theme.colorDanger
                font.pixelSize: Config.Theme.fontSizeCaption
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            CustomButton {
                Layout.fillWidth: true
                text: qsTr("Zurück")
                onClicked: mainStackView.pop()
            }
        }
    }
}
