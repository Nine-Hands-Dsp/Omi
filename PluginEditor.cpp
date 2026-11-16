#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    const Rectangle<float> presetRect = layout::presetRect;
    const Colour inkCream (0xffece4d0);     // serigraphie creme
    const Colour accent (0xff3c8cff);       // bleu de la LED
    const Colour panel (0xff060a12);        // fond de la pastille
    const Colour scrCyan (0xff5acdff), scrWhite (0xffebf4ff), scrGrey (0xff788ca5), scrDim (0xff1e2c3e);

    const char* const valueIds[] = { "sens", "dyn", "bend", "trans", "active", "feel", "channel", "lowest", "thru", "fold", "octHold" };
    constexpr int numValueIds = 11;
    constexpr float meterLo = -72.0f, meterHi = -6.0f;
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
// EDITEUR
//==============================================================================
OmiAudioProcessorEditor::OmiAudioProcessorEditor (OmiAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      footswitch (*p.apvts.getParameter ("active"))
{
    background = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    glow       = ImageCache::getFromMemory (BinaryData::glow_blue_png, BinaryData::glow_blue_pngSize);
    ledOn      = ImageCache::getFromMemory (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    footswitch.reflection = ImageCache::getFromMemory (BinaryData::fs_refl_png, BinaryData::fs_refl_pngSize);
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    boldFace  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
    lightFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsLight_ttf, BinaryData::PoppinsLight_ttfSize);
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogoWhite_svg, BinaryData::NineHandsLogoWhite_svgSize)))
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
    bgScaled = (pw >= background.getWidth()) ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);
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
    if (id == "thru")    return v > 0.5f ? "guitar audio muted" : "guitar audio thru";
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
    const float hr = 20.0f;
    ColourGradient halo (accent.withAlpha (0.30f * level * glowAmt()), c.x, c.y, accent.withAlpha (0.0f), c.x + hr, c.y, true);
    halo.addColour (0.4, accent.withAlpha (0.08f * level * glowAmt()));
    g.setGradientFill (halo);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);
}

//==============================================================================
// Ecran : note jouee (et ecart en cents), note MIDI envoyee, velocite, plage de bend, vu-metre d'entree + seuil SENS
//==============================================================================
void OmiAudioProcessorEditor::drawScreen (Graphics& g)
{
    const auto r = layout::screenRect;
    const auto& s = drawnScr;
    const float pad = 16.0f;
    auto font = [this] (int w, float h) { return Font (FontOptions (w == 1 ? boldFace : (w == 2 ? lightFace : labelFace)).withHeight (h)); };
    auto text = [&] (const String& t, float x, float y, float w, float h, Justification j, Colour c, Font f)
    { g.setColour (c); g.setFont (f); g.drawText (t, Rectangle<float> (x, y, w, h), j, false); };

    const float half = r.getWidth() * 0.5f;
    const Colour off = scrDim;
    text ("IN", r.getX() + pad, r.getY() + 8.0f, 60.0f, 16.0f, Justification::centredLeft, scrGrey, font (0, 15.0f));
    text ("MIDI OUT  CH " + String (s.chan), r.getRight() - pad - 200.0f, r.getY() + 8.0f, 200.0f, 16.0f, Justification::centredRight, scrGrey, font (0, 15.0f));

    // note jouee
    if (s.inNote >= 0)
    {
        static const char* nn[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const String name (nn[s.inNote % 12]);
        const auto big = font (1, 70.0f);
        const float nw = GlyphArrangement::getStringWidth (big, name);
        text (name, r.getX() + pad, r.getY() + 28.0f, nw + 4.0f, 72.0f, Justification::centredLeft, scrWhite, big);
        text (String (s.inNote / 12 - 1), r.getX() + pad + nw + 4.0f, r.getY() + 64.0f, 40.0f, 32.0f, Justification::centredLeft, scrWhite, font (0, 30.0f));
        text ((s.cents > 0 ? "+" : "") + String (s.cents) + " ct", r.getX() + pad + nw + 30.0f, r.getY() + 34.0f, 90.0f, 18.0f, Justification::centredLeft, scrCyan, font (0, 16.0f));
    }
    else
        text ("--", r.getX() + pad, r.getY() + 28.0f, 120.0f, 72.0f, Justification::centredLeft, off, font (1, 70.0f));

    // fleche
    {
        const float ax = r.getX() + half + 8.0f, ay = r.getY() + 64.0f;
        Path p; p.startNewSubPath (ax - 18.0f, ay); p.lineTo (ax + 16.0f, ay);
        p.startNewSubPath (ax + 8.0f, ay - 8.0f); p.lineTo (ax + 17.0f, ay); p.lineTo (ax + 8.0f, ay + 8.0f);
        g.setColour (s.outNote >= 0 ? scrGrey : off);
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // note MIDI envoyee
    const float rx = r.getRight() - pad;
    if (! s.active)
        text ("BYPASS", rx - 200.0f, r.getY() + 38.0f, 200.0f, 44.0f, Justification::centredRight, scrGrey, font (1, 34.0f));
    else if (s.outNote >= 0)
    {
        text (OmiAudioProcessor::noteName (s.outNote), rx - 200.0f, r.getY() + 36.0f, 200.0f, 48.0f, Justification::centredRight, scrCyan, font (1, 46.0f));
        text ("NOTE " + String (s.outNote) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 VEL ")) + String (s.vel), rx - 240.0f, r.getY() + 82.0f, 240.0f, 18.0f, Justification::centredRight, scrGrey, font (0, 15.0f));
    }
    else
        text ("--", rx - 200.0f, r.getY() + 36.0f, 200.0f, 48.0f, Justification::centredRight, off, font (1, 46.0f));
    text (OmiAudioProcessor::bendSemis (s.bendIdx) == 0 ? String ("CHROMA") : juce::String (juce::CharPointer_UTF8 ("BEND \xc2\xb1")) + String (OmiAudioProcessor::bendSemis (s.bendIdx)),
          rx - 200.0f, r.getY() + 100.0f, 200.0f, 18.0f, Justification::centredRight, scrGrey, font (0, 15.0f));

    // justesse : point sur une echelle de -50 a +50 cents
    {
        const float x0 = r.getX() + pad, x1 = r.getX() + half - 20.0f, y = r.getY() + 108.0f;
        g.setColour (Colour (0xff3c5069));
        g.drawLine (x0, y, x1, y, 1.2f);
        for (int k = -4; k <= 4; ++k)
        {
            const float xk = (x0 + x1) * 0.5f + (float) k * (x1 - x0) / 8.0f, hk = k == 0 ? 5.0f : 3.0f;
            g.setColour (Colour (0xff50647d));
            g.drawLine (xk, y - hk, xk, y + hk, 1.2f);
        }
        if (s.inNote >= 0)
        {
            const float xm = (x0 + x1) * 0.5f + jlimit (-1.0f, 1.0f, (float) s.cents / 50.0f) * (x1 - x0) * 0.5f;
            g.setColour (std::abs (s.cents) <= 5 ? scrCyan : Colour (0xffffb45a));
            g.fillEllipse (xm - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
    }

    // vu-metre d'entree (40 segments) + repere du seuil SENS
    {
        const float x0 = r.getX() + pad, x1 = r.getRight() - pad, y = r.getBottom() - 22.0f;
        const int segs = 40;
        const float sw = (x1 - x0) / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool lit = i < s.level;
            Colour c = lit ? scrCyan : Colour (0xff1e2c3e);
            if (lit && i >= 36) c = Colour (0xffff8c5a);
            if (lit && s.level > 0 && (float) i < (float) segs * ((float) s.sens - meterLo) / (meterHi - meterLo)) c = c.withMultipliedAlpha (0.45f);   // sous le seuil : pas de note
            g.setColour (c);
            g.fillRect (x0 + (float) i * sw, y - 5.0f, sw - 2.2f, 10.0f);
        }
        const float xt = x0 + jlimit (0.0f, 1.0f, ((float) s.sens - meterLo) / (meterHi - meterLo)) * (x1 - x0);
        Path tri; tri.addTriangle (xt, y - 8.0f, xt - 5.0f, y - 15.0f, xt + 5.0f, y - 15.0f);
        g.setColour (scrWhite);
        g.fillPath (tri);
        g.drawLine (xt, y - 8.0f, xt, y + 8.0f, 1.6f);
        text ("SENS", xt + 9.0f, y - 22.0f, 60.0f, 14.0f, Justification::centredLeft, scrWhite, font (0, 12.0f));
    }
}

// Pastille unique : barre de presets au repos, ligne d'etat pendant les reglages (fondu croise)
void OmiAudioProcessorEditor::drawPresetBar (Graphics& g)
{
    const auto r = presetRect;
    const float a = statusAlpha, pa = 1.0f - a;
    g.setColour (panel.withAlpha (0.84f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colour (0xff5a7090).withAlpha ((hoverZone != 0 ? 0.75f : 0.45f) * pa + 0.55f * a));
    g.drawRoundedRectangle (r.reduced (0.6f), r.getHeight() * 0.5f, 1.0f);
    const Font f (FontOptions (labelFace).withHeight (layout::presetFont));
    const float aw = 34.0f;
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
        g.setColour ((midiFlash > 0 ? scrCyan : Colour (0xffecf0f6)).withAlpha ((hoverZone == 2 ? 1.0f : 0.92f) * pa));
        g.drawFittedText (lastPreset, r.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    }
    if (a > 0.0f)
    {
        g.setFont (f);
        g.setColour ((midiFlash > 0 ? scrCyan : Colour (0xffecf0f6)).withAlpha (a));
        g.drawFittedText (lastLine1.toUpperCase().replace (" DB", " dB"), r.reduced (r.getHeight() * 0.5f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    }
}

//==============================================================================
// Serigraphie vectorielle
//==============================================================================
void OmiAudioProcessorEditor::buildPrint()
{
    printPath.clear();
    for (const auto& t : layout::texts)
    {
        const Typeface::Ptr face = t.font == 1 ? boldFace : (t.font == 2 ? lightFace : labelFace);
        const Font font (FontOptions (face).withPointHeight (t.em));
        const String s (t.s);
        float total = 0.0f;
        for (int i = 0; i < s.length(); ++i)
            total += GlyphArrangement::getStringWidth (font, String::charToString (s[i])) + (i > 0 ? t.track : 0.0f);
        const float base = (font.getAscent() - font.getDescent()) * 0.5f;
        GlyphArrangement ga;
        float x = -total * 0.5f;
        for (int i = 0; i < s.length(); ++i)
        {
            const String ch = String::charToString (s[i]);
            ga.addLineOfText (font, ch, x, base);
            x += GlyphArrangement::getStringWidth (font, ch) + t.track;
        }
        Path p; ga.createPath (p);
        p.applyTransform (AffineTransform::rotation (-t.rot * MathConstants<float>::pi / 180.0f).translated (t.x, t.y));
        printPath.addPath (p);
    }
    for (const auto& d : layout::dots)  printPath.addEllipse (d.x - d.r, d.y - d.r, d.r * 2.0f, d.r * 2.0f);
    for (const auto& r : layout::rules) printPath.addRectangle (r);
}

void OmiAudioProcessorEditor::drawPrint (Graphics& g)
{
    g.setColour (inkCream);
    g.fillPath (printPath);
    if (logo != nullptr)
    {
        const auto vb = logo->getDrawableBounds();
        const float w = layout::logoW, h = w * vb.getHeight() / jmax (1.0f, vb.getWidth());
        logo->drawWithin (g, { layout::logoPos.x - w * 0.5f, layout::logoPos.y - h * 0.5f, w, h }, RectanglePlacement::centred, 0.92f);
    }
}

void OmiAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colour (0xff111112));
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
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
    if (p.x < presetRect.getX() + 34.0f) return 1;
    if (p.x > presetRect.getRight() - 34.0f) return 3;
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
}

bool OmiAudioProcessorEditor::handleReaperItem (int r)
{
    auto& rb = proc.reaperBridge;
    if (r == 390 || r == 391)
    {
        const bool ok = rb.createInstrumentTrack (r == 391 ? omi::ReaperBridge::lastInstrument() : String());
        learnLine = ok ? String ("MIDI track created") : String ("REAPER only");
        learnLineUntil = Time::getMillisecondCounterHiRes() + 2500.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
        return true;
    }
    if (r == 392) { omi::ReaperBridge::setAutoCreate (! omi::ReaperBridge::autoCreate()); return true; }
    if (r >= 400 && r < 464) { rb.showReceiver (r - 400); return true; }
    return false;
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
