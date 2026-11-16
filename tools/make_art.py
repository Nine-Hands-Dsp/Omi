"""
OMI (NINE HANDS | DSP) - visuel du plugin, entierement synthetise : pedale simple (image 1235 x 1800, pedale plein cadre).

Convertisseur guitare -> MIDI dans l'esprit des interfaces GK-MIDI Roland (GI-20) : habillage original NINE HANDS | DSP.
Boitier alu peint bleu nuit metallise, serigraphie creme (Poppins, dessinee en vectoriel par le plugin), ecran de note
en haut (vitre noire encastree, contenu dessine par le plugin), quatre potards SENS / DYN / BEND / TRANS, nom OMI,
pastille presets / etat, LED bleue (activite MIDI), footswitch, logo NINE HANDS | DSP blanc.
Motif imprime a l'encre sombre : l'onde de la guitare entre par IN (a droite) et ressort en notes MIDI (piano roll) vers OUT.
Une seule lumiere : en haut a gauche a 45 degres (ombres vers le bas a droite) + eclairage de facade a 45 degres.
Pieces photo du kit NINE HANDS | DSP (potard, LED, footswitch, embases jack) : tools/photo_controls.py.
Usage : python3 tools/make_art.py
"""
import os, sys, math, subprocess, io, json
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import photo_controls as PC

AS = 2
UW, UH = 760, 958
W, H = UW * AS, UH * AS
L = np.array([-0.55, -0.55, 0.63]); L /= np.linalg.norm(L)          # lumiere en haut a gauche, 45 degres
V = np.array([0.0, 0.0, 1.0]); HV = (L + V) / np.linalg.norm(L + V)

OUT_H, OUT_W = 1800, 1235
JACK_LEN = 104.0
_jtest = PC.jack_sprite(JACK_LEN, AS, 1)
JACK_PROT = _jtest[1] - 1.5 / AS
PY0, PH, PR = 34.0, 890.0, 30.0
PW = OUT_W / OUT_H * PH - 2 * JACK_PROT
PX0 = (UW - PW) / 2
CX = PX0 + PW / 2
CROP_X0, CROP_W = PX0 - JACK_PROT, PW + 2 * JACK_PROT
SC = (OUT_H / 2.0) / PH

# ------------------------------------------------------------------ geometrie (unites de travail)
SCREEN = (PX0 + 44.0, 66.0, PW - 88.0, 168.0)             # ecran de note (vitre encastree)
KNOB_R = 46.0
KNOB_Y = 338.0
KSP = 128.0
KNOBS = [(CX + (i - 1.5) * KSP, KNOB_Y, n, KNOB_R) for i, n in enumerate(('SENS', 'DYN', 'BEND', 'TRANS'))]
TITLE_Y, SUB_Y = 470.0, 522.0
PRESET = (CX - 200.0 * PW / 566.0 * 0 - 200.0, 588.0, 400.0, 44.0)   # pastille de reference 400 x 38 (unites plugin) recalee plus bas
LED = (CX, 708.0)
FS = (CX, 804.0)
LOGO_C, LOGO_W = (CX, 896.0), 196.0
JACK_Y = PY0 + 0.62 * PH

YY, XX = np.mgrid[0:H, 0:W].astype(np.float32)
UX, UY = (XX + 0.5) / AS, (YY + 0.5) / AS

def rrect_sd(x, y, x0, y0, w, h, r):
    cx, cy = x0 + w / 2, y0 + h / 2
    qx = np.abs(x - cx) - (w / 2 - r); qy = np.abs(y - cy) - (h / 2 - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r

def fbm(shape, scales, amps, seed=0):
    r = np.random.default_rng(seed); out = np.zeros(shape, np.float32)
    for s, a in zip(scales, amps):
        n = r.standard_normal((max(2, shape[0] // s + 2), max(2, shape[1] // s + 2))).astype(np.float32)
        out += a * cv2.resize(n, (shape[1], shape[0]), interpolation=cv2.INTER_CUBIC)
    return out

def over(dst, rgba, x0, y0):
    h, w = rgba.shape[:2]
    X0, Y0 = int(round(x0)), int(round(y0))
    sx0, sy0 = max(0, -X0), max(0, -Y0)
    ex, ey = min(w, W - X0), min(h, H - Y0)
    a = rgba[sy0:ey, sx0:ex, 3:4] / 255.0
    reg = dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex]
    dst[Y0 + sy0:Y0 + ey, X0 + sx0:X0 + ex] = reg * (1 - a) + rgba[sy0:ey, sx0:ex, :3] * a

# ------------------------------------------------------------------ 1. boitier : peinture bleu nuit metallisee
img = np.zeros((H, W, 3), np.float32) + 14.0
sd = rrect_sd(UX, UY, PX0, PY0, PW, PH, PR)
body = np.clip(0.5 - sd * AS, 0, 1)
EDGE = 9.0
d_in = np.clip(-sd, 0, None)
hgt = np.where(d_in < EDGE, np.sqrt(np.clip(EDGE ** 2 - (EDGE - d_in) ** 2, 0, None)), EDGE)
gy, gx = np.gradient(cv2.GaussianBlur(hgt, (0, 0), 1.0), 1.0 / AS)
nrm = np.dstack([-gx, -gy, np.ones_like(hgt)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
ndl = np.clip(nrm @ L, 0, 1); ndh = np.clip(nrm @ HV, 0, 1)
paint = np.array([20.0, 34.0, 62.0])                                   # bleu nuit
encl = paint * (0.50 + 0.80 * ndl)[..., None] + (64 * ndh ** 40 + 16 * ndh ** 6)[..., None] * np.array([0.85, 0.92, 1.0])
rng = np.random.default_rng(11)
flake = cv2.GaussianBlur((rng.random((H, W)) ** 18).astype(np.float32), (0, 0), 0.6) * 150
sheen = np.exp(-(((UX - 260) / 320) ** 2 + ((UY - 230) / 330) ** 2))
grain = fbm((H, W), [1, 2], [1.3, 0.8], 3) + 0.7 * fbm((H, W), [90, 260], [0.7, 0.9], 4)
encl += grain[..., None] * np.array([0.5, 0.65, 1.0])
encl += (flake * (0.35 + 0.9 * sheen))[..., None] * np.array([0.55, 0.7, 1.0])
encl += (20.0 * sheen)[..., None] * np.array([0.45, 0.6, 1.0])
img = img * (1 - body[..., None]) + encl * body[..., None]

# ------------------------------------------------------------------ 2. motif imprime : onde (IN, a droite) -> notes MIDI (OUT, a gauche)
SS = 2
art = np.zeros((H * SS, W * SS), np.uint8)
K = AS * SS
ART_Y = 664.0
# onde amortie a droite : du bord droit vers le centre, epaisseur 3 unites
xs = np.linspace(CX + 30, PX0 + PW - 8, 900)
t = (PX0 + PW - xs) / (PW / 2)
ys = ART_Y + 17.0 * np.sin(2 * math.pi * (PX0 + PW - xs) / 44.0) * (0.10 + 0.90 * (1 - t) ** 1.4)   # attaque a droite (IN), s'eteint vers le centre
pts = np.stack([xs * K, ys * K], 1).astype(np.int32)
cv2.polylines(art, [pts], False, 255, int(3.0 * K), cv2.LINE_AA)
# piano roll a gauche : notes en escalier (blocs arrondis)
notes = [(-12, 34, 0), (-52, 30, 0), (-88, 22, 2), (-116, 40, 1), (-162, 26, 3), (-194, 30, 3), (-230, 24, 5)]
for (x_off, wdt, step) in notes:
    x1 = CX + x_off; x0 = x1 - wdt
    if x0 < PX0 + 10: continue
    y = ART_Y + 14 - step * 7.0
    cv2.rectangle(art, (int(x0 * K), int((y - 3.5) * K)), (int(x1 * K), int((y + 3.5) * K)), 255, -1, cv2.LINE_AA)
art = cv2.resize(art.astype(np.float32) / 255.0, (W, H), interpolation=cv2.INTER_AREA)
keep = np.zeros((H, W), np.float32)
keep = np.maximum(keep, (np.hypot(UX - LED[0], UY - LED[1]) < 22).astype(np.float32))
keep = np.maximum(keep, (np.hypot(UX - FS[0], UY - FS[1]) < 66).astype(np.float32))

keep = cv2.GaussianBlur(keep, (0, 0), 1.2 * AS)
on_top = np.clip((-sd - 6.0) / 2.0, 0, 1)
art_a = art * 0.55 * (1 - np.clip(keep * 1.6, 0, 1)) * on_top
img = img * (1 - art_a[..., None]) + np.array([8.0, 13.0, 26.0]) * art_a[..., None]

# ------------------------------------------------------------------ 3. ecran encastre (vitre noire ; le contenu est dessine par le plugin)
sx, sy, sw, sh = SCREEN
s_sd = rrect_sd(UX, UY, sx, sy, sw, sh, 12.0)
bez_sd = rrect_sd(UX, UY, sx - 7, sy - 7, sw + 14, sh + 14, 18.0)
bez = np.clip(0.5 - bez_sd * AS, 0, 1)
win = np.clip(0.5 - s_sd * AS, 0, 1)
# cuvette : paroi ombree cote lumiere (haut gauche), arete eclairee en bas a droite
inner = np.clip(-bez_sd, 0, 7) / 7.0
gyb, gxb = np.gradient(cv2.GaussianBlur(bez_sd, (0, 0), 2.0))
facing = np.clip(-(gxb * L[0] + gyb * L[1]) * 4.0, -1, 1)
bez_col = paint * 0.38 + 26.0 * np.clip(-facing, 0, 1)[..., None] * np.array([0.8, 0.9, 1.0]) - 6.0 * np.clip(facing, 0, 1)[..., None]
img = img * (1 - bez[..., None]) + bez_col * bez[..., None]
# vitre : presque noire, teinte bleutee, reflet diagonal tres discret (fixe)
glass = np.array([5.0, 7.0, 11.0]) + (fbm((H, W), [1], [0.6], 7))[..., None]
refl = np.clip(1 - np.abs(((UX - sx) + (UY - sy)) - 70.0) / 60.0, 0, 1) * 7.0
glass = glass + refl[..., None] * np.array([0.8, 0.9, 1.0])
vign = np.clip(1 - np.clip(-s_sd, 0, 10) / 10.0, 0, 1)
glass = glass * (1 - 0.6 * vign)[..., None]
img = img * (1 - win[..., None]) + glass * win[..., None]
# ombre portee interne en haut a gauche de la vitre
ish = cv2.GaussianBlur(np.clip(0.5 - rrect_sd(UX - 4, UY - 4, sx, sy, sw, sh, 12.0) * AS, 0, 1), (0, 0), 3 * AS)
img *= (1 - 0.45 * win * (1 - ish))[..., None]

# ------------------------------------------------------------------ 4. eclairage de facade a 45 degres (fond seulement)
diag = ((UX - PX0) + (UY - PY0)) / (PW + PH)
gain = 1.10 - 0.24 * np.clip(diag, 0, 1)
satin = 12.0 * np.exp(-((diag - 0.34) / 0.10) ** 2)
img = img * (1 + (gain - 1) * body)[..., None] + (satin * body * (1 - bez))[..., None]
img += (fbm((H, W), [1], [0.8], 22) * body * (1 - win))[..., None]

# ------------------------------------------------------------------ 5. potards, LED, footswitch (photos du kit)
def occlusion(mask, strength=1.0):
    contact = cv2.GaussianBlur(mask, (0, 0), 3.0 * AS)
    soft = cv2.GaussianBlur(mask, (0, 0), 14.0 * AS)
    o = np.clip(contact * 0.55 + soft * 0.9, 0, 0.5) * strength * (1.0 - mask)
    img[...] = img * (1.0 - o * body)[..., None]
_m = np.zeros((H, W), np.float32)
for (x, y, _, KR) in KNOBS:
    _m = np.maximum(_m, np.clip((KR - np.hypot(UX - x, UY - y)) * AS, 0, 1))
_m = np.maximum(_m, np.clip((12.0 * 1.07 - np.hypot(UX - LED[0], UY - LED[1])) * AS, 0, 1) * 0.6)
FS_FLATS = 100.0
FSPR, FSREFL, FS_W, FS_BTN = PC.footswitch_sprite(FS_FLATS, AS, refl_colour=(60.0, 140.0, 255.0))
_fa = np.zeros((H, W), np.float32); _n = FSPR.shape[0]
_x0 = int(round((FS[0] - FS_W / 2) * AS)); _y0 = int(round((FS[1] - FS_W / 2) * AS))
_fa[_y0:_y0 + _n, _x0:_x0 + _n] = np.clip((FSPR[..., 3] / 255.0 - 0.86) / 0.14, 0, 1)
_m = np.maximum(_m, _fa)
occlusion(_m)
KN, KS = PC.knob_sprite(KNOB_R, AS)
for (x, y, _, KR) in KNOBS:
    over(img, KN, (x - KS / 2) * AS, (y - KS / 2) * AS)
Image.fromarray(np.clip(KN, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'design', 'knob_sprite.png'))
LED_LENS = 12.0
LD, LDON, LS = PC.led_sprites(LED_LENS, AS, colour='blue')
over(img, LD, (LED[0] - LS / 2) * AS, (LED[1] - LS / 2) * AS)
Image.fromarray(np.clip(LDON, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'led_on.png'))
over(img, FSPR, (FS[0] - FS_W / 2) * AS, (FS[1] - FS_W / 2) * AS)
Image.fromarray(FSREFL, 'RGBA').save(os.path.join(ROOT, 'fs_refl.png'))

# ------------------------------------------------------------------ 6. jacks : OUT a gauche, IN a droite
alpha = body.copy()
for side in (-1, 1):
    j, jw_u, jh_u = PC.jack_sprite(JACK_LEN, AS, side)
    jh, jw = j.shape[:2]
    jw2 = int(math.floor(JACK_PROT * AS))
    if jw2 != jw:
        pm_ = j.copy(); pm_[..., :3] *= pm_[..., 3:4] / 255.0
        pm_ = cv2.resize(pm_, (jw2, jh), interpolation=cv2.INTER_AREA)
        a_ = np.maximum(pm_[..., 3:4], 1e-3); pm_[..., :3] = np.where(pm_[..., 3:4] > 0.5, pm_[..., :3] * 255.0 / a_, 0)
        j = pm_; jw = jw2
    x0 = int(round((PX0 * AS - jw) if side < 0 else ((PX0 + PW) * AS)))
    y0 = int(round(JACK_Y * AS - jh / 2))
    occ = np.zeros((H, W), np.float32); occ[y0:y0 + jh, x0:x0 + jw] = j[..., 3] / 255.0
    occ = np.clip(cv2.GaussianBlur(occ, (0, 0), 3.0 * AS) * 0.9 + cv2.GaussianBlur(occ, (0, 0), 18.0 * AS) * 1.6, 0, 0.62)
    occ *= 0.2 if side > 0 else 0.0
    img *= (1.0 - occ * body)[..., None]
    over(img, j, x0, y0)
    ja = np.zeros((H, W), np.float32); ja[y0:y0 + jh, x0:x0 + jw] = j[..., 3] / 255.0
    alpha = np.maximum(alpha, ja)

# ------------------------------------------------------------------ 7. sorties
def to_out(a):
    x0, x1 = CROP_X0 * AS, (CROP_X0 + CROP_W) * AS
    y0, y1 = PY0 * AS, (PY0 + PH) * AS
    M = np.float32([[OUT_W / (x1 - x0), 0, -x0 * OUT_W / (x1 - x0)], [0, OUT_H / (y1 - y0), -y0 * OUT_H / (y1 - y0)]])
    return cv2.warpAffine(a, M, (OUT_W, OUT_H), flags=cv2.INTER_AREA if OUT_H < (y1 - y0) else cv2.INTER_CUBIC)
out_rgb = np.clip(to_out(img), 0, 255); out_a = np.clip(to_out(alpha), 0, 1)
Image.fromarray(np.dstack([out_rgb, out_a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'background.png'), optimize=True)

# halo de la LED bleue sur la facade (calque alpha)
GR = 110.0
x0p, y0p = round((LED[0] - GR) * AS), round((LED[1] - GR) * AS); size = round(2 * GR * AS)
reg = img[y0p:y0p + size, x0p:x0p + size].astype(np.float64)
yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
d = np.hypot((xx + 0.5) / AS - GR, (yy + 0.5) / AS - GR)
E = 1.1 * 8.0 ** 3 / (d ** 2 + 64.0) ** 1.5 + 0.176 * 30.0 ** 3 / (d ** 2 + 900.0) ** 1.5
E *= np.clip((GR - d) / 30.0, 0, 1); E[d < LED_LENS * 1.07] = 0
col = np.array([0.25, 0.55, 1.0])
lum = reg.mean(2, keepdims=True)
add = (reg * 2.2 + 12.0 * np.clip(lum / 40.0, 0.3, 1.6)) * E[..., None] * col
a = np.clip(np.abs(add).max(-1) / 12.0, 0, 1)
rgb = np.where(a[..., None] > 1e-3, reg + add / np.maximum(a, 1e-3)[..., None], reg)
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), a * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'glow_blue.png'))
GLR = (x0p / AS, y0p / AS, size / AS, size / AS)

def X(v): return (v - CROP_X0) * SC
def Y(v): return (v - PY0) * SC
pw = 400.0                                                             # pastille de reference (unites plugin)
json.dump({"frame": [OUT_W / 2, OUT_H / 2],
           "knobs": [[X(k[0]), Y(k[1]), k[3] * SC, k[2]] for k in KNOBS],
           "screen": [X(SCREEN[0]), Y(SCREEN[1]), SCREEN[2] * SC, SCREEN[3] * SC],
           "led": [X(LED[0]), Y(LED[1]), LS * SC], "glow": [X(GLR[0]), Y(GLR[1]), GLR[2] * SC, GLR[3] * SC],
           "fs": [X(FS[0]), Y(FS[1]), FS_BTN * SC, FS_W * SC],
           "preset": [OUT_W / 4 - pw / 2, Y(PRESET[1]), pw, 38.0],
           "title": [X(CX), Y(TITLE_Y)], "sub": [X(CX), Y(SUB_Y)],
           "logo": [X(LOGO_C[0]), Y(LOGO_C[1]), LOGO_W * SC],
           "jackY": Y(JACK_Y), "inX": X(PX0 + PW - 26), "outX": X(PX0 + 26), "boxX": [X(PX0), X(PX0 + PW)]},
          open(os.path.join(ROOT, 'tools', 'layout.json'), 'w'), indent=1)
print('ok', PW, JACK_PROT)
