import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config

// The "Tisch" page of the settings (settings audit 2026-11-18, PLAN.md §7).
//
// It used to be the configuration of the old local game of the widgets client -
// starting cash, the first small blind, raising blinds, the manual blind order,
// the bot engine version. None of that reaches a game in this build: a cash
// table takes its blinds from the limit ladder and a tournament its structure
// from the catalogue, and the bots always run on engine 4 (engine 3 would ignore
// their calibrated strength/style). What is left are the controls that really
// affect the table: the default number of seats and the pace.
Rectangle {
    id: localGameSettings
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    color: "transparent"

    ColumnLayout {
        id: localGameSettingsContent
        anchors.fill: parent

        SettingsHeader { title: qsTr("Tisch"); topGap: 4 }

        ScrollView {
            id: localScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.leftMargin: 12
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: parent.width - 12
                spacing: 12

                // Players: the default for the spinbox on the cash page. A
                // tournament takes its seat count from its own event.
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Spieler")

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Anzahl der Spieler:")
                            color: Config.StaticData.palette.secondary.col200
                        }

                        CustomSpinBox {
                            id: numberOfPlayers
                            from: 2
                            // The common table limit, as on the cash page.
                            to: (typeof SettingsManager !== "undefined" && SettingsManager)
                                ? SettingsManager.maxPlayers : 9
                            value: Math.max(2, Math.min(
                                       (typeof SettingsManager !== "undefined" && SettingsManager)
                                       ? SettingsManager.readConfigInt("NumberOfPlayers") : 9,
                                       (typeof SettingsManager !== "undefined" && SettingsManager)
                                       ? SettingsManager.maxPlayers : 9))
                            onValueModified: {
                                if (SettingsManager) SettingsManager.writeConfigInt("NumberOfPlayers", value)
                            }
                        }
                    }
                }

                // Pace: the action clock, the pause between hands and the one
                // pacing knob of the local table - the pause before a computer
                // player's move (QmlGuiInterface::beRoAnimation2).
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Tempo")

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Zeitlimit pro Aktion\n(Sekunden, 0 = aus):")
                            color: Config.StaticData.palette.secondary.col200
                        }

                        CustomSpinBox {
                            id: actionTimer
                            from: 0
                            to: 60
                            stepSize: 5
                            value: SettingsManager ? SettingsManager.readConfigInt("ActionTimer") : 30
                            onValueModified: {
                                if (SettingsManager) SettingsManager.writeConfigInt("ActionTimer", value)
                            }
                        }

                        ConfigCheckBox {
                            Layout.columnSpan: 2
                            text: qsTr("Pause zwischen den Händen")
                            configKey: "PauseBetweenHands"
                            defaultChecked: false
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Pause vor dem Zug des Computers\n(Millisekunden):")
                            color: Config.StaticData.palette.secondary.col200
                        }

                        CustomSpinBox {
                            id: botThinkMs
                            from: 150
                            to: 900
                            stepSize: 50
                            value: {
                                if (typeof SettingsManager === "undefined" || !SettingsManager)
                                    return 300
                                var v = SettingsManager.readConfigInt("BotThinkMs")
                                return Math.max(150, Math.min(900, v > 0 ? v : 300))
                            }
                            onValueModified: {
                                if (SettingsManager) SettingsManager.writeConfigInt("BotThinkMs", value)
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
