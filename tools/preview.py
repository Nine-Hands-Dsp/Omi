"""Apercu de l'interface d'OMI : fond + ce que le plugin dessine en vectoriel (serigraphie, reperes des potards,
contenu de l'ecran, pastille, LED allumee, logo). Sortie : design/preview.png (1235 x 1800) et design/preview_small.png."""
import os, sys, math, json, subprocess, io
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LY = json.load(open(os.path.join(ROOT, 'tools', 'layout.json')))
P = 2.0                                              # pixels par unite du plugin
F = os.path.join(ROOT, 'assets', 'fonts')
def font(w, s): return ImageFont.truetype(os.path.join(F, f'Poppins-{w}.ttf'), int(round(s * P)))

bg = Image.open(os.path.join(ROOT, 'background.png')).convert('RGBA')
Wd, Hd = bg.size
canvas = Image.new('RGBA', bg.size, (14, 14, 15, 255)); canvas.alpha_composite(bg)

def paste(name, rect):
    im = Image.open(os.path.join(ROOT, name)).convert('RGBA')
    x, y, w, h = [v * P for v in rect]
    im = im.resize((int(round(w)), int(round(h))), Image.LANCZOS)
    canvas.alpha_composite(im, (int(round(x)), int(round(y))))

# LED allumee + halo + reflet sur le footswitch
paste('glow_blue.png', LY['glow'])
lx, ly, ls = LY['led']; paste('led_on.png', (lx - ls / 2, ly - ls / 2, ls, ls))
fx, fy, fb, fw = LY['fs']; paste('fs_refl.png', (fx - fw / 2, fy - fw / 2, fw, fw))

hi = Image.new('RGBA', (Wd * 2, Hd * 2), (0, 0, 0, 0))     # calque vectoriel sur-echantillonne
d = ImageDraw.Draw(hi)
Q = P * 2
CREAM = (236, 228, 208, 255)

def spaced(x, y, txt, f, track, fill, anchor='m'):
    widths = [f.getlength(c) for c in txt]
    total = sum(widths) + track * Q * (len(txt) - 1)
    cx = x * Q - total / 2 if anchor == 'm' else (x * Q if anchor == 'l' else x * Q - total)
    for c, w in zip(txt, widths):
        d.text((cx, y * Q), c, font=f, fill=fill, anchor='lm'); cx += w + track * Q
    return total / Q
def fontQ(w, s): return ImageFont.truetype(os.path.join(F, f'Poppins-{w}.ttf'), int(round(s * Q)))

# potards : nom, graduations, repere
vals = {'SENS': 0.62, 'DYN': 0.7, 'BEND': 0.5, 'TRANS': 0.25}
f_lab = fontQ('Medium', 21)
for (x, y, r, name) in LY['knobs']:
    spaced(x, y - r - 30, name, f_lab, 3.0, CREAM)
    for i in range(11):
        a = math.radians(-135 + 27 * i); rr = r + 11
        px, py = x + rr * math.sin(a), y - rr * math.cos(a)
        d.ellipse(((px - 1.8) * Q, (py - 1.8) * Q, (px + 1.8) * Q, (py + 1.8) * Q), fill=CREAM)
    a = math.radians(-135 + 270 * vals[name])
    x0, y0 = x + 0.42 * r * math.sin(a), y - 0.42 * r * math.cos(a)
    x1, y1 = x + 0.86 * r * math.sin(a), y - 0.86 * r * math.cos(a)
    d.line((x0 * Q, y0 * Q, x1 * Q, y1 * Q), fill=(240, 236, 226, 255), width=int(3.4 * Q))

# titre et sous-titre
tx, ty = LY['title']; sx_, sy_ = LY['sub']
tw = spaced(tx, ty, 'OMI', fontQ('Bold', 62), 14.0, CREAM)
f_sub = fontQ('Light', 18)
sw = spaced(sx_, sy_, 'GUITAR TO MIDI', f_sub, 7.0, CREAM)
for sgn in (-1, 1):
    a0 = sx_ + sgn * (sw / 2 + 14); a1 = sx_ + sgn * (sw / 2 + 64)
    d.rectangle((min(a0, a1) * Q, (sy_ - 0.9) * Q, max(a0, a1) * Q, (sy_ + 0.9) * Q), fill=CREAM)

# ecran : contenu
sx, sy, sw_, sh = LY['screen']
CY = (90, 205, 255, 255); DIM = (90, 205, 255, 90); WH = (235, 244, 255, 255); GR = (120, 140, 165, 255)
pad = 16
d.text(((sx + pad) * Q, (sy + 16) * Q), 'IN', font=fontQ('Medium', 12), fill=GR, anchor='lm')
d.text(((sx + sw_ - pad) * Q, (sy + 16) * Q), 'MIDI OUT  CH 1', font=fontQ('Medium', 12), fill=GR, anchor='rm')
# note detectee (grande) a gauche, note MIDI envoyee a droite
d.text(((sx + pad) * Q, (sy + 64) * Q), 'G', font=fontQ('Bold', 64), fill=WH, anchor='lm')
gw = fontQ('Bold', 64).getlength('G') / Q
d.text(((sx + pad + gw + 5) * Q, (sy + 80) * Q), '1', font=fontQ('Medium', 26), fill=WH, anchor='lm')
d.text(((sx + pad + gw + 28) * Q, (sy + 50) * Q), '+4 ct', font=fontQ('Medium', 13), fill=CY, anchor='lm')
ax, ay = sx + sw_ / 2 + 8, sy + 64
d.line(((ax - 18) * Q, ay * Q, (ax + 16) * Q, ay * Q), fill=GR, width=int(2 * Q))
d.line(((ax + 8) * Q, (ay - 8) * Q, (ax + 17) * Q, ay * Q, (ax + 8) * Q, (ay + 8) * Q), fill=GR, width=int(2 * Q), joint='curve')
d.text(((sx + sw_ - pad) * Q, (sy + 58) * Q), 'G0', font=fontQ('Bold', 40), fill=CY, anchor='rm')
d.text(((sx + sw_ - pad) * Q, (sy + 90) * Q), 'NOTE 19 · VEL 96', font=fontQ('Medium', 12), fill=GR, anchor='rm')
# barre de justesse (pitch bend) sous la note
bx0, bx1, byc = sx + pad, sx + sw_ / 2 - 20, sy + 108
d.line((bx0 * Q, byc * Q, bx1 * Q, byc * Q), fill=(60, 80, 105, 255), width=int(1.2 * Q))
for k in range(-4, 5):
    xk = (bx0 + bx1) / 2 + k * (bx1 - bx0) / 8
    hk = 5 if k == 0 else 3
    d.line((xk * Q, (byc - hk) * Q, xk * Q, (byc + hk) * Q), fill=(80, 100, 125, 255), width=int(1.2 * Q))
xm = (bx0 + bx1) / 2 + 0.08 * (bx1 - bx0) / 2
d.ellipse(((xm - 4) * Q, (byc - 4) * Q, (xm + 4) * Q, (byc + 4) * Q), fill=CY)
d.text(((sx + sw_ - pad) * Q, (byc) * Q), 'BEND ±2', font=fontQ('Medium', 12), fill=GR, anchor='rm')
# vu-metre d'entree + seuil SENS
mx0, mx1, my = sx + pad, sx + sw_ - pad, sy + sh - 22
segs = 40; lvl = 0.71; thr = 0.38
for i in range(segs):
    a0 = mx0 + i * (mx1 - mx0) / segs; a1 = a0 + (mx1 - mx0) / segs - 2.2
    on = i / segs < lvl
    c = CY if on else (30, 44, 62, 255)
    if on and i / segs > 0.88: c = (255, 140, 90, 255)
    d.rectangle((a0 * Q, (my - 5) * Q, a1 * Q, (my + 5) * Q), fill=c)
xt = mx0 + thr * (mx1 - mx0)
d.polygon([(xt * Q, (my - 8) * Q), ((xt - 5) * Q, (my - 15) * Q), ((xt + 5) * Q, (my - 15) * Q)], fill=WH)
d.line((xt * Q, (my - 8) * Q, xt * Q, (my + 8) * Q), fill=WH, width=int(1.6 * Q))
d.text(((xt + 9) * Q, (my - 15) * Q), 'SENS', font=fontQ('Medium', 10), fill=WH, anchor='lm')

# pastille presets
px, py, pw, ph = LY['preset']
d.rounded_rectangle((px * Q, py * Q, (px + pw) * Q, (py + ph) * Q), radius=ph / 2 * Q, fill=(6, 10, 18, 215), outline=(70, 90, 120, 160), width=int(1 * Q))
d.text(((px + pw / 2) * Q, (py + ph / 2) * Q), 'P1  Bass Mono', font=fontQ('Medium', 23 * 0.72), fill=(236, 240, 246, 255), anchor='mm')
for sgn, xx in ((-1, px + 17), (1, px + pw - 17)):
    t = 5.5
    d.polygon([((xx + sgn * t) * Q, (py + ph / 2) * Q), ((xx - sgn * t * 0.6) * Q, (py + ph / 2 - t) * Q), ((xx - sgn * t * 0.6) * Q, (py + ph / 2 + t) * Q)], fill=(160, 166, 176, 255))

# IN / OUT
f_io = fontQ('Medium', 16)
for txt, xx in (('OUT', LY['outX']), ('IN', LY['inX'])):
    w = int(f_io.getlength(txt) + 3 * Q * (len(txt) - 1)) + 8; h = int(f_io.size * 1.6)
    t = Image.new('RGBA', (w, h), (0, 0, 0, 0)); td = ImageDraw.Draw(t); x = 4
    for c in txt:
        td.text((x, h / 2), c, font=f_io, fill=CREAM, anchor='lm'); x += f_io.getlength(c) + 3 * Q
    t = t.rotate(90, expand=True)
    hi.alpha_composite(t, (int(xx * Q - t.width / 2), int(LY['jackY'] * Q - t.height / 2)))

hi = hi.resize((Wd, Hd), Image.LANCZOS)
canvas.alpha_composite(hi)

# logo (SVG officiel blanc)
lx, ly, lw = LY['logo']
png = subprocess.run(['rsvg-convert', '-w', str(int(lw * P)), os.path.join(ROOT, 'NineHandsLogoWhite.svg')], capture_output=True, check=True).stdout
lg = Image.open(io.BytesIO(png)).convert('RGBA')
canvas.alpha_composite(lg, (int(lx * P - lg.width / 2), int(ly * P - lg.height / 2)))

canvas.convert('RGB').save(os.path.join(ROOT, 'design', 'preview.png'))
canvas.convert('RGB').resize((Wd * 493 // Hd, 493), Image.LANCZOS).save(os.path.join(ROOT, 'design', 'preview_60.png'))
print('ok')
