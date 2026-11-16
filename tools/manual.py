# OMI (NINE HANDS | DSP) - notice PDF en francais, illustree avec le rendu reel du plugin (couverture minimale commune).
# Usage : python3 tools/manual.py <dossier des captures (tools/render)> <fichier PDF de sortie>
import os, sys
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = "1.0.5"
TMP = os.path.join(ROOT, "tools", "manual_assets"); os.makedirs(TMP, exist_ok=True)
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"),
             ("Title", "BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#1F4E9A"); ACC_L = colors.HexColor("#5ACDFF")        # bleu nuit de la pedale / cyan de l'ecran
DARK = colors.HexColor("#141A26"); GREY = colors.HexColor("#5E6676"); LIGHT = colors.HexColor("#EAF0F8")

# reperes du plugin (repere = fond / 2, 900 unites de haut), ecrits par tools/make_art.py
import json
LAY = json.load(open(os.path.join(ROOT, "tools", "layout.json")))
KNOBS = [tuple(k[:2]) for k in LAY["knobs"]]          # sens, dyn, bend, trans
LED, FS, JACK_Y = tuple(LAY["led"][:2]), tuple(LAY["fs"][:2]), LAY["jackY"]
PRESET, SCREEN = tuple(LAY["preset"]), tuple(LAY["screen"])
BX0, BX1 = LAY["boxX"]

def shot(name): return Image.open(os.path.join(SHOTS, name)).convert("RGB")
src = shot("omi_status.png")
k = src.height / LAY["frame"][1]

# ---------- capture annotee ----------
marks = [(1, KNOBS[0][0] - 50, KNOBS[0][1] + 52), (2, KNOBS[1][0] - 50, KNOBS[1][1] + 52), (3, KNOBS[2][0] + 50, KNOBS[2][1] + 52),
         (4, KNOBS[3][0] + 50, KNOBS[3][1] + 52), (5, SCREEN[0] + SCREEN[2] / 2, SCREEN[1] - 14), (6, PRESET[0] + 4, PRESET[1] - 16),
         (7, LED[0] + 34, LED[1]), (8, FS[0] + 80, FS[1]), (9, BX0 + 22, JACK_Y - 66), (10, BX1 - 22, JACK_Y - 66)]
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), 30)
for n, x, y in marks:
    cx, cy, r = x * k, y * k, 27
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(31, 78, 154), width=5)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(20, 26, 38), anchor="mm")
annotated = os.path.join(TMP, "omi_annotated.png"); ann.save(annotated)

def crop_u(im, box): return im.crop(tuple(int(v * k) for v in box))
def strip(parts, path, gap=18):
    s = Image.new("RGB", (sum(p.width for p in parts) + gap * (len(parts) - 1), max(p.height for p in parts)), (255, 255, 255))
    x = 0
    for p in parts: s.paste(p, (x, 0)); x += p.width + gap
    s.save(path); return path
box_scr = (SCREEN[0] - 4, SCREEN[1] - 4, SCREEN[0] + SCREEN[2] + 4, SCREEN[1] + SCREEN[3] + 4)
scr_img = os.path.join(TMP, "omi_screen.png"); crop_u(shot("omi_note.png"), box_scr).save(scr_img)
box_on = (LED[0] - 110, LED[1] - 40, LED[0] + 110, FS[1] + 76)
on_img = strip([crop_u(shot("omi_bypass.png"), box_on), crop_u(shot("omi_note.png"), box_on)], os.path.join(TMP, "omi_onoff.png"))
box_bar = (PRESET[0] - 10, PRESET[1] - 10, PRESET[0] + PRESET[2] + 10, PRESET[1] + PRESET[3] + 10)
bar_img = os.path.join(TMP, "omi_bar.png")
_a = crop_u(shot("omi_note.png"), box_bar); _b = crop_u(src, box_bar)
_s = Image.new("RGB", (_a.width, _a.height * 2 + 14), (255, 255, 255)); _s.paste(_a, (0, 0)); _s.paste(_b, (0, _a.height + 14)); _s.save(bar_img)

# ---------- couverture : pedale detouree (boitier + jacks), reflet au sol, halo bleu tres doux ----------
# masque de la pedale (boitier + jacks) : canal alpha du fond (1800 px de haut), a la taille de la capture
msk = np.asarray(Image.open(os.path.join(ROOT, "background.png")).getchannel("A").resize(src.size, Image.LANCZOS))
cut = Image.new("RGBA", src.size, (0, 0, 0, 0)); cut.paste(shot("omi_note.png"), (0, 0), Image.fromarray(msk))   # pedale en jeu (note affichee)
cut = cut.crop(cut.getbbox())
cw, ch = cut.size
pad = int(cw * 0.16)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 30
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
halo = np.dstack([glow * 0.15, glow * 0.40, glow * 0.95, np.full_like(glow, 255)]).astype(np.uint8)
canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM)
ra = np.array(refl).astype(np.float64)
fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
ra[..., 3] *= fade[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "omi_cover_cutout.png"); canvas.convert("RGB").save(cover)

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

# ---------- styles ----------
st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "tip": ParagraphStyle("tip", fontName="Pop", fontSize=9.0, leading=12.4, textColor=DARK, spaceAfter=3),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cells": ParagraphStyle("cells", fontName="Pop", fontSize=7.8, leading=10, textColor=DARK),
    "cellbs": ParagraphStyle("cellbs", fontName="PopM", fontSize=7.8, leading=10, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="Title", fontSize=50, leading=56, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#C9BDBC"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=ACC_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items, s="p"): return [P("&bull;&nbsp;&nbsp;" + i, s) for i in items]
def table(rows, widths, header=True, small=False):
    data = [[P(c, ("cellbs" if small else "cellb") if (header and r == 0) else ("cells" if small else "cell")) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#E3D3D1")),
             ("TOPPADDING", (0, 0), (-1, -1), 2 if small else 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 2 if small else 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------- donnees (identiques au code livre : PluginProcessor.cpp) ----------
BEND = ["CHROMA", "±1", "±2", "±3", "±4", "±5", "±7", "±12", "±24"]
FACTORY = [("Neutral", -42, 5.0, 4, 0), ("7 Strings in G", -48, 7.0, 0, 0), ("Bass Mono", -48, 7.0, 0, -12), ("Bass Bend", -48, 7.0, 2, -12),
           ("8 Strings Sub", -46, 6.0, 0, 0), ("Unison", -48, 8.0, 0, 0), ("Synth Lead", -50, 8.0, 7, 0), ("Octave Up Pad", -50, 3.0, 0, 12),
           ("Fixed Velocity", -48, 0.0, 0, 0), ("Noisy Pickups", -40, 7.0, 0, -12)]
FACT_DESC = {"fr": [
 "Tous les potards à midi : le réglage chargé à l'ouverture d'Omi, point de départ neutre.",
 "Pour une 7 cordes en Sol : règle aussi Lowest note sur G1, notes franches, à la hauteur réelle (la corde de G sort en G1, déjà dans le registre de la basse).",
 "La guitare joue la basse une octave plus bas, notes franches (avec Lowest MIDI note out sur B0, le jeu remonte d'une octave si la corde la plus grave passerait sous B0).",
 "Idem, avec les glissements, bends et vibratos transmis en pitch bend (±2) : réglez le VST de basse sur ±2 s'il ignore le RPN.",
 "Pour une 8 cordes : règle aussi Lowest note sur F#1, la note jouée est envoyée telle quelle, déjà dans le registre de la basse.",
 "La note jouée, sans transposition, très dynamique : doublure synthé ou orgue.",
 "Bends et vibratos de lead sur ±12 : pour un synthé solo.",
 "Une octave au-dessus, vélocité peu dynamique : nappes et cordes.",
 "Vélocité constante (100) : pour les sons qui réagissent mal à la dynamique.",
 "Seuil plus haut (−40 dB) : micros simples, bruit de fond, ronflette."]}

T = {
"fr": dict(
 lang="FRANÇAIS", sub="Guitare vers MIDI",
 intro_t="1. Présentation",
 intro=["<b>OMI</b> est un plugin NINE HANDS | DSP qui transforme le jeu de la guitare en <b>notes MIDI</b>, dans l'esprit des interfaces GK-MIDI de Roland (GI-20). Il écoute la guitare, reconnaît la note jouée, sa justesse et sa force, et envoie notes, vélocité et pitch bend à un instrument virtuel : un VST de basse, un synthé, un orgue…",
        "Omi est <b>monophonique</b> : il suit une note à la fois, ce qui convient parfaitement aux lignes de basse. Un micro ordinaire mélange toutes les cordes : pour convertir des accords, Roland utilise un micro hexaphonique GK (une voie par corde).",
        "La détection de hauteur est <b>renforcée pour les accordages très graves</b> des 7 et 8 cordes (jusqu'à A0, 27,5 Hz) : sur ces cordes, la fondamentale captée par le micro est faible et l'harmonique 2 domine, ce qui fait d'ordinaire « décrocher » les convertisseurs à l'octave du dessus. Omi mesure chaque période candidate sur une fenêtre adaptée à sa longueur, préfère une période double nettement plus cohérente, confirme chaque nouvelle note sur plusieurs analyses et ne coupe jamais une note sur une analyse ratée.",
        "Le visuel est un design original NINE HANDS | DSP : boîtier bleu nuit métallisé, écran de note, et un motif imprimé où l'onde de la guitare entre côté IN et ressort en notes MIDI côté OUT. Omi n'est pas affilié à Roland ; « GI-20 » et « GK » ne servent qu'à décrire le principe."],
 inst_t="2. Installation",
 inst=[("Windows", "Cliquez sur le bouton <b>Télécharger Windows</b> de la page GitHub Nine-Hands-Dsp/omi (il donne toujours la dernière version), décompressez <b>Omi-VST3-Windows.zip</b> et copiez le dossier <b>Omi.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Cliquez sur <b>Télécharger macOS</b>, décompressez <b>Omi-VST3-macOS.zip</b> et copiez <b>Omi.vst3</b> dans <b>~/Bibliothèque/Audio/Plug-Ins/VST3</b>. Si macOS le bloque, tapez <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Omi.vst3</b> dans le Terminal. Le binaire est universel (Apple Silicon et Intel)."),
       ("Reaper : brancher Omi sur un VST de basse", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b> ; le plugin apparaît sous le nom <b>Omi (NINE HANDS | DSP)</b>. Mettez Omi en tête de la piste guitare : environ une seconde après, REAPER ouvre sa fenêtre <b>Insert virtual instrument on new track</b>. Choisissez votre VST de basse : la piste est créée juste sous celle d'Omi, aussitôt reliée à son MIDI (Audio : None, MIDI : All → All) et nommée « Omi - &lt;instrument&gt; » ; les fenêtres du plugin se referment d'elles-mêmes, inutile d'y toucher. Rien n'est créé quand un projet est rechargé. Un <b>clic sur l'écran</b> d'Omi ouvre le même menu : créer une autre piste (<b>Create MIDI track + choose instrument...</b>), la recréer directement avec le dernier instrument utilisé (<b>Create MIDI track with &lt;instrument&gt;</b>), ouvrir l'instrument d'une piste déjà reliée, ou décocher <b>Create the MIDI track automatically when Omi is inserted</b>. Ctrl+Z annule. Si la fenêtre de choix n'apparaît pas, regardez vos autres écrans : REAPER la rouvre là où elle était la dernière fois."),
       ("À la main", "Bouton <b>ROUTE</b> de la piste de l'instrument &gt; <b>Add new receive</b> &gt; piste guitare, réglé sur <b>Audio : None</b> et <b>MIDI : All → All</b>. Variante : placez le VST de basse juste après Omi sur la piste guitare (le MIDI d'Omi descend la chaîne d'effets) et choisissez clic droit &gt; <b>Guitar audio</b> &gt; <b>Mute</b> pour n'entendre que l'instrument.")],
 ov_t="3. Vue d'ensemble de l'interface",
 ov_rows=[["#", "Commande", "Rôle"],
   ["1", "SENS", "Seuil de détection. Glisser vers le haut / le bas, molette, double-clic = valeur par défaut, clic droit = MIDI Learn (pour toutes les commandes)."],
   ["2", "DYN", "Réponse de la vélocité."],
   ["3", "BEND", "Notes franches (CHROMA) ou plage de pitch bend."],
   ["4", "TRANS", "Transposition."],
   ["5", "Écran", "Note jouée, note MIDI envoyée, vélocité, vu-mètre et seuil. Clic : piste d'instrument REAPER."],
   ["6", "Presets / ligne d'état", "Une seule pastille : presets au repos, valeur du réglage en cours pendant un mouvement."],
   ["7", "LED", "Bleue tant qu'une note MIDI est envoyée."],
   ["8", "Footswitch", "MIDI actif / arrêt."],
   ["9", "OUT", "Sortie audio (guitare, si Guitar audio = Thru)."],
   ["10", "IN", "Entrée de la guitare."]],
 on_cap="Arrêt (BYPASS), puis en jeu : la LED bleue suit les notes envoyées.",
 ctl_t="4. Les commandes en détail",
 ctl=[("SENS (seuil)", "De −72 à −12 dB (niveau crête de la guitare). En dessous du seuil, aucune note ne part : c'est ce qui évite les fausses notes dues au bruit, aux cordes voisines qui résonnent ou au souffle des micros. Le repère ▼ du vu-mètre montre le seuil ; la partie du vu-mètre sous le seuil est atténuée. Réglez-le juste au-dessus du bruit de fond, guitare au repos : trop haut, les notes jouées doucement ne partent pas ; trop bas, des notes parasites apparaissent. La note s'arrête quand le niveau retombe 6 dB sous le seuil pendant 40 ms."),
      ("DYN (dynamique)", "Règle la façon dont la force de l'attaque devient la vélocité MIDI. À 0, la vélocité est fixe (100) ; à fond, toute la dynamique du jeu passe (environ 36 dB entre la note la plus douce et la plus forte au-dessus du seuil). Entre les deux, la vélocité se resserre autour de 100."),
      ("BEND (plage de pitch bend)", "À gauche, <b>CHROMA</b> : Omi envoie des notes franches, demi-ton par demi-ton, sans pitch bend ; un glissement ou un bend produit une suite de notes. Ensuite <b>±1, ±2, ±3, ±4, ±5, ±7, ±12, ±24</b> demi-tons : les bends, vibratos et glissements sont transmis en pitch bend, lissé, tant qu'ils restent dans la plage ; au-delà, une nouvelle note part. Omi envoie la plage au VST (message RPN 0) ; si votre VST l'ignore, réglez sa plage de pitch bend à la même valeur."),
      ("TRANS (transposition)", "De −24 à +24 demi-tons, par pas de un. −12 fait jouer une basse une octave sous la guitare. L'écran montre la note jouée à gauche et la note MIDI envoyée à droite.")],
 fs_t="5. Écran, footswitch et LED",
 fs=["<b>Écran</b> : à gauche, la note reconnue (nom, octave et écart en cents, avec une échelle de −50 à +50 cents ; le point est bleu quand la note est juste à 5 cents près) ; à droite, la note MIDI envoyée, son numéro et sa vélocité, puis la plage de bend. En bas, le vu-mètre de l'entrée et le repère du seuil SENS. En haut à droite, le canal MIDI de sortie.",
     "<b>Footswitch</b> : il bascule dès l'appui et le bouton s'assombrit tant qu'on appuie. À l'arrêt (BYPASS sur l'écran), Omi n'envoie plus rien et coupe la note en cours (All Notes Off) : aucune note ne reste bloquée.",
     "<b>LED</b> : bleue tant qu'une note MIDI est tenue, en fondu ; la façade autour et le flanc de l'écrou du footswitch reçoivent sa lumière (clic droit &gt; <b>Glow intensity</b> : 25 / 50 / 75 / 100 %).",
     "<b>Play feel</b> (clic droit) : <b>Fast</b> confirme une nouvelle note sur 2 analyses (latence minimale, conseillé pour le jeu rapide), <b>Normal</b> sur 3, <b>Stable</b> sur 5 avec une analyse plus exigeante (le moins de fausses notes). Une analyse très nette demande une confirmation de moins ; un saut d'octave en demande deux fois plus. À chaque nouvelle attaque, Omi n'analyse que ce qui la suit : en passant d'une corde à l'autre, la corde précédente ne fausse plus la note.",
     "<b>Lowest note</b> (clic droit) : la note la plus grave cherchée : E2 (6 cordes), B1 (7 cordes), G1 (7 cordes en Sol), F#1 (8 cordes), E1 (8 cordes en drop E, réglage par défaut) ou G#0 (accordages très bas). <b>Réglez-le sur votre corde la plus grave</b> : aucune note n'étant cherchée plus bas, une corde plus aiguë ne peut plus être prise pour une plus grave.",
     "<b>Lowest MIDI note out</b> (clic droit) : la plupart des basses virtuelles s'arrêtent à B0 (basse 5 cordes) ou E1 (4 cordes). Avec <b>B0</b> (par défaut) ou <b>E1</b>, si votre corde la plus grave (Lowest note), transposée par TRANS, tombe sous cette limite, <b>tout le jeu</b> monte d'une octave : l'ordre des cordes est respecté. Exemple : 7 cordes en Sol, Lowest note G1, TRANS −12 : G0 est sous B0, Omi joue donc à la hauteur réelle (G1 donne G1, D2 donne D2). <b>A0</b> pour les instruments à tessiture étendue, <b>Off</b> pour appliquer TRANS tel quel.",
     "<b>Octave hold</b> (clic droit, activé par défaut) : si la note en cours sonne encore et que vous jouez la même note une ou deux octaves au-dessus, Omi garde la première note : la basse reste sur la fondamentale pendant un jeu en octaves. Omi le reconnaît aux harmoniques impaires de la première corde, qui restent présentes tant qu'elle sonne ; si vous l'étouffez, l'octave part normalement.",
     "<b>MIDI out channel</b> (clic droit) : canal 1 à 16. <b>Guitar audio</b> : Thru (la guitare continue de passer, par défaut) ou Mute (seul le MIDI sort)."],
 midi_t="6. MIDI entrant",
 midi_intro=["Les commandes d'Omi se pilotent depuis un contrôleur MIDI (footswitchs, pédales d'expression, potards). <b>Aucun CC n'est assigné d'office</b> : vous choisissez chacun avec le <b>MIDI Learn</b>. Les CC non assignés et les autres messages MIDI entrants passent tels quels vers l'instrument suivant."],
 midi_hdr=["Commande", "Assignation", "Type"],
 midi=[["Sens", "MIDI Learn", "Continu (0-127 = toute la course)"], ["Dyn", "MIDI Learn", "Continu"], ["Bend", "MIDI Learn", "Par crans (9 plages)"],
       ["Trans", "MIDI Learn", "Par crans (demi-tons)"], ["Footswitch", "MIDI Learn", "À accrochage (127 = on / 0 = off) ou momentané (bascule à chaque appui)"]],
 learn=["<b>MIDI Learn</b> : clic droit sur une commande, choisissez <b>MIDI Learn: &lt;nom&gt;</b>, puis bougez la pédale d'expression ou appuyez sur le bouton MIDI. La ligne d'état affiche <b>MIDI LEARN · Sens</b>, puis confirme pendant 2 secondes, par ex. <b>Sens = CC 64</b>. <b>Clear MIDI</b> retire une assignation ; clic droit &gt; MIDI &gt; <b>Clear all MIDI assignments</b> les retire toutes. Les assignations sont sauvegardées avec le projet.",
        "La course réelle de votre pédale d'expression est apprise automatiquement (une pédale qui n'envoie que 2 à 116 atteint quand même 0 et 100 %). Pour un potard sur un CC de 0 à 31, le CC n+32 est lu comme sa partie fine (14 bits).",
        "<b>Footswitch MIDI</b> : à accrochage (par défaut) ou momentané, clic droit &gt; MIDI. <b>Program Change 1 à 10</b> : rappel des presets usine. Tous les paramètres sont aussi automatisables dans le DAW."],
 win_t="7. Fenêtre",
 win=["Omi utilise le format pédale simple de NINE HANDS | DSP : la fenêtre montre la pédale seule, plein cadre (visuel de 1235 × 1800 px), textes, écran et logo dessinés en vectoriel, nets à toutes les tailles ; elle s'ouvre à 60 % et ne peut jamais être réduite en dessous. Clic droit &gt; <b>Plugin size</b> : <b>60 %</b>, <b>75 %</b> ou <b>100 %</b> ; <b>Default size (new windows)</b> choisit la taille d'ouverture des nouvelles fenêtres. Vous pouvez aussi tirer le coin en bas à droite jusqu'à 150 % (proportions conservées).",
      "La <b>barre de presets</b> et la <b>ligne d'état</b> partagent la même pastille. Au repos, elle affiche le preset (ou <i>Edited</i>) avec ses flèches. Dès qu'un réglage bouge (souris, molette, automation, MIDI), la ligne d'état prend sa place en fondu et affiche la valeur en direct (par ex. <i>SENS -44 dB</i>) ou les messages du MIDI Learn ; 2,5 s après le dernier mouvement, la barre de presets revient. Un clic sur la pastille la fait revenir tout de suite.",
      "Les menus clic droit s'ouvrent à l'endroit du clic."],
 bar_cap="Barre de presets au repos, ligne d'état pendant un réglage.",
 pre_t="8. Presets",
 pre=["Utilisez les flèches de la barre de presets, cliquez sur son nom, faites un clic droit n'importe où, ou passez par le menu de presets de votre DAW. Clic droit &gt; <b>Save user preset</b> enregistre jusqu'à 9 presets personnels (avec Play feel et Lowest note)."],
 fac_hdr=["PC", "Preset", "Sens / Dyn / Bend / Trans", "Description"],
 tips_t="9. Conseils d'utilisation",
 tips=["Placez Omi <b>en tête</b> de la piste guitare, avant les saturations et l'ampli (Ampire, Impulse) : il lui faut le son direct de la guitare. Un noise gate placé avant n'est pas utile : le seuil SENS en fait office.",
       "Jouez <b>proprement, une note à la fois</b>, en étouffant la corde précédente : deux cordes qui sonnent ensemble brouillent la détection. Le micro manche donne une fondamentale plus forte, donc un suivi plus rapide et plus sûr sur les cordes graves.",
       "Sur une 7 ou 8 cordes, réglez <b>Lowest note</b> sur votre corde la plus grave et gardez <b>Play feel</b> sur Normal ; passez sur Stable si des notes parasites apparaissent, sur Fast pour un jeu rapide dans les médiums.",
       "Il faut une à deux périodes d'une corde pour en connaître la hauteur : comptez environ 15 à 30 ms dans les médiums et les aigus, environ 40 ms en jeu rapide sur les cordes graves d'une 7 cordes en Sol et environ 80 ms sur A0. Étouffez la corde précédente quand vous changez de corde (paume ou main gauche) : deux cordes qui sonnent ensemble forment un accord, qu'un micro ordinaire ne permet pas de séparer."],
 tech_t="10. Caractéristiques techniques",
 tech=[["Format", "VST3 avec sortie MIDI (Windows 64 bits, macOS universel)"], ["Entrée", "Mono ou stéréo (les voies sont additionnées pour l'analyse)"],
       ["Sortie MIDI", "Note On / Off, vélocité, pitch bend (lissé), RPN 0 (plage de bend), All Notes Off ; un canal (1-16)"],
       ["Détection", "monophonique ; passe-haut 18 Hz, passe-bas Butterworth 6e ordre à 1,4 kHz, décimation à ≈ 8 kHz ; méthode McLeod (NSDF) à fenêtre par décalage (0,8 période, 12 ms min.), analyse limitée à ce qui suit chaque attaque, interpolation parabolique, correction d'octave prudente, médiane glissante, confirmations, hystérésis 0,6 demi-ton"],
       ["Plage", "de la note réglée dans Lowest note (jusqu'à G#0, 26 Hz) à environ 1,5 kHz"],
       ["Latence", "≈ 15–30 ms (médiums, aigus), ≈ 40 ms (cordes graves en jeu rapide), ≈ 80 ms (A0) ; aucune latence audio"],
       ["Processeur", "environ 2 à 3 % d'un cœur à 48 kHz"],
       ["MIDI entrant", "MIDI Learn sur les 5 commandes (rien d'assigné par défaut), CC 14 bits, Program Change 1-10, autres messages transmis"],
       ["Fenêtre", "Plugin size 60 % / 75 % / 100 %, jusqu'à 150 %, jamais sous 60 % ; anti-gel v2 sous Windows"]]),
}

COVER_TEXT = {}
W = A4[0] - 36 * mm
LOGO_PNG = os.path.join(TMP, "nine_hands_logo_blanc.png")
import subprocess
subprocess.run(["rsvg-convert", "-w", "1200", os.path.join(ROOT, "NineHandsLogoWhite.svg"), "-o", LOGO_PNG], check=True)
def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p"))
    COVER_TEXT.update(lang=t["lang"], sub=t["sub"])      # couverture dessinee a positions fixes (cover_bg)
    f.append(Spacer(1, 1)); f.append(PageBreak())
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(PageBreak())
    f.append(P(t["ov_t"], "h1"))
    ov = Table([[[img(annotated, 80 * mm)],
                 [table(t["ov_rows"], [9 * mm, 27 * mm, W - 96 * mm - 36 * mm], small=True), Spacer(1, 5 * mm), img(on_img, 58 * mm), P(t["on_cap"], "small")]]],
               colWidths=[96 * mm, W - 96 * mm])
    ov.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(ov); f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(P(t["fs_t"], "h1")); f.append(img(scr_img, 110 * mm)); f.append(Spacer(1, 2 * mm)); f += bullets(t["fs"])
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi_intro"]]
    f.append(KeepTogether([table([t["midi_hdr"]] + t["midi"], [36 * mm, 26 * mm, W - 62 * mm])]))
    f.append(Spacer(1, 2 * mm)); f += bullets(t["learn"])
    f.append(P(t["win_t"], "h1")); f.append(P(t["win"][0]))
    wt = Table([[[P(t["win"][1]), P(t["win"][2])], [img(bar_img, 66 * mm), P(t["bar_cap"], "small")]]], colWidths=[W - 70 * mm, 70 * mm])
    wt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (0, 0), 0)]))
    f.append(wt)
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    rows = [t["fac_hdr"]]
    for i, (n, sn, dy, bd, tr) in enumerate(FACTORY):
        rows.append([str(i + 1), "<b>" + n + "</b>", f"{sn} dB / {round(dy * 10)} % / {BEND[bd]} / {'+' if tr > 0 else ''}{tr}", FACT_DESC[lang][i]])
    f.append(table(rows, [8 * mm, 28 * mm, 34 * mm, W - 70 * mm], small=True))
    # conseils + caracteristiques ensemble : le tableau ne se retrouve jamais seul sur une page
    f.append(KeepTogether([P(t["tips_t"], "h1")] + bullets(t["tips"], "tip") +
                          [P(t["tech_t"], "h1"), table(t["tech"], [28 * mm, W - 28 * mm], header=False, small=True)])); f.append(Spacer(1, 4 * mm))
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Omi (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="OMI - Notice v" + VERSION, author="NINE HANDS | DSP")
# ---- Couverture « minimale » (modele 6, valide nov. 2026) : gabarit fixe commun a toutes les notices ----
# Mesures en mm depuis le HAUT de la page A4, sauf le logo et le bandeau (depuis le bas). Aucun autre element.
COVER_NAME = "OMI"
PEDAL_H, PEDAL_MAXW, PEDAL_TOP = 82 * mm, 115 * mm, 84 * mm      # pedale (boitier + jacks, sans reflet) centree horizontalement
LANG_TOP = 18 * mm                                               # « FRANÇAIS » centre, Poppins Medium 8,5 pt, accent clair
NAME_TOP, NAME_PT = 194 * mm, 30                                 # nom : Barlow Condensed ExtraBold 30 pt, approche 0,5 x corps
SUB_TOP, SUB_PT = 208 * mm, 10                                   # mention courte : Poppins 10 pt, approche 1,5 pt, gris clair
LOGO_W, LOGO_BOTTOM = 38 * mm, 22 * mm                           # logo blanc centre en bas
BAND_H = 3 * mm                                                  # bandeau couleur d'accent en BAS de page
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, A4[0], BAND_H, stroke=0, fill=1)
    def line(text, top, font, size, cs, col):
        w = pdfmetrics.stringWidth(text, font, size) + cs * (len(text) - 1)
        t = c.beginText((A4[0] - w) / 2, A4[1] - top - size * 0.8); t.setFont(font, size); t.setCharSpace(cs)
        t.setFillColor(col); t.textOut(text); c.drawText(t)
    line(COVER_TEXT["lang"], LANG_TOP, "PopM", 8.5, 0, ACC_L)
    ci = Image.open(cover); iw, ih = ci.size
    cw_ = iw / 1.32; ch_ = (ih - int(cw_ * 0.16)) / 1.2; pad_ = int(cw_ * 0.16)     # geometrie de omi_cover_cutout.png
    kk = min(PEDAL_H / ch_, PEDAL_MAXW / cw_)
    dw, dh = iw * kk, ih * kk
    y0 = A4[1] - (PEDAL_TOP + (PEDAL_H - ch_ * kk) / 2) + pad_ * kk
    c.drawImage(cover, (A4[0] - dw) / 2, y0 - dh, dw, dh)
    line(COVER_NAME, NAME_TOP - NAME_PT * 0.02, "Title", NAME_PT, NAME_PT * 0.5, colors.white)
    line(COVER_TEXT["sub"], SUB_TOP, "Pop", SUB_PT, 1.5, colors.HexColor("#BDC4B8"))
    lw, lh = Image.open(LOGO_PNG).size
    c.drawImage(LOGO_PNG, (A4[0] - LOGO_W) / 2, LOGO_BOTTOM, LOGO_W, LOGO_W * lh / lw, mask="auto")
    c.restoreState()
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
fr = section("fr")              # notices en francais uniquement (couverture noire FRANCAIS)
doc.build(fr[:-1])
print(OUT)
