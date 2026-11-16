#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"
#include "NHKnob.h"
#include "NineHandsMenu.h"
#include "NineHandsWindow.h"

//==============================================================================
// Footswitch (photo du kit, dans le fond) : fonctionnement d'Echoflex : bascule a l'appui,
// bouton assombri tant qu'on appuie ; reflet bleu de la LED sur le flanc de l'ecrou, en fondu avec elle.
//==============================================================================
class FootSwitchArea : public juce::Component
{
public:
    explicit FootSwitchArea (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick, onUserChange;
    juce::Image reflection;
    float ledLevel = 0.0f;
private:
    bool value = false, pressed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class OmiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit OmiAudioProcessorEditor (OmiAudioProcessor&);
    ~OmiAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // Repere du fond (pedale plein cadre, image 1235 x 1800 px / 2). Pedale simple : 493 px (« 60 % ») a 150 %.
    static constexpr float baseW = layout::frameW;
    static constexpr float baseH = layout::frameH;
    static constexpr int defaultH = 493;
    static constexpr int refH = 958;
    static constexpr int maxH = refH * 3 / 2;

    void setScaledHeight (int h);
    void forceRenderScale (float sc) { forcedScale = sc; rebuildCaches (sc); }
    static constexpr int numSizes = 3;
    static int sizeHeight (int idx);
    void showStatus();
    juce::PopupMenu buildContextMenu (int learnIdx = -1);
    void setGlowPercent (int pc);
    void tickForCapture() { timerCallback(); }            // outil de rendu

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void visibilityChanged() override;
    void askRepaint (juce::Rectangle<int> r);
    void showContextMenu (int learnIdx = -1);
    void addReaperItems (juce::PopupMenu& menu);
    bool handleReaperItem (int r);
    void showReaperMenu();
    bool hintShown = false;
    void handleMidiMenu (int r);
    void showPresetMenu();
    void stepPreset (int delta);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void buildPrint();
    void drawPrint (juce::Graphics&);
    void drawLed (juce::Graphics&);
    void drawPresetBar (juce::Graphics&);
    void drawScreen (juce::Graphics&);

    OmiAudioProcessor& proc;

    juce::Image background, bgScaled, glow, glowS, ledOn;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f, forcedScale = 0.0f;
    int resizeSettle = 0;
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    juce::Typeface::Ptr labelFace, boldFace, lightFace;
    NineHandsMenu::MenuLnf menuLnf;
    NineHandsWindow::HostLock hostLock;
    juce::Path printPath;
    std::unique_ptr<juce::Drawable> logo;

    std::unique_ptr<nh::Knob> knobs[OmiAudioProcessor::numKnobs];
    FootSwitchArea footswitch;

    // ecran : valeurs affichees (redessin seulement si elles changent)
    struct ScreenState
    {
        int inNote = -1, cents = 0, outNote = -1, vel = 0, bend = 8192, level = 0, sens = 0, chan = 1, bendIdx = 0, active = 1, dryOff = 0;
        bool operator== (const ScreenState& o) const
        { return inNote == o.inNote && cents == o.cents && outNote == o.outNote && vel == o.vel && bend == o.bend && level == o.level
                 && sens == o.sens && chan == o.chan && bendIdx == o.bendIdx && active == o.active && dryOff == o.dryOff; }
    } scr, drawnScr;
    float meterDb = -120.0f, heldIn = -1.0f;
    int holdTicks = 0;

    float ledLevel = 0.0f, drawnLed = -1.0f;
    int glowPc = 100;
    double lastGlowReadMs = 0.0;
    float glowAmt() const { return (float) glowPc / 100.0f; }
    int lastChanged = -1;
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    double statusUntil = 0.0;
    juce::String lastLine1, lastPreset;
    float lastValues[12];
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0;
    int hoverZone = 0, drawnHover = -1;
    juce::String learnLine;
    double learnLineUntil = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmiAudioProcessorEditor)
};
