"""OMI : Layout.h (repere du plugin 617,5 x 900 = fond 1235 x 1800 / 2) d'apres tools/layout.json (ecrit par make_art.py),
serigraphie vectorielle (textes Poppins, graduations, filets) et calques du repere des potards pour nh::Knob
(knob_marker.png, knob_shadow.png : 1024 x 1024, centre = axe de rotation, repere pointe vers le haut).
Les potards eux-memes (alu noir tourne, photo du kit) sont dans le fond : leur surface est de revolution, ses reflets
restent donc fixes comme sur un vrai potard ; seuls le repere et son ombre tournent (avec flou de mouvement)."""
import os, json, math
import numpy as np, cv2
from PIL import Image, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
LY = json.load(open(os.path.join(HERE, 'layout.json')))
F = os.path.join(ROOT, 'assets', 'fonts')

def width(txt, weight, em, track):
    f = ImageFont.truetype(os.path.join(F, f'Poppins-{weight}.ttf'), 400)
    return sum(f.getlength(c) for c in txt) * em / 400 + track * (len(txt) - 1)

texts, dots, rules = [], [], []
for (x, y, r, name) in LY['knobs']:
    texts.append((name, 0, 21.0, 3.0, x, y - r - 30, 0))
    for i in range(11):
        a = math.radians(-135 + 27 * i); rr = r + 11
        dots.append((x + rr * math.sin(a), y - rr * math.cos(a), 1.8))
tx, ty = LY['title']; sx, sy = LY['sub']
texts.append(('OMI', 1, 62.0, 14.0, tx, ty, 0))
texts.append(('GUITAR TO MIDI', 2, 18.0, 7.0, sx, sy, 0))
sw = width('GUITAR TO MIDI', 'Light', 18.0, 7.0)
for sgn in (-1, 1):
    a0 = sx + sgn * (sw / 2 + 14); a1 = sx + sgn * (sw / 2 + 64)
    rules.append((min(a0, a1), sy - 0.9, abs(a1 - a0), 1.8))
texts.append(('OUT', 0, 16.0, 3.0, LY['outX'], LY['jackY'], 90))
texts.append(('IN', 0, 16.0, 3.0, LY['inX'], LY['jackY'], 90))

def f(v): return f"{v:.2f}f"
with open(os.path.join(ROOT, 'Layout.h'), 'w') as o:
    fw, fh = LY['frame']
    o.write(f"// genere par tools/make_layout.py - ne pas modifier a la main (repere {fw} x {fh} = fond 1235 x 1800 px / 2)\n")
    o.write("#pragma once\n#include <juce_graphics/juce_graphics.h>\nnamespace layout\n{\n")
    o.write(f"    constexpr float frameW = {f(fw)}, frameH = {f(fh)};\n")
    o.write("    // ordre : sens, dyn, bend, trans\n")
    o.write("    const juce::Point<float> knobPos[4] = { " + ", ".join(f"{{ {f(k[0])}, {f(k[1])} }}" for k in LY['knobs']) + " };\n")
    o.write("    const float knobR[4] = { " + ", ".join(f(k[2]) for k in LY['knobs']) + " };\n")
    s = LY['screen']; o.write(f"    const juce::Rectangle<float> screenRect {{ {f(s[0])}, {f(s[1])}, {f(s[2])}, {f(s[3])} }};\n")
    l = LY['led']; o.write(f"    const juce::Point<float> ledPos {{ {f(l[0])}, {f(l[1])} }}; const float ledS = {f(l[2])};\n")
    g = LY['glow']; o.write(f"    const juce::Rectangle<float> glowRect {{ {f(g[0])}, {f(g[1])}, {f(g[2])}, {f(g[3])} }};\n")
    s = LY['fs']; o.write(f"    const juce::Point<float> fsPos {{ {f(s[0])}, {f(s[1])} }}; const float footR = {f(s[2])}, fsS = {f(s[3])};\n")
    p = LY['preset']; o.write(f"    const juce::Rectangle<float> presetRect {{ {f(p[0])}, {f(p[1])}, {f(p[2])}, {f(p[3])} }};   // pastille de reference 400 x 38\n")
    o.write("    const float presetFont = 23.0f;\n")
    o.write(f"    const float jackY = {f(LY['jackY'])};\n")
    o.write("    struct Txt { const char* s; int font; float em, track, x, y, rot; };   // police : 0 medium, 1 bold, 2 light\n")
    o.write("    const Txt texts[] = {\n" + ",\n".join(f'        {{ "{t[0]}", {t[1]}, {f(t[2])}, {f(t[3])}, {f(t[4])}, {f(t[5])}, {f(t[6])} }}' for t in texts) + "\n    };\n")
    o.write("    struct Dot { float x, y, r; };\n")
    o.write("    const Dot dots[] = { " + ", ".join(f"{{ {f(d[0])}, {f(d[1])}, {f(d[2])} }}" for d in dots) + " };\n")
    o.write("    const juce::Rectangle<float> rules[] = { " + ", ".join(f"{{ {f(r[0])}, {f(r[1])}, {f(r[2])}, {f(r[3])} }}" for r in rules) + " };\n")
    lg = LY['logo']; o.write(f"    const juce::Point<float> logoPos {{ {f(lg[0])}, {f(lg[1])} }}; const float logoW = {f(lg[2])};\n")
    o.write("}\n")

# calques du repere : 1024 px = diametre du potard (composant nh::Knob pose sur le potard du fond)
N, S = 1024, 4
big = np.zeros((N * S, N * S), np.uint8)
R = N * S / 2
c = N * S / 2
w = int(0.068 * R)
cv2.line(big, (int(c), int(c - 0.42 * R)), (int(c), int(c - 0.86 * R)), 255, w, cv2.LINE_AA)
m = cv2.resize(big.astype(np.float32) / 255.0, (N, N), interpolation=cv2.INTER_AREA)
# repere creme, tres legerement plus clair en haut (cote lumiere)
yy = np.linspace(1.03, 0.97, N)[:, None]
rgb = np.dstack([236 * yy * np.ones((N, N)), 228 * yy * np.ones((N, N)), 208 * yy * np.ones((N, N))])
Image.fromarray(np.dstack([np.clip(rgb, 0, 255), m * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'knob_marker.png'))
sh = cv2.GaussianBlur(m, (0, 0), 6) * 0.55
Image.fromarray(np.dstack([np.zeros((N, N, 3)), sh * 255]).astype(np.uint8), 'RGBA').save(os.path.join(ROOT, 'knob_shadow.png'))
print('Layout.h + calques du repere ok')
