import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts

import "../config" as Config
import "../components"

// The wallet: the permanent balance, the profile counters and the rebuy
// (PLAN.md section 2). It is reached from the balance in the top bar; the back
// caret returns to the page that was open before.
Rectangle {
    id: walletPage
    objectName: "walletPage"
    Layout.fillWidth: true
    Layout.fillHeight: true
    color: Config.StaticData.palette.secondary.col700

    // "m:ss" for the rebuy cooldown.
    function remainingText(seconds) {
        if (seconds <= 0)
            return qsTr("jetzt")
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return m + ":" + (s < 10 ? "0" + s : s)
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: scroller.width
        contentHeight: scrollContent.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: scroller.contentHeight > scroller.height + 1
                    ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        Item {
            id: scrollContent
            width: scroller.width
            height: Math.max(scroller.height,
                             pageColumn.implicitHeight + Config.Theme.margin * 2)

            ColumnLayout {
                id: pageColumn
                anchors.fill: parent
                anchors.margins: Config.Theme.margin
                spacing: Config.Theme.spacing

                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("Geldbeutel")
                    font.bold: true
                    font.pixelSize: 16
                    color: Config.StaticData.palette.secondary.col200
                }

                SectionDivider {}

                Item { Layout.fillHeight: true }

                // ── Balance ────────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: balanceColumn.implicitHeight + Config.Theme.margin * 2
                    radius: 8
                    color: Config.StaticData.palette.secondary.col600

                    ColumnLayout {
                        id: balanceColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Config.Theme.margin
                        spacing: 6

                        AppLabel {
                            Layout.fillWidth: true
                            text: Wallet ? ("$" + Wallet.balance) : "–"
                            color: Config.StaticData.palette.secondary.col100
                            font.pixelSize: 32
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        AppLabel {
                            Layout.fillWidth: true
                            text: qsTr("Guthaben")
                            color: Config.StaticData.palette.secondary.col300
                            font.pixelSize: Config.Theme.fontSizeCaption
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Config.Theme.spacing

                    StatTile {
                        label: qsTr("Hände gespielt")
                        value: Wallet ? Wallet.handsPlayed : "–"
                    }
                    StatTile {
                        label: qsTr("Hände gewonnen")
                        value: Wallet ? Wallet.handsWon : "–"
                    }
                    StatTile {
                        label: qsTr("Spieler")
                        value: Wallet ? Wallet.playerName : ""
                    }
                }

                // ── Rebuy ──────────────────────────────────────────────────
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Rebuy")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        CustomButton {
                            Layout.fillWidth: true
                            text: qsTr("Rebuy $%1 nehmen").arg(Wallet ? Wallet.rebuyAmount : 20)
                            enabled: Wallet ? Wallet.canRebuy : false
                            onClicked: Wallet.rebuy()
                        }

                        AppLabel {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Config.StaticData.palette.secondary.col300
                            font.pixelSize: Config.Theme.fontSizeCaption
                            text: {
                                if (!Wallet)
                                    return ""
                                if (Wallet.canRebuy)
                                    return qsTr("Das Guthaben ist unter $%1 – du kannst $%2 nachlegen.")
                                           .arg(Wallet.rebuyBelow).arg(Wallet.rebuyAmount)
                                if (Wallet.atTable)
                                    return qsTr("Nicht am Tisch möglich – verlasse zuerst das Spiel.")
                                if (Wallet.rebuySecondsLeft > 0)
                                    return qsTr("Nächster Rebuy in %1.")
                                           .arg(walletPage.remainingText(Wallet.rebuySecondsLeft))
                                return qsTr("Der Rebuy wird angeboten, wenn das Guthaben unter $%1 liegt.")
                                       .arg(Wallet.rebuyBelow)
                            }
                        }
                    }
                }

                // ── A note (e.g. an interrupted session was returned) ──────
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: noticeRow.implicitHeight + 16
                    visible: Wallet && Wallet.notice !== ""
                    radius: 6
                    color: Config.Theme.colorBox
                    border.color: Config.StaticData.palette.secondary.col400
                    border.width: 1

                    RowLayout {
                        id: noticeRow
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        AppLabel {
                            Layout.fillWidth: true
                            text: Wallet ? Wallet.notice : ""
                            color: Config.StaticData.palette.secondary.col200
                            font.pixelSize: Config.Theme.fontSizeCaption
                            wrapMode: Text.WordWrap
                        }
                        CustomButton {
                            text: qsTr("OK")
                            onClicked: Wallet.clearNotice()
                        }
                    }
                }

                // ── A problem with the state file ──────────────────────────
                AppLabel {
                    Layout.fillWidth: true
                    visible: Wallet && Wallet.stateProblem !== ""
                    text: qsTr("Der Spielstand kann nicht gespeichert werden: %1")
                          .arg(Wallet ? Wallet.stateProblem : "")
                    color: Config.Theme.colorDanger
                    font.pixelSize: Config.Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                // ── Where the money lives ──────────────────────────────────
                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("Statusdatei: %1").arg(Wallet ? Wallet.stateFilePath : "")
                    color: Config.StaticData.palette.secondary.col400
                    font.pixelSize: Config.Theme.fontSizeCaption
                    elide: Text.ElideMiddle
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
}
