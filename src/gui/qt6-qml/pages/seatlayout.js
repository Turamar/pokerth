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
// The semi-axis ratio (x : y) of the landscape oval the seats sit on. The player
// asked for a "slightly elongated oval" (PLAN §6.4); a poker table is wider than
// tall, but the ring must not degenerate into a horizontal band with vertical side
// walls – which is exactly what the old, very flat ellipse (radiusX 0.36·width
// against radiusY limited to the strip above the self box, ratio ≈ 2.7–3) looked
// like on 9–10 seats. The oval is derived from the available height, so the ratio
// is what the shape is built from; the zone width can only make it rounder.
var landscapeOvalAspect = 2.0

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

// The angular order of the landscape ring: slot name of the 1st, 2nd, … seat
// counted counter-clockwise from the place right of the self box. The NAMES are
// rotated by half a ring – not the seats: `GamePage.seatNudge` (flankWide) pulls
// the two ring seats named `opp1` / `oppN` down and OUTWARDS into the corners
// whenever they sit in the lower half – a leftover of the old flat ring, which had
// to squeeze the bottom pair past the self box. On the oval the bottom pair is part
// of the arc and there are no corners any more, so the numbering starts half a ring
// higher: those two names then live near the TOP vertex, where that nudge can never
// fire. The seat → position mapping stays the natural one (seat 1 = first place
// after the self box), because slotSeq() below returns this same order.
function landscapeRingName(n, i) {
    return "opp" + (((i + Math.floor(n / 2)) % n) + 1)
}

var slotSeqLandscape = (function() {
    var dict = {}
    for (var n = 1; n <= 9; n++) {
        var seq = []
        for (var i = 0; i < n; i++) seq.push(landscapeRingName(n, i))
        dict[n] = seq
    }
    return dict
})()

// It depends on the orientation (the slot names differ), hence a
// function instead of a table.
// Watching at the table there is no self box: all seats sit on the same oval and
// are numbered in angular order – no rotated names here (the nudge above is
// switched off for spectators anyway), the bottom point is an ordinary seat.
function slotSeqSpectate(wide) {
    var dict  = {}
    dict[1] = [wide ? "opp0" : "BC"]
    for (var n = 2; n <= 10; n++) {
        var seq = [wide ? "opp0" : "BC"]
        if (wide) {
            for (var i = 1; i < n; i++) seq.push("opp" + i)
        } else {
            var rest = slotSeqPortrait[n - 1]
            if (!rest) continue
            seq = seq.concat(rest)
        }
        dict[n] = seq
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
    var width = env.width, height = env.height,
        spectating = env.spectating, ringCount = env.ringCount,
        peakSeatCount = env.peakSeatCount,
        oppBaseWidth = env.oppBaseWidth, oppBaseHeight = env.oppBaseHeight,
        selfBaseHeight = env.selfBaseHeight,
        Config = env.Config
    var W = Math.max(width, 1)
    var H = Math.max(height, 1)
    var rc = (ringCnt === undefined) ? ringCount : ringCnt
    var visualW = oppBaseWidth * s
    var visualH = oppBaseHeight * s
    // A spectator: no self box below the oval – the bottom point of the oval is
    // itself the centre of the lowest ring seat.
    var selfVisualH = spectating ? 0 : selfBaseHeight * s
    // Air between a plate and the zone border: the plate always lies inside the
    // zone with at least 8 px (the player's minimum, PLAN §6.4).
    var edge = 8
    // ONLY in the probe: a reservation for a bet badge BELOW the topmost box. The
    // drawn layout does not need it (the top centre box shows the bet and the puck
    // to its LEFT/RIGHT since betSplit), but the bisection deliberately keeps the
    // room free on desktop compact and caps the boxes there accordingly.
    var topBadgeExt = (forProbe && Config.Responsive.landscapeCompact
                       && !Config.Responsive.isMobile) ? 39 * s : 0
    // The top vertex of the oval: the centre of the topmost plate.
    var topPx = edge + visualH / 2 + topBadgeExt
    // The bottom vertex of the oval: the centre of the self box. The player is thus
    // the lowest seat of the ring, and the bottom pair of opponents flanks it on the
    // bottom arc. (The old ring stopped ABOVE the self box and was therefore a flat
    // band as soon as the box scale grew.) A spectator has no self box: there the
    // lowest ring seat sits on the bottom point with the same edge.
    var bottomPx = spectating
        ? H - edge - visualH / 2
        : H - 12 - selfVisualH / 2
    var cyPx = (topPx + bottomPx) / 2
    var ryPx = (bottomPx - topPx) / 2
    if (ryPx < 1) ryPx = 1
    // The shape is derived from the PEAK seat count, not from the current one: the
    // angle the self box cuts out of the oval is the smallest it ever gets then, so
    // its lowest possible seat is inside the zone – and the oval keeps its shape
    // while players drop out (only the seats redistribute on it).
    var refSeats = Math.max(2, Math.max(spectating ? rc : rc + 1,
                                        peakSeatCount || 1))
    // The lowest seat of a refSeats distribution sits 2π/refSeats away from the
    // bottom vertex: its plate has to stay inside the zone as well.
    var bottomSeatCos = Math.cos(2 * Math.PI / refSeats)
    var maxRy = (H - edge - topPx - visualH / 2) / (1 + bottomSeatCos)
    if (maxRy > 0 && ryPx > maxRy) ryPx = maxRy

    // ── The oval: slightly elongated, one sector per seat ─────────────────────
    // The player's complaint ("the seats and the player must form a slightly
    // elongated oval, now it is unclear what", PLAN §6.4) came from the flat ring:
    // radiusX was pinned at 0.36 · width, radiusY was cut off by the strip above the
    // self box (ratio ≈ 2.7–3), the seats were spaced along the ARC – which on such
    // an ellipse produces a level top row, vertical side walls and two plates pulled
    // into the bottom corners by GamePage.seatNudge. Read as a rectangle.
    //
    // Now the oval is the starting point, not the leftover: the semi-axes keep the
    // ratio landscapeOvalAspect, and the seats are placed at EQUAL ANGLES – one
    // sector per seat, the self box takes the bottom sector (the bottom vertex), the
    // opponents the rest. Equal angles need a ratio near the recommended one: on a
    // flat ellipse they would bunch at the ends, on the oval they give the classic
    // even ring. Symmetry to the vertical axis holds by construction, because the
    // seats are mirrored around the bottom vertex.
    var rxPx = Math.min(landscapeOvalAspect * ryPx,
                        Math.max(1, W / 2 - visualW / 2 - edge))
    var slots = {}
    if (spectating) {
        // No self box: all ring seats (seat 0 included) at equal angles, seat 0 on
        // the bottom point (that is what selfVisualTopY looks up in GamePage).
        var mS = Math.max(1, rc)
        for (var iS = 0; iS < rc; iS++) {
            var angS = Math.PI / 2 + iS * 2 * Math.PI / mS
            slots["opp" + iS] = [0.5 + rxPx * Math.cos(angS) / W,
                                 (cyPx + ryPx * Math.sin(angS)) / H]
        }
    } else {
        var nOpp = Math.max(1, rc)
        var mO = nOpp + 1
        // The rotated angular order – see landscapeRingName()/slotSeqLandscape.
        var seqO = slotSeqLandscape[nOpp] || []
        for (var iO = 0; iO < nOpp; iO++) {
            var angO = Math.PI / 2 + (iO + 1) * 2 * Math.PI / mO
            slots[seqO[iO] || ("opp" + (iO + 1))] =
                [0.5 + rxPx * Math.cos(angO) / W,
                 (cyPx + ryPx * Math.sin(angO)) / H]
        }
    }
    // geom allows the probe to recover cos and sin from
    // [xNorm, yNorm] (x = 0.5 + radiusX·cos, y = centerY + radiusY·sin).
    return { slots: slots,
             geom: { radiusX: rxPx / W, radiusY: ryPx / H, centerY: cyPx / H } }
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
        // The ring seats in ANGULAR order. The slot NAMES are rotated by half a ring
        // (slotSeqLandscape), so the name order is NOT the order around the table -
        // the pairs have to be checked in the order the seats really follow each
        // other. (The old natural "opp1..oppN" order was the angular order.)
        var probeSeq = []
        if (spectating) {
            for (var iSeqS = 0; iSeqS < ringCnt; iSeqS++) probeSeq.push("opp" + iSeqS)
        } else {
            var seqW = slotSeqLandscape[ringCnt] || []
            for (var iSeq = 0; iSeq < seqW.length; iSeq++) probeSeq.push(seqW[iSeq])
        }

        function feasibleAt(sTest) {
            if (sTest <= 0) return false
            var visualW = oppBaseWidth * sTest
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
                    ? height - 8 - visualH
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
                    // Upper seats only. The band around zero is skipped deliberately:
                    // the seats at the waist sit far to the SIDES (|cos| ≈ 1), their
                    // plates never reach the card row, and sin(0)/sin(360°) differ in
                    // the last bit – without the band a waist seat would randomly
                    // count as an "upper" one and cap the whole table for nothing.
                    if (vY >= -0.06) continue
                    var vX = (pC[0] - 0.5) / geom.radiusX
                    var b = pC[1] * height + visualH / 2
                          + (Math.abs(vX) < 0.25 ? sTest * 25 : 0)
                    if (b > topOppBottom) topOppBottom = b
                }
                // The total community height (card row 64 + pot block 26 + 6 gap +
                // winner badge) · communityScale + the pad.
                if (topOppBottom > -1e9
                    && bottomContentTop - topOppBottom
                       < 0.72 * sTest * 124 + 28)
                    return false
            }

            // ── the room for a bet chip of the upper row ──────────────────────
            // GamePage.betFeltPoint clamps a chip of the upper row to the upper edge
            // of the pot block: it may not reach into the block, so the strip between
            // that plate and the block has to be able to hold the whole chip, or the
            // chip lies ON its own plate (found by sweep.js with the taller pot block
            // of 2026-11-12). The block position follows GamePage.communityScale +
            // communityCenterY and is mirrored here, because it is what makes the
            // strip: the block is pushed down by the LOWEST upper plate that hangs
            // over the card row. That is why a dense ring has room and a four-seat
            // ring (the top plate is the only one over the cards) has not.
            var isCmp = Config.Responsive.landscapeCompact
            var topSlot = null
            for (var kTop in pos)
                if (topSlot === null || pos[kTop][1] < topSlot[1]) topSlot = pos[kTop]
            if (topSlot !== null) {
                var topBottom = topSlot[1] * height + visualH / 2
                var selfTopC = spectating ? height - 8 - visualH
                                          : height - 12 - selfVisualH
                var dropY
                if (isCmp || spectating) {
                    dropY = (topBottom + selfTopC) / 2
                } else {
                    var sumY = height - 12 - selfBaseHeight * sTest / 2, nY = 1
                    for (var kY in pos) { sumY += pos[kY][1] * height; nY++ }
                    dropY = sumY / nY
                }
                // communityScale (GamePage): fills the band around the drop point
                var topB = topBottom + (isCmp ? 39 : 26) * sTest
                var avail = Math.min(dropY - topB - 6, selfTopC - dropY - 6)
                var gapFill = avail > 0 ? avail / (isCmp ? 82 : 84) : 0
                var cs = Math.max(0.55, Math.min(
                    Math.min(isCmp ? 2.1 : 1.8, sTest * 2.0, (0.70 * width) / 264),
                    Math.max(sTest * (isCmp ? 0.85 : 0.76), gapFill)))
                var bandC = selfTopC - topBottom
                if (bandC > 0) cs = Math.min(cs, (bandC - 16) / 96)
                // the upper edge of the pot block (communityCenterY minus its height)
                var blockTop
                if (isCmp || spectating) {
                    blockTop = (topBottom + selfTopC) / 2 - 64 * cs
                } else {
                    var halfRow = 132 * cs + oppBaseWidth * sTest / 2
                    var realBottom = topBottom
                    for (var kR in pos) {
                        var pR = pos[kR]
                        if (pR[1] * height >= dropY) continue
                        if (Math.abs(pR[0] * width - width / 2) >= halfRow) continue
                        var bR = pR[1] * height + visualH / 2
                        if (bR > realBottom) realBottom = bR
                    }
                    var cyComm = (realBottom + selfTopC) / 2 + 16 * cs
                    var loComm = realBottom + 64 * cs
                    var hiComm = selfTopC - 32 * cs
                    if (loComm <= hiComm)
                        cyComm = Math.max(loComm, Math.min(hiComm, cyComm))
                    blockTop = cyComm - 64 * cs
                }
                // Only a chip that is horizontally inside the pot block gets clamped
                // to it (otherwise it may come down to the card row). The chip
                // clearance is mirrored from GamePage.betFeltPoint (24/21 half sizes
                // since the bet is a chip stack with the amount under it, 2026-11-12);
                // keep both files in sync or the bisection starts lying.
                if (Math.abs(topSlot[0] - 0.5) * width < 34 * cs + 24 * sTest
                    && blockTop - topBottom < 42 * sTest + 6)
                    return false
            }

            // The bottom pair flanks the self box – it must not cover it. The oval
            // is derived to keep that clearance (the bottom vertex IS the self box),
            // this check only catches zones that are too small for it: the bisection
            // then shrinks the boxes until the plates fit beside the self box.
            if (!spectating) {
                var selfHalfW = selfBaseWidth * sTest / 2
                var selfTopEdge = height - 12 - selfVisualH
                var selfBotEdge = height - 12
                for (var keyS in pos) {
                    var pS = pos[keyS]
                    var sCy = pS[1] * height
                    if (sCy + visualH / 2 <= selfTopEdge) continue
                    if (sCy - visualH / 2 >= selfBotEdge) continue
                    if (Math.abs(pS[0] * width - width / 2)
                        >= selfHalfW + visualW / 2 + 4 * sTest)
                        continue
                    return false
                }
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

        // A common limit for the opponent boxes, the self box and the community badges:
        // 1.4 prevents text that is too large and bet overlaps in
        // fullscreen/maximized; compact stays at 1.7 (wider, flatter).
        // fillCap() damps the maximum with few players in addition.
        // Heads-up has no neighbouring pair to check any more, so feasibleAt() only
        // applies the community band there – the one thing that actually binds on a
        // two-player table, which is why there is no separate branch for it.
        var lo = 0.55, hi = fillCap(Config.Responsive.landscapeCompact ? 2.3 : 1.9)
        if (!feasibleAt(lo)) {
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

