import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config
import "../components"

// The "Benutzeroberfläche" page of the settings. After the settings audit of
// 2026-11-18 (PLAN.md §7) it holds only controls that actually do something in
// this single-player build: the chat translation/time-stamp, emoji reactions and
// the network ping colour were removed together with the "Netzwerk" tab (its
// two live switches moved into this list).
Rectangle {
    id: guiSettings
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    color: "transparent"

    ColumnLayout {
        id: guiSettingsContent
        anchors.fill: parent

        SettingsHeader { title: qsTr("Benutzeroberfläche") }

        ScrollView {
            id: generalTab
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: parent.width

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.topMargin: 16

                    Label {
                        text: qsTr("Dark Mode:")
                        color: Config.StaticData.palette.secondary.col200
                        font.pointSize: 12
                    }

                    ComboBox {
                        id: darkModeSelector
                        model: [qsTr("Automatisch"), qsTr("Hell"), qsTr("Dunkel")]
                        // Config: 0=Hell, 1=Dunkel, 2=Auto → Index: 0=Auto, 1=Hell, 2=Dunkel
                        Component.onCompleted: {
                            if (SettingsManager) {
                                var v = SettingsManager.readConfigInt("DarkMode")
                                currentIndex = (v === 2) ? 0 : (v === 0) ? 1 : 2
                            }
                        }
                        onActivated: {
                            if (SettingsManager) {
                                var cfgVal = (currentIndex === 0) ? 2 : (currentIndex === 1) ? 0 : 1
                                SettingsManager.writeConfigInt("DarkMode", cfgVal)
                                Config.StaticData.darkMode = cfgVal
                                Config.Theme.darkMode = cfgVal
                                // The lobby chat history carries its colours as
                                // placeholders; without this signal it would stay in
                                // the colours of the old mode (light text on a light
                                // background).
                                if (typeof Lobby !== "undefined" && Lobby)
                                    Lobby.refreshChatColors()
                            }
                        }
                    }
                }

                RowLayout {
                    id: language
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.topMargin: 4

                    Label {
                        Layout.preferredHeight: 24
                        Layout.fillHeight: false
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Sprache:")
                        color: Config.StaticData.palette.secondary.col200
                        font.pointSize: 12
                    }

                    ComboBox {
                        id: languageSelector
                        model: Config.StaticData.languages
                        textRole: "langName"
                        Component.onCompleted: {
                            var currentCode = Config.Parameters.language
                            for (var i = 0; i < model.length; ++i) {
                                if (model[i].code === currentCode) {
                                    currentIndex = i
                                    return
                                }
                            }
                        }
                        onActivated: {
                            var code = model[currentIndex].code
                            Config.Parameters.language = code
                            LanguageManager.switchLanguage(code)   // user interface right away
                            // Write the authoritative value into the ConfigFile (short
                            // code, so that the widgets client finds its .qm).
                            // The chat translation reads the key live -> takes effect immediately.
                            if (typeof SettingsManager !== "undefined" && SettingsManager)
                                SettingsManager.language =
                                    Config.StaticData.localeToConfigLanguage(code)
                        }
                    }
                }

                AppCheckBox {
                    objectName: "reduceEffectsCheckbox"
                    text: qsTr("Grafikeffekte reduzieren (Schatten/Glow) – für schwache Systeme")
                    checked: SettingsManager ? SettingsManager.readConfigInt("QmlReduceEffects") !== 0 : false
                    onToggled: {
                        if (SettingsManager) SettingsManager.writeConfigInt("QmlReduceEffects", checked ? 1 : 0)
                        Config.Theme.effectsEnabled = !checked
                    }
                }

                ConfigCheckBox {
                    objectName: "fadeOutLosingCardsAnimationCheckbox"
                    text: qsTr("Ausblend-Animation für Verliererkarten")
                    configKey: "ShowFadeOutCardsAnimation"
                }

                ConfigCheckBox {
                    objectName: "animatedCardsCheckbox"
                    text: qsTr("Animierte Karten (Aufdeck-Animation)")
                    configKey: "ShowFlipCardsAnimation"
                }

                ConfigCheckBox {
                    objectName: "reverseFKeysOrderCheckbox"
                    text: qsTr("Alternative F-Tasten-Belegung (F1-F4)")
                    configKey: "AlternateFKeysUserActionMode"
                    defaultChecked: false
                }

                ConfigCheckBox {
                    objectName: "showPotPercentButtonsCheckbox"
                    text: qsTr("Pot-Prozent-Schaltflächen anzeigen")
                    configKey: "ShowPotPercentButtons"
                }

                ConfigCheckBox {
                    objectName: "showOwnCardsOnMouseClickCheckbox"
                    text: qsTr("Anti-Peek: Eigene Karten erst bei Klick anzeigen")
                    configKey: "AntiPeekMode"
                    defaultChecked: false
                }

                // The current hand of the player on the table (flop/turn/river)
                // as a calm dark pill – off by default (PLAN.md §6, 2026-11-10).
                ConfigCheckBox {
                    objectName: "showCurrentHandNameCheckbox"
                    text: qsTr("Immer die aktuelle Hand anzeigen")
                    configKey: "ShowCurrentHandName"
                    defaultChecked: false
                }

                AppCheckBox {
                    objectName: "disableSplashScreenOnStartupCheckbox"
                    text: qsTr("Startbildschirm beim Startvorgang deaktivieren")
                    checked: SettingsManager ? SettingsManager.disableSplashScreen : false
                    onToggled: { if (SettingsManager) SettingsManager.disableSplashScreen = checked }
                }

                AppCheckBox {
                    objectName: "doNotTranslatePokerTermsCheckbox"
                    text: qsTr("Internationale Pokerausdrücke (Check, Call, Raise) nicht übersetzen")
                    checked: SettingsManager ? SettingsManager.readConfigInt("DontTranslateInternationalPokerStringsFromStyle") !== 0 : false
                    onToggled: { if (SettingsManager) SettingsManager.writeConfigInt("DontTranslateInternationalPokerStringsFromStyle", checked ? 1 : 0) }
                }

                AppCheckBox {
                    objectName: "showTooltipsCheckbox"
                    visible: !Config.Responsive.isMobile
                    height: visible ? implicitHeight : 0
                    text: qsTr("Tooltips anzeigen")
                    checked: Config.Parameters.showTooltips
                    onCheckedChanged: Config.Parameters.showTooltips = checked
                }

                AppCheckBox {
                    visible: Config.Responsive.compact
                    height: visible ? implicitHeight : 0
                    text: qsTr("Tischzoom aktivieren (Wischen & Zoomen, nur Mobilmodus)")
                    checked: Config.Parameters.tableZoomEnabled
                    onCheckedChanged: Config.Parameters.tableZoomEnabled = checked
                }

                AppCheckBox {
                    objectName: "keepEmptySeatsCheckbox"
                    text: qsTr("Plätze verlassener Spieler am Tisch freihalten (verbleibende Spielerboxen bleiben an ihrem Platz)")
                    checked: Config.Parameters.keepEmptySeats
                    onCheckedChanged: Config.Parameters.keepEmptySeats = checked
                }

                // The two switches that used to live in the "Netzwerk" tab; both
                // act on the LOCAL action bar as well.
                ConfigCheckBox {
                    objectName: "focusBetInputOnTurnCheckbox"
                    text: qsTr("Fokus ins Einsatz-Eingabefeld setzen, wenn Sie an der Reihe sind")
                    configKey: "EnableBetInputFocusSwitch"
                    defaultChecked: false
                }

                ConfigCheckBox {
                    objectName: "preventAccidentalCallAfterBigRaiseCheckbox"
                    text: qsTr("Versehentliches Call nach einem großen Raise verhindern")
                    configKey: "AccidentallyCallBlocker"
                }
            }
        }
    }
}
