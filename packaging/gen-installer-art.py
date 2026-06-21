#!/usr/bin/env python3
"""Generate the WiX installer artwork (banner.bmp + dialog.bmp) from brand assets.

WiX sizes (fixed by Windows Installer):
  banner.bmp  493 x 58   — top banner on License / InstallDir / Progress dialogs
  dialog.bmp  493 x 312  — full background of the Welcome / Finish dialogs

Layout keeps the WiX-drawn (black) dialog text on a light area:
  - dialog: dark branded left band (~165px) + light text area on the right
  - banner: light, branding on the far right so the dialog title (left) stays readable
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE   = os.path.dirname(os.path.abspath(__file__))
ROOT   = os.path.dirname(HERE)
ICON   = os.path.join(ROOT, "resources", "branding", "app_icon.png")

CREAM  = (244, 241, 234)
DARK   = (21, 23, 28)
ORANGE = (255, 140, 66)
WHITE  = (255, 255, 255)
MUTED  = (154, 160, 170)
INK    = (26, 26, 26)

def font(names, size):
    for n in names:
        for path in (n, os.path.join(r"C:\Windows\Fonts", n)):
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                continue
    return ImageFont.load_default()

F_BOLD = lambda s: font(["segoeuib.ttf", "arialbd.ttf"], s)
F_REG  = lambda s: font(["segoeui.ttf", "arial.ttf"], s)

def load_icon(box):
    im = Image.open(ICON).convert("RGBA")
    im.thumbnail((box, box), Image.LANCZOS)
    return im

def centered(draw, text, fnt, cx, y, fill):
    w = draw.textlength(text, font=fnt)
    draw.text((cx - w / 2, y), text, font=fnt, fill=fill)

def make_dialog(path):
    W, H, BAND = 493, 312, 165
    img = Image.new("RGB", (W, H), CREAM)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, BAND, H], fill=DARK)
    d.rectangle([BAND, 0, BAND + 3, H], fill=ORANGE)

    icon = load_icon(104)
    img.paste(icon, ((BAND - icon.width) // 2, 60), icon)
    cx = BAND / 2
    centered(d, "GlyphPDF", F_BOLD(26), cx, 178, WHITE)
    centered(d, "Privacy-first PDF", F_REG(12), cx, 214, MUTED)
    centered(d, "No cloud · No telemetry", F_REG(11), cx, 232, (110, 116, 126))
    img.save(path, "BMP")
    print("wrote", path, img.size)

def make_banner(path):
    W, H = 493, 58
    img = Image.new("RGB", (W, H), CREAM)
    d = ImageDraw.Draw(img)
    d.rectangle([0, H - 2, W, H], fill=ORANGE)
    icon = load_icon(38)
    img.paste(icon, (W - icon.width - 14, (H - icon.height) // 2), icon)
    label = "GlyphPDF"
    fnt = F_BOLD(18)
    lw = d.textlength(label, font=fnt)
    d.text((W - icon.width - 14 - lw - 8, (H - 22) // 2), label, font=fnt, fill=INK)
    img.save(path, "BMP")
    print("wrote", path, img.size)

if __name__ == "__main__":
    make_dialog(os.path.join(HERE, "dialog.bmp"))
    make_banner(os.path.join(HERE, "banner.bmp"))
