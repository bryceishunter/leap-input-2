#!/usr/bin/env python3
"""Regenerates Leapdesk KVM's icons and logos from the masters in .assets/.

    python res/make-icons.py [--retrace]

The masters:
    .assets/icon-colour-1024.png     the app icon, a rounded square
    .assets/logo-white-mono-1024.png the monogram alone, white on transparent
    .assets/monogram.svg             the monogram traced to a path, for the SVG
                                     icons.  --retrace redoes it from the PNG,
                                     which needs potracer and numpy.

Everything else needs only Pillow (pip install pillow).  The script writes the
Windows .ico, the macOS .icns, the Linux .svg, the GUI's window, tray and About
images, and the installer images, and overwrites them all each time.
"""

import argparse
import io
import re
import struct
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / ".assets"

SIZE = 1024
# corner radius of the icon's rounded square, measured on the master
RADIUS = 163

# status badges on the tray icons, as fractions of the icon
BADGE_RADIUS = 0.2
BADGE_GAP = 0.05
CONNECTED_COLOUR = (34, 197, 94)
TRANSFERRING_COLOUR = (245, 158, 11)
# the grey that "KVM" is written in: readable on light and dark backgrounds
WORDMARK_GREY = (138, 143, 152)

FONT_DIRS = [Path("C:/Windows/Fonts"), Path("/usr/share/fonts"), Path("/Library/Fonts"),
             Path.home() / ".local/share/fonts"]


def resize(image, size):
    """Downscales with premultiplied alpha, so that edges don't pick up the
    colour of transparent pixels."""
    if isinstance(size, int):
        size = (size, size)
    return image.convert("RGBa").resize(size, Image.LANCZOS, reducing_gap=3.0).convert("RGBA")


def rounded_mask(size, radius, inset=0):
    """An anti-aliased rounded square of the given size."""
    scale = 4
    mask = Image.new("L", (size * scale, size * scale), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (inset * scale, inset * scale, (size - inset) * scale - 1, (size - inset) * scale - 1),
        radius=(radius - inset) * scale, fill=255)
    return mask.resize((size, size), Image.LANCZOS)


def circle_mask(size, centre, radius):
    scale = 4
    mask = Image.new("L", (size * scale, size * scale), 0)
    cx, cy, r = centre[0] * scale, centre[1] * scale, radius * scale
    ImageDraw.Draw(mask).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    return mask.resize((size, size), Image.LANCZOS)


def load_icon():
    """The app icon as a clean square.  The source has stray pixels in the rows
    above and below the rounded square and a pale fringe around its edge, so
    this crops to the square, fills a band inside the edge from further in, and
    cuts the corners again."""
    source = Image.open(ASSETS / "icon-colour-1024.png").convert("RGBA")

    # the body is opaque and blue; the stray rows are pale or translucent
    body = Image.eval(source.getchannel("A"), lambda a: 255 if a > 200 else 0)
    red, _, blue, _ = source.split()
    bluish = ImageChops.subtract(blue, red, offset=-25)  # blue - red - 25 > 0
    bluish = Image.eval(bluish, lambda v: 255 if v > 0 else 0)
    dark = Image.eval(red, lambda v: 255 if v < 100 else 0)
    box = ImageChops.multiply(ImageChops.multiply(body, bluish), dark).getbbox()
    side = max(box[2] - box[0], box[3] - box[1])
    icon = resize(source.crop((box[0], box[1], box[0] + side, box[1] + side)), SIZE)

    # the gradient is smooth, so pixels a little way in stand in for the fringe
    band = 16
    shifted = resize(icon.crop((band, band, SIZE - band, SIZE - band)), SIZE)
    inside = rounded_mask(SIZE, RADIUS, inset=band).filter(ImageFilter.GaussianBlur(3))
    icon = Image.composite(icon, shifted, inside)
    icon.putalpha(rounded_mask(SIZE, RADIUS))
    return icon


def load_monogram():
    """The monogram's coverage, cropped to its bounds."""
    alpha = Image.open(ASSETS / "logo-white-mono-1024.png").convert("RGBA").getchannel("A")
    return alpha.crop(alpha.point(lambda a: 255 if a >= 128 else 0).getbbox())


def glyph_box(icon):
    """Bounds of the pale monogram inside the app icon."""
    red = icon.getchannel("R").point(lambda v: 255 if v > 150 else 0)
    opaque = icon.getchannel("A").point(lambda a: 255 if a > 200 else 0)
    return ImageChops.multiply(red, opaque).getbbox()


def gradient_stops(icon, count=7):
    """Samples the background gradient, which runs from the bottom-left corner
    to the top-right one, as (offset, colour) pairs.  Each sample is the median
    along a line of equal offset, leaving out the monogram."""
    red_band = icon.getchannel("R")
    pixels = icon.load()
    stops = []
    for i in range(count):
        t = 0.06 + 0.88 * i / (count - 1)
        # points with (x + (SIZE - y)) / (2 * SIZE) == t
        samples = []
        total = 2 * SIZE * t
        for x in range(0, SIZE, 4):
            y = SIZE - (total - x)
            if not 0 <= y < SIZE:
                continue
            r, g, b, a = pixels[x, int(y)]
            if a == 255 and red_band.getpixel((x, int(y))) < 100:
                samples.append((r, g, b))
        if len(samples) < 5:
            continue
        colour = tuple(sorted(s[c] for s in samples)[len(samples) // 2] for c in range(3))
        stops.append((t, colour))
    return stops


def glyph_colour(icon):
    red = icon.getchannel("R")
    pixels = icon.load()
    box = glyph_box(icon)
    samples = [pixels[x, y][:3] for x in range(box[0], box[2], 3) for y in range(box[1], box[3], 3)
               if red.getpixel((x, y)) > 200]
    return tuple(sorted(s[c] for s in samples)[len(samples) // 2] for c in range(3))


def hex_colour(colour):
    return "#%02X%02X%02X" % colour[:3]


def fade(luma):
    """Lifts the darks and lowers the contrast of a grey level, for "not connected"."""
    return int(70 + luma * 0.62)


def grey(colour):
    r, g, b = colour[:3]
    level = fade(0.299 * r + 0.587 * g + 0.114 * b)
    return (level, level, level)


def greyscale(icon):
    """The icon without colour and with less contrast, for "not connected"."""
    level = ImageOps.grayscale(icon.convert("RGB")).point(fade)
    return Image.merge("RGBA", (level, level, level, icon.getchannel("A")))


def with_badge(icon, colour, ring=False):
    """Punches a gap in the bottom-right corner and puts a status dot in it."""
    size = icon.width
    radius = BADGE_RADIUS * size
    centre = (size - radius, size - radius)
    gap = circle_mask(size, centre, radius + BADGE_GAP * size)
    result = icon.copy()
    result.putalpha(ImageChops.subtract(icon.getchannel("A"), gap))
    dot = circle_mask(size, centre, radius)
    if ring:
        dot = ImageChops.subtract(dot, circle_mask(size, centre, radius * 0.5))
    badge = Image.new("RGBA", icon.size, colour + (255,))
    badge.putalpha(dot)
    result.alpha_composite(badge)
    return result


def monogram_image(size, colour, padding=0.04, opacity=1.0):
    """The monogram alone, centred on a transparent square."""
    glyph = load_monogram()
    inner = int(size * (1 - 2 * padding))
    scale = inner / max(glyph.size)
    glyph = glyph.resize((round(glyph.width * scale), round(glyph.height * scale)), Image.LANCZOS)
    if opacity < 1.0:
        glyph = glyph.point(lambda a: int(a * opacity))
    result = Image.new("RGBA", (size, size), colour + (0,))
    solid = Image.new("RGBA", glyph.size, colour + (255,))
    solid.putalpha(glyph)
    result.alpha_composite(solid, ((size - glyph.width) // 2, (size - glyph.height) // 2))
    return result


def write_png(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, "PNG", optimize=True)
    print("wrote", path.relative_to(ROOT))


def ico_bitmap(image):
    """One icon image as a 32-bit DIB with its AND mask."""
    size = image.width
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    flipped = image.transpose(Image.FLIP_TOP_BOTTOM)
    colours = flipped.tobytes("raw", "BGRA")
    # 1 marks a transparent pixel; rows are padded to 32 bits
    row_bytes = (size + 31) // 32 * 4
    alpha = flipped.getchannel("A").load()
    mask = bytearray()
    for y in range(size):
        row = bytearray(row_bytes)
        for x in range(size):
            if alpha[x, y] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += row
    return header + colours + bytes(mask)


def write_ico(path, images):
    """Writes a .ico with a PNG at 256px, as Windows expects, and bitmaps below
    that, which every resource compiler and loader accepts."""
    blobs = []
    for image in images:
        if image.width >= 256:
            buffer = io.BytesIO()
            image.save(buffer, "PNG", optimize=True)
            blobs.append(buffer.getvalue())
        else:
            blobs.append(ico_bitmap(image))
    data = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for image, blob in zip(images, blobs):
        dimension = image.width % 256  # 0 means 256
        data += struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    data += b"".join(blobs)
    path.write_bytes(data)
    print("wrote", path.relative_to(ROOT))


def mac_icon(icon):
    """The icon on Apple's grid: an 824px square with a soft shadow, centred in
    1024px, so that it sits in the Dock at the size of other apps."""
    body = 824
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    offset = (SIZE - body) // 2
    shadow_mask = Image.new("L", (SIZE, SIZE), 0)
    shadow_mask.paste(rounded_mask(body, RADIUS * body // SIZE).point(lambda a: a * 3 // 10),
                      (offset, offset + 12))
    shadow.putalpha(shadow_mask.filter(ImageFilter.GaussianBlur(14)))
    canvas.alpha_composite(shadow)
    canvas.alpha_composite(resize(icon, body), (offset, offset))
    return canvas


def write_icns(path, image):
    sizes = [16, 32, 64, 128, 256, 512]
    image.save(path, "ICNS", append_images=[resize(image, s) for s in sizes])
    print("wrote", path.relative_to(ROOT))


def find_font(name):
    for directory in FONT_DIRS:
        if directory.is_dir():
            for candidate in directory.rglob(name):
                return str(candidate)
    sys.exit(f"font {name} not found; install Noto Sans")


def wordmark(height, colour, secondary):
    """The icon followed by "Leapdesk KVM", as one image of the given height."""
    icon = resize(load_icon(), height)
    bold = ImageFont.truetype(find_font("NotoSans-Bold.ttf"), int(height * 0.62))
    regular = ImageFont.truetype(find_font("NotoSans-Regular.ttf"), int(height * 0.62))
    gap = int(height * 0.2)
    space = int(height * 0.16)
    name_width = int(bold.getlength("Leapdesk"))
    kvm_width = int(regular.getlength("KVM"))
    width = height + gap + name_width + space + kvm_width + 2
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    image.alpha_composite(icon)
    draw = ImageDraw.Draw(image)
    # centre the capitals on the icon
    cap_top, cap_bottom = bold.getbbox("L")[1], bold.getbbox("L")[3]
    y = (height - (cap_bottom - cap_top)) / 2 - cap_top
    x = height + gap
    draw.text((x, y), "Leapdesk", font=bold, fill=colour)
    draw.text((x + name_width + space, y), "KVM", font=regular, fill=secondary)
    return image


def flatten(image, background=(255, 255, 255)):
    base = Image.new("RGBA", image.size, background + (255,))
    base.alpha_composite(image)
    return base.convert("RGB")


def write_bmp(path, image):
    image.save(path, "BMP")
    print("wrote", path.relative_to(ROOT))


def installer_images(icon, stops, glyph):
    """Images for the MSI (WiX) and Inno Setup wizards."""
    # WiX welcome and finish pages: the brand gradient down the left, white where the text goes
    dialog = Image.new("RGBA", (493, 312), (255, 255, 255, 255))
    panel_width = 164
    panel = Image.new("RGBA", (panel_width, 312))
    draw = ImageDraw.Draw(panel)
    for y in range(312):
        t = 1 - y / 311
        draw.line((0, y, panel_width, y), fill=interpolate(stops, 0.1 + 0.8 * t) + (255,))
    dialog.alpha_composite(panel)
    mark = monogram_image(104, glyph, padding=0)
    dialog.alpha_composite(mark, ((panel_width - 104) // 2, 70))
    font = ImageFont.truetype(find_font("NotoSans-Bold.ttf"), 17)
    draw = ImageDraw.Draw(dialog)
    text_width = draw.textlength("Leapdesk KVM", font=font)
    draw.text(((panel_width - text_width) / 2, 196), "Leapdesk KVM", font=font, fill=glyph)
    write_bmp(ROOT / "res" / "dialog.bmp", dialog.convert("RGB"))

    # WiX inner pages: the icon at the right-hand end of the banner
    banner = Image.new("RGBA", (493, 58), (255, 255, 255, 255))
    banner.alpha_composite(resize(icon, 44), (493 - 44 - 8, 7))
    write_bmp(ROOT / "res" / "banner.bmp", banner.convert("RGB"))

    # Inno Setup's header image at each display scale from 100% to 250%
    for size in (55, 69, 83, 96, 110, 138):
        padding = max(1, round(size * 0.04))
        image = Image.new("RGBA", (size, size), (255, 255, 255, 255))
        image.alpha_composite(resize(icon, size - 2 * padding), (padding, padding))
        write_bmp(ROOT / "dist" / "inno" / f"wizard-small-{size}.bmp", image.convert("RGB"))


def interpolate(stops, t):
    if t <= stops[0][0]:
        return stops[0][1]
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        if t <= t1:
            f = (t - t0) / (t1 - t0)
            return tuple(round(a + (b - a) * f) for a, b in zip(c0, c1))
    return stops[-1][1]


def retrace():
    """Traces the monogram PNG into .assets/monogram.svg."""
    import numpy
    import potrace

    alpha = Image.open(ASSETS / "logo-white-mono-1024.png").convert("RGBA").getchannel("A")
    # trace at twice the size from a slightly blurred edge, so that the curves
    # follow the outline rather than the steps of its anti-aliasing
    scale = 2
    big = alpha.resize((alpha.width * scale, alpha.height * scale), Image.LANCZOS)
    big = big.filter(ImageFilter.GaussianBlur(1.5))
    # potracer traces the dark pixels, so the glyph is passed as the False ones
    bitmap = potrace.Bitmap(numpy.array(big) < 128)
    path = bitmap.trace(turdsize=40, turnpolicy=potrace.POTRACE_TURNPOLICY_MINORITY,
                        alphamax=1.0, opticurve=True, opttolerance=0.2)

    def point(p):
        return "%s %s" % tuple(f"{v / scale:.1f}".rstrip("0").rstrip(".") for v in (p.x, p.y))

    parts = []
    for curve in path.curves:
        parts.append("M" + point(curve.start_point))
        for segment in curve.segments:
            if segment.is_corner:
                parts.append(f"L{point(segment.c)}L{point(segment.end_point)}")
            else:
                parts.append(f"C{point(segment.c1)} {point(segment.c2)} {point(segment.end_point)}")
        parts.append("Z")
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{alpha.width}" height="{alpha.height}" '
           f'viewBox="0 0 {alpha.width} {alpha.height}">\n'
           f'<!-- traced from logo-white-mono-1024.png by res/make-icons.py -->\n'
           f'<path fill="#fff" d="{"".join(parts)}"/>\n</svg>\n')
    (ASSETS / "monogram.svg").write_text(svg, encoding="ascii")
    print("wrote", (ASSETS / "monogram.svg").relative_to(ROOT))


def monogram_path():
    svg = (ASSETS / "monogram.svg").read_text(encoding="ascii")
    return re.search(r' d="([^"]+)"', svg).group(1)


def path_transform(box):
    """The transform that fits the traced monogram into box, keeping its shape."""
    glyph_bounds = Image.open(ASSETS / "logo-white-mono-1024.png").convert("RGBA") \
        .getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    scale = min((box[2] - box[0]) / (glyph_bounds[2] - glyph_bounds[0]),
                (box[3] - box[1]) / (glyph_bounds[3] - glyph_bounds[1]))
    tx = (box[0] + box[2]) / 2 - scale * (glyph_bounds[0] + glyph_bounds[2]) / 2
    ty = (box[1] + box[3]) / 2 - scale * (glyph_bounds[1] + glyph_bounds[3]) / 2
    return f"matrix({scale:.5f} 0 0 {scale:.5f} {tx:.2f} {ty:.2f})"


def svg_icon(stops, glyph, glyph_transform, badge=None, ring=False, faded=False):
    """The app icon as SVG, optionally with a status badge like the PNGs."""
    if faded:
        stops = [(t, grey(c)) for t, c in stops]
        glyph = grey(glyph)
    stop_tags = "".join(f'<stop offset="{t:.3f}" stop-color="{hex_colour(c)}"/>' for t, c in stops)
    defs = (f'<linearGradient id="background" x1="0" y1="{SIZE}" x2="{SIZE}" y2="0" '
            f'gradientUnits="userSpaceOnUse">{stop_tags}</linearGradient>')
    body = (f'<rect width="{SIZE}" height="{SIZE}" rx="{RADIUS}" fill="url(#background)"/>'
            f'<path fill="{hex_colour(glyph)}" transform="{glyph_transform}" d="{monogram_path()}"/>')
    extra = ""
    if badge is not None:
        radius = BADGE_RADIUS * SIZE
        centre = SIZE - radius
        defs += (f'<mask id="gap"><rect width="{SIZE}" height="{SIZE}" fill="#fff"/>'
                 f'<circle cx="{centre:g}" cy="{centre:g}" r="{radius + BADGE_GAP * SIZE:g}" fill="#000"/></mask>')
        body = f'<g mask="url(#gap)">{body}</g>'
        extra = badge_svg(centre, radius, badge, ring)
    return svg_document(f"<defs>{defs}</defs>{body}{extra}")


def badge_svg(centre, radius, colour, ring):
    if ring:
        return (f'<circle cx="{centre:g}" cy="{centre:g}" r="{radius * 0.75:g}" fill="none" '
                f'stroke="{hex_colour(colour)}" stroke-width="{radius * 0.5:g}"/>')
    return f'<circle cx="{centre:g}" cy="{centre:g}" r="{radius:g}" fill="{hex_colour(colour)}"/>'


def svg_mask_icon(badge=False, ring=False, opacity=1.0):
    """The monogram alone in black, for template (mask) tray icons."""
    padding = 0.04 * SIZE
    transform = path_transform((padding, padding, SIZE - padding, SIZE - padding))
    body = f'<path fill="#000" transform="{transform}" d="{monogram_path()}"/>'
    if opacity < 1.0:
        body = f'<g opacity="{opacity:g}">{body}</g>'
    if badge:
        radius = BADGE_RADIUS * SIZE
        centre = SIZE - radius
        mask = (f'<defs><mask id="gap"><rect width="{SIZE}" height="{SIZE}" fill="#fff"/>'
                f'<circle cx="{centre:g}" cy="{centre:g}" r="{radius + BADGE_GAP * SIZE:g}" fill="#000"/>'
                f'</mask></defs>')
        body = f'{mask}<g mask="url(#gap)">{body}</g>' + badge_svg(centre, radius, (0, 0, 0), ring)
    return svg_document(body)


def svg_document(content):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" '
            f'viewBox="0 0 {SIZE} {SIZE}">\n'
            f'<!-- generated by res/make-icons.py from .assets/ -->\n{content}\n</svg>\n')


def write_text(path, text):
    path.write_text(text, encoding="ascii", newline="\n")
    print("wrote", path.relative_to(ROOT))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--retrace", action="store_true",
                        help="trace .assets/monogram.svg again from the monogram PNG")
    args = parser.parse_args()
    if args.retrace or not (ASSETS / "monogram.svg").exists():
        retrace()

    icon = load_icon()
    stops = gradient_stops(icon)
    glyph = glyph_colour(icon)
    transform = path_transform(glyph_box(icon))
    black = (0, 0, 0)

    # Windows: executables, installer and uninstaller entry
    write_ico(ROOT / "res" / "leapdesk.ico",
              [resize(icon, s) for s in (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)])
    write_png(ROOT / "res" / "leapdesk.png", resize(icon, 256))
    installer_images(icon, stops, glyph)

    # macOS: the app bundle
    mac = mac_icon(icon)
    write_icns(ROOT / "dist" / "macos" / "bundle" / "Leapdesk.app" / "Contents" / "Resources" / "Leapdesk.icns", mac)
    write_icns(ROOT / "src" / "gui" / "res" / "mac" / "Leapdesk.icns", mac)

    # Linux: the desktop entry's icon
    write_text(ROOT / "res" / "io.github.bryceishunter.leapdesk-kvm.svg", svg_icon(stops, glyph, transform))

    # the settings window: its own icon, the About logo and the tray icon in each state
    gui = ROOT / "src" / "gui" / "res"
    write_png(gui / "icons" / "256x256" / "leapdesk.png", resize(icon, 256))
    write_png(gui / "image" / "about.png", wordmark(180, icon_teal(stops), WORDMARK_GREY))
    tray = gui / "icons" / "128x128"
    write_png(tray / "leapdesk-connected.png", resize(with_badge(icon, CONNECTED_COLOUR), 128))
    write_png(tray / "leapdesk-disconnected.png", resize(greyscale(icon), 128))
    write_png(tray / "leapdesk-transferring.png", resize(with_badge(icon, TRANSFERRING_COLOUR), 128))
    mono = monogram_image(SIZE, black)
    write_png(tray / "leapdesk-connected-mask.png", resize(with_badge(mono, black), 128))
    write_png(tray / "leapdesk-disconnected-mask.png", resize(monogram_image(SIZE, black, opacity=0.45), 128))
    write_png(tray / "leapdesk-transferring-mask.png", resize(with_badge(mono, black, ring=True), 128))

    # vector sources of the tray icons, named as the settings window asks the icon theme for them
    icons = ROOT / "res" / "icons"
    write_text(icons / "leapdesk-connected.svg", svg_icon(stops, glyph, transform, badge=CONNECTED_COLOUR))
    write_text(icons / "leapdesk-disconnected.svg", svg_icon(stops, glyph, transform, faded=True))
    write_text(icons / "leapdesk-transferring.svg", svg_icon(stops, glyph, transform, badge=TRANSFERRING_COLOUR))
    write_text(icons / "leapdesk-connected-mask.svg", svg_mask_icon(badge=True))
    write_text(icons / "leapdesk-disconnected-mask.svg", svg_mask_icon(opacity=0.45))
    write_text(icons / "leapdesk-transferring-mask.svg", svg_mask_icon(badge=True, ring=True))


def icon_teal(stops):
    """The teal at the light end of the gradient, for the wordmark."""
    return stops[-1][1]


if __name__ == "__main__":
    main()
