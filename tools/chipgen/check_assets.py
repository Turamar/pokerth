# -*- coding: utf-8 -*-
"""Checks the generated chip assets and builds the contact sheet.

For the 55 SVG files in src/gui/qt6-qml/resources/chips:
  * every file is well-formed XML, 1024x1024 viewBox, no <filter>/<mask>/<clipPath>
    (QtSvg cannot render those), and no filter/mask/clip-path reference;
  * every file renders through resvg at 24/28/35/48/96 px;
  * sm/lg/num output is byte-identical to the original player's generator
    (E:\\Projects\\poker\\pokerth-chips\\chipgen.py), so the tier split did not
    disturb the existing look;
  * all renders are saved to dist/shots/chips/gen/png/;
  * a felt-green contact sheet is written to dist/shots/chips/gen/contact.png:
      - "tnum 1:1": all 8 values at 24 / 28 / 35 px,
      - "лежачие 1:1": all 8 values lying (obln) at 28 px and the same chips
        without a value (obl) at 24 px,
      - "кэш 1:1": the nine cash labels lying (obln) at 22 / 28 px,
      - "sm/lg 1:1": all 10 chips at 16 (sm) and 22 (lg) px,
      - "x3": 5 / 100 / 2.5K / 10K at 96 px.

    python tools/chipgen/check_assets.py
"""
import importlib.util
import os
import sys
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

import chipgen

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "..", "..", ".."))
ASSETS = os.path.join(ROOT, "pokerth-src", "src", "gui", "qt6-qml",
                      "resources", "chips")
ORIG = os.path.join(ROOT, "pokerth-chips", "chipgen.py")
SHOTS = os.path.join(ROOT, "dist", "shots", "chips", "gen")
PNG_DIR = os.path.join(SHOTS, "png")

NOMINALS = [("5", "5"), ("10", "10"), ("25", "25"), ("100", "100"),
            ("500", "500"), ("2500", "2.5K"), ("10000", "10K"),
            ("50000", "50K")]
ALL_CHIPS = NOMINALS + [("spare_ink", "spare ink"),
                        ("spare_bronze", "spare bronze")]
# Cash-table labels (chipgen.CASH), only the lying numbered tier exists for them.
CASH_CHIPS = [("c1", "1¢"), ("c5", "5¢"), ("c10", "10¢"), ("c25", "25¢"),
              ("d1", "1"), ("d5", "5"), ("d25", "25"), ("d100", "100"),
              ("d500", "500")]
ZOOM = [("5", "5"), ("100", "100"), ("2500", "2.5K"), ("10000", "10K")]

CHECK_SIZES = (24, 28, 35, 48, 96)
TNUM_SIZES = (24, 28, 35)
CASH_SIZES = (22, 28, 35, 48, 96)


def asset_path(name, tier):
    return os.path.join(ASSETS, "chip_%s_%s.svg" % (name, tier))


def load_original():
    spec = importlib.util.spec_from_file_location("orig_chipgen", ORIG)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def check_svg_files():
    """Parse + forbiddens. Returns (ok, problems, files)."""
    problems = []
    files = sorted(f for f in os.listdir(ASSETS) if f.endswith(".svg"))
    forbidden = ("<filter", "<mask", "<clippath", "filter=", "mask=", "clip-path")
    for fn in files:
        path = os.path.join(ASSETS, fn)
        with open(path, "r", encoding="utf-8") as f:
            txt = f.read()
        try:
            root = ET.fromstring(txt)
        except ET.ParseError as e:
            problems.append("%s: XML parse error: %s" % (fn, e))
            continue
        if not root.tag.endswith("svg"):
            problems.append("%s: root tag is %s" % (fn, root.tag))
        if root.get("viewBox") != "0 0 1024 1024":
            problems.append("%s: viewBox=%r" % (fn, root.get("viewBox")))
        if root.get("width") != "1024" or root.get("height") != "1024":
            problems.append("%s: size %sx%s" % (fn, root.get("width"),
                                               root.get("height")))
        low = txt.lower()
        for bad in forbidden:
            if bad in low:
                problems.append("%s: contains %r" % (fn, bad))
        # QtSvg does not support <image> either; nothing here should use it
        if "<image" in low:
            problems.append("%s: contains <image" % fn)
    return (not problems), problems, files


def check_original_compat():
    """sm/lg/num must match the player's generator byte for byte."""
    orig = load_original()
    problems = []
    checked = 0
    for name, spec in chipgen.CHIPS:
        tiers = ("sm", "lg", "num") if spec.get("label") else ("sm", "lg")
        for tier in tiers:
            want = orig.build_svg(spec, tier)
            got = chipgen.build_svg(spec, tier)
            checked += 1
            if want != got:
                problems.append("%s/%s differs from the original generator"
                                % (name, tier))
    return (not problems), problems, checked


def render_all():
    os.makedirs(PNG_DIR, exist_ok=True)
    errors = []
    rendered = 0
    for name, _disp in ALL_CHIPS:
        nominal = any(name == n for n, _ in NOMINALS)
        tiers = ("sm", "lg", "tnum", "obln", "obl") if nominal \
            else ("sm", "lg", "obl")
        if "obln" in tiers:
            tiers = tuple(x for x in tiers if x != "obln") + ("obln",)
        for tier in tiers:
            with open(asset_path(name, tier), "r", encoding="utf-8") as f:
                svg = f.read()
            sizes = set(CHECK_SIZES)
            if tier == "tnum":
                sizes |= set(TNUM_SIZES)
            for px in sorted(sizes):
                try:
                    data = chipgen.render(svg, px)
                except Exception as e:  # noqa: BLE001 - report any resvg failure
                    errors.append("%s %s %d px: %s" % (name, tier, px, e))
                    continue
                p = os.path.join(PNG_DIR, "chip_%s_%s_%d.png" % (name, tier, px))
                with open(p, "wb") as f:
                    f.write(data)
                try:
                    im = Image.open(p)
                    im.load()
                    if im.size != (px, px):
                        errors.append("%s %s %d px: PNG is %s" %
                                      (name, tier, px, im.size))
                except Exception as e:  # noqa: BLE001
                    errors.append("%s %s %d px: bad PNG: %s" % (name, tier, px, e))
                rendered += 1
    # extra 1:1 sizes for the sheet: sm 16 (table minimum) and lg 22
    for name, _disp in ALL_CHIPS:
        for tier, px in (("sm", 16), ("lg", 22)):
            with open(asset_path(name, tier), "r", encoding="utf-8") as f:
                svg = f.read()
            try:
                data = chipgen.render(svg, px)
            except Exception as e:  # noqa: BLE001
                errors.append("%s %s %d px: %s" % (name, tier, px, e))
                continue
            p = os.path.join(PNG_DIR, "chip_%s_%s_%d.png" % (name, tier, px))
            with open(p, "wb") as f:
                f.write(data)
            rendered += 1
    # the cash labels: only the lying numbered tier exists
    for name, _disp in CASH_CHIPS:
        with open(asset_path(name, "obln"), "r", encoding="utf-8") as f:
            svg = f.read()
        for px in CASH_SIZES:
            try:
                data = chipgen.render(svg, px)
            except Exception as e:  # noqa: BLE001
                errors.append("%s obln %d px: %s" % (name, px, e))
                continue
            p = os.path.join(PNG_DIR, "chip_%s_obln_%d.png" % (name, px))
            with open(p, "wb") as f:
                f.write(data)
            rendered += 1
    return (not errors), errors, rendered


# ─────────────────────────── contact sheet ──────────────────────────────────
W = 1340
FELT = (23, 69, 46)          # #17452e — the table felt
WHITE = (238, 241, 245)
DIM = (167, 183, 172)
ACCENT = (232, 197, 106)
RULE = (40, 92, 66)

FONT = r"C:\Windows\Fonts\segoeui.ttf"
FONTB = r"C:\Windows\Fonts\segoeuib.ttf"


def font(px, bold=False):
    return ImageFont.truetype(FONTB if bold else FONT, px)


def ctext(draw, cx, y, text, f, fill):
    w = draw.textlength(text, font=f)
    draw.text((cx - w / 2, y), text, font=f, fill=fill)


def chip(name, tier, px):
    p = os.path.join(PNG_DIR, "chip_%s_%s_%d.png" % (name, tier, px))
    return Image.open(p).convert("RGBA")


def build_sheet():
    H = 1320                      # cropped to content afterwards
    img = Image.new("RGB", (W, H), FELT)
    d = ImageDraw.Draw(img)
    f_title = font(24, True)
    f_sub = font(14)
    f_block = font(18, True)
    f_row = font(15, True)
    f_cap = font(12)
    f_small = font(11)

    y = 26
    d.text((40, y), "PokerTH — фишки на столе: контрольный лист (tnum + лежачие)", font=f_title, fill=WHITE)
    y += 34
    d.text((40, y), "55 SVG в src/gui/qt6-qml/resources/chips (8 номиналов × sm/lg/tnum/obln/obl, "
                    "2 запасных × sm/lg/obl, 9 кэш-вариантов × obln), "
                    "рендер resvg 1:1", font=f_sub, fill=DIM)
    y += 20
    d.text((40, y), "tnum: борт как у lg — инлей 0.70 R, вставки от 0.745 R, шаг 0.034, без пина; "
                    "цифра до 0.88 инлея / 1.70 ширины (проверка 0.90)", font=f_sub, fill=DIM)
    y += 30
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 18

    # ── block 1: tnum at real table sizes ─────────────────────────────────
    d.text((40, y), "tnum 1:1 — все 8 номиналов в 24 / 28 / 35 px",
           font=f_block, fill=WHITE)
    d.text((40, y + 26), "чип на столе — 22 базовых px = 27–39 экранных; цифры должны читаться с 24 px",
           font=f_sub, fill=DIM)
    y += 46
    cell = 132
    x0 = (W - cell * len(NOMINALS)) // 2
    for px in TNUM_SIZES:
        d.text((x0 - 58, y + px / 2 - 9), "%d px" % px, font=f_row, fill=ACCENT)
        for i, (name, disp) in enumerate(NOMINALS):
            cx = x0 + i * cell + cell // 2
            im = chip(name, "tnum", px)
            img.paste(im, (cx - im.width // 2, y), im)
            ctext(d, cx, y + px + 4, disp, f_cap, DIM)
        y += px + 34
    y += 8
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 18

    # ── block 2: sm/lg at 16 and 22 px ────────────────────────────────────
    d.text((40, y), "sm/lg 1:1 — все 10 чипов в 16 и 22 px",
           font=f_block, fill=WHITE)
    d.text((40, y + 26), "sm — до 21 px, lg — от 21 px; без цифр, только цвет и вставки",
           font=f_sub, fill=DIM)
    y += 46
    cell = 116
    x0 = (W - cell * len(ALL_CHIPS)) // 2
    for px, tier in ((16, "sm"), (22, "lg")):
        d.text((x0 - 58, y + px / 2 - 9), "%d px" % px, font=f_row, fill=ACCENT)
        for i, (name, disp) in enumerate(ALL_CHIPS):
            cx = x0 + i * cell + cell // 2
            im = chip(name, tier, px)
            img.paste(im, (cx - im.width // 2, y), im)
            ctext(d, cx, y + px + 4, "%s %s" % (disp, tier), f_cap, DIM)
        y += px + 34
    y += 8
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 18

    # ── block 3: the lying chips ──────────────────────────────────────────
    d.text((40, y), "лежачие 1:1 — obln 28 px (с цифрой) и obl 24 px (без цифры: чип мельче порога)",
           font=f_block, fill=WHITE)
    d.text((40, y + 26), "лицо 0.80 ширины + боковина 10 %: фишка лежит на сукне, стопка = сдвиг на высоту боковины",
           font=f_sub, fill=DIM)
    y += 46
    cell = 116
    x0 = (W - cell * len(NOMINALS)) // 2
    for px, tier in ((28, "obln"), (24, "obl")):
        d.text((x0 - 58, y + px / 2 - 9), "%d px" % px, font=f_row, fill=ACCENT)
        for i, (name, disp) in enumerate(NOMINALS):
            cx = x0 + i * cell + cell // 2
            im = chip(name, tier, px)
            img.paste(im, (cx - im.width // 2, y), im)
            ctext(d, cx, y + px + 4, "%s · %s" % (disp, tier), f_cap, DIM)
        y += px + 34
    y += 8
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 18

    # ── block 4: the cash labels ──────────────────────────────────────────
    d.text((40, y), "кэш 1:1 — obln 22 / 28 px: сумма напечатана на фишке",
           font=f_block, fill=WHITE)
    d.text((40, y + 26), "центы со знаком ¢ (1¢ 5¢ 10¢ 25¢), доллары числом (1 5 25 100 500): "
                         "«$500» пришлось бы мельчить против «$100»",
           font=f_sub, fill=DIM)
    y += 46
    cell = 116
    x0 = (W - cell * len(CASH_CHIPS)) // 2
    for px in (22, 28):
        d.text((x0 - 58, y + px / 2 - 9), "%d px" % px, font=f_row, fill=ACCENT)
        for i, (name, disp) in enumerate(CASH_CHIPS):
            cx = x0 + i * cell + cell // 2
            im = chip(name, "obln", px)
            img.paste(im, (cx - im.width // 2, y), im)
            ctext(d, cx, y + px + 4, disp, f_cap, DIM)
        y += px + 34
    y += 8
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 18

    # ── block 5: x3 zoom ──────────────────────────────────────────────────
    d.text((40, y), "×3 — 5 / 100 / 2.5K / 10K крупно (96 px)",
           font=f_block, fill=WHITE)
    d.text((40, y + 26), "3× от 32 px: видно форму цифр и кромку; те же tnum SVG, просто больше",
           font=f_sub, fill=DIM)
    y += 46
    cell = 200
    x0 = (W - cell * len(ZOOM)) // 2
    for i, (name, disp) in enumerate(ZOOM):
        cx = x0 + i * cell + cell // 2
        im = chip(name, "tnum", 96)
        img.paste(im, (cx - im.width // 2, y), im)
        ctext(d, cx, y + 96 + 6, "%s · tnum · 96 px" % disp, f_cap, DIM)
    y += 96 + 34
    y += 14
    d.line([40, y, W - 40, y], fill=RULE, width=1)
    y += 12
    d.text((40, y), "Источник: pokerth-src/tools/chipgen/chipgen.py → resources/chips/*.svg (55 файлов) · "
                    "фон #17452e · рендер resvg_py",
           font=f_small, fill=DIM)
    y += 26

    img = img.crop((0, 0, W, y))
    out = os.path.join(SHOTS, "contact.png")
    img.save(out)
    return out, img.size


def main():
    ok_xml, xml_problems, files = check_svg_files()
    ok_compat, compat_problems, compat_checked = check_original_compat()
    ok_render, render_errors, rendered = render_all()
    out, size = build_sheet()

    print("SVG files: %d" % len(files))
    for fn in files:
        p = os.path.join(ASSETS, fn)
        print("  %-28s %6d bytes" % (fn, os.path.getsize(p)))
    total = sum(os.path.getsize(os.path.join(ASSETS, f)) for f in files)
    print("TOTAL: %d files, %d bytes (%.1f KB)" % (len(files), total, total / 1024.0))
    print()
    print("XML/forbidden check: %s (%d problem(s))"
          % ("OK" if ok_xml else "FAIL", len(xml_problems)))
    for p in xml_problems:
        print("  !", p)
    print("Original-generator 1:1 check (sm/lg/num): %s (%d variant(s))"
          % ("OK" if ok_compat else "FAIL", compat_checked))
    for p in compat_problems:
        print("  !", p)
    print("resvg render check: %s (%d PNGs, %d error(s))"
          % ("OK" if ok_render else "FAIL", rendered, len(render_errors)))
    for e in render_errors:
        print("  !", e)
    print("contact sheet:", out, size)


if __name__ == "__main__":
    main()
