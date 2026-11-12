# -*- coding: utf-8 -*-
"""
PokerTH table chips — parametric generator.

Single source of truth: one spec dict per chip -> SVG (vector master) ->
PNG renders by resvg at any size.  No filters, no masks, no clip paths:
only circles, annular-sector paths and linear/radial gradients, i.e. exactly
what QtSvg can render.

This is the repository copy of the player's chip set
(E:\\Projects\\poker\\pokerth-chips).  It is the same generator; the difference
is the extra tier "tnum": on our table a chip is 22 base px (27–39 px on
screen), so the value has to stay legible from 24 px.  The rim stays as thick as
in lg (inlay 0.70 R, inserts from 0.745 R) and carries the value fitted up to
0.88 of the inlay.  The SVG assets under
src/gui/qt6-qml/resources/chips are built from this file (tiers sm / lg / tnum
only; no PNG — the client does not use them).

    python chipgen.py --out DIR                 # SVG masters only (default: svg/)
    python chipgen.py --out DIR --png           # also PNGs (default: png/ next to this file)
    python chipgen.py --out DIR --png --png-dir PNGDIR
    python chipgen.py --out DIR --tiers sm,lg,tnum   # subset of tiers

Without --png nothing is rasterised; the tier "tnum" is SVG-only unless --png
is passed, in which case it renders 24/28/32/36/40/48/96 px.
"""
import argparse
import math
import os

import resvg_py
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

HERE = os.path.dirname(os.path.abspath(__file__))
BOX = 1024.0
C = BOX / 2.0
R = 506.0            # 4 px transparent margin, so the rim never gets clipped

# ─────────────────────────── palette ────────────────────────────────────────
# Tier logic: 6 inserts = "working" chips, 8 inserts = the two premium ones.
CHIPS = [
    ("5",      dict(body="#b31d1d", inlay="#f4ece0", ins="#fbf7ef", n=6, label="5")),
    ("10",     dict(body="#1f4fc4", inlay="#f4ece0", ins="#fbf7ef", n=6, label="10")),
    ("25",     dict(body="#187a3c", inlay="#f4ece0", ins="#fbf7ef", n=6, label="25")),
    ("100",    dict(body="#23262c", inlay="#f4ece0", ins="#fbf7ef", n=6, label="100")),
    ("500",    dict(body="#6a2cc0", inlay="#f4ece0", ins="#fbf7ef", n=6, label="500")),
    ("2500",   dict(body="#b85410", inlay="#f4ece0", ins="#fbf7ef", n=6, label="2.5K")),
    ("10000",  dict(body="#b8860b", inlay="#2b2318", ins="#f7e8c2", n=8,
                    mat="gold",  premium=True, label="10K", ink="#f0cf78")),
    ("50000",  dict(body="#3a4048", inlay="#f3ecdd", ins="#d9b04a", n=8,
                    mat="graphite", premium=True, label="50K", ink="#2c3138")),
    ("spare_ink",    dict(body="#1b2330", inlay="#e6ecf4", ins="#c8d2e0", n=6)),
    ("spare_bronze", dict(body="#8a5a26", inlay="#f6ecd9", ins="#f2d9a8", n=6)),
]

# Numbers only where they fit: 48 px and up. More than three digits -> K.
FONT_BOLD = r"C:\Windows\Fonts\segoeuib.ttf"

# ─────────────────────────── colour helpers ─────────────────────────────────
def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def hx(c):
    return "#%02x%02x%02x" % tuple(max(0, min(255, int(round(v)))) for v in c)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def light(c, f):   # f > 1
    return mix(c, (255, 255, 255), min(1.0, (f - 1) * 0.92))


def dark(c, f):    # f > 1 = darker
    return mix(c, (0, 0, 0), min(1.0, 1.0 - 1.0 / f))


def polar(r, deg, cx=C, cy=C):
    t = math.radians(deg)
    return cx + r * math.cos(t), cy + r * math.sin(t)


def wedge(r_out, r_in, centre_deg, half_deg):
    """Annular sector: inserts that reach the silhouette."""
    p1 = polar(r_out, centre_deg - half_deg)
    p2 = polar(r_out, centre_deg + half_deg)
    q1 = polar(r_in, centre_deg - half_deg)
    q2 = polar(r_in, centre_deg + half_deg)
    return ("M %.2f %.2f A %.2f %.2f 0 0 1 %.2f %.2f L %.2f %.2f "
            "A %.2f %.2f 0 0 0 %.2f %.2f Z"
            % (p1[0], p1[1], r_out, r_out, p2[0], p2[1],
               q2[0], q2[1], r_in, r_in, q1[0], q1[1]))


def arc(r, a1, a2, w, colour, op):
    p1 = polar(r, a1)
    p2 = polar(r, a2)
    large = 1 if abs((a2 - a1) % 360) > 180 else 0
    sweep = 1 if a2 > a1 else 0
    return ('<path d="M %.2f %.2f A %.2f %.2f 0 %d %d %.2f %.2f" fill="none" '
            'stroke="%s" stroke-opacity="%.2f" stroke-width="%.2f"/>'
            % (p1[0], p1[1], r, r, large, sweep, p2[0], p2[1], colour, op, w))


def ring_path(r_out, r_in):
    """Annulus as one path (two circle subpaths + evenodd): gradient-filled rings
    instead of stroked arcs — a stroke leaves visible blunt ends where the
    shading stops, and per-segment arcs read as a dashed ring."""
    def circle(r):
        return ("M %.2f %.2f A %.2f %.2f 0 1 1 %.2f %.2f A %.2f %.2f 0 1 1 %.2f %.2f Z"
                % (C + r, C, r, r, C - r, C, r, r, C + r, C))
    return circle(r_out) + " " + circle(r_in)


# ─────────────────────────── text (value in the inlay) ──────────────────────
_FONT = {}


def _font(path=FONT_BOLD):
    if path not in _FONT:
        f = TTFont(path)
        _FONT[path] = (f.getGlyphSet(), f.getBestCmap(), f["hmtx"])
    return _FONT[path]


def text_outlines(text):
    """Glyphs as SVG paths + the natural bounding box, in font units (y up)."""
    gs, cmap, hmtx = _font()
    x = 0.0
    placed = []
    for ch in text:
        gn = cmap.get(ord(ch))
        if gn is None:
            continue
        pen = SVGPathPen(gs)
        gs[gn].draw(pen)
        placed.append((gn, pen.getCommands(), x))
        # the period in 2.5K sits too far from its neighbours otherwise
        x += hmtx[gn][0] * (0.78 if ch == "." else 1.0)
    bp = BoundsPen(gs)
    for gn, _d, ox in placed:
        gs[gn].draw(TransformPen(bp, (1, 0, 0, 1, ox, 0)))
    xmin, ymin, xmax, ymax = bp.bounds
    return placed, (xmin, ymin, xmax, ymax)


# How the value is fitted into the inlay, per tier:
#   h_fit — cap on the text height, in inlay radii;
#   w_fit — cap on the text width, in inlay radii;
#   c_fit — the text box must stay inside the inscribed circle of that radius.
# sm/lg/num keep the original 0.74 / 1.52 / 0.86, i.e. the old behaviour 1:1.
# tnum (the small-on-table chip) keeps the lg-sized inlay but trades the margin
# for legibility: the value may reach 0.88 of the inlay and 1.70 wide, checked
# against 0.90.
LABEL_FIT = {
    "sm":   (0.74, 1.52, 0.86),
    "lg":   (0.74, 1.52, 0.86),
    "num":  (0.74, 1.52, 0.86),
    "tnum": (0.88, 1.70, 0.90),
}


def label_svg(spec, r_inlay, tier):
    """The value, centred in the inlay and fitted to the inscribed circle."""
    text = spec.get("label")
    if not text:
        return ""
    h_fit, w_fit, c_fit = LABEL_FIT.get(tier, LABEL_FIT["num"])
    placed, (xmin, ymin, xmax, ymax) = text_outlines(text)
    w, h = xmax - xmin, ymax - ymin
    aspect = w / h
    # fit the box into the inlay with a margin, then cap by width so that the
    # number never reaches the printed ring
    h_final = min(h_fit * r_inlay,
                  2 * c_fit * r_inlay / math.sqrt(aspect * aspect + 1))
    s = h_final / h
    if w * s > w_fit * r_inlay:
        s = w_fit * r_inlay / w
    tx = C - (xmin + w / 2) * s
    ty = C + (ymin + h / 2) * s
    ink = spec.get("ink") or hx(dark(rgb(spec["body"]), 2.6))
    out = ['<g transform="translate(%.2f %.2f) scale(%.4f -%.4f)">'
           % (tx, ty, s, s)]
    for _gn, d, ox in placed:
        if d:
            out.append('<path d="%s" transform="translate(%.1f 0)" fill="%s"/>'
                       % (d, ox, ink))
    out.append("</g>")
    return "".join(out)


# ─────────────────────────── geometry per tier ──────────────────────────────
TIERS = {
    # 12–20 px: the body colour must win the chip; the inserts are separated
    # from the inlay by a body-colour ring and are a step brighter than it,
    # otherwise inlay + inserts merge into one pale "flower".
    "sm": dict(ins_in=0.545, ins_from=0.735, ang=15.0, step=0.110, pin=0.0,
               spec=0.100, dark=0.100, edge=0.026, inlay_tint=0.93, ins_tint=1.05),
    # 21 px and up: real proportions of a casino chip + a rounded raised rim.
    "lg": dict(ins_in=0.655, ins_from=0.700, ang=22.0, step=0.034, pin=0.016,
               spec=0.050, dark=0.050, edge=0.008, inlay_tint=1.0, ins_tint=1.0),
    # 48 px and up: the same chip with the value in the inlay. The inlay is a
    # touch bigger so that 2.5K has room without touching the printed ring.
    "num": dict(ins_in=0.680, ins_from=0.720, ang=22.0, step=0.034, pin=0.0,
                spec=0.050, dark=0.050, edge=0.008, inlay_tint=1.0, ins_tint=1.0,
                label=True),
    # 24 px and up: the table chip. 22 base px is 27–39 screen px. The rim keeps
    # the lg thickness (inlay 0.70 R, inserts from 0.745 R, step 0.034) so the
    # chip still reads as a coloured disc, not a pale disc with a thin ring; the
    # printed pin ring is dropped and the value is fitted by the tnum rule
    # (0.88 / 1.70 / 0.90) — digits stay readable from 24 px.
    "tnum": dict(ins_in=0.700, ins_from=0.745, ang=22.0, step=0.034, pin=0.0,
                 spec=0.050, dark=0.050, edge=0.008, inlay_tint=1.0, ins_tint=1.0,
                 label=True),
}


def body_stops(spec):
    b = rgb(spec["body"])
    mat = spec.get("mat")
    if mat == "gold":
        return [(0, "#eec95a"), (.28, "#cfa02e"), (.70, "#8d6814"), (1, "#42300a")]
    if mat == "platinum":
        return [(0, "#ffffff"), (.28, "#e4eaf2"), (.72, "#b3c1d2"), (1, "#6d7e93")]
    if mat == "graphite":
        return [(0, "#6a727d"), (.28, "#3c434b"), (.72, "#23272c"), (1, "#0d0f12")]
    if mat == "silver":
        return [(0, "#ffffff"), (.28, "#eef1f5"), (.72, "#c6cfd9"), (1, "#7e8a98")]
    return [(0, hx(light(b, 1.30))), (.30, hx(b)),
            (.72, hx(dark(b, 1.12))), (1, hx(dark(b, 1.85)))]


def inlay_stops(spec):
    f = rgb(spec["inlay"])
    return [(0, hx(light(f, 1.05))), (.55, hx(f)), (1, hx(dark(f, 1.14)))]


def ins_stops(spec):
    # a variant may carry its own insert gradient (e.g. an iridescent edge)
    if spec.get("ins_grad"):
        return spec["ins_grad"]
    s = rgb(spec["ins"])
    return [(0, hx(light(s, 1.06))), (1, hx(dark(s, 1.16)))]


def build_svg(spec, tier):
    g = TIERS[tier]
    sm = tier == "sm"
    b = rgb(spec["body"])
    n = spec["n"]
    r_in = R * g["ins_in"]
    r_ins_from = R * g.get("ins_from", g["ins_in"])
    face_c = hx(mix(rgb(spec["inlay"]), (0, 0, 0), 1 - g.get("inlay_tint", 1.0)))
    ins_c = hx(mix(rgb(spec.get("ins", spec["body"])), (255, 255, 255),
                   g.get("ins_tint", 1.0) - 1.0))
    local = dict(spec, inlay=face_c, ins=ins_c)

    def grad_lin(gid, stops, x1=C - R, y1=C - R, x2=C + R, y2=C + R):
        s = ""
        for st in stops:
            o, c = st[0], st[1]
            op = st[2] if len(st) > 2 else 1.0
            s += '<stop offset="%.3f" stop-color="%s" stop-opacity="%.2f"/>' % (o, c, op)
        return ('<linearGradient id="%s" gradientUnits="userSpaceOnUse" '
                'x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f">%s</linearGradient>'
                % (gid, x1, y1, x2, y2, s))

    def grad_rad(gid, stops, cx, cy, r):
        s = "".join('<stop offset="%.3f" stop-color="%s"/>' % (o, c) for o, c in stops)
        return ('<radialGradient id="%s" gradientUnits="userSpaceOnUse" '
                'cx="%.1f" cy="%.1f" r="%.1f">%s</radialGradient>'
                % (gid, cx, cy, r, s))

    f = rgb(face_c)
    defs = [
        grad_rad("body", body_stops(spec), C - 190, C - 205, R * 1.62),
        grad_rad("inlay", inlay_stops(local), C - 105, C - 115, R * 1.15),
        grad_lin("ins", ins_stops(local)),
        grad_lin("sheen", [(0, "#ffffff", .30), (.45, "#ffffff", .05), (1, "#ffffff", 0)]),
        grad_lin("shade", [(0, "#000000", 0), (.55, "#000000", .05), (1, "#000000", .22)]),
        grad_lin("stepdark", [(0, hx(dark(f, 1.8)), .62), (.55, hx(dark(f, 1.8)), .10),
                              (1, hx(dark(f, 1.8)), 0)]),
        grad_lin("steplight", [(0, hx(light(f, 1.15)), 0), (.45, hx(light(f, 1.15)), .10),
                               (1, hx(light(f, 1.15)), .70)]),
    ]

    el = []
    # body + the dark physical edge
    el.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="url(#body)"/>' % (C, C, R))

    # inserts
    if n:
        for i in range(n):
            centre = 270.0 + 360.0 * i / n
            el.append('<path d="%s" fill="url(#ins)"/>'
                      % wedge(R * 1.001, r_ins_from, centre, g["ang"] / 2))

    # inlay
    el.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="url(#inlay)"/>'
              % (C, C, R * (g["ins_in"] + 0.004)))

    # recessed step: a gradient-filled annulus. Tinted colours, not black/white,
    # otherwise the ring reads as a chrome bezel instead of a moulded edge.
    half = R * g["step"] * 0.5
    el.append('<path d="%s" fill="url(#stepdark)" fill-rule="evenodd"/>'
              % ring_path(r_in + half, r_in - half))
    el.append('<path d="%s" fill="url(#steplight)" fill-rule="evenodd"/>'
              % ring_path(r_in + half, r_in - half))

    if g["pin"]:
        pin_c = "#f3d98a" if spec.get("mat") == "gold" else \
                "#33526e" if spec.get("mat") == "platinum" else hx(dark(b, 1.35))
        el.append(arc(R * (g["ins_in"] - 0.105), 0, 359.9, R * g["pin"], pin_c, 0.55))
        if spec.get("premium"):
            el.append(arc(R * (g["ins_in"] - 0.135), 0, 359.9, R * g["pin"] * 0.55,
                          pin_c, 0.35))

    # the value, printed in the inlay
    if g.get("label"):
        el.append(label_svg(spec, R * g["ins_in"], tier))

    # rim: rounded bevel all around, brighter top-left, shadow bottom-right
    r_bev = R * 0.938
    if g["spec"]:
        el.append('<path d="%s" fill="url(#sheen)" fill-rule="evenodd"/>'
                  % ring_path(R, r_bev))
    if g["dark"]:
        el.append('<path d="%s" fill="url(#shade)" fill-rule="evenodd"/>'
                  % ring_path(R, r_bev))
    el.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="#000000" '
              'stroke-opacity="0.42" stroke-width="%.2f"/>'
              % (C, C, R - R * max(g["edge"], 0.008) * 0.5, R * max(g["edge"], 0.008)))

    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d"><defs>%s</defs>%s</svg>'
            % (int(BOX), int(BOX), int(BOX), int(BOX), "".join(defs), "".join(el)))


def render(svg, px):
    return resvg_py.svg_to_bytes(svg_string=svg, width=int(px), height=int(px))


# Raster sizes per tier; tnum exists for the table and needs no big masters.
PNG_SIZES = (13, 15, 16, 20, 24, 32, 48, 64, 96, 128, 256, 512, 1024)
NUM_PNG_SIZES = (48, 64, 96, 128, 256, 512, 1024)
TNUM_PNG_SIZES = (24, 28, 32, 36, 40, 48, 96)


def main(argv=None):
    ap = argparse.ArgumentParser(description="PokerTH chip asset generator")
    ap.add_argument("--out", default=os.path.join(HERE, "svg"),
                    help="SVG output directory (default: <script>/svg)")
    ap.add_argument("--png", action="store_true",
                    help="also render PNGs (off by default: the client uses SVG)")
    ap.add_argument("--png-dir", default=os.path.join(HERE, "png"),
                    help="PNG output directory (default: <script>/png)")
    ap.add_argument("--tiers", default="",
                    help="comma-separated tier subset, e.g. sm,lg,tnum")
    args = ap.parse_args(argv)

    wanted = [t.strip() for t in args.tiers.split(",") if t.strip()]
    out_svg = os.path.abspath(args.out)
    os.makedirs(out_svg, exist_ok=True)
    if args.png:
        os.makedirs(os.path.abspath(args.png_dir), exist_ok=True)

    total = 0
    for name, spec in CHIPS:
        tiers = ("sm", "lg", "num", "tnum") if spec.get("label") else ("sm", "lg")
        if wanted:
            tiers = tuple(t for t in tiers if t in wanted)
        for tier in tiers:
            svg = build_svg(spec, tier)
            p = os.path.join(out_svg, "chip_%s_%s.svg" % (name, tier))
            with open(p, "w", encoding="utf-8") as f:
                f.write(svg)
            print(p, len(svg.encode("utf-8")), "bytes")
            total += 1
            if not args.png:
                continue
            if tier == "tnum":
                sizes = TNUM_PNG_SIZES
            elif tier == "num":
                sizes = NUM_PNG_SIZES
            else:
                sizes = PNG_SIZES
            for px in sizes:
                with open(os.path.join(os.path.abspath(args.png_dir),
                                       "chip_%s_%s_%d.png" % (name, tier, px)), "wb") as f:
                    f.write(render(svg, px))
    print("%d SVG files -> %s" % (total, out_svg))


if __name__ == "__main__":
    main()
