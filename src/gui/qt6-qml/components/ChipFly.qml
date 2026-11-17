import QtQuick

// One chip of a chip flight — the small animation that makes the table read as a
// table instead of a diagram (2026-11-13):
//   • a new bet comes from the player: `GamePlayerBox` / `GamePlayerSelfBox` fly up
//     to three chips from the avatar to the pile — a THROW (`glide` false, an arc);
//   • a betting round is collected: `GamePage` GLIDES a few chips from EVERY seat's
//     bet into the pot and holds the pot sum back until they land.
// The chips that go to the pot stay ON the felt: no arc, no spin, a straight even
// glide (player decision 2026-11-13 "по столу, не полёт"; refined 2026-11-18: "то же
// самое плавное движение, а не полёт низкой дугой" — the old path bowed upwards even
// with arc 0, because the control point sat on the HIGHER end instead of the middle).
//
// The chip is one `ChipImage` (the lying tier — the same asset as the piles) that
// travels from `from` to `to` — the CENTRES of the chip in the PARENT's coordinates
// — along a quadratic path and fades out as it lands. `glide` false: the control
// point lifts the path above the higher end (`arc`), i.e. a chip is thrown. `glide`
// true: the control point sits on the middle of the segment, the path is a perfectly
// straight line on the felt, and the easing is symmetric (`InOutSine`) instead of the
// throw's `OutQuad`.
//
// It is NOT turned on the way (2026-11-17): a chip on the felt can only turn about
// the vertical axis, and a turn is a frame of the whole asset (face + rim together),
// not a rotation of the picture — and the toss no longer spins at all (the player
// kept the turns on the table, where they give the piles life, and dropped them in
// flight).
//
// SIZE: a chip that travels between the seats and the pot crosses two different
// table scales — a bet is drawn at the boxes' scale (`oppScale`), the pot at the
// cards' scale (`communityScale`), and on a small table those differ by up to 30 %.
// The chip therefore starts at `scaleFrom` and ends at `scaleTo` (2026-11-18): it
// matches the pile it leaves and the pile it lands in, instead of jumping in size
// at one of the ends.
//
// In the player boxes the parent is the unscaled seat box (the box scale does the
// table scaling); in the pot layer GamePage sets the two scales and the matching
// `renderScale` (see ChipImage for the raster rules).
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
    // How high the arc rises above the straight line (px); 0 = no arc. Only a THROW
    // (`glide` false) uses it — a glide is always a straight line on the felt.
    property real arc: 26
    // true = slide along the felt (a straight, symmetric glide); false = throw (an arc,
    // fast start). The bet fly-in from the plate is a throw, the collect is a glide.
    property bool glide: false
    property int chipSize: 22
    property int value: 5
    property bool cash: false
    // Effective screen scale of the parents — see ChipImage.renderScale.
    property real renderScale: 1
    // Scale of the pile the chip takes off from and of the one it lands in.
    property real scaleFrom: 1
    property real scaleTo: 1
    // Flight time and the pause before it starts.
    property int duration: 340
    property int delay: 0
    // Fade-out time at the end: the chip does not stop dead, it sinks into the pile.
    property int fadeOut: 110

    width: chipSize
    height: chipSize
    // The animation below takes `scale` over; transformOrigin is the centre, so the
    // chip stays centred on the point the path animates.
    scale: scaleFrom
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
            // The path: a quadratic from the start to the end. `glide` puts the control
            // point on the MIDDLE of the segment — a quadratic Bézier with its control
            // point on the segment IS that segment, so the chip slides straight over the
            // felt. A throw lifts the control point above the HIGHER end (Math.min — y
            // grows downwards), so a chip thrown "up" from the plate arcs over the felt
            // and comes down on the pile.
            PathAnimation {
                target: fly
                duration: fly.duration
                easing.type: fly.glide ? Easing.InOutSine : Easing.OutQuad
                path: Path {
                    startX: fly.from.x - fly.chipSize / 2
                    startY: fly.from.y - fly.chipSize / 2
                    PathQuad {
                        x: fly.to.x - fly.chipSize / 2
                        y: fly.to.y - fly.chipSize / 2
                        controlX: (fly.from.x + fly.to.x) / 2 - fly.chipSize / 2
                        controlY: fly.glide
                                  ? (fly.from.y + fly.to.y) / 2 - fly.chipSize / 2
                                  : Math.min(fly.from.y, fly.to.y)
                                    - fly.arc - fly.chipSize / 2
                    }
                }
            }
            // Size: from the scale of the pile it leaves to the scale of the pile it
            // lands in (a bet and the pot are drawn at different table scales).
            NumberAnimation {
                target: fly
                property: "scale"
                to: fly.scaleTo
                duration: fly.duration
                easing.type: fly.glide ? Easing.InOutSine : Easing.OutQuad
            }
            // Fade-out: the chip does not stop dead, it sinks into the pile.
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
