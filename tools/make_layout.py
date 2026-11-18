"""OMI v2 : Layout.h (repere du plugin 617,5 x 900 = fond 1235 x 1800 / 2) d'apres tools/layout.json (ecrit par make_art.py)
et la maquette assets/omi2/Omi_Design_Mockup_v001.svg.
Serigraphie vectorielle convertie en traces (fontTools), positions, tailles et interlettrages de la maquette :
  - noir : SENS / DYN / BEND / TRANS, OUT / IN, DRY / MIDI (Orbitron Medium), graduations des potards ;
  - gris 424243 : logo OMI (Borg 9) et AUDIO 2 MIDI (Orbitron Medium).
Calques du repere des potards pour nh::Knob (knob_marker.png, knob_shadow.png : 1024 x 1024, repere vers le haut)."""
import os, json, math
import numpy as np, cv2
from PIL import Image
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
LY = json.load(open(os.path.join(HERE, 'layout.json')))
F = os.path.join(ROOT, 'assets', 'fonts')
FX0, SC = LY['fx0'], LY['sc']
U = SC / 2.0

def text_path(font_file, txt, size, ls_em, tx, ty, rot=0.0, sx=1.0):
    """meme rendu que <text transform="translate(tx ty) rotate(rot) scale(sx 1)"> de la maquette ; renvoie un trace SVG
    dans le repere du plugin"""
    f = TTFont(os.path.join(F, font_file)); gs = f.getGlyphSet(); cmap = f.getBestCmap()
    upm = f['head'].unitsPerEm; k = size / upm
    a = math.radians(rot); ca, sa = math.cos(a), math.sin(a)
    pen = SVGPathPen(gs); x = 0.0
    for ch in txt:
        gn = cmap.get(ord(ch))
        if gn is None: continue
        # glyphe (unites police, y vers le haut) -> local (x + gx k, -gy k) -> scale(sx,1) -> rotate -> translate -> plugin
        m = [[sx * k, 0.0, sx * x], [0.0, -k, 0.0]]
        A = [[ca * m[0][0] - sa * m[1][0], ca * m[0][1] - sa * m[1][1], ca * m[0][2] - sa * m[1][2] + tx],
             [sa * m[0][0] + ca * m[1][0], sa * m[0][1] + ca * m[1][1], sa * m[0][2] + ca * m[1][2] + ty]]
        T = (A[0][0] * U, A[1][0] * U, A[0][1] * U, A[1][1] * U, (A[0][2] - FX0) * U, A[1][2] * U)
        gs[gn].draw(TransformPen(pen, T))
        x += gs[gn].width * k + ls_em * size
    return pen.getCommands()

OM = 'Orbitron-Medium.ttf'
black = [
    text_path(OM, 'SENS', 36, 0.1, 1507.21, 670.98, 0, 1.07),
    text_path(OM, 'DYN', 36, 0.1, 1774.07, 670.98, 0, 1.07),
    text_path(OM, 'BEND', 36, 0.1, 2006.87, 670.98, 0, 1.07),
    text_path(OM, 'TRANS', 36, 0.1, 2239.27, 670.98, 0, 1.07),
    text_path(OM, 'OUT', 30, 0.1, 1431.31, 1143.64, -90, 1.07),
    text_path(OM, 'IN', 30, 0.1, 2473.02, 1073.70, 90, 1.07),
    text_path(OM, 'DRY', 26.24, 0.1, 2236.08, 1483.15, 0, 1.07),
    text_path(OM, 'MIDI', 26.24, 0.2, 2236.23, 1664.44, 0, 1.07),
]
grey = [
    text_path('borg9.ttf', 'OMI', 286.41, 0.0, 1589.4, 1125.94, 0, 0.96),
    text_path(OM, 'AUDIO 2 MIDI', 46.02, 0.74, 1601.86, 1177.69, 0, 1.0),
]
# graduations : 11 points de -135 a +135 degres, memes proportions qu'avant (rayon r x 1,2365, point r x 0,0387)
dots = []
for (x, y, r) in LY['knobs']:
    for i in range(11):
        a = math.radians(-135 + 27 * i); rr = r * 1.2365
        dots.append((x + rr * math.sin(a), y - rr * math.cos(a), r * 0.0387))

def f(v): return f"{v:.2f}f"
with open(os.path.join(ROOT, 'Layout.h'), 'w') as o:
    fw, fh = LY['frame']
    o.write(f"// genere par tools/make_layout.py - ne pas modifier a la main (repere {fw} x {fh} = fond 1235 x 1800 px / 2)\n")
    o.write("#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{\n")
    o.write(f"    constexpr float frameW = {f(fw)}, frameH = {f(fh)};\n")
    o.write("    // ordre : sens, dyn, bend, trans\n")
    o.write("    const juce::Point<float> knobPos[4] = { " + ", ".join(f"{{ {f(k[0])}, {f(k[1])} }}" for k in LY['knobs']) + " };\n")
    o.write("    const float knobR[4] = { " + ", ".join(f(k[2]) for k in LY['knobs']) + " };\n")
    s = LY['screen']; o.write(f"    const juce::Rectangle<float> screenRect {{ {f(s[0])}, {f(s[1])}, {f(s[2])}, {f(s[3])} }}; const float screenCorner = {f(s[4])};\n")
    p = LY['preset']; o.write(f"    const juce::Rectangle<float> presetRect {{ {f(p[0])}, {f(p[1])}, {f(p[2])}, {f(p[3])} }};   // petit ecran (vitre dans le fond)\n")
    o.write("    const float presetFont = 21.0f;\n")
    l = LY['led']; o.write(f"    const juce::Point<float> ledPos {{ {f(l[0])}, {f(l[1])} }}; const float ledS = {f(l[2])};\n")
    g = LY['glow']; o.write(f"    const juce::Rectangle<float> glowRect {{ {f(g[0])}, {f(g[1])}, {f(g[2])}, {f(g[3])} }};\n")
    s = LY['fs']; o.write(f"    const juce::Point<float> fsPos {{ {f(s[0])}, {f(s[1])} }}; const float footR = {f(s[2])}, fsS = {f(s[3])};\n")
    t = LY['toggle']; o.write(f"    const juce::Point<float> togPos {{ {f(t[0])}, {f(t[1])} }}; const float togS = {f(t[2])}, togR = {f(t[3])}, togH = {f(t[4])};\n")
    o.write(f"    const float jackY = {f(LY['jackY'])};\n")
    o.write("    struct Dot { float x, y, r; };\n")
    o.write("    const Dot dots[] = { " + ", ".join(f"{{ {f(d[0])}, {f(d[1])}, {f(d[2])} }}" for d in dots) + " };\n")
    o.write("    // serigraphie noire (textes) et logo OMI gris 424243 : traces SVG dans le repere du plugin\n")
    o.write("    const char* const printBlack[] = {\n" + ",\n".join(f'        "{p}"' for p in black) + "\n    };\n")
    o.write("    const char* const printGrey[] = {\n" + ",\n".join(f'        "{p}"' for p in grey) + "\n    };\n")
    # logo NINE HANDS | DSP (noir) : boite de la barre de la maquette (x 1755,6..2147,1 ; y 48,2..83,2)
    lx0, lx1, ly0, ly1 = 1755.62, 2147.05, 48.24, 83.17
    o.write(f"    const juce::Rectangle<float> logoRect {{ {f((lx0 - FX0) * U)}, {f(ly0 * U)}, {f((lx1 - lx0) * U)}, {f((ly1 - ly0) * U)} }};\n")
    o.write("}\n")

# calques du repere : 1024 px = diametre du potard (composant nh::Knob pose sur le potard du fond)
N, S = 1024, 4
big = np.zeros((N * S, N * S), np.uint8)
R = N * S / 2; c = N * S / 2; w = int(0.068 * R)
cv2.line(big, (int(c), int(c - 0.42 * R)), (int(c), int(c - 0.86 * R)), 255, w, cv2.LINE_AA)
m = cv2.resize(big.astype(np.float32) / 255.0, (N, N), interpolation=cv2.INTER_AREA)
yy = np.linspace(1.03, 0.97, N)[:, None]
rgb = np.dstack([236 * yy * np.ones((N, N)), 230 * yy * np.ones((N, N)), 216 * yy * np.ones((N, N))])
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), m * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'knob_marker.png'))
sh = cv2.GaussianBlur(m, (0, 0), 6) * 0.55
Image.fromarray(np.dstack([np.zeros((N, N, 3)), sh * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'knob_shadow.png'))
print('Layout.h + calques du repere ok')
