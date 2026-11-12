import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import "../config" as Config

// Status bar at the top of the table: on the left the pot info (total/bets), on the
// right the game phase, game ID and hand number – analogous to the Qt widgets client
// next to the community cards. Height/layout are set by the caller via Layout.*.
Rectangle {
    // Matte graphite strip: an opaque dark surface with a single hairline at the
    // lower edge instead of the old translucent black with a plastic frame.
    color: Config.Theme.tableBarSurface

    // Horizontally centred at the top: table name (network games only). For local
    // games Lobby.currentGameId == 0 → currentGameName() is empty and the display
    // disappears automatically. The binding references currentGameId so that it is
    // re-evaluated reactively when changing/joining a game.
    readonly property string tableName:
        (typeof Lobby !== "undefined" && Lobby && Lobby.currentGameId > 0)
            ? Lobby.currentGameName() : ""

    // URL of the table statistics overview of the running network table, built from
    // the live seats of the GameHandler (1:1 like the widgets client). Bound to
    // GameTable.players so that it is re-evaluated reactively on join/leave/knockout.
    // For local games it returns empty.
    readonly property string tableStatsUrl:
        (GameTable && GameTable.players) ? GameTable.tableStatsUrl() : ""

    // Opens the table ranking as a native page (XHR on the JSON endpoint of the
    // pokerth.net table view) – like the community ranking pages, no browser
    // needed. mainStackView resolves via the context chain (pattern
    // GameWaitPage), because GameStatusBar sits deep inside GamePage and the
    // StackView attached properties are not available there.
    function openTableStatsPage() {
        if (tableStatsUrl === "" || !GameTable)
            return
        // Double click protection: do not push the page twice on top of each other.
        if (mainStackView.currentItem
                && mainStackView.currentItem.objectName === "gameTableStatsPage")
            return
        mainStackView.push("../pages/GameTableStatsPage.qml", {
            nicks: GameTable.tableStatsNicks(),
            tableName: tableName,
            // Pass the settings of the running table along as a snapshot.
            gameInfo: (typeof Lobby !== "undefined" && Lobby)
                      ? Lobby.currentGameInfo() : ({})
        })
    }

    // Browser fallback (context menu): the original pokerth.net page via
    // the same AppImage safe opener as "Show Player Stats".
    function openTableStats() {
        if (tableStatsUrl !== "" && typeof Lobby !== "undefined" && Lobby)
            Lobby.openExternalUrl(tableStatsUrl)
    }

    AppText {
        id: tableNameLabel
        z: 1
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        // Do not stick into the pot/phase columns at the edges.
        width: Math.min(implicitWidth, parent.width * 0.5)
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        visible: text !== ""
        text: tableName
        color: "#FFFFFF"
        // Optical table scale: 17 px in the usual 1024×621 window (was 14).
        font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 11 : 15)
        font.weight: Font.DemiBold
        font.letterSpacing: 0.5
        readonly property bool clickable: tableStatsUrl !== ""
        font.underline: clickable && nameMouse.containsMouse
    }

    // Click area as a SIBLING of the label (not as a child of the Text) and with
    // a higher z – that way it does not depend on peculiarities of the text size and
    // lies safely above the RowLayout. Only active/clickable when a URL is present.
    MouseArea {
        id: nameMouse
        z: 2
        anchors.fill: tableNameLabel
        visible: tableNameLabel.visible
        enabled: tableNameLabel.visible && tableStatsUrl !== ""
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        // A left click opens the native page directly; a right click offers the
        // menu with the browser fallback.
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                tableNameMenu.popup()
            else
                openTableStatsPage()
        }

        Menu {
            id: tableNameMenu
            MenuItem {
                text: qsTr("Show table ranking")
                onTriggered: openTableStatsPage()
            }
            MenuItem {
                text: qsTr("Open in browser")
                onTriggered: openTableStats()
            }
        }
    }

    // Matte surface: only one hairline at the lower edge, no frame.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Config.Theme.tableBarEdge
    }

    RowLayout {
        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
        spacing: 0

        // Left: pot info (1:1 like the widget client to the left of the community cards)
        // "Total" = accumulated pot (getPot), "Bets" = running bets of this round (getSets)
        Column {
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
            spacing: 0
            Row {
                spacing: 4
                AppText {
                    text: qsTr("Total:")
                    color: "#a3adb9"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 9 : 13)
                    font.weight: Font.Medium
                }
                AppText {
                    text: GameTable ? GameTable.amount(GameTable.pot) : ""
                    color: "#b7ea2f"
                    // The pot is the loudest number on the table: 16 px (was 13).
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 10 : 14)
                    font.bold: true
                }
            }
            Row {
                spacing: 4
                AppText {
                    text: qsTr("Bets:")
                    color: "#a3adb9"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 9 : 12)
                    font.weight: Font.Medium
                }
                AppText {
                    text: GameTable ? GameTable.amount(GameTable.totalPot - GameTable.pot) : ""
                    color: "#9ecb1f"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 10 : 12)
                    font.weight: Font.DemiBold
                }
            }
        }

        // Hairline separator between the pot block and the centred table name.
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 1
            Layout.preferredHeight: parent.height * 0.5
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            color: Config.Theme.tableBarEdge
        }

        Item { Layout.fillWidth: true }

        // A clear hint that we are only spectating: otherwise the table looks
        // (apart from the missing action bar) like a normal game.
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 10
            visible: GameTable ? GameTable.spectating : false
            implicitWidth: spectatingLabel.implicitWidth + 14
            implicitHeight: spectatingLabel.implicitHeight + 6
            radius: height / 2
            color: Config.Theme.withAlpha(Config.Theme.colorAccent, 0.18)
            border.color: Config.Theme.colorAccent
            border.width: 1

            AppText {
                id: spectatingLabel
                anchors.centerIn: parent
                text: qsTr("Spectating")
                color: Config.Theme.colorAccent
                font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 9 : 12)
                font.bold: true
            }
        }

        // Spectator display (eye + count) to the left of the right-aligned
        // phase/game info. Only visible if the running game has at least
        // one spectator – analogous to the Qt widgets client. Names in the tooltip.
        Row {
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 12
            spacing: 4
            visible: GameTable ? GameTable.spectatorCount > 0 : false

            SvgIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: Config.Responsive.landscapeCompact ? 15 : 18
                height: width
                source: "../resources/eye.svg"
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: "#FFFFFF"
                }
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                text: GameTable ? GameTable.spectatorCount : 0
                color: "#FFFFFF"
                font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 10 : 14)
                font.bold: true
            }

            HoverHandler { id: spectatorHover }
            ToolTip {
                visible: spectatorHover.hovered && text.length > 0
                         && Config.Parameters.showTooltips
                text: GameTable ? GameTable.spectatorNames.join("\n") : ""
            }
        }

        // Hairline separator between the centre and the phase/hand block.
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 1
            Layout.preferredHeight: parent.height * 0.5
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            color: Config.Theme.tableBarEdge
        }

        // Right: phase + game ID + level + hand number (1:1 like the widget client to
        // the right of the community cards; the level only in a tournament).
        Column {
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            spacing: 0
            AppText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: GameTable ? GameTable.phaseText : qsTr("Preflop")
                color: "#FFFFFF"
                font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 10 : 14)
                font.weight: Font.DemiBold
                font.letterSpacing: 0.5
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                AppText {
                    // Only a network game has a meaningful game ID; a local game
                    // counts its tables from 1, which says nothing to the player.
                    visible: GameTable ? GameTable.isInternetGameRunning() : false
                    text: qsTr("Game: %1").arg(GameTable ? GameTable.gameId : 0)
                    color: "#a3adb9"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 9 : 11)
                    font.weight: Font.Medium
                }
                // A tournament plays levels: which one is running and what it
                // costs to see a flop. Chips, so no currency sign (a cash table
                // keeps the blinds of the limit it was bought at – mentioning
                // them every hand would only take room).
                AppText {
                    visible: GameTable ? GameTable.tournamentMode : false
                    text: qsTr("Level %1 · %2/%3")
                              .arg(GameTable ? GameTable.blindLevel : 0)
                              .arg(GameTable ? GameTable.smallBlind : 0)
                              .arg(GameTable ? GameTable.bigBlind : 0)
                    color: "#b7ea2f"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 10 : 11)
                    font.weight: Font.DemiBold
                }
                AppText {
                    text: qsTr("Hand: %1").arg(GameTable ? GameTable.handNumber : 1)
                    color: "#a3adb9"
                    font.pixelSize: Config.Theme.tableFont(Config.Responsive.landscapeCompact ? 9 : 11)
                    font.weight: Font.Medium
                }
            }
        }
    }
}
