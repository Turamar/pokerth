import QtQuick
import QtQuick.Effects

import "../config" as Config

// Matte graphite plate of a player box: an opaque graphite gradient, a 1 px
// slate edge and one thin specular line on the upper rim. It stays a "raised
// card" (pill radius, soft shadow), but it is deliberately matte: no gloss wash,
// no bright outline.
//
// Since 2026-11-12 the table style accent (<PlayerBoxAccent>) may NO LONGER tint
// the surface. For the style "lemming" the accent is grey (#9aa0a6) and the
// former 22–34 % tint turned the whole plate grey, "like a game from 2000"
// (player report 2026-11-10). The accent now lives only on the thin upper
// highlight – a hairline, not a fill. Without the tag the neutral theme
// highlight is used, so the bundled look stays.
Rectangle {
    id: boxBackground

    readonly property string accentName:
        (typeof StyleProvider !== "undefined" && StyleProvider)
        ? StyleProvider.playerBoxAccent : ""
    readonly property bool hasAccent: accentName !== ""
    readonly property color accent: hasAccent ? accentName : "transparent"

    // The matte graphite surface. Opaque on purpose – since 2026-11-10 the hole
    // cards lie BEHIND the plate and must not shimmer through it.
    readonly property color surfaceTop: Config.Theme.tablePlateTop
    readonly property color surfaceBottom: Config.Theme.tablePlateBottom

    anchors.fill: parent
    radius: 6
    opacity: 1.0
    gradient: Gradient {
        GradientStop { position: 0.0; color: boxBackground.surfaceTop }
        GradientStop { position: 1.0; color: boxBackground.surfaceBottom }
    }
    border.width: 1
    border.color: Config.Theme.tablePlateEdge

    // The only "gloss": a 1 px specular line on the flat part of the upper rim,
    // inset by the corner radius. A style accent may colour this hairline;
    // it never becomes a fill.
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 1
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(0, parent.width - 2 * parent.radius)
        height: 1
        radius: 0.5
        color: boxBackground.hasAccent
               ? Config.Theme.withAlpha(boxBackground.accent, 0.20)
               : Config.Theme.tablePlateHighlight
    }

    // Soft drop shadow: lift the matte plate off the felt without a hard edge.
    layer.enabled: Config.Theme.effectsEnabled
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: "#000000"
        shadowOpacity: 0.45
        shadowBlur: 1.0
        shadowVerticalOffset: 2
        shadowHorizontalOffset: 0
    }
}
