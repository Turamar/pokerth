import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config

// The "Spieler" page of the settings (settings audit 2026-11-18, PLAN.md §7).
//
// Only the player themselves can be configured here: the name and the avatar of
// the own seat. The page used to have nine opponent slots as well (the widgets
// client's local game read names and avatars for the computer players). Those
// rows are gone - the bots keep the names the engine has always given them
// ("Player 1"…), and the config keys "OpponentNName/Avatar" stay untouched in
// the file, so the core keeps working exactly as before.
Rectangle {
    id: nicknameAvatarSettings
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    color: "transparent"

    ColumnLayout {
        id: nicknameAvatarSettingsContent
        anchors.fill: parent

        SettingsHeader { title: qsTr("Spieler"); topGap: 4 }

        ScrollView {
            id: nickScrollView
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

                // Mein Name / Avatar
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Mein Spieler")

                    ColumnLayout {
                        anchors.fill: parent

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.preferredWidth: 120
                                text: qsTr("Mein Nickname:")
                                color: Config.StaticData.palette.secondary.col200
                            }

                            TextField {
                                id: myNicknameField
                                Layout.fillWidth: true
                                maximumLength: 64
                                text: SettingsManager ? SettingsManager.readConfigString("MyName") : ""
                                onEditingFinished: {
                                    if (SettingsManager) SettingsManager.writeConfigString("MyName", text.trim())
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                Layout.alignment: Qt.AlignVCenter
                                radius: 6
                                color: Config.StaticData.palette.secondary.col600
                                clip: true

                                Image {
                                    id: myAvatarPreview
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    source: SettingsManager
                                            ? SettingsManager.avatarDisplayUrl(myAvatarField.text)
                                            : ""
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                    visible: status === Image.Ready
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("Mein Avatar:")
                                    color: Config.StaticData.palette.secondary.col200
                                }

                                AppText {
                                    Layout.fillWidth: true
                                    text: myAvatarField.text.length > 0
                                          ? myAvatarField.text.split("/").pop()
                                          : qsTr("Kein Avatar gewählt")
                                    font.pointSize: 10
                                    color: Config.StaticData.palette.secondary.col400
                                    elide: Text.ElideLeft
                                }
                            }

                            TextField {
                                id: myAvatarField
                                visible: false
                                text: SettingsManager ? SettingsManager.readConfigString("MyAvatar") : ""
                            }

                            ColumnLayout {
                                spacing: 4

                                Button {
                                    Layout.fillWidth: true
                                    text: qsTr("Auswählen...")
                                    onClicked: {
                                        if (!SettingsManager) return
                                        let path = SettingsManager.pickImageFile(qsTr("Avatar auswählen"))
                                        if (path) {
                                            myAvatarField.text = path
                                            SettingsManager.writeConfigString("MyAvatar", path)
                                        }
                                    }
                                }

                                Button {
                                    Layout.fillWidth: true
                                    text: qsTr("Beispiele...")
                                    onClicked: {
                                        avatarPicker.onPicked = function(path) {
                                            myAvatarField.text = path
                                            if (SettingsManager) SettingsManager.writeConfigString("MyAvatar", path)
                                        }
                                        avatarPicker.openPicker()
                                    }
                                }
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

    // Gallery of the bundled example avatars.
    ExampleAvatarPicker {
        id: avatarPicker

        property var onPicked: null

        onSelected: function(path) {
            if (onPicked) onPicked(path)
        }
    }
}
