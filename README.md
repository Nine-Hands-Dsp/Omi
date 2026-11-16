# Omi (NINE HANDS | DSP) — VST3

Convertisseur **guitare → MIDI** au format pédale simple, dans l'esprit des interfaces GK-MIDI Roland (GI-20) :
Omi écoute la guitare, reconnaît la note jouée et sa dynamique, et envoie des notes MIDI qui pilotent un instrument
virtuel (VST de basse, synthé…) dans Reaper. Monophonique, pensé pour les 7 et 8 cordes accordées très bas. Windows et macOS.

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/omi/releases/latest/download/Omi-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/omi/releases/latest/download/Omi-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/omi/releases/latest/download/Omi-Notice.pdf"><img alt="Télécharger la notice (PDF)" src="https://img.shields.io/badge/%F0%9F%93%96%20Notice-PDF-919496?style=for-the-badge"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/omi/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/omi?label=derni%C3%A8re%20version&color=1c3a6e"></a>
</p>

## Installation

1. Les boutons ci-dessus téléchargent toujours la dernière version (Release publiée à chaque build de `main`).
   - **Windows** : copier `Omi.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `Omi.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
     (premier lancement : `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Omi.vst3` si macOS le bloque)
2. Relancer l'analyse des plugins. Le plugin apparaît comme **Omi (NINE HANDS | DSP)**.

## Dans Reaper

1. Piste **guitare** : insérer Omi en premier (il lui faut le son direct de la guitare).
2. **À l'insertion d'Omi**, environ une seconde après, REAPER ouvre sa fenêtre **Insert virtual instrument on new track** :
   choisir le VST d'instrument (basse, synthé...). La piste créée juste sous celle d'Omi est aussitôt reliée à son MIDI
   (receive **Audio : None**, **MIDI : All → All**, armement retiré) et nommée « Omi - <instrument> ». Rien n'est créé quand
   un projet est rechargé. **Clic sur l'écran d'Omi** : créer une autre piste (*Create MIDI track + choose instrument...*),
   la recréer directement avec le dernier instrument (*Create MIDI track with <instrument>*), ouvrir l'instrument d'une
   piste déjà reliée, ou désactiver la création d'office. (Cette fenêtre de REAPER s'ouvre là où REAPER l'a placée la
   dernière fois, parfois sur un autre écran.)
3. Jouer : la LED bleue s'allume à chaque note envoyée, l'écran montre la note jouée et la note MIDI envoyée.

À la main : bouton **ROUTE** de la piste de l'instrument > *Add new receive* > piste guitare, **Audio : None**, **MIDI : All → All**.

**Tessiture du VST de basse** : la plupart des basses virtuelles s'arrêtent à B0 (basse 5 cordes) ou E1 (4 cordes). Une 8 cordes
transposée de −12 descend plus bas (G#1 joué → G#0 envoyé) : clic droit > *Lowest MIDI note out* (par défaut **B0**) remonte d'une
octave toute note qui tomberait sous cette limite, pour que le VST la joue. *Off* envoie toutes les notes telles quelles.

Variante : mettre le VST de basse directement après Omi sur la piste guitare (le MIDI d'Omi descend la chaîne) ;
clic droit > *Guitar audio* > *Mute* pour ne garder que l'instrument.

## Commandes

| Commande | Rôle |
|---|---|
| **SENS** | Seuil de détection (−72 à −12 dB, repère ▼ sur le vu-mètre) : en dessous, aucune note ne part (bruit, cordes qui résonnent) |
| **DYN** | Réponse de la vélocité : 0 = vélocité fixe (100), à fond = toute la dynamique du jeu |
| **BEND** | CHROMA (notes franches, demi-ton par demi-ton) ou plage de pitch bend ±1, 2, 3, 4, 5, 7, 12, 24 (envoyée au VST en RPN 0) |
| **TRANS** | Transposition −24 à +24 demi-tons (−12 = basse une octave sous la guitare) |
| Footswitch | Marche / arrêt du MIDI (All Notes Off à l'arrêt : aucune note bloquée) |
| Écran (clic) | REAPER : créer une piste d'instrument qui reçoit le MIDI d'Omi, ouvrir l'instrument, création d'office à l'insertion (oui par défaut) |
| Clic droit | Piste MIDI (REAPER), presets, *Play feel* (Fast / Normal / Stable), *Lowest note*, canal MIDI de sortie, *Lowest MIDI note out* (Off / B0 / E1 / A0), *Guitar audio* (Thru / Mute), MIDI Learn, taille, Glow |

## Détection de hauteur (renforcée pour les accordages très graves)

Moteur `OmiDSP.h`, sans JUCE, sans allocation ni verrou dans le thread audio :

- **Pré-traitement** : passe-haut 18 Hz, passe-bas Butterworth 6e ordre à 1,4 kHz (les harmoniques aiguës trompent l'analyse), décimation vers ≈ 8 kHz.
- **Méthode McLeod (NSDF) à fenêtre par décalage** : chaque période candidate est mesurée sur 0,8 période (12 ms minimum). Une note aiguë est reconnue en quelques millisecondes, une corde grave dispose de la fenêtre dont elle a besoin.
- **Passage d'une corde à l'autre** : chaque nouvelle attaque est repérée à l'enveloppe et l'analyse ne regarde plus que ce qui la suit : la corde précédente (souvent plus grave) ne fausse plus la note suivante.
- **Note la plus grave (*Lowest note*)** : E2, B1, G1 (7 cordes en Sol), F#1, E1 (par défaut) ou G#0. Aucune note n'est cherchée plus bas : c'est ce qui empêche une corde de prendre l'octave du dessous. Réglez-le sur votre corde la plus grave.
- **Correction d'octave prudente** : sur les cordes graves, la fondamentale captée par le micro est faible et l'harmonique 2 domine. Une période double (ou triple) n'est préférée que si la période trouvée est peu nette et la période double nettement meilleure.
- **Stabilisation** : médiane glissante des estimations, confirmation sur 2 / 3 / 5 analyses (*Play feel*), un saut d'octave demande deux fois plus de confirmations, hystérésis de 0,6 demi-ton entre deux notes.
- **Tenue sans décrochage** : une analyse ratée (fin de note, bruit) ne coupe jamais la note ; seuls la porte de niveau (SENS, 6 dB d'hystérésis, 40 ms) ou une nouvelle note confirmée y mettent fin. Une nouvelle attaque sur la même note est reconnue à l'enveloppe et redéclenche la note.
- **Latence** : ≈ 15–30 ms dans les médiums et aigus ; en jeu rapide sur 7 cordes en Sol, ≈ 37–40 ms en *Play feel* Fast et ≈ 40–45 ms en Normal ; ≈ 80 ms sur A0. Il faut au moins une à deux périodes de la corde pour en connaître la hauteur (même limite que les boîtiers matériels).

Tests : `tests/tracker_test.cpp` (cordes synthétiques à fondamentale faible et inharmonicité, F#1, E1, D1, C1, A0, G#0, G1, legato, aigus ; 44,1 / 48 / 96 kHz : aucune fausse note), `tests/fast_test.cpp` (jeu rapide d'une corde à l'autre, cordes étouffées ou qui sonnent encore) et `tools/render` (bout en bout : audio → MIDI dans le vrai processeur). ≈ 2,5 % d'un cœur.

Monophonique : un micro ordinaire mélange les six (sept, huit) cordes, les accords ne peuvent pas être convertis de façon fiable (les Roland utilisent un micro hexaphonique GK, une voie par corde).

## Interface

- Visuel 1235 × 1800 px (`tools/make_art.py`), pédale plein cadre ; peinture bleu nuit métallisé, motif imprimé : l'onde de la guitare entre côté IN et ressort en notes MIDI côté OUT ; façade éclairée à 45° en haut à gauche.
- Écran : note jouée et écart en cents, note MIDI envoyée, vélocité, plage de bend, vu-mètre d'entrée avec le seuil SENS.
- Potards photo du kit (alu tourné, reflets fixes) ; repère animé `nh::Knob` (`NHKnob.h`) avec flou de mouvement. Footswitch du kit (référence Echoflex), LED bleue = note MIDI en cours, halo réglable (*Glow intensity*).
- Sérigraphie, logo NINE HANDS | DSP (`NineHandsLogoWhite.svg`) et écran dessinés en vectoriel (Poppins).
- Fenêtre 338 × 493 (« 60 % »), non réductible, jusqu'à 150 % ; menus au design d'Octopus, ouverts là où l'on clique ; barre de presets et ligne d'état dans la même pastille.
- Anti-gel v2 : rendu logiciel sous Windows, caches reconstruits par le timer, chien de garde, aucune requête à l'hôte depuis le thread audio. JUCE 8.0.15.

## MIDI entrant

**Aucun CC assigné par défaut** : clic droit sur une commande > *MIDI Learn* (Sens, Dyn, Bend, Trans, Footswitch). Les CC non assignés
et les autres messages passent vers l'instrument suivant. Program Change 1-8 = presets d'usine.

## Presets d'usine

Bass Mono · Bass Bend · 8 Strings Sub · Unison · Synth Lead · Octave Up Pad · Fixed Velocity · Noisy Pickups.

## Fichiers

- `OmiDSP.h` : suivi de hauteur et génération MIDI ; `PluginProcessor.*`, `PluginEditor.*`, `NHKnob.h` ; `ReaperBridge.*` : API de REAPER (piste d'instrument).
- `tools/make_art.py` (+ `tools/photo_controls.py`) : visuel ; `tools/make_layout.py` : `Layout.h` et calques du repère.
- `tools/render` : compile la vraie fenêtre sous Linux, exporte les captures et vérifie la chaîne audio → MIDI.

## Versions

- **1.0.3** (16 nov. 2026) : la fenêtre de choix d'instrument de REAPER s'ouvre bien (Insert virtual instrument on new track) et la piste créée est reliée automatiquement à Omi ; caractères spéciaux (·, ±, →) corrigés sous Windows.
- **1.0.2** (16 nov. 2026) : détection plus rapide et plus sûre au passage d'une corde à l'autre (analyse limitée à ce qui suit l'attaque, fenêtre 0,8 période, attaques mieux repérées, Play feel Fast fiabilisé) ; plus de corde prise pour une plus grave (correction d'octave prudente, *Lowest note* G1 et E1, E1 par défaut) ; piste MIDI créée d'office à l'insertion d'Omi dans REAPER.
- **1.0.1** (16 nov. 2026) : *Lowest MIDI note out* (B0 par défaut : les notes sous la tessiture du VST de basse remontent d'une octave) ; dans REAPER, création en un clic de la piste d'instrument qui reçoit le MIDI d'Omi (`ReaperBridge.cpp`, interface IReaperHostApplication).
- **1.0.0** (16 nov. 2026) : première version.
