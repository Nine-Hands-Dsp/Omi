#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"
#include <map>

using namespace juce;

namespace
{
    const Rectangle<float> presetRect = layout::presetRect;
    const Colour accent (0xff3c8cff);       // bleu de la LED
    const Colour scrCyan (0xff5acdff), scrWhite (0xffebf4ff), scrGrey (0xff788ca5), scrDim (0xff1e2c3e);

    const char* const valueIds[] = { "sens", "dyn", "bend", "trans", "active", "feel", "channel", "lowest", "thru", "fold", "octHold" };
    constexpr int numValueIds = 11;
    constexpr float meterLo = -72.0f, meterHi = -6.0f;

    // Agrandissement bilineaire exact (JUCE laisse une trame de lignes a peine visibles en agrandissant)
    Image upscaleBilinear (const Image& src, int w, int h)
    {
        Image out (Image::ARGB, w, h, false);
        const Image::BitmapData s (src, Image::BitmapData::readOnly);
        Image::BitmapData d (out, Image::BitmapData::writeOnly);
        const float kx = (float) src.getWidth() / (float) w, ky = (float) src.getHeight() / (float) h;
        for (int y = 0; y < h; ++y)
        {
            const float fy = jlimit (0.0f, (float) src.getHeight() - 1.001f, ((float) y + 0.5f) * ky - 0.5f);
            const int y0 = (int) fy; const float ty = fy - (float) y0;
            for (int x = 0; x < w; ++x)
            {
                const float fx = jlimit (0.0f, (float) src.getWidth() - 1.001f, ((float) x + 0.5f) * kx - 0.5f);
                const int x0 = (int) fx; const float tx = fx - (float) x0;
                const PixelARGB p00 = s.getPixelColour (x0, y0).getPixelARGB(), p10 = s.getPixelColour (x0 + 1, y0).getPixelARGB();
                const PixelARGB p01 = s.getPixelColour (x0, y0 + 1).getPixelARGB(), p11 = s.getPixelColour (x0 + 1, y0 + 1).getPixelARGB();
                auto mix = [&] (uint8 a, uint8 b, uint8 c, uint8 e)
                { return (uint8) jlimit (0, 255, roundToInt ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + e * tx) * ty)); };
                PixelARGB q (mix (p00.getAlpha(), p10.getAlpha(), p01.getAlpha(), p11.getAlpha()),
                             mix (p00.getRed(), p10.getRed(), p01.getRed(), p11.getRed()),
                             mix (p00.getGreen(), p10.getGreen(), p01.getGreen(), p11.getGreen()),
                             mix (p00.getBlue(), p10.getBlue(), p01.getBlue(), p11.getBlue()));
                *(PixelARGB*) d.getPixelPointer (x, y) = q;          // valeurs premultipliees conservees
            }
        }
        return out;
    }
}

//==============================================================================
// FOOTSWITCH (fonctionnement d'Echoflex)
//==============================================================================
FootSwitchArea::FootSwitchArea (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool FootSwitchArea::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * (layout::footR * 1.35f / layout::fsS);
}

void FootSwitchArea::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (ledLevel > 0.003f && reflection.isValid())
    {
        g.setOpacity (ledLevel);
        g.drawImage (reflection, b);
        g.setOpacity (1.0f);
    }
    if (pressed)
    {
        const float r = b.getWidth() * (layout::footR / layout::fsS) * 0.98f;
        const auto c = b.getCentre();
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}

void FootSwitchArea::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    pressed = true;
    repaint();
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);    // bascule des l'appui
    if (onUserChange) onUserChange();
}

void FootSwitchArea::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    pressed = false;
    repaint();
}

//==============================================================================
// INVERSEUR DRY / MIDI
//==============================================================================
LeverSwitch::LeverSwitch (RangedAudioParameter& p, bool upMeansZero)
    : upZero (upMeansZero),
      attachment (p, [this] (float v)
                  {
                      const bool nu = upZero ? v < 0.5f : v > 0.5f;
                      if (nu != up)
                      {
                          up = nu;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void LeverSwitch::tick (double) {}

void LeverSwitch::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    const int row = up ? 0 : 1;                             // bande : bouton en haut (DRY) / en bas (MIDI)
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, row * fs, fs, fs);
}

bool LeverSwitch::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float k = (float) getWidth() / layout::togS;
    return std::abs ((float) x - c.x) <= layout::togR * k * 1.15f && std::abs ((float) y - c.y) <= layout::togH * 0.5f * k;
}

void LeverSwitch::moveTo (bool nu)
{
    if (nu == up) return;
    attachment.setValueAsCompleteGesture ((nu == upZero) ? 0.0f : 1.0f);
    if (onUserChange) onUserChange();
}

void LeverSwitch::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    // clic dans la moitie haute = DRY, moitie basse = MIDI ; clic sur la position deja prise = bascule
    const bool wantUp = e.position.y < (float) getHeight() * 0.5f;
    moveTo (wantUp != up ? wantUp : ! up);
}

void LeverSwitch::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f) moveTo (w.deltaY > 0);
}

//==============================================================================
// EDITEUR
//==============================================================================
OmiAudioProcessorEditor::OmiAudioProcessorEditor (OmiAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      footswitch (*p.apvts.getParameter ("active")),
      dryToggle (*p.apvts.getParameter ("thru"), true)
{
    background = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    glow       = ImageCache::getFromMemory (BinaryData::glow_blue_png, BinaryData::glow_blue_pngSize);
    ledOn      = ImageCache::getFromMemory (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    footswitch.reflection = ImageCache::getFromMemory (BinaryData::fs_refl_png, BinaryData::fs_refl_pngSize);
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    boldFace  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
    lightFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsLight_ttf, BinaryData::PoppinsLight_ttfSize);
    orbMed = Typeface::createSystemTypefaceFor (BinaryData::OrbitronMedium_ttf, BinaryData::OrbitronMedium_ttfSize);
    orbReg = Typeface::createSystemTypefaceFor (BinaryData::OrbitronRegular_ttf, BinaryData::OrbitronRegular_ttfSize);
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogoBlack_svg, BinaryData::NineHandsLogoBlack_svgSize)))
        logo = Drawable::createFromSVG (*xml);
    buildPrint();
    menuLnf.f = { lightFace, labelFace, boldFace };
    glowPc = NineHandsGlow::percent (JucePlugin_Name);
    setLookAndFeel (&menuLnf);             // menus (et sous-menus) au design d'Octopus

    // potards : seul le repere (et son ombre) tourne, avec flou de mouvement ; le potard photo est dans le fond
    nh::Knob::Layers L;
    L.marker = ImageCache::getFromMemory (BinaryData::knob_marker_png, BinaryData::knob_marker_pngSize);
    L.shadow = ImageCache::getFromMemory (BinaryData::knob_shadow_png, BinaryData::knob_shadow_pngSize);
    nh::Knob::Style st;
    st.shadowOffset = { 0.012f, 0.012f };   // repere peint, a peine en relief ; lumiere en haut a gauche a 45 degres
    for (int i = 0; i < OmiAudioProcessor::numKnobs; ++i)
    {
        knobs[i] = std::make_unique<nh::Knob> (*proc.apvts.getParameter (OmiAudioProcessor::knobId (i)), L, st);
        knobs[i]->onRightClick = [this, i] (const MouseEvent&) { showContextMenu (i); };
        knobs[i]->setMouseCursor (MouseCursor::PointingHandCursor);
        addAndMakeVisible (*knobs[i]);
    }

    footswitch.onRightClick = [this] { showContextMenu (4); };
    dryToggle.strip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    dryToggle.onRightClick = [this] { showContextMenu (5); };
    dryToggle.onUserChange = [this] { showStatus(); };
    addAndMakeVisible (dryToggle);
    footswitch.onUserChange = [this] { showStatus(); };
    addAndMakeVisible (footswitch);

    for (int i = 0; i < numValueIds; ++i) lastValues[i] = proc.raw (valueIds[i]);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();

    const int defaultW = roundToInt ((float) defaultH * baseW / baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    hostLock.setLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    if (auto* cons = getConstrainer())
        cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    setScaledHeight (sizeHeight (NineHandsWindow::openIndex (JucePlugin_Name, proc.uiSizeIdx.load(), numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

OmiAudioProcessorEditor::~OmiAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
int OmiAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, roundToInt (refH * 0.75f), (int) refH };
    return h[jlimit (0, numSizes - 1, idx)];
}

void OmiAudioProcessorEditor::setScaledHeight (int h)
{
    h = jlimit ((int) defaultH, (int) maxH, h);
    setSize (roundToInt ((float) h * baseW / baseH), h);
    hostLock.enforce();
}

Rectangle<int> OmiAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    const float s = (float) getHeight() / baseH;
    return r.transformedBy (AffineTransform::scale (s)).getSmallestIntegerContainer();
}

float OmiAudioProcessorEditor::physicalScale() const
{
    if (forcedScale > 0.0f) return forcedScale;
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void OmiAudioProcessorEditor::showStatus() { statusUntil = Time::getMillisecondCounterHiRes() + 2500.0; }

void OmiAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;
    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    // fond toujours a la taille exacte en pixels physiques (une seule mise a l'echelle de qualite, pas d'etirement au dessin)
    if (pw == background.getWidth() && ph == background.getHeight()) bgScaled = background;
    else if (ph > background.getHeight())                            bgScaled = upscaleBilinear (background, pw, ph);
    else                                                             bgScaled = background.rescaled (pw, ph, Graphics::highResamplingQuality);
    const float s = (float) h / baseH;
    const int gpx = roundToInt (layout::glowRect.getWidth() * s * sc);
    glowS = (gpx >= glow.getWidth()) ? glow : glow.rescaled (gpx, gpx, Graphics::highResamplingQuality);
    repaint();
}

void OmiAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici
    for (int i = 0; i < OmiAudioProcessor::numKnobs; ++i)
    {
        const auto c = layout::knobPos[i]; const float r = layout::knobR[i];
        knobs[i]->setBounds (scaled ({ c.x - r, c.y - r, r * 2.0f, r * 2.0f }));
    }
    const float fr = layout::fsS * 0.5f;
    footswitch.setBounds (scaled ({ layout::fsPos.x - fr, layout::fsPos.y - fr, fr * 2.0f, fr * 2.0f }));
    {
        // carre en pixels entiers, centre sur la gorge : les deux images tombent exactement au meme endroit
        const float s = (float) getHeight() / baseH;
        const int side = roundToInt (layout::togS * s);
        dryToggle.setBounds (roundToInt (layout::togPos.x * s) - side / 2, roundToInt (layout::togPos.y * s) - side / 2, side, side);
    }
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (NINE HANDS | DSP)
//==============================================================================
void OmiAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void OmiAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); hostLock.track (*this); }
void OmiAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void OmiAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String OmiAudioProcessorEditor::buildStatusLine() const
{
    const int i = lastChanged;
    if (i < 0) return {};
    const String id = valueIds[i];
    const float v = proc.raw (valueIds[i]);
    if (id == "sens")    return "sens  " + String (roundToInt (v)) + " dB";
    if (id == "dyn")     return v < 0.05f ? String ("dyn  fixed velocity") : "dyn  " + String (roundToInt (v * 10.0f)) + " %";
    if (id == "bend")    { const int b = OmiAudioProcessor::bendSemis (roundToInt (v)); return b == 0 ? String ("bend  chromatic") : juce::String (juce::CharPointer_UTF8 ("bend range  \xc2\xb1")) + String (b); }
    if (id == "trans")   { const int t = roundToInt (v); return "transpose  " + String (t > 0 ? "+" : "") + String (t) + " st"; }
    if (id == "feel")    return "play feel  " + OmiAudioProcessor::feelNames()[jlimit (0, 2, roundToInt (v))].toLowerCase();
    if (id == "channel") return "midi channel  " + String (roundToInt (v));
    if (id == "lowest")  return "lowest note  " + OmiAudioProcessor::lowestNames()[jlimit (0, OmiAudioProcessor::numLowest - 1, roundToInt (v))].upToFirstOccurrenceOf (" ", false, false);
    if (id == "fold")    { const int n = OmiAudioProcessor::foldNote (roundToInt (v)); return n < 0 ? String ("lowest midi note  free") : "lowest midi note  " + OmiAudioProcessor::noteName (n); }
    if (id == "thru")    return v > 0.5f ? "midi only (dry off)" : "dry guitar + midi";
    if (id == "octHold") return v > 0.5f ? "octave hold on" : "octave hold off";
    return v > 0.5f ? "midi on" : "bypass";
}

String OmiAudioProcessorEditor::buildPresetLine() const
{
    const int cp = proc.currentPreset.load();
    String s;
    if (cp >= 0 && cp < OmiAudioProcessor::getNumFactoryPresets())
        s << "P" << (cp + 1) << "  " << OmiAudioProcessor::getFactoryPreset (cp).name;
    else if (cp > 100) s << "User " << (cp - 100);
    else               s << "Edited";
    return s;
}

//==============================================================================
void OmiAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    dryToggle.tick (now);
    if (isShowing() && proc.chooserRequested.exchange (false)) showInstrumentChooser();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }
    hostLock.track (*this);

    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0; lastPeer = nullptr;
        useSoftwareRenderer(); repaint();
    }

    int changed = -1, numChanged = 0;
    for (int i = 0; i < numValueIds; ++i)
    {
        const float v = proc.raw (valueIds[i]);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pcount = proc.presetCounter.load();
    if (pcount != lastPresetCounter) { lastPresetCounter = pcount; numChanged = 0; }
    if (numChanged == 1)
    {
        lastChanged = changed; showStatus(); learnLineUntil = 0.0;
        if (changed < 4 && proc.currentPreset.load() >= 0) proc.currentPreset = -1;
    }

    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnLine = String (OmiAudioProcessor::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load());
        learnLineUntil = now + 2000.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
    }
    else if (lt >= 0)
    {
        learnLine = juce::String (juce::CharPointer_UTF8 ("MIDI LEARN \xc2\xb7 ")) + String (OmiAudioProcessor::learnDef (lt).name);
        learnLineUntil = now + 200.0;
        statusUntil = jmax (statusUntil, now + 200.0);
    }
    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const float target = now < statusUntil ? 1.0f : 0.0f;
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.25f);
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    if (now - lastGlowReadMs > 3000.0)
    {
        lastGlowReadMs = now;
        const int pc = NineHandsGlow::percent (JucePlugin_Name);
        if (pc != glowPc) setGlowPercent (pc);
    }

    // LED bleue : allumee tant qu'une note MIDI est envoyee (pedale active)
    const bool active = proc.rawAt (OmiAudioProcessor::rActive) > 0.5f;
    const int outNote = proc.uiOutNote.load (std::memory_order_relaxed);
    const bool on = active && outNote >= 0;
    ledLevel += ((on ? 1.0f : 0.0f) - ledLevel) * (on ? 0.45f : 0.28f);
    if (ledLevel < 0.003f) ledLevel = 0.0f;
    if (ledLevel > 0.997f) ledLevel = 1.0f;
    if (std::abs (ledLevel - drawnLed) > 0.004f)
    {
        drawnLed = ledLevel;
        footswitch.ledLevel = ledLevel * glowAmt();
        askRepaint (scaled (layout::glowRect));
        footswitch.repaint();
    }

    // REAPER : aucune piste ne recoit encore le MIDI d'Omi -> indication une fois
    if (! hintShown && isShowing() && proc.reaperBridge.available())
    {
        hintShown = true;
        if (proc.reaperBridge.receivers().isEmpty())
        {
            learnLine = juce::String (juce::CharPointer_UTF8 ("CLICK THE SCREEN \xc2\xb7 CREATE MIDI TRACK"));
            learnLineUntil = now + 6000.0;
            statusUntil = jmax (statusUntil, learnLineUntil);
        }
    }

    // ecran
    const float det = proc.uiDetected.load (std::memory_order_relaxed);
    if (det > 0.0f) { heldIn = det; holdTicks = 15; }
    else if (holdTicks > 0 && --holdTicks == 0) heldIn = -1.0f;      // la derniere note reste affichee 0,5 s
    const float lvl = proc.uiLevelDb.load (std::memory_order_relaxed);
    meterDb = lvl > meterDb ? lvl : jmax (lvl, meterDb - 1.5f);       // vu-metre : retombee 45 dB/s
    ScreenState s;
    s.inNote = heldIn > 0.0f ? roundToInt (heldIn) : -1;
    s.cents  = heldIn > 0.0f ? roundToInt ((heldIn - (float) s.inNote) * 100.0f) : 0;
    s.outNote = outNote;
    s.vel = proc.uiOutVel.load (std::memory_order_relaxed);
    s.bend = proc.uiBend.load (std::memory_order_relaxed) / 64;
    s.level = roundToInt ((meterDb - meterLo) / (meterHi - meterLo) * 40.0f);
    s.sens = roundToInt (proc.rawAt (OmiAudioProcessor::rSens));
    s.chan = roundToInt (proc.rawAt (OmiAudioProcessor::rChannel));
    s.bendIdx = roundToInt (proc.rawAt (OmiAudioProcessor::rBend));
    s.active = active ? 1 : 0;
    s.dryOff = proc.rawAt (OmiAudioProcessor::rThru) > 0.5f ? 1 : 0;
    scr = s;
    if (! (scr == drawnScr)) { drawnScr = scr; askRepaint (scaled (layout::screenRect)); }

    const auto l1 = (learnLineUntil > 0.0 && now < learnLineUntil + 800.0 ? learnLine : buildStatusLine()), pl = buildPresetLine();
    const bool textChanged = l1 != lastLine1;
    lastLine1 = l1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetRect.expanded (3.0f)));
    }
}

//==============================================================================
void OmiAudioProcessorEditor::drawLed (Graphics& g)
{
    const float level = ledLevel;
    if (level <= 0.003f) return;
    if (glowS.isValid()) { g.setOpacity (level * glowAmt()); g.drawImage (glowS, layout::glowRect); }
    const auto c = layout::ledPos;
    const float hs = layout::ledS * 0.5f;
    if (ledOn.isValid()) { g.setOpacity (level); g.drawImage (ledOn, { c.x - hs, c.y - hs, hs * 2.0f, hs * 2.0f }); }
    g.setOpacity (1.0f);
}

//==============================================================================
// Ecran : note jouee (et ecart en cents), note MIDI envoyee, velocite, plage de bend, vu-metre d'entree + seuil SENS
//==============================================================================
void OmiAudioProcessorEditor::drawScreen (Graphics& g)
{
    // grand ecran (design v2) : vitre noire dans le fond ; contenu en Orbitron
    const auto r = layout::screenRect.reduced (layout::screenCorner * 0.55f, 14.0f);
    const auto& s = drawnScr;
    auto font = [this] (bool med, float h) { return Font (FontOptions (med ? orbMed : orbReg).withHeight (h)); };
    auto text = [&] (const String& t, float x, float y, float w, float h, Justification j, Colour c, Font f)
    { g.setColour (c); g.setFont (f); g.drawText (t, Rectangle<float> (x, y, w, h), j, false); };

    const float half = r.getWidth() * 0.5f;
    const Colour off = scrDim;
    const float y0 = r.getY();
    text ("IN", r.getX(), y0, 60.0f, 18.0f, Justification::centredLeft, scrGrey, font (true, 14.0f));
    if (s.dryOff)      // son de la guitare coupe : seul l'instrument est entendu
        text ("DRY OFF", r.getX() + 34.0f, y0, 130.0f, 18.0f, Justification::centredLeft, Colour (0xffffb45a), font (true, 14.0f));
    text ("MIDI OUT  CH " + String (s.chan), r.getRight() - 220.0f, y0, 220.0f, 18.0f, Justification::centredRight, scrGrey, font (true, 14.0f));

    // note jouee
    const float ny = y0 + 30.0f;
    if (s.inNote >= 0)
    {
        static const char* nn[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const String name (nn[s.inNote % 12]);
        const auto big = font (true, 86.0f);
        const float nw = GlyphArrangement::getStringWidth (big, name);
        text (name, r.getX() - 2.0f, ny, nw + 4.0f, 96.0f, Justification::centredLeft, scrWhite, big);
        text (String (s.inNote / 12 - 1), r.getX() + nw + 6.0f, ny + 48.0f, 50.0f, 40.0f, Justification::centredLeft, scrWhite, font (true, 34.0f));
        text ((s.cents > 0 ? "+" : "") + String (s.cents) + " ct", r.getX() + nw + 8.0f, ny + 10.0f, 100.0f, 20.0f, Justification::centredLeft, scrCyan, font (false, 16.0f));
    }
    else
        text ("--", r.getX(), ny, 140.0f, 96.0f, Justification::centredLeft, off, font (true, 86.0f));

    // fleche
    {
        const float ax = r.getX() + half + 4.0f, ay = ny + 50.0f;
        Path p; p.startNewSubPath (ax - 20.0f, ay); p.lineTo (ax + 18.0f, ay);
        p.startNewSubPath (ax + 9.0f, ay - 9.0f); p.lineTo (ax + 19.0f, ay); p.lineTo (ax + 9.0f, ay + 9.0f);
        g.setColour (s.outNote >= 0 ? scrGrey : off);
        g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // note MIDI envoyee
    const float rx = r.getRight();
    if (! s.active)
        text ("BYPASS", rx - 200.0f, ny + 16.0f, 200.0f, 56.0f, Justification::centredRight, scrGrey, font (true, 38.0f));
    else if (s.outNote >= 0)
    {
        text (OmiAudioProcessor::noteName (s.outNote), rx - 210.0f, ny + 6.0f, 210.0f, 64.0f, Justification::centredRight, scrCyan, font (true, 54.0f));
        text ("NOTE " + String (s.outNote) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 VEL ")) + String (s.vel), rx - 240.0f, ny + 74.0f, 240.0f, 18.0f, Justification::centredRight, scrGrey, font (false, 14.0f));
    }
    else
        text ("--", rx - 210.0f, ny + 6.0f, 210.0f, 64.0f, Justification::centredRight, off, font (true, 54.0f));
    text (OmiAudioProcessor::bendSemis (s.bendIdx) == 0 ? String ("CHROMA") : juce::String (juce::CharPointer_UTF8 ("BEND \xc2\xb1")) + String (OmiAudioProcessor::bendSemis (s.bendIdx)),
          rx - 210.0f, ny + 96.0f, 210.0f, 18.0f, Justification::centredRight, scrGrey, font (false, 14.0f));

    // justesse : point sur une echelle de -50 a +50 cents
    {
        const float x0 = r.getX(), x1 = r.getX() + half - 24.0f, y = ny + 118.0f;
        g.setColour (Colour (0xff3c5069));
        g.drawLine (x0, y, x1, y, 1.3f);
        for (int k = -4; k <= 4; ++k)
        {
            const float xk = (x0 + x1) * 0.5f + (float) k * (x1 - x0) / 8.0f, hk = k == 0 ? 6.0f : 3.5f;
            g.setColour (Colour (0xff50647d));
            g.drawLine (xk, y - hk, xk, y + hk, 1.3f);
        }
        if (s.inNote >= 0)
        {
            const float xm = (x0 + x1) * 0.5f + jlimit (-1.0f, 1.0f, (float) s.cents / 50.0f) * (x1 - x0) * 0.5f;
            g.setColour (std::abs (s.cents) <= 5 ? scrCyan : Colour (0xffffb45a));
            g.fillEllipse (xm - 4.5f, y - 4.5f, 9.0f, 9.0f);
        }
    }

    // vu-metre d'entree (40 segments) + repere du seuil SENS
    {
        const float x0 = r.getX(), x1 = r.getRight(), y = r.getBottom() - 9.0f;
        const int segs = 40;
        const float sw = (x1 - x0) / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool lit = i < s.level;
            Colour c = lit ? scrCyan : Colour (0xff1e2c3e);
            if (lit && i >= 36) c = Colour (0xffff8c5a);
            if (lit && s.level > 0 && (float) i < (float) segs * ((float) s.sens - meterLo) / (meterHi - meterLo)) c = c.withMultipliedAlpha (0.45f);   // sous le seuil : pas de note
            g.setColour (c);
            g.fillRect (x0 + (float) i * sw, y - 6.0f, sw - 2.4f, 12.0f);
        }
        const float xt = x0 + jlimit (0.0f, 1.0f, ((float) s.sens - meterLo) / (meterHi - meterLo)) * (x1 - x0);
        Path tri; tri.addTriangle (xt, y - 9.0f, xt - 5.5f, y - 17.0f, xt + 5.5f, y - 17.0f);
        g.setColour (scrWhite);
        g.fillPath (tri);
        g.drawLine (xt, y - 9.0f, xt, y + 9.0f, 1.6f);
        text ("SENS", xt + 9.0f, y - 26.0f, 70.0f, 16.0f, Justification::centredLeft, scrWhite, font (true, 12.0f));
    }
}

// Pastille unique : barre de presets au repos, ligne d'etat pendant les reglages (fondu croise)
void OmiAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    const float a = statusAlpha, pa = 1.0f - a;
    // petit ecran : la vitre noire est dans le fond (decoupe du boitier)
    const Font f (FontOptions (orbReg).withHeight (layout::presetFont));
    const float aw = r.getHeight() * 0.95f;
    if (pa > 0.0f)
    {
        auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
        {
            Path p; const auto c = ar.getCentre(); const float sz = 5.5f;
            if (left) p.addTriangle (c.x + sz * 0.6f, c.y - sz, c.x + sz * 0.6f, c.y + sz, c.x - sz * 0.8f, c.y);
            else      p.addTriangle (c.x - sz * 0.6f, c.y - sz, c.x - sz * 0.6f, c.y + sz, c.x + sz * 0.8f, c.y);
            g.setColour ((hot ? Colours::white : Colours::white.withAlpha (0.6f)).withMultipliedAlpha (pa));
            g.fillPath (p);
        };
        arrow (r.withWidth (aw), true, hoverZone == 1);
        arrow (r.withTrimmedLeft (r.getWidth() - aw), false, hoverZone == 3);
        g.setFont (f);
        g.setColour ((midiFlash > 0 ? scrCyan : scrWhite).withAlpha ((hoverZone == 2 ? 1.0f : 0.90f) * pa));
        g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    }
    if (a > 0.0f)
    {
        g.setFont (f);
        g.setColour ((midiFlash > 0 ? scrCyan : scrWhite).withAlpha (a));
        g.drawFittedText (lastLine1.toUpperCase().replace (" DB", " dB"), r.reduced (r.getHeight() * 0.5f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    }
}

//==============================================================================
// Serigraphie vectorielle
//==============================================================================
void OmiAudioProcessorEditor::buildPrint()
{
    printPath.clear(); greyPath.clear();
    for (auto* d : layout::printBlack) printPath.addPath (Drawable::parseSVGPath (d));
    for (const auto& d : layout::dots)  printPath.addEllipse (d.x - d.r, d.y - d.r, d.r * 2.0f, d.r * 2.0f);
    for (auto* d : layout::printGrey)  greyPath.addPath (Drawable::parseSVGPath (d));
}

void OmiAudioProcessorEditor::drawPrint (Graphics& g)
{
    g.setColour (Colour (0xff424243));          // logo OMI / AUDIO 2 MIDI
    g.fillPath (greyPath);
    g.setColour (Colours::black);               // textes, graduations
    g.fillPath (printPath);
    if (logo != nullptr)
        logo->drawWithin (g, layout::logoRect, RectanglePlacement::centred, 1.0f);
}

void OmiAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colour (0xff111112));
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::mediumResamplingQuality);   // bilineaire : jamais de trame si la taille ne tombe pas pile
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float s = (float) getHeight() / baseH;
    g.addTransform (AffineTransform::scale (s));
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    drawPrint (g);
    drawScreen (g);
    drawLed (g);
    drawPresetBar (g);
}

//==============================================================================
// Souris : barre de presets, clic droit = menu
//==============================================================================
static int presetZoneAt (Point<float> p)
{
    if (! presetRect.contains (p)) return 0;
    if (p.x < presetRect.getX() + presetRect.getHeight()) return 1;
    if (p.x > presetRect.getRight() - presetRect.getHeight()) return 3;
    return 2;
}

void OmiAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    const float s = (float) getHeight() / baseH;
    hoverZone = presetZoneAt (e.position / s);
    const bool overScreen = proc.reaperBridge.available() && layout::screenRect.contains (e.position / s);   // clic = piste MIDI
    setMouseCursor (hoverZone != 0 || overScreen ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void OmiAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void OmiAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    const float s = (float) getHeight() / baseH;
    if (layout::screenRect.contains (e.position / s)) { showReaperMenu(); return; }
    if (presetRect.contains (e.position / s) && statusAlpha > 0.0f)
        { statusUntil = 0.0; learnLineUntil = 0.0; }
    switch (presetZoneAt (e.position / s))
    {
        case 1: stepPreset (-1); break;
        case 2: showPresetMenu(); break;
        case 3: stepPreset (1); break;
        default: break;
    }
}

void OmiAudioProcessorEditor::stepPreset (int delta)
{
    const int n = OmiAudioProcessor::getNumFactoryPresets();
    int cp = proc.currentPreset.load();
    if (cp < 0 || cp >= n) cp = delta > 0 ? -1 : 0;
    proc.applyFactoryPreset ((cp + delta + n) % n);
}

void OmiAudioProcessorEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < OmiAudioProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + OmiAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    m.addSeparator();
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);
    m.setLookAndFeel (&menuLnf);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled (presetRect))),
        [safe = Component::SafePointer<OmiAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if      (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            safe->showStatus();
        });
}

void OmiAudioProcessorEditor::handleMidiMenu (int r)
{
    if (r >= 1000 && r < 1000 + OmiAudioProcessor::numLearn)      proc.learnTarget.store (r - 1000);
    else if (r >= 1100 && r < 1100 + OmiAudioProcessor::numLearn) proc.ccMap[r - 1100].store (-1);
    else if (r == 1200) proc.learnTarget.store (-1);
    else if (r == 1201) { proc.learnTarget.store (-1); proc.resetMidiMap(); }
}

void OmiAudioProcessorEditor::setGlowPercent (int pc)
{
    glowPc = NineHandsGlow::sanitize (pc);
    drawnLed = -1.0f;
}

PopupMenu OmiAudioProcessorEditor::buildContextMenu (int learnIdx)
{
    using namespace NineHandsMenu;
    PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String ("not assigned");
    };
    const int lt = proc.learnTarget.load();
    if (learnIdx >= 0)
    {
        const String name = OmiAudioProcessor::learnDef (learnIdx).name;
        addHeader (menu, menuLnf.f, learnIdx < 4 ? icoKnob : icoPedal, accent, name, "MIDI: " + ccText (learnIdx));
        if (learnIdx == 5) { menu.addItem (393, "Dry guitar sound (uncheck = instrument only)", true, proc.rawAt (OmiAudioProcessor::rThru) < 0.5f); menu.addSeparator(); }
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name, true, lt == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        menu.addItem (1200, "Cancel MIDI learn", lt >= 0);
        return menu;
    }

    addHeader (menu, menuLnf.f, icoPedal, accent, "Omi", "Guitar to MIDI  v" + String (JucePlugin_VersionString));
    addReaperItems (menu);
    PopupMenu factory;
    for (int i = 0; i < OmiAudioProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + OmiAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    // reglages propres a Omi
    PopupMenu feel, lowest, chan, thru;
    const int fNow = roundToInt (proc.rawAt (OmiAudioProcessor::rFeel));
    const char* feelHelp[3] = { "Fast (lowest latency)", "Normal", "Stable (fewest wrong notes)" };
    for (int i = 0; i < 3; ++i) feel.addItem (50 + i, feelHelp[i], true, fNow == i);
    menu.addSubMenu ("Play feel", feel);
    const int lNow = roundToInt (proc.rawAt (OmiAudioProcessor::rLowest));
    for (int i = 0; i < OmiAudioProcessor::numLowest; ++i) lowest.addItem (60 + i, OmiAudioProcessor::lowestNames()[i], true, lNow == i);
    menu.addSubMenu ("Lowest note", lowest);
    const int cNow = roundToInt (proc.rawAt (OmiAudioProcessor::rChannel));
    for (int i = 1; i <= 16; ++i) chan.addItem (70 + i, "Channel " + String (i), true, cNow == i);
    menu.addSubMenu ("MIDI out channel", chan);
    PopupMenu fold;
    const int foNow = roundToInt (proc.rawAt (OmiAudioProcessor::rFold));
    fold.addSectionHeader ("Lower notes go up one octave");
    for (int i = 0; i < 4; ++i) fold.addItem (40 + i, OmiAudioProcessor::foldNames()[i], true, foNow == i);
    menu.addSubMenu ("Lowest MIDI note out", fold);
    menu.addItem (44, "Octave hold (keep the ringing note when its octave is played)", true, proc.rawAt (OmiAudioProcessor::rOctHold) > 0.5f);
    const bool muted = proc.rawAt (OmiAudioProcessor::rThru) > 0.5f;
    thru.addItem (90, "Thru (guitar still heard)", true, ! muted);
    thru.addItem (91, "Mute (MIDI only)", true, muted);
    menu.addSubMenu ("Guitar audio", thru);
    menu.addSeparator();

    PopupMenu midi;
    midi.addSectionHeader ("MIDI Learn");
    for (int i = 0; i < OmiAudioProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (OmiAudioProcessor::learnDef (i).name) + "  (" + ccText (i) + ")", true, lt == i);
    midi.addSeparator();
    midi.addItem (1200, "Cancel MIDI learn", lt >= 0);
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    NineHandsWindow::addMidiFootswitchMode (midi, proc.rawAt (OmiAudioProcessor::rMidiFsMode) < 0.5f);
    midi.addSeparator();
    NineHandsWindow::addProgramChangeInfo (midi);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();

    const char* names[numSizes] = { "60 %", "75 %", "100 %" };
    const int heights[numSizes] = { sizeHeight (0), sizeHeight (1), sizeHeight (2) };
    menu.addSubMenu ("Plugin size", NineHandsWindow::sizeMenu (JucePlugin_Name, numSizes, names, heights, getHeight()));
    menu.addSubMenu ("Glow intensity", NineHandsWindow::glowMenu (glowPc));
    return menu;
}

void OmiAudioProcessorEditor::showContextMenu (int learnIdx)
{
    auto menu = buildContextMenu (learnIdx);
    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),       // toujours la ou l'on clique
        [safe = Component::SafePointer<OmiAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            int inst = pr.uiSizeIdx.load();
            const int sz = NineHandsWindow::handleSize (r, JucePlugin_Name, numSizes, inst);
            if (sz >= 0) { pr.uiSizeIdx = inst; ed.setScaledHeight (ed.sizeHeight (sz)); return; }
            if (const int pc = NineHandsWindow::handleGlow (r, JucePlugin_Name))
            {
                ed.setGlowPercent (pc);
                ed.learnLine = "Glow  " + String (pc) + " %";
                ed.learnLineUntil = Time::getMillisecondCounterHiRes() + 1500.0;
                ed.statusUntil = jmax (ed.statusUntil, ed.learnLineUntil);
                return;
            }
            if      (r == NineHandsWindow::idLatch)     pr.setParamValue ("midiFsMode", 0.0f);
            else if (r == NineHandsWindow::idMomentary) pr.setParamValue ("midiFsMode", 1.0f);
            else if (r >= 40 && r <= 43) pr.setParamValue ("fold", (float) (r - 40));
            else if (r == 44) pr.setParamValue ("octHold", pr.rawAt (OmiAudioProcessor::rOctHold) > 0.5f ? 0.0f : 1.0f);
            else if (r >= 50 && r <= 52) pr.setParamValue ("feel", (float) (r - 50));
            else if (r >= 60 && r < 60 + OmiAudioProcessor::numLowest) pr.setParamValue ("lowest", (float) (r - 60));
            else if (r >= 71 && r <= 86) pr.setParamValue ("channel", (float) (r - 70));
            else if (r == 90 || r == 91) pr.setParamValue ("thru", (float) (r - 90));
            else if (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            else if (ed.handleReaperItem (r)) return;
            else if (r >= 1000) { ed.handleMidiMenu (r); return; }
            ed.showStatus();
        });
}

//==============================================================================
// REAPER : piste d'instrument qui recoit le MIDI d'Omi (receive Audio : None, MIDI : All -> All)
//==============================================================================
void OmiAudioProcessorEditor::addReaperItems (PopupMenu& menu)
{
    auto& rb = proc.reaperBridge;
    if (! rb.available()) return;
    const auto recv = rb.receivers();
    for (int i = 0; i < recv.size(); ++i)
    {
        const auto& t = recv.getReference (i);
        if (t.instrument.isNotEmpty()) omi::ReaperBridge::setLastInstrument (t.instrument);
        menu.addItem (400 + i, juce::String (juce::CharPointer_UTF8 ("MIDI \xe2\x86\x92 ")) + t.trackName + (t.instrument.isNotEmpty() ? "  (" + t.instrument.fromFirstOccurrenceOf (": ", false, false) + ")" : String ("  (no instrument)")));
    }
    menu.addItem (390, recv.isEmpty() ? "Create MIDI track + choose instrument..." : "Create another MIDI track + choose instrument...");
    const auto last = omi::ReaperBridge::lastInstrument();
    if (last.isNotEmpty())
        menu.addItem (391, "Create MIDI track with " + last.fromFirstOccurrenceOf (": ", false, false));
    menu.addItem (392, "Create the MIDI track automatically when Omi is inserted", true, omi::ReaperBridge::autoCreate());
    menu.addSeparator();
    menu.addItem (393, "Dry guitar sound (uncheck = instrument only)", true, proc.rawAt (OmiAudioProcessor::rThru) < 0.5f);
    menu.addSeparator();
}

bool OmiAudioProcessorEditor::handleReaperItem (int r)
{
    auto& rb = proc.reaperBridge;
    if (r == 393) { proc.setParamValue ("thru", proc.rawAt (OmiAudioProcessor::rThru) > 0.5f ? 0.0f : 1.0f); return true; }
    if (r == 390) { showInstrumentChooser(); return true; }
    if (r == 391)
    {
        const bool ok = rb.createInstrumentTrack (omi::ReaperBridge::lastInstrument());
        learnLine = ok ? String ("MIDI track created") : String ("REAPER only");
        learnLineUntil = Time::getMillisecondCounterHiRes() + 2500.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
        return true;
    }
    if (r == 392) { omi::ReaperBridge::setAutoCreate (! omi::ReaperBridge::autoCreate()); return true; }
    if (r >= 400 && r < 464) { rb.showReceiver (r - 400); return true; }
    return false;
}

// Choix de l'instrument de la piste MIDI : liste des instruments installes (VSTi, VST3i, CLAPi), par format puis par
// fabricant, au design des menus d'Octopus ; la piste MIDI est creee sous celle d'Omi avec l'instrument choisi
void OmiAudioProcessorEditor::showInstrumentChooser()
{
    auto& rb = proc.reaperBridge;
    if (! rb.available()) return;
    chooserList = rb.installedInstruments();
    if (chooserList.isEmpty()) { rb.createInstrumentTrack ({}); return; }      // REAPER ancien : sa propre fenetre de choix
    PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    NineHandsMenu::addHeader (menu, menuLnf.f, NineHandsMenu::icoMidi, accent, "New MIDI track", "Choose the instrument");
    const auto last = omi::ReaperBridge::lastInstrument();
    if (last.isNotEmpty()) { menu.addItem (2000, "Last used: " + last.fromFirstOccurrenceOf (": ", false, false)); menu.addSeparator(); }
    // format (VST3i, VSTi, CLAPi...) -> fabricant -> instrument
    std::map<String, std::map<String, PopupMenu>> tree;
    for (int i = 0; i < chooserList.size(); ++i)
    {
        const auto& n = chooserList.getReference (i);
        const auto format = n.upToFirstOccurrenceOf (":", false, false).trim();
        const auto rest = n.fromFirstOccurrenceOf (":", false, false).trim();
        auto vendor = rest.fromFirstOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false).trim();
        if (vendor.isEmpty()) vendor = "Other";
        tree[format][vendor].addItem (3000 + i, rest.upToFirstOccurrenceOf (" (", false, false));
    }
    for (auto& f : tree)
    {
        PopupMenu fm;
        for (auto& v : f.second) fm.addSubMenu (v.first, v.second);
        menu.addSubMenu (f.first, fm);
    }
    menu.addSeparator();
    menu.addItem (2001, "Cancel (no MIDI track)");
    auto opts = PopupMenu::Options();
    opts = isShowing() ? opts.withTargetScreenArea (localAreaToGlobal (scaled (layout::screenRect))) : opts.withMousePosition();
    menu.showMenuAsync (opts, [safe = Component::SafePointer<OmiAudioProcessorEditor> (this)] (int r)
    {
        if (safe == nullptr || r == 0 || r == 2001) return;
        auto& ed = *safe;
        String inst;
        if (r == 2000) inst = omi::ReaperBridge::lastInstrument();
        else if (r >= 3000 && r - 3000 < ed.chooserList.size()) inst = ed.chooserList[r - 3000];
        if (inst.isEmpty()) return;
        const bool ok = ed.proc.reaperBridge.createInstrumentTrack (inst);
        ed.learnLine = ok ? String ("MIDI track created") : String ("REAPER only");
        ed.learnLineUntil = Time::getMillisecondCounterHiRes() + 2500.0;
        ed.statusUntil = jmax (ed.statusUntil, ed.learnLineUntil);
    });
}

void OmiAudioProcessorEditor::showReaperMenu()
{
    if (! proc.reaperBridge.available()) { showContextMenu(); return; }
    PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    NineHandsMenu::addHeader (menu, menuLnf.f, NineHandsMenu::icoMidi, accent, "MIDI out", "Instrument track in REAPER");
    addReaperItems (menu);
    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),
        [safe = Component::SafePointer<OmiAudioProcessorEditor> (this)] (int r)
        { if (safe != nullptr && r != 0) safe->handleReaperItem (r); });
}
