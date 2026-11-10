.pragma library

// ─────────────────────────────────────────────────────────────────────────────
// The seat layout of the game table: the box scale and the slot positions.
//
// Pure geometry, without access to QML objects - which is why it lives here and
// no longer in GamePage.qml. All inputs come in the `env` object (see
// tableZone.layoutEnv): the zone dimensions, the seat counts, the base dimensions of the boxes and
// the relevant config switches.
//
// The central property: boxScale() looks for the LARGEST scale by bisection
// at which all boxes lie without overlapping. For that the feasibility probe evaluates
// exactly the slots that would be drawn at this scale as well - the probe
// and the drawing share buildLandscapeSlots() or buildPortraitSlots().
// ─────────────────────────────────────────────────────────────────────────────
// Desktop portrait: hard-wired slot fractions.
var slotPosPortraitFixed = ({
    "L_bottom": [0.15, 0.785],
    "L_lower":  [0.15, 0.65],
    "L_upper":  [0.15, 0.345],
    "TL":       [0.15, 0.21],
    "TC":       [0.50, 0.075],
    "TR":       [0.85, 0.21],
    "R_upper":  [0.85, 0.345],
    "R_lower":  [0.85, 0.65],
    "R_bottom": [0.85, 0.785],
    "BC":       [0.50, 0.90]
})

// The slot order depending on the number of opponents M – distributed symmetrically left/right,
// so that a circular symmetry arises independently of the number of players.
var slotSeqPortrait = ({
    1: ["TC"],
    2: ["TL", "TR"],
    3: ["TL", "TC", "TR"],
    4: ["L_upper", "TL", "TR", "R_upper"],
    5: ["L_upper", "TL", "TC", "TR", "R_upper"],
    6: ["L_lower", "L_upper", "TL", "TR", "R_upper", "R_lower"],
    7: ["L_lower", "L_upper", "TL", "TC", "TR", "R_upper", "R_lower"],
    8: ["L_bottom", "L_lower", "L_upper", "TL", "TR", "R_upper", "R_lower", "R_bottom"],
    9: ["L_bottom", "L_lower", "L_upper", "TL", "TC", "TR", "R_upper", "R_lower", "R_bottom"]
})

var slotSeqLandscape = (function() {
    var dict = {}
    for (var n = 1; n <= 9; n++) {
        var seq = []
        for (var i = 1; i <= n; i++) seq.push("opp" + i)
        dict[n] = seq
    }
    return dict
})()

// It depends on the orientation (the slot names differ), hence a
// function instead of a table.
function slotSeqSpectate(wide) {
    var base  = wide ? slotSeqLandscape : slotSeqPortrait
    var first = wide ? "opp0" : "BC"
    var dict  = {}
    dict[1] = [first]
    for (var n = 2; n <= 10; n++) {
        var rest = base[n - 1]
        if (rest) dict[n] = [first].concat(rest)
    }
    return dict
}

// The slot order of the current distribution.
function slotSeq(env) {
    return env.spectating ? slotSeqSpectate(env.wide)
                          : (env.wide ? slotSeqLandscape : slotSeqPortrait)
}

function portraitUpperSlot(name) {
    return name === "TC" || name === "TL" || name === "TR"
        || name === "L_upper" || name === "R_upper"
}

function portraitBandAt(env, s, pos, seq) {
    var width = env.width, height = env.height, wide = env.wide,
        spectating = env.spectating, seatCount = env.seatCount,
        ringCount = env.ringCount, _peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseWidth = env.selfBaseWidth, selfBaseHeight = env.selfBaseHeight,
        opponentGapBase = env.opponentGapBase,
        opponentHorizontalGapBase = env.opponentHorizontalGapBase,
        selfGapBase = env.selfGapBase, selfBadgeGapBase = env.selfBadgeGapBase,
        sideBadgeGapBase = env.sideBadgeGapBase,
        landscapeRowCount = env.landscapeRowCount,
        Config = env.Config
    var vH = oppBaseHeight * s
    var top = 4
    var bottom = spectating ? height - 4 : height - 4 - selfBaseHeight * s
    for (var i = 0; i < seq.length; ++i) {
        var p = pos[seq[i]]
        if (!p) continue
        var cy = p[1] * height
        if (portraitUpperSlot(seq[i])) {
            if (cy + vH / 2 > top) top = cy + vH / 2
        } else if (cy - vH / 2 < bottom) {
            bottom = cy - vH / 2
        }
    }
    return [top, bottom]
}

function portraitCommunityNeed(env, s) {
    var width = env.width, height = env.height, wide = env.wide,
        spectating = env.spectating, seatCount = env.seatCount,
        ringCount = env.ringCount, _peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseWidth = env.selfBaseWidth, selfBaseHeight = env.selfBaseHeight,
        opponentGapBase = env.opponentGapBase,
        opponentHorizontalGapBase = env.opponentHorizontalGapBase,
        selfGapBase = env.selfGapBase, selfBadgeGapBase = env.selfBadgeGapBase,
        sideBadgeGapBase = env.sideBadgeGapBase,
        landscapeRowCount = env.landscapeRowCount,
        Config = env.Config
    var cs = Math.max(0.55, Math.min(0.8 * s, 1.8, Math.max(0, width - 16) / 264))
    return 124 * cs + 20
}

function buildPortraitSlots(env, s, ringCnt) {
    var width = env.width, height = env.height, wide = env.wide,
        spectating = env.spectating, seatCount = env.seatCount,
        ringCount = env.ringCount, _peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseWidth = env.selfBaseWidth, selfBaseHeight = env.selfBaseHeight,
        opponentGapBase = env.opponentGapBase,
        opponentHorizontalGapBase = env.opponentHorizontalGapBase,
        selfGapBase = env.selfGapBase, selfBadgeGapBase = env.selfBadgeGapBase,
        sideBadgeGapBase = env.sideBadgeGapBase,
        landscapeRowCount = env.landscapeRowCount,
        Config = env.Config
    var W = Math.max(width, 1)
    var H = Math.max(height, 1)
    var vW = oppBaseWidth  * s
    var vH = oppBaseHeight * s
    var selfVH = spectating ? 0 : selfBaseHeight * s
    // The row spacing is deliberately LARGER than the probe distance gapP (8) of the
    // bisection: otherwise the constructed and the demanded distance lie
    // exactly on top of each other and the probe tips over at the rounding error of the
    // detour via the 0..1 slot fractions (the boxes would then fall back to the
    // scale floor of 0.55).
    var gapY = Math.max(opponentGapBase, opponentGapBase * s)
    // The columns move outwards with the box (a 4 px margin) instead of running
    // against the screen edge at fixed 15 %/85 %.
    var colX = Math.max(0.15, (4 + vW / 2) / W)
    // The rows from the top: the topmost box sticks (including its base) 4 px below
    // the zone edge, every further row one box height + air lower.
    var step = vH + gapY
    var yTC = 4 + vH / 2
    var yTop = yTC + step
    var yUpper = yTop + step
    // The lowest row: just above the self box – or, as a spectator,
    // above the bottom seat BC, which replaces the self box there.
    var yBC = H - 4 - vH / 2
    var yBottom = spectating
        ? yBC - step
        : H - 4 - selfVH - Math.max(8, selfBadgeGapBase * s) - vH / 2
    // The bottom row is only occupied from 8 ring seats on; while it
    // stays empty, the lower row itself moves to the lower stop.
    var rc = (ringCnt === undefined) ? ringCount : ringCnt
    var yLower = ((spectating ? rc - 1 : rc) >= 8) ? yBottom - step : yBottom
    // An emergency brake when the height is not enough for all rows even at the
    // scale floor (0.55): the upper and the lower group must not
    // penetrate each other – they then move apart symmetrically to the minimum
    // distance (in that case the community has no band any more, but the
    // boxes stay cleanly stacked).
    if (yLower - yUpper < step) {
        var mid = (yUpper + yLower) / 2
        yUpper = mid - step / 2
        yLower = mid + step / 2
    }
    return {
        "L_bottom": [colX,     yBottom / H],
        "L_lower":  [colX,     yLower  / H],
        "L_upper":  [colX,     yUpper  / H],
        "TL":       [colX,     yTop    / H],
        "TC":       [0.50,     yTC     / H],
        "TR":       [1 - colX, yTop    / H],
        "R_upper":  [1 - colX, yUpper  / H],
        "R_lower":  [1 - colX, yLower  / H],
        "R_bottom": [1 - colX, yBottom / H],
        "BC":       [0.50,     yBC     / H]
    }
}

function buildLandscapeSlots(env, s, ringCnt, forProbe) {
    var width = env.width, height = env.height, wide = env.wide,
        spectating = env.spectating, seatCount = env.seatCount,
        ringCount = env.ringCount, _peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseWidth = env.selfBaseWidth, selfBaseHeight = env.selfBaseHeight,
        opponentGapBase = env.opponentGapBase,
        opponentHorizontalGapBase = env.opponentHorizontalGapBase,
        selfGapBase = env.selfGapBase, selfBadgeGapBase = env.selfBadgeGapBase,
        sideBadgeGapBase = env.sideBadgeGapBase,
        landscapeRowCount = env.landscapeRowCount,
        Config = env.Config
    var rc = (ringCnt === undefined) ? ringCount : ringCnt
    var visualW = oppBaseWidth * s
    var visualH = oppBaseHeight * s
    // A spectator: no self box below the ellipse – its bottom point
    // is itself the centre of the lowest ring seat.
    var selfVisualH = spectating ? 0 : selfBaseHeight * s
    var sideMargin = Math.max(18, width * 0.025) + sideBadgeGapBase * s
    var wantedGapY = opponentGapBase * s
    var gapY = Math.max(8, wantedGapY)
    // In landscapeCompact we pull the lower half of the ellipse closer
    // to the self box: that gives the side pairs more
    // vertical room (otherwise player 7↔8 / 2↔3 visually stick
    // together). selfBadgeGapBase stays as a minimum, so that bet/
    // action badges below the bottom row do not stick into the self avatar.
    // hineinragen.
    var selfGapY = spectating ? 0
        : (Config.Responsive.landscapeCompact
           ? Math.max(8, selfBadgeGapBase * s * 0.5)
           : selfBadgeGapBase * s)
    var sideX = (sideMargin + visualW / 2) / Math.max(width, 1)
    // radiusX as large as possible (the side seats land at the edge).
    // The top trio fits into this arc segment automatically thanks to the boxScale cap
    // (see boxScale above), without us having to inflate radiusX
    // further here (otherwise the side seats slip out).
    // Cap radiusX at 0.36 → the side seats stay closer to the centre
    // in wide windows (instead of moving all the way to the
    // edge).
    var radiusX = Math.min(0.36, Math.max(0.22, 0.5 - sideX))
    // The top and bottom margins are deliberately small – the open ellipse
    // should take up as much vertical room as possible, so that
    // with a medium scale player 2↔3 (L↔TLo) get enough
    // air.
    // Compact: the topmost box flush with the upper table edge (0 instead of 4) –
    // it creates air between its bet badge and the pot badge.
    // The desktop top pad 8 (instead of 4): more distance from the topmost box to the
    // status/info bar.
    // ONLY in the probe: a reservation for a bet badge BELOW the
    // topmost box. The drawn layout does not need it (the
    // top centre box shows the bet and the puck to its LEFT/
    // RIGHT since betSplit), but the bisection deliberately keeps the room free on the
    // desktop compact and caps the boxes there
    // accordingly. On mobile devices 0, on desktop ultrawide (not
    // compact) as well - nothing hangs below there any more.
    var topBadgeExt = (forProbe && Config.Responsive.landscapeCompact
                       && !Config.Responsive.isMobile) ? 39 * s : 0
    var topY = ((Config.Responsive.landscapeCompact ? 0 : 12)
                + visualH / 2 + topBadgeExt) / Math.max(height, 1)
    var selfTop = height - 4 - selfVisualH
    var bottomY = (selfTop - selfGapY - visualH / 2) / Math.max(height, 1)
    var centerY = (topY + bottomY) / 2
    // Limit radiusY STRICTLY to the available track segment:
    // otherwise the top slot centre at `centerY - radiusY` can slip below
    // the `topY` value → the box visual is drawn beyond the tableZone
    // and overlaps the status bar. If the
    // resulting radiusY is too small for the necessary pair separation,
    // the boxScale cap (bisection) makes sure that the
    // boxes get smaller.
    var radiusY = (bottomY - topY) / 2

    // ── The pearl necklace, measured in PIXELS along the oval ────────────────
    // The seats sit on an empty oval like pearls on a rail, and their distance
    // ALONG the curve is equal. Equal ANGLES do not work: the table zone is wide
    // and shallow, the oval is flat, and with equal angles the side seats bunch
    // into a vertical wall while a wide hole stays at the top corners. That is
    // exactly the player's complaint ("player 3 sits close to 2, move him towards
    // 4"; player 7 mirrors 3).
    //
    // The self box claims an arc of its own at the bottom of the oval - as much
    // as the box needs plus half a gap on both sides. The opponents spread evenly
    // over the rest, so the ring stays symmetric to the vertical axis by
    // construction.
    var cxPx = 0.5 * width
    var cyPx = centerY * height
    var rxPx = radiusX * width
    var ryPx = radiusY * height
    var SAMPLES = 1440
    var arcTable = new Array(SAMPLES + 1)
    arcTable[0] = 0
    for (var sample = 1; sample <= SAMPLES; sample++) {
        var angleA = 2 * Math.PI * (sample - 1) / SAMPLES
        var angleB = 2 * Math.PI * sample / SAMPLES
        var stepX = rxPx * (Math.cos(angleB) - Math.cos(angleA))
        var stepY = ryPx * (Math.sin(angleB) - Math.sin(angleA))
        arcTable[sample] = arcTable[sample - 1] + Math.sqrt(stepX * stepX + stepY * stepY)
    }
    var perimeter = arcTable[SAMPLES]
    // Arc position (in px from the right-hand point, running downwards first) → point.
    function pointAtArc(distance) {
        var d = distance % perimeter
        if (d < 0) d += perimeter
        var lo = 0, hi = SAMPLES
        while (lo + 1 < hi) {
            var mid = (lo + hi) >> 1
            if (arcTable[mid] <= d) lo = mid
            else hi = mid
        }
        var span = arcTable[hi] - arcTable[lo]
        var frac = span > 0 ? (d - arcTable[lo]) / span : 0
        var angle = 2 * Math.PI * (lo + frac) / SAMPLES
        return [0.5 + radiusX * Math.cos(angle),
                centerY + radiusY * Math.sin(angle)]
    }
    // Half of the arc the self box claims at the bottom: the box itself plus the
    // neighbours' halves plus a gap. The curve is flat there, so half of its width
    // is a fair measure.
    var selfHalfArc = spectating ? 0
        : selfBaseWidth * s / 2 + visualW / 2 + Math.max(10, selfGapY * 0.5)
    var bottomArc = perimeter * 0.25   // 90° – the bottom point of the oval
    // The opponents follow the parameter (NOT seatCount): the probe computes with
    // _peakSeatCount, so that players who drop out do not change the box size,
    // while the drawing uses the current seat count. rc is the number of ring
    // seats in both cases (as a spectator including seat 0).
    var opps = Math.max(1, spectating ? rc - 1 : rc)
    var slots = {}
    if (spectating) {
        // Seat 0 goes exactly on the bottom point, the rest evenly around.
        var stepS = perimeter / (opps + 1)
        slots["opp0"] = pointAtArc(bottomArc)
        for (var sIdx = 1; sIdx <= opps; sIdx++)
            slots["opp" + sIdx] = pointAtArc(bottomArc + sIdx * stepS)
    } else {
        var stepO = (perimeter - 2 * selfHalfArc) / opps
        for (var oIdx = 1; oIdx <= opps; oIdx++)
            slots["opp" + oIdx] = pointAtArc(bottomArc + selfHalfArc + (oIdx - 0.5) * stepO)
    }
    // geom allows the probe to recover cos and vFactor from
    // [xNorm, yNorm] (x = 0.5 + radiusX·cos, y = centerY + radiusY·v).
    return { slots: slots,
             geom: { radiusX: radiusX, radiusY: radiusY, centerY: centerY } }
}

function boxScale(env) {
    var width = env.width, height = env.height, wide = env.wide,
        spectating = env.spectating, seatCount = env.seatCount,
        ringCount = env.ringCount, _peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseWidth = env.selfBaseWidth, selfBaseHeight = env.selfBaseHeight,
        opponentGapBase = env.opponentGapBase,
        opponentHorizontalGapBase = env.opponentHorizontalGapBase,
        selfGapBase = env.selfGapBase, selfBadgeGapBase = env.selfBadgeGapBase,
        sideBadgeGapBase = env.sideBadgeGapBase,
        landscapeRowCount = env.landscapeRowCount,
        Config = env.Config
    if (width <= 0 || height <= 0) return 1.0
    // Use the peak value for the scale computation, so that
    // players who drop out do not change the box size.
    var oppCnt = _peakSeatCount - 1
    // The seats on the ring (as a spectator including seat 0), see ringCount.
    var ringCnt = spectating ? _peakSeatCount : oppCnt
    var s

    // The upper limit of the box scale grows with the number of players: with few
    // players the boxes should NOT fill the whole empty table.
    // Without this cap the scale jumped to the maximum (1.4 or 1.7) with, say, only
    // two players – the self and the
    // opponent box looked far too large and their bet badges touched
    // the community cards. From 6 opponents (7+ players) on, the
    // full upper limit is reached, so the densely occupied tables stay
    // unchanged. The cap only LOWERS (never raises) and can
    // therefore create no new overlaps.
    function fillCap(maxScale, minBase) {
        var base = (minBase === undefined) ? 0.95 : minBase
        var t = Math.max(0, Math.min(1, (oppCnt - 1) / 5))
        var countCap = base + (maxScale - base) * t
        // Let few players grow along with large (maximized/fullscreen) windows:
        // without this boxScale stays capped
        // while the ellipse (a fraction of the width) spreads → in
        // fullscreen tiny boxes at the edge + an empty middle. It only grows
        // above the usual window size (√(w·h) ≈ 760 at
        // 1024×521) and only with few players (1-t); capped
        // at 2.2, so that the boxes/text do not get grotesquely large.
        // √(width·height) instead of only the height, because in
        // fullscreen landscape the problem arises above all from the WIDTH:
        // at 1920×960 the √ is ≈ 1358 instead of 960 → grow is three times as strong
        // → boxScale ~1.75 instead of ~1.28 → the boxes fill proportionally
        // to the screen. Dense tables and the start resolution stay
        // unchanged.
        var grow = (1 - t) * Math.max(0, (Math.sqrt(width * height) - 760) / 700)
        // Fully occupied tables (t≈1) with wide windows: the
        // bottom side seats (player 1/N) get more distance to their upper
        // neighbours through the growing radiusX –
        // feasibleAt() thereby allows a considerably higher boxScale than
        // with a normal window. With high bet amounts the
        // badge of player 1/N could stick into the neighbouring box. Linear from
        // 1024 px on, at most –0.15 (a cap of ≥ 1.25 for 9+ players).
        var denseShrink = t * Math.min(0.15, Math.max(0, (width - 1024) / 4000))
        return Math.min(2.2, countCap * (1 + grow) - denseShrink)
    }

    // The strategy: the box scale = the MAXIMUM that fulfils all geometric
    // constraints. That way the boxes fill the
    // available table optimally – wide windows get
    // large boxes (the vertical row fit as the upper limit),
    // narrow windows automatically get smaller boxes
    // (the slot visibility as the upper limit). No artificial
    // ref/700 floor any more, which kept wide windows small on the smaller
    // dimension.
    if (wide) {
        // The landscape cap by bisection: the maximum boxScale for which
        // ALL neighbouring ellipse seat pairs stay separated either horizontally
        // OR vertically.
        //
        // IMPORTANT: every probe point computes `radiusX`/`radiusY`
        // with exactly the same s dependent formulas as
        // `buildLandscapeSlots()`. Earlier static estimates
        // (`sideMarginBase` without an s factor, `approxRy = 0.30`)
        // underestimated the BL/BR pair demand at large s →
        // with 9–10 players and a very wide window the boxes
        // overlapped, although the old cap formula still gave
        // the green light.
        // A pure safety distance between neighbouring box pairs
        // in the bisection probe. Smaller = the boxes may pack more tightly
        // = a higher feasible boxScale (larger boxes/cards/text).
        // In landscapeCompact (small phone landscape resolutions such as the
        // iPhone mini, a height < 600) considerably more generous: there the
        // upper/side opponent pairs (player 2↔3 / 7↔8) stood so tightly that
        // they touched visually. More safety distance → the
        // bisection caps boxScale lower → smaller boxes without
        // overlaps.
        var gap = Config.Responsive.landscapeCompact ? 12 : 4
        // The ring seats in angular order. With spectators, opp0 sits
        // on the bottom point (90°) in addition, so that the
        // pair opp0↔opp1 is checked as well; the wrap pair oppN↔opp0 is
        // mirror symmetric to it and thereby covered.
        var probeSeq = []
        if (spectating) probeSeq.push("opp0")
        for (var iSeq = 1; iSeq <= (spectating ? ringCnt - 1 : ringCnt); iSeq++)
            probeSeq.push("opp" + iSeq)

        function feasibleAt(sTest) {
            if (oppCnt < 2) return true
            var visualH = oppBaseHeight * sTest
            // A spectator: there is no self box below the ellipse,
            // the lowest seat IS the bottom point of the ellipse.
            var selfVisualH = spectating ? 0 : selfBaseHeight * sTest
            // Exactly the geometry that would be drawn at this scale
            // as well - no reproduction any more, so no drift is possible.
            var built = buildLandscapeSlots(env, sTest, ringCnt, true)
            var pos  = built.slots
            var geom = built.geom
            if (geom.radiusY <= 0 || geom.radiusX <= 0) return false

            // The community cards are placed centrally into the gap between the
            // lower edge of the topmost opponent box and the upper edge of the
            // self box (see communityCenterY). This gap has to be
            // large enough for the card row + the pot badge to fit
            // in - otherwise reduce boxScale. Regular wide only;
            // in landscapeCompact the community sits by a formula of its own.
            if (!Config.Responsive.landscapeCompact) {
                // The upper edge of the lowest table content, against which the
                // community row is checked: the self box or - as a
                // spectator - the lowest ring seat itself.
                var bottomContentTop = spectating
                    ? height - 4 - visualH
                    : height - 12 - selfVisualH
                // The lowest bottom edge of the upper seats - the central
                // top box additionally counts its bet badge pointing
                // downwards (otherwise it sticks into the card row).
                var topOppBottom = -1e9
                for (var key in pos) {
                    var pC = pos[key]
                    // Compute the ellipse vector back:
                    //   x = 0.5 + radiusX·cos,  y = centerY + radiusY·v
                    var vY = (pC[1] - geom.centerY) / geom.radiusY
                    if (vY >= 0) continue          // upper seats only
                    var vX = (pC[0] - 0.5) / geom.radiusX
                    var b = pC[1] * height + visualH / 2
                          + (Math.abs(vX) < 0.25 ? sTest * 25 : 0)
                    if (b > topOppBottom) topOppBottom = b
                }
                // The total community height (card row 64 + pot badge 40 +
                // winner badge 20) · communityScale + the pad.
                if (topOppBottom > -1e9
                    && bottomContentTop - topOppBottom
                       < 0.72 * sTest * 124 + 28)
                    return false
            }

            // Pair separation: neighbouring ring seats have to have enough distance
            // either horizontally OR vertically.
            // Count in the bet badges on both sides (chip+text+spacing).
            // Without this surcharge the bisection allows scales that are too large
            // and the bet display reaches into the neighbouring box.
            var xNeeded = sTest * (oppBaseWidth + sideBadgeGapBase) + gap
            var yNeeded = sTest * oppBaseHeight + gap
            for (var iPair = 0; iPair < probeSeq.length - 1; iPair++) {
                var a = pos[probeSeq[iPair]]
                var b2 = pos[probeSeq[iPair + 1]]
                if (!a || !b2) continue
                if (Math.abs(a[0] - b2[0]) * width  < xNeeded
                    && Math.abs(a[1] - b2[1]) * height < yNeeded)
                    return false
            }
            return true
        }

        // Heads-up (1 opponent): feasibleAt() has no neighbouring pair to
        // check. Instead make sure that the community row placed centrally into the
        // gap between the top centred opponent box (incl. its bet badge pointing
        // downwards) and the self box fits
        // in – critical in flat windows. The same logic
        // as the community check in feasibleAt().
        function feasibleHeadsUp(sTest) {
            if (sTest <= 0) return false
            var visualH = oppBaseHeight * sTest
            // The upper edge of the lowest box: the self box or – as a
            // spectator – the lowest ring seat of the same size.
            var bottomContentTop = spectating
                ? height - 4 - visualH
                : height - 12 - selfBaseHeight * sTest
            var topYband = (Config.Responsive.landscapeCompact ? 0 : 4) + visualH / 2
            var topOppBottom = topYband + visualH / 2 + sTest * 25
            return bottomContentTop - topOppBottom
                   >= 0.72 * sTest * 124 + 28
        }

        // A common limit for the opponent boxes, the self box and the community badges:
        // 1.4 prevents text that is too large and bet overlaps in
        // fullscreen/maximized; compact stays at 1.7 (wider, flatter).
        // fillCap() damps the maximum with few players in addition.
        var lo = 0.55, hi = fillCap(Config.Responsive.landscapeCompact ? 2.3 : 1.9)
        if (oppCnt < 2) {
            // Go up to the (capped) hi as long as the badges do not touch the
            // community.
            if (!feasibleHeadsUp(lo)) {
                s = lo
            } else if (feasibleHeadsUp(hi)) {
                s = hi
            } else {
                for (var iterH = 0; iterH < 14; iterH++) {
                    var midH = (lo + hi) / 2
                    if (feasibleHeadsUp(midH)) lo = midH
                    else hi = midH
                }
                s = lo
            }
        } else if (!feasibleAt(lo)) {
            s = lo
        } else {
            for (var iter = 0; iter < 14; iter++) {
                var mid = (lo + hi) / 2
                if (feasibleAt(mid)) lo = mid
                else hi = mid
            }
            s = lo
        }
    } else {
        // The portrait cap by bisection (analogous to the wide screen).
        //
        // MOBILE: the probe points take their slots from
        // buildPortraitSlots(env, sTest) – exactly the function that draws
        // the layout as well. It is a pure function of the scale,
        // so there is no circularity and no second, slightly
        // deviating copy of the formula as in landscape. Only
        // the limits that really apply are left:
        //   • the screen width (the columns move with the box)
        //   • the distance of the two columns incl. the bet/puck next to them
        //   • the middle band for the community row
        // The row spacing itself is fulfilled by the construction;
        // the pair loop below stays as a safeguard.
        //
        // DESKTOP (static slots, slotPosPortraitFixed): the old wall and
        // pair constraints unchanged.
        //   • wall left/right:    the side columns x=0.15/0.85
        //   • wall at the top:    TC at y=0.075
        //   • wall at the bottom: the bottom row (L_bottom/R_bottom
        //                         at y=0.785) must not touch the self box
        //                         (bottomMargin=20).
        //   • pair separation:    neighbouring seats in
        //                         slotSeqPortrait[oppCnt] have to have enough
        //                         distance from each other either horizontally
        //                         OR vertically.
        var gapP = 8
        var mobileP = Config.Responsive.isMobile
        var seqP = (spectating ? slotSeqSpectate(wide) : slotSeqPortrait)[ringCnt] || []

        function feasibleAtP(sTest) {
            if (sTest <= 0) return false
            var visualW = oppBaseWidth * sTest
            var visualH = oppBaseHeight * sTest
            var selfVisualH = selfBaseHeight * sTest
            var posP = mobileP ? buildPortraitSlots(env, sTest, ringCnt)
                               : slotPosPortraitFixed
            // Between the two columns two boxes face each other
            // – in the style "classic" EACH carries its bet
            // towards the table centre, in the style "inset" only the puck
            // (Config.SeatStyle.betSideOutset).
            var xNeeded = mobileP
                ? sTest * (oppBaseWidth + 2 * Config.SeatStyle.betSideOutset) + gapP
                : sTest * oppBaseWidth + gapP
            var yNeeded = sTest * oppBaseHeight + gapP

            if (mobileP) {
                // Wall left/right: only the screen width any more –
                // the columns themselves move outwards with the box.
                if (visualW > width - 8) return false
                // The middle band has to carry the community row.
                var bandP = portraitBandAt(env, sTest, posP, seqP)
                if (bandP[1] - bandP[0] < portraitCommunityNeed(env, sTest))
                    return false
            } else {
                // Wand-Checks
                if (visualW > 2 * (0.15 * width - 4)) return false
                if (visualH > 2 * (0.075 * height - 4)) return false
                if (spectating) {
                    // Wall at the bottom: the BC seat (y=0.90) must not touch the lower
                    // edge. There is no self box.
                    if (0.90 * height + visualH / 2 > height - 4) return false
                } else if (oppCnt >= 8) {
                    // The self box vs. the bottom row (L_bottom/R_bottom with oppCnt>=8).
                    // seatNudge=+14 for these slots is taken into account:
                    //   self_top    = height - 4 - selfVisualH  (scale-kompensierbares bottomMargin)
                    //   bottom_kant = 0.785*height + 14 + visualH/2
                    //   distance    = 0.215*height - 18 - selfVisualH - visualH/2
                    //   constraint  = distance >= gapP  →  0.215*H - 26 - ... >= 0
                    if (0.215 * height - 26 - selfVisualH - visualH / 2 < gapP)
                        return false
                }
            }

            // Paar-Trennung
            if (seqP.length < 2) return true
            for (var i = 0; i < seqP.length - 1; i++) {
                var a = posP[seqP[i]]
                var b = posP[seqP[i + 1]]
                if (!a || !b) continue
                var dxPix = Math.abs(a[0] - b[0]) * width
                var dyPix = Math.abs(a[1] - b[1]) * height
                if (dxPix < xNeeded && dyPix < yNeeded)
                    return false
            }
            return true
        }

        // On mobile the upper limit may lie higher (and the floor of the
        // player count cap as well): with the dynamic rows
        // real geometry limits it now instead of fixed fractions, and a
        // two player table in portrait should use the room as well.
        var loP = 0.55, hiP = mobileP ? fillCap(2.0, 1.15) : fillCap(1.85)
        if (!feasibleAtP(loP)) {
            s = loP
        } else {
            for (var iterP = 0; iterP < 14; iterP++) {
                var midP = (loP + hiP) / 2
                if (feasibleAtP(midP)) loP = midP
                else hiP = midP
            }
            s = loP
        }
    }

    // A readability floor – the text/cards scale along, but must
    // not become arbitrarily small.
    return Math.max(0.55, s)
}
