import QtQuick

// One chip of a chip flight — the small animation that makes the table read as a
// table instead of a diagram (2026-11-13):
//   • a new bet comes from the player: `GamePlayerBox` / `GamePlayerSelfBox` fly up
//     to three chips from the avatar to the pile (an arc — a chip is thrown);
//   • a betting round is collected: `GamePage` slides a few chips from EVERY
//     seat's bet into the pot and holds the pot sum back until they land;
//   • the pot is paid out: `GamePage` slides chips from the pot to every winner
//     and empties the pot when the last one has landed.
// The chips that go to/from the pot stay ON the felt: arc 0 and no spin, distance
// gives the time (player decision 2026-11-13: "по столу, не полёт").
//
// The chip is one `ChipImage` (the lying tier — the same asset as the piles) that
// travels from `from` to `to` — the CENTRES of the chip in the PARENT's coordinates
// — along a shallow arc (arc 0 = straight slide), turns a little on the way and
// fades out as it lands. In the player boxes the parent is the unscaled seat box
// (the box scale does the table scaling); in the pot layer GamePage sets
// `scale: oppScale` and the matching `renderScale` (see ChipImage for the raster
// rules).
//
// Nothing here owns time or lifetime: the owner keeps a plain list of flights as a
// Repeater model and drops the entries once `delay + duration` has passed. That
// keeps the animation stateless — a re-layout or a re-instantiation of the layer
// simply drops a chip that happened to be in the air (no orphaned items, no stuck
// state after a resize).
Item {
    id: fly

    // Centre of the chip at the start and at the end (parent coordinates).
    property point from: Qt.point(0, 0)
    property point to: Qt.point(0, 0)
    // How high the arc rises above the straight line (px); 0 = a straight slide.
    property real arc: 26
    property int chipSize: 22
    property int value: 5
    property bool cash: false
    // Effective screen scale of the parents — see ChipImage.renderScale.
    property real renderScale: 1
    // Total turn over the flight (degrees); the sign picks the direction.
    property real spin: 0
    // Flight time and the pause before it starts.
    property int duration: 340
    property int delay: 0
    // Fade-out time at the end: the chip does not stop dead, it sinks into the pile.
    property int fadeOut: 110

    width: chipSize
    height: chipSize
    // PathAnimation animates x/y (the item's top-left), so the start/end points are
    // shifted by half a chip to make `from`/`to` the centres.
    x: from.x - width / 2
    y: from.y - height / 2
    // A degenerate flight (input arrived twice, a zero-length way) draws nothing.
    visible: from.x !== to.x || from.y !== to.y

    ChipImage {
        anchors.fill: parent
        chipSize: fly.chipSize
        value: fly.value
        cash: fly.cash
        renderScale: fly.renderScale
    }

    SequentialAnimation {
        id: flight
        PauseAnimation { duration: fly.delay }
        ParallelAnimation {
            // The arc: a quadratic path from the start to the end. The control point
            // sits above the HIGHER of the two ends (Math.min — y grows downwards), so
            // a chip thrown "up" from the plate arcs over the felt and comes down on
            // the pile instead of sliding along a straight line.
            PathAnimation {
                target: fly
                duration: fly.duration
                easing.type: Easing.OutQuad
                path: Path {
                    startX: fly.from.x - fly.chipSize / 2
                    startY: fly.from.y - fly.chipSize / 2
                    PathQuad {
                        x: fly.to.x - fly.chipSize / 2
                        y: fly.to.y - fly.chipSize / 2
                        controlX: (fly.from.x + fly.to.x) / 2 - fly.chipSize / 2
                        controlY: Math.min(fly.from.y, fly.to.y)
                                  - fly.arc - fly.chipSize / 2
                    }
                }
            }
            // A slow turn: a chip is a disc, so it reads as tossed rather than
            // pushed; the faces never line up exactly with the pile below.
            NumberAnimation {
                target: fly
                property: "rotation"
                from: 0
                to: fly.spin
                duration: fly.duration
                easing.type: Easing.InOutQuad
            }
            SequentialAnimation {
                PauseAnimation { duration: Math.max(0, fly.duration - fly.fadeOut) }
                NumberAnimation {
                    target: fly
                    property: "opacity"
                    to: 0
                    duration: fly.fadeOut
                }
            }
        }
    }

    Component.onCompleted: flight.start()
}
