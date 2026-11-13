import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import "../config" as Config

// The round avatar of a seat. It sits on the RIGHT END of the player plate and
// is a little larger than the plate is high, so that it stands out of it – the
// profile is the anchor of the box, the cards hide behind the plate.
//
// On the right half of the table the plate is mirrored (decision 2026-11-12) and
// the avatar then hangs on its LEFT end; the picture itself is flipped as well
// (`mirrored`), so the face looks towards the centre of the table. Only the
// picture is mirrored – the network dot and its tooltip keep their corner.
//
// The network status dot (self box, setting ShowPingStateInAvatar) and the
// mouseover overlay with the server response times live here as before.
Item {
    id: root

    property string source: ""
    // Flip the picture horizontally (mirrored plate, see GamePlayerBox.mirrored).
    property bool mirrored: false
    // Out of the game → desaturate the avatar.
    property bool playerActive: true
    property bool showNetworkStatus: false
    property color networkStatusColor: "transparent"
    property int networkPingAvg: -1
    property int networkPingMin: -1
    property int networkPingMax: -1

    implicitWidth: 48
    implicitHeight: 48
    width: implicitWidth
    height: implicitHeight

    // Clipping to the round shape: the photo fills the circle.
    Rectangle {
        id: rim
        anchors.fill: parent
        radius: width / 2
        color: Config.Theme.colorBox
        border.width: 2
        border.color: Qt.rgba(1, 1, 1, 0.22)
        clip: true

        Image {
            anchors.fill: parent
            anchors.margins: 2
            fillMode: root.source !== "" ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            source: root.source !== "" ? root.source : "qrc:resources/pokerth.svg"
            asynchronous: true
            cache: true
            // Mirrored plate: the face looks towards the centre of the table.
            mirror: root.mirrored
            layer.enabled: !root.playerActive
            layer.effect: MultiEffect { saturation: -1.0 }
        }
    }

    // Network status dot (traffic light) at the bottom right of the avatar. On the
    // desktop a mouseover shows an overlay with the server response times
    // (avg/min/max ms) – as in the Qt widgets client.
    Rectangle {
        id: netDot
        visible: root.showNetworkStatus
        width: Math.max(6, Math.round(root.width * 0.22))
        height: width
        radius: width / 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        color: root.networkStatusColor
        border.width: 1
        border.color: Qt.darker(root.networkStatusColor, 2.0)

        // A more generous hover area than the small dot itself.
        MouseArea {
            id: netHover
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: !Config.Responsive.isMobile
            acceptedButtons: Qt.NoButton
        }

        ToolTip.visible: netHover.containsMouse
                         && !Config.Responsive.isMobile
                         && Config.Parameters.showTooltips
                         && root.networkPingAvg >= 0
        ToolTip.delay: 300
        ToolTip.text: qsTr("Server response times")
                      + "\n" + qsTr("Average: %1 ms").arg(root.networkPingAvg)
                      + "\n" + qsTr("Minimum: %1 ms").arg(root.networkPingMin)
                      + "\n" + qsTr("Maximum: %1 ms").arg(root.networkPingMax)
    }
}
