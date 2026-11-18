"""
OMI (NINE HANDS | DSP) - visuel du plugin v2 (nov. 2026), d'apres la maquette de l'utilisateur
assets/omi2/Omi_Design_Mockup_v001.svg : proportions, positions et tailles reprises a l'identique.

Pedale simple : image 1235 x 1800 (boitier + jacks plein cadre). On travaille directement dans le repere de la maquette
(unites SVG), a AS px par unite, puis on reduit a 1800 px de haut (meme echelle en largeur).
  - boitier E5E6E7, bevel plat exterieur dans la meme teinte (58595B dans la maquette) ;
  - grand ecran : vitre noire (rugosite 0.25) dans une cuvette a bevel plat inverse A5A7AA ;
  - petit ecran des presets et LED : decoupes dans le boitier, arete exterieure eclairee ;
  - potards (photo du kit), footswitch (photo du kit), inverseur DRY / MIDI (photos fournies), diode blanche
    (photo fournie, bleue allumee), embases jack (photo du kit, ecrou seul) ; les photos eclairees d'en haut a gauche
    (potard, footswitch, inverseur, diode) ne sont retournees que si la lumiere est mise a droite (LX = +1) ;
  - lumiere en haut a GAUCHE a 45 degres (LX, regle de l'utilisateur) : ombres portees vers le bas a droite
    (pas pour le petit ecran, la LED ni les jacks), occlusion partout sauf les jacks.
Serigraphie (textes Orbitron, logo OMI Borg 9, graduations, logo NINE HANDS | DSP) : vectorielle, dessinee par le plugin
(tools/make_layout.py).
Usage : python3 tools/make_art.py
"""
import os, sys, math, json
import numpy as np, cv2
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
KIT = os.path.join(ROOT, 'assets', 'kit'); O2 = os.path.join(ROOT, 'assets', 'omi2')

AS = 2.0                                   # px de travail par unite de maquette
OUT_W, OUT_H = 1235, 1800
MH = 1809.75                               # hauteur de la maquette (boitier)
SC = OUT_H / MH                            # unites maquette -> px de l'image
CXB = 1952.54                              # axe du boitier
FW = OUT_W / SC                            # largeur du cadre en unites maquette
FX0 = CXB - FW / 2
W, H = int(round(FW * AS)), int(round(MH * AS))

LX = -1.0                                  # cote de la lumiere : -1 = en haut a gauche (regle de l'utilisateur), +1 = en haut a droite
Lv = np.array([0.55 * LX, -0.55, 0.63], np.float32); Lv /= np.linalg.norm(Lv)  # lumiere en haut, 45 degres
MIRROR = LX > 0                            # photos du kit / fournies eclairees d'en haut a gauche : retournees si la lumiere est a droite
Hv = (Lv + np.array([0, 0, 1], np.float32)); Hv /= np.linalg.norm(Hv)

# ------------------------------------------------------------------ geometrie (maquette)
BOX_O = (1360.04, 0.0, 1185.0, 1809.75, 82.36)            # contour exterieur (bevel 58595B)
BEVEL_PX = 30.0                                            # largeur du bevel plat exterieur dans l'image 1800 px (demande de l'utilisateur)
_bw = BEVEL_PX / (1800.0 / 1809.75)
BOX_F = (BOX_O[0] + _bw, BOX_O[1] + _bw, BOX_O[2] - 2 * _bw, BOX_O[3] - 2 * _bw, BOX_O[4] - _bw)   # face E5E6E7
SCR_O = (1467.54, 100.0, 970.0, 500.0, 47.47)              # ouverture de la cuvette (bevel inverse A5A7AA)
SCR_G = (1474.54, 107.0, 956.0, 486.0, 40.47)              # vitre du grand ecran
PRE_G = (1601.04, 1226.0, 703.0, 88.0, 44.0)               # vitre du petit ecran (presets)
RIM = 3.0                                                  # arete eclairee autour des decoupes
LED_C, LED_R = (1952.54, 1381.9), 26.5
KNOB_R = 82.5
KNOBS = [(1577.54, 795.0), (1827.54, 795.0), (2077.54, 795.0), (2327.54, 795.0)]
FS_C, FS_FLATS = (1952.54, 1564.0), 200.0
TOG_C, TOG_H = (2274.09, 1564.0), 114.18                   # inverseur : centre et hauteur exterieure de la gorge
JACK_Y0, JACK_LEN = 1012.5, 175.0

yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
UX = FX0 + (xx + 0.5) / AS; UY = (yy + 0.5) / AS
del yy, xx

def rrect_sd(r):
    x0, y0, w, h, rad = r
    cx, cy = x0 + w / 2, y0 + h / 2
    qx = np.abs(UX - cx) - (w / 2 - rad); qy = np.abs(UY - cy) - (h / 2 - rad)
    return (np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rad).astype(np.float32)

def circ_sd(c, r):
    return (np.hypot(UX - c[0], UY - c[1]) - r).astype(np.float32)

def cov(sd):                                # couverture anti-crenelee
    return np.clip(0.5 - sd * AS, 0, 1).astype(np.float32)

def fbm(scales, amps, seed):
    r = np.random.default_rng(seed); out = np.zeros((H, W), np.float32)
    for s, a in zip(scales, amps):
        n = r.standard_normal((max(2, H // s + 2), max(2, W // s + 2))).astype(np.float32)
        out += a * cv2.resize(n, (W, H), interpolation=cv2.INTER_CUBIC)
    return out

def grad_dir(sd, blur=1.0):
    gy, gx = np.gradient(cv2.GaussianBlur(sd, (0, 0), blur))
    n = np.maximum(np.hypot(gx, gy), 1e-6)
    return gx / n, gy / n                    # direction exterieure (sd croissant)

def shift(a, dx, dy):                       # dx, dy en unites
    return cv2.warpAffine(a, np.float32([[1, 0, dx * AS], [0, 1, dy * AS]]), (W, H), borderValue=0)

def blur(a, s):                             # s en unites
    return cv2.GaussianBlur(a, (0, 0), max(0.3, s * AS))

def flat_bevel(gx, gy, base, inverted, spec=26.0, gain=0.873):
    """bevel plat a 45 degres : normale inclinee vers l'exterieur (ou l'interieur si inverse)"""
    k = -1.0 if inverted else 1.0
    nx, ny, nz = k * gx * 0.7071, k * gy * 0.7071, np.full_like(gx, 0.7071)
    ndl = np.clip(nx * Lv[0] + ny * Lv[1] + nz * Lv[2], 0, 1)
    ndh = np.clip(nx * Hv[0] + ny * Hv[1] + nz * Hv[2], 0, 1)
    col = np.asarray(base, np.float32) * (1.0 - 0.629 * gain + gain * ndl)[..., None]   # face a plat = teinte de base
    return col + (spec * ndh ** 24)[..., None]

img = np.zeros((H, W, 3), np.float32)

# ------------------------------------------------------------------ 1. boitier : bevel plat + face
sd_o, sd_f = rrect_sd(BOX_O), rrect_sd(BOX_F)
body = cov(sd_o)
face = cov(sd_f)
gx, gy = grad_dir(sd_f, 2.0)
bev = flat_bevel(gx, gy, (229, 230, 231), False, spec=4.0, gain=0.70)            # bevel plat exterieur dans la teinte du boitier (demande de l'utilisateur)
bev += fbm([4, 9], [1.0, 0.7], 2)[..., None]          # grain >= 4 px : pas de moire a l'agrandissement
img = bev * body[..., None]
# face : E5E6E7, eclairage d'ambiance a 45 degres depuis le haut a droite + satin + grain de peinture tres fin
diag = ((BOX_F[0] + BOX_F[2] - UX if LX > 0 else UX - BOX_F[0]) + (UY - BOX_F[1])) / (BOX_F[2] + BOX_F[3])
gain = 1.025 - 0.06 * np.clip(diag, 0, 1)
satin = 5.0 * np.exp(-((diag - 0.30) / 0.10) ** 2)
fcol = np.array([229.0, 230.0, 231.0], np.float32) * gain[..., None] + satin[..., None]
fcol += (fbm([4, 8], [0.8, 0.6], 3) + 0.6 * fbm([120, 380], [0.6, 0.8], 4))[..., None]
img = img * (1 - face[..., None]) + fcol * face[..., None]
del bev, gx, gy, diag, gain, satin, fcol
FACE_COL = np.array([229.0, 230.0, 231.0], np.float32)

def shade_face(amount):                     # assombrit la face (ombres, occlusion) ; amount 0..1
    img[...] = img * (1 - np.clip(amount, 0, 0.95) * face)[..., None]

def drop_shadow(mask, h, amt=0.55, contact=0.35):
    """ombre portee d'un objet de hauteur h (unites) : vers le bas a gauche (lumiere en haut a droite)"""
    cast = blur(shift(mask, -0.7 * h * LX, 0.7 * h), 0.55 * h) * amt
    cont = blur(shift(mask, -0.12 * h * LX, 0.12 * h), 0.12 * h) * contact
    return np.clip(cast + cont, 0, 0.85) * (1 - mask)

def occlusion(mask, s1=3.0, s2=14.0, k1=0.30, k2=0.22):
    return np.clip(blur(mask, s1) * k1 + blur(mask, s2) * k2, 0, 0.45) * (1 - mask)

# ------------------------------------------------------------------ 2. vitres (rugosite 0.25) : noir, reflet large et doux
def glass(r, depth):
    sd = rrect_sd(r); a = cov(sd)
    x0, y0, w, h, _ = r
    tr = np.hypot((UX - (x0 + w if LX > 0 else x0)) / w, (UY - y0) / h)   # distance au coin haut cote lumiere
    refl = 16.0 * np.exp(-(tr / 0.55) ** 2)
    band = 6.0 * np.exp(-((((x0 + w - UX if LX > 0 else UX - x0) + (UY - y0)) / (w + h) - 0.28) / 0.09) ** 2)
    col = np.array([5.0, 5.5, 7.0], np.float32) + (refl + band)[..., None] * np.array([0.92, 0.96, 1.0], np.float32)
    col += fbm([4], [0.5], 9)[..., None]
    # ombre interne : l'arete haute / droite masque la lumiere ; occlusion le long des bords
    lit = blur(shift(a, -depth * LX, depth), 0.5 * depth)
    edge = np.clip(-sd, 0, 2.5 * depth) / (2.5 * depth)
    col *= (0.55 + 0.45 * lit * (0.5 + 0.5 * edge))[..., None]
    return a, col

# grand ecran : cuvette a bevel plat inverse (A5A7AA), puis vitre
sd_so = rrect_sd(SCR_O); cup = cov(sd_so)
gx, gy = grad_dir(sd_so, 2.0)
wall = flat_bevel(gx, gy, (165, 167, 170), True)
# ombre de l'arete haute / droite sur la paroi du bas / gauche n'existe pas (paroi eclairee) ; la paroi haute / droite est sombre par sa normale
img = img * (1 - cup[..., None]) + wall * cup[..., None]
ga, gcol = glass(SCR_G, 7.0)
img = img * (1 - ga[..., None]) + gcol * ga[..., None]
shade_face(occlusion(cup, 2.0, 8.0, 0.10, 0.06))
del gx, gy, wall, gcol

# decoupes (88C8FC dans la maquette) : petit ecran des presets et LED. La bande de la decoupe (3 unites) est noire :
# c'est le trou dans le boitier ; juste a l'exterieur, l'arete du boitier recoit la lumiere (bas / gauche face a la lumiere).
def cutout(sd_hole):
    """sd_hole : contour exterieur de la decoupe ; tout l'interieur est noir"""
    hole = cov(sd_hole)
    gx, gy = grad_dir(sd_hole, 1.0)
    rim = np.clip(1 - np.abs(sd_hole - 1.0) / 1.0, 0, 1) * (sd_hole > -0.3)      # arete de 2 unites hors de la decoupe
    rim = blur(rim.astype(np.float32), 0.35)
    facing = -(gx * Lv[0] + gy * Lv[1]) / math.hypot(Lv[0], Lv[1])               # pente de l'arete tournee vers la lumiere
    lit = np.clip(facing, 0, 1); dark = np.clip(-facing, 0, 1)
    img[...] = img + (rim * (30.0 * lit + 9.0) * face)[..., None]
    img[...] = img * (1 - 0.32 * rim * dark * face)[..., None]
    shade_face(occlusion(hole, 1.5, 6.0, 0.12, 0.08))
    img[...] = img * (1 - hole[..., None]) + np.array([3.0, 3.0, 4.0], np.float32) * hole[..., None]
    return hole

# petit ecran : decoupe noire de 3 unites autour de la vitre (contour 88C8FC exterieur : 1599,5 .. 2305,5 x 1224,5 .. 1315,5)
cutout(rrect_sd((PRE_G[0] - 1.5, PRE_G[1] - 1.5, PRE_G[2] + 3.0, PRE_G[3] + 3.0, PRE_G[4] + 1.5)))
pa, pcol = glass(PRE_G, 4.0)
img = img * (1 - pa[..., None]) + pcol * pa[..., None]
# LED : decoupe noire jusqu'a r = 28 (anneau 88C8FC 25..28), la diode (r 26,5) posee dedans
cutout(circ_sd(LED_C, 28.0))
del pcol

# ------------------------------------------------------------------ 3. pieces photo
def load(path):
    return np.asarray(Image.open(path).convert('RGBA')).astype(np.float32)

def place(src, c, k, cx, cy, mirror=False):
    """pose la photo (point c, echelle k unites/px photo) centree en (cx, cy) unites ; renvoie rgb, alpha plein cadre"""
    pm = src.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
    if mirror:
        pm = pm[:, ::-1].copy(); c = (src.shape[1] - 1 - c[0], c[1])
    kk = k * AS
    k1 = min(1.0, kk * 2.0)                                    # reduction en deux temps (net, sans crenelage)
    if k1 < 1.0:
        pm = cv2.resize(pm, None, fx=k1, fy=k1, interpolation=cv2.INTER_AREA); c = (c[0] * k1, c[1] * k1); kk /= k1
    M = np.float32([[kk, 0, (cx - FX0) * AS - c[0] * kk], [0, kk, cy * AS - c[1] * kk]])
    out = cv2.warpAffine(pm, M, (W, H), flags=cv2.INTER_AREA if kk < 1 else cv2.INTER_CUBIC, borderValue=0)
    a = np.clip(out[..., 3] / 255.0, 0, 1)
    rgb = np.where(a[..., None] > 1e-4, out[..., :3] / np.maximum(a, 1e-4)[..., None], 0)
    return rgb.astype(np.float32), a.astype(np.float32)

def comp(rgb, a):
    img[...] = img * (1 - a[..., None]) + rgb * a[..., None]

sys.path.insert(0, HERE)
import photo_controls as PC

# potards : photo du kit sans repere (le repere dessine par le plugin tourne), retournee pour la lumiere en haut a droite
kn = PC.knob_clean()
kmask = np.zeros((H, W), np.float32)
krgb, ka = [], []
for (x, y) in KNOBS:
    r_, a_ = place(kn, PC.KNOB_C, KNOB_R / PC.KNOB_RPX, x, y, mirror=MIRROR)
    krgb.append(r_); ka.append(a_); kmask = np.maximum(kmask, a_)
# footswitch : photo du kit (ecrou hexagonal + bouton), retournee
fsp = load(os.path.join(KIT, 'footswitch_photo.png'))
fs_rgb, fs_a = place(fsp, PC.FS_C, FS_FLATS / PC.FS_FLATS_PX, FS_C[0], FS_C[1], mirror=MIRROR)
FS_BTN = PC.FS_BTN_PX * FS_FLATS / PC.FS_FLATS_PX
# inverseur : gorge chromee (meme contour sur les deux photos) ; ombre + occlusion dans le fond, photos dans le plugin
TOG_PH = {'up': ('Selecteur_argente_de_pedale_guitare.png', (395, 851, 116, 1126)),
          'down': ('Interrupteur_chrome_bouton_en_bas.png', (393, 854, 114, 1127))}
tog_mask = np.clip(0.5 - rrect_sd((TOG_C[0] - 28.55, TOG_C[1] - TOG_H / 2, 57.1, TOG_H, 28.55)) * AS, 0, 1)

shade_face(occlusion(kmask, 3.0, 16.0, 0.30, 0.20))
shade_face(drop_shadow(kmask, 40.0, 0.50, 0.40))        # ombres x2 (demande de l'utilisateur)
shade_face(occlusion(fs_a, 3.0, 16.0, 0.30, 0.20))
shade_face(drop_shadow(fs_a, 32.0, 0.45, 0.40))
shade_face(occlusion(tog_mask, 2.0, 9.0, 0.30, 0.18))
shade_face(drop_shadow(tog_mask, 16.0, 0.40, 0.35))
for r_, a_ in zip(krgb, ka): comp(r_, a_)
comp(fs_rgb, fs_a)
del krgb, ka

# diode (eteinte) : photo fournie, posee dans sa decoupe
dio = load(os.path.join(O2, 'Diode_Blanche_Custom.png'))
if MIRROR: dio = dio[:, ::-1].copy()                                    # retournee : reflet du cote de la lumiere
DIO_C, DIO_R = ((75.5 if MIRROR else 74.5), 74.5), 47.0
d_rgb, d_a = place(dio, DIO_C, LED_R / DIO_R, LED_C[0], LED_C[1])
comp(d_rgb, d_a)

# ------------------------------------------------------------------ 4. jacks : photo du kit, ecrou seul (175 x 26 unites)
jp = load(os.path.join(KIT, 'jack_photo.png'))[110:188, 46:558]          # ecrou hexagonal (sans la rondelle)
alpha = body.copy()
for side in (-1, 1):
    rot = np.rot90(jp, k=-1 if side > 0 else 1).copy()                   # face collee au boitier vers le boitier
    th = rot.shape[1] * JACK_LEN / rot.shape[0]                          # epaisseur (unites)
    cx = (BOX_O[0] - th / 2) if side < 0 else (BOX_O[0] + BOX_O[2] + th / 2)
    r_, a_ = place(rot, (rot.shape[1] / 2, rot.shape[0] / 2), JACK_LEN / rot.shape[0], cx, JACK_Y0 + JACK_LEN / 2)
    g = np.clip(1.08 - 0.22 * (UY - JACK_Y0) / JACK_LEN, 0.86, 1.08)
    r_ = r_ * g[..., None]
    comp(r_, a_); alpha = np.maximum(alpha, a_)
JACK_T = th

# ------------------------------------------------------------------ 5. sorties
def to_out(a):
    return cv2.resize(a, (OUT_W, OUT_H), interpolation=cv2.INTER_AREA)

bg = np.round(np.dstack([np.clip(to_out(img), 0, 255), np.clip(to_out(alpha), 0, 1) * 255])).astype(np.uint8)   # arrondi : alpha 255 exact (pas de lignes a 254)
Image.fromarray(bg, 'RGBA').save(os.path.join(ROOT, 'background.png'), optimize=True)

def P(x, y): return ((x - FX0) * SC / 2.0, y * SC / 2.0)      # maquette -> repere du plugin (617,5 x 900)
U = SC / 2.0

# diode allumee (bleue) : meme photo, recoloree ; halo bleu sur la face (calque)
n = int(round(2 * LED_R * U * 4)); k = n / (2 * DIO_R)
pm = dio.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
sm = cv2.resize(pm[int(DIO_C[1] - DIO_R):int(DIO_C[1] + DIO_R) + 1, int(DIO_C[0] - DIO_R):int(DIO_C[0] + DIO_R) + 1], (n, n), interpolation=cv2.INTER_AREA)
a_ = np.clip(sm[..., 3] / 255.0, 0, 1)
lum = np.where(a_ > 1e-4, sm[..., :3].mean(2) / np.maximum(a_, 1e-4), 0) / 255.0
on = np.dstack([255 * (0.10 + 0.75 * lum ** 2.2), 255 * (0.28 + 0.65 * lum ** 1.4), 255 * np.clip(0.75 + 0.35 * lum, 0, 1)])
Image.fromarray(np.dstack([np.clip(on, 0, 255), a_ * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'led_on.png'))
GR = 120.0                                                                 # rayon du halo (unites maquette)
gn = int(round(2 * GR * U * 2)); gyy, gxx = np.mgrid[0:gn, 0:gn].astype(np.float32)
d = np.hypot((gxx + 0.5) / gn * 2 * GR - GR, (gyy + 0.5) / gn * 2 * GR - GR)
E = 1.0 * 14.0 ** 3 / (d ** 2 + 196.0) ** 1.5 + 0.22 * 48.0 ** 3 / (d ** 2 + 2304.0) ** 1.5
E *= np.clip((GR - d) / 40.0, 0, 1); E[d < LED_R] = 0
Image.fromarray(np.dstack([np.full((gn, gn), 50.0), np.full((gn, gn), 130.0), np.full((gn, gn), 255.0),
                           np.clip(E * 0.85, 0, 1) * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'glow_blue.png'))

# reflet bleu de la LED sur le flanc haut de l'ecrou du footswitch (calque en fondu avec la LED)
fsS = FS_FLATS * 1.9
n = int(round(fsS * U * 4))
_, refl_rgba, _, _ = PC.footswitch_sprite(FS_FLATS * U, 4)
refl_rgba = cv2.resize((refl_rgba[:, ::-1] if MIRROR else refl_rgba).copy(), (n, n), interpolation=cv2.INTER_AREA)
Image.fromarray(refl_rgba, 'RGBA').save(os.path.join(ROOT, 'fs_refl.png'))

# inverseur : 2 images (haut = DRY, bas = MIDI), carre centre sur la gorge, 4 px par unite du plugin
TOG_S = 140.0                                                            # cote du carre (unites maquette)
n = int(round(TOG_S * U * 4)); frames = []
for key in ('up', 'down'):
    f, (x0, x1, y0, y1) = TOG_PH[key]
    src = load(os.path.join(O2, f))
    if MIRROR:                                                           # retournee : lumiere en haut a droite
        src = src[:, ::-1].copy(); x0, x1 = src.shape[1] - 1 - x1, src.shape[1] - 1 - x0
    k_ = (TOG_H * U * 4) / (y1 - y0 + 1)
    c_ = ((x0 + x1) / 2, (y0 + y1) / 2)
    pm = src.copy(); pm[..., :3] *= pm[..., 3:4] / 255.0
    M = np.float32([[k_, 0, n / 2 - c_[0] * k_], [0, k_, n / 2 - c_[1] * k_]])
    o = cv2.warpAffine(cv2.resize(pm, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA),
                       np.float32([[2 * k_, 0, n / 2 - c_[0] * k_], [0, 2 * k_, n / 2 - c_[1] * k_]]), (n, n), flags=cv2.INTER_AREA, borderValue=0)
    a_ = np.clip(o[..., 3] / 255.0, 0, 1)
    rgb = np.where(a_[..., None] > 1e-4, o[..., :3] / np.maximum(a_, 1e-4)[..., None], 0)
    frames.append(np.dstack([np.clip(rgb, 0, 255), a_ * 255]).astype(np.uint8))
Image.fromarray(np.concatenate(frames, 0), 'RGBA').save(os.path.join(ROOT, 'toggle_strip.png'), optimize=True)

kp = [P(x, y) for (x, y) in KNOBS]
json.dump({"frame": [OUT_W / 2, OUT_H / 2], "fx0": FX0, "sc": SC, "lightX": LX,
           "knobs": [[p[0], p[1], KNOB_R * U] for p in kp],
           "screen": [*P(SCR_G[0], SCR_G[1]), SCR_G[2] * U, SCR_G[3] * U, SCR_G[4] * U],
           "preset": [*P(PRE_G[0], PRE_G[1]), PRE_G[2] * U, PRE_G[3] * U],
           "led": [*P(*LED_C), 2 * LED_R * U],
           "glow": [*P(LED_C[0] - GR, LED_C[1] - GR), 2 * GR * U, 2 * GR * U],
           "fs": [*P(*FS_C), FS_BTN * U, fsS * U],
           "toggle": [*P(*TOG_C), TOG_S * U, 28.55 * U, TOG_H * U],
           "jackY": P(0, JACK_Y0 + JACK_LEN / 2)[1],
           "boxX": [P(BOX_O[0], 0)[0], P(BOX_O[0] + BOX_O[2], 0)[0]]},
          open(os.path.join(HERE, 'layout.json'), 'w'), indent=1)
print('ok', W, H, 'jack', round(JACK_T, 1))
