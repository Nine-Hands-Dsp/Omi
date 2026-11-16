#pragma once
#include <cstring>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "OmiDSP.h"

//==============================================================================
// OMI (NINE HANDS | DSP) - convertisseur guitare -> MIDI (monophonique), voir README.md et OmiDSP.h.
// Effet VST3 a sortie MIDI : dans Reaper, envoi de la piste guitare vers la piste du VST de basse (Audio : None, MIDI : All).
//==============================================================================

struct OmiPreset
{
    const char* name;
    float sens, dyn;      // sens en dB, dyn 0..10
    int bend, trans;      // bend : index de plage (0 = chromatique), trans en demi-tons
};

class OmiAudioProcessor : public juce::AudioProcessor,
                          private juce::AsyncUpdater,
                          private juce::Timer
{
public:
    OmiAudioProcessor();
    ~OmiAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Omi"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    static int getNumFactoryPresets();
    static const OmiPreset& getFactoryPreset (int index);
    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override { applyFactoryPreset (index); }
    const juce::String getProgramName (int index) override { return getFactoryPreset (index).name; }
    void changeProgramName (int, const juce::String&) override {}
    void applyFactoryPreset (int index);

    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float plain);

    // anti-gel : pointeurs des parametres mis en cache (aucune recherche par chaine dans processBlock)
    enum RawIndex { rSens, rDyn, rBend, rTrans, rActive, rMidiFsMode, rFeel, rChannel, rLowest, rThru, numRaw };
    float rawAt (int i) const { return rawPtr[i]->load (std::memory_order_relaxed); }
    float raw (const char* id) const
    {
        for (int i = 0; i < numRaw; ++i)
            if (std::strcmp (id, rawIds[i]) == 0) return rawAt (i);
        return apvts.getRawParameterValue (id)->load();
    }
    static constexpr const char* rawIds[numRaw] = { "sens", "dyn", "bend", "trans", "active", "midiFsMode", "feel", "channel", "lowest", "thru" };
    std::atomic<float>* rawPtr[numRaw] {};

    static constexpr int numBendSteps = 9;
    static int bendSemis (int idx) { static const int b[numBendSteps] = { 0, 1, 2, 3, 4, 5, 7, 12, 24 }; return b[juce::jlimit (0, numBendSteps - 1, idx)]; }
    static juce::String bendText (int idx) { const int b = bendSemis (idx); return b == 0 ? juce::String ("CHROMA") : "\xc2\xb1" + juce::String (b); }
    static juce::StringArray feelNames()   { return { "Fast", "Normal", "Stable" }; }
    static juce::StringArray lowestNames() { return { "E2  (6 strings)", "B1  (7 strings)", "F#1  (8 strings)", "A0  (8 strings, very low tunings)" }; }
    static float lowestHz (int idx) { static const float f[4] = { 78.0f, 58.0f, 44.0f, 26.0f }; return f[juce::jlimit (0, 3, idx)]; }
    static juce::String noteName (int midi);

    // ---- informations pour la fenetre (ecrites par le thread audio, lues par l'interface) ----
    std::atomic<float> uiDetected { -1.0f }, uiClarity { 0.0f }, uiLevelDb { -120.0f };
    std::atomic<int>   uiOutNote { -1 }, uiOutVel { 0 }, uiBend { 8192 }, uiNoteCounter { 0 };
    std::atomic<bool>  uiGate { false };

    std::atomic<int> uiSizeIdx { -1 };
    std::atomic<int> currentPreset { 0 };
    std::atomic<int> presetCounter { 0 };
    std::atomic<int> lastMidiCC { -1 }, lastMidiValue { 0 }, lastMidiProgram { -1 }, midiCounter { 0 };

    static constexpr int numKnobs = 4;
    static const char* knobId (int i);

    enum LearnType { learnContinuous, learnFootswitch };
    struct LearnDef { const char* id; const char* name; int type; };
    static constexpr int numLearn = 5;              // 0-3 potards, 4 footswitch
    static const LearnDef& learnDef (int i);
    std::atomic<int> ccMap[numLearn];
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> lastLearned { -1 };
    void resetMidiMap();

private:
    void handleMidi (juce::MidiBuffer& midi, juce::MidiBuffer& passThru);
    void setParamFromAudio (int queueIndex, float plain);
    void timerCallback() override;
    static constexpr int numQueued = numLearn;
    std::atomic<float> pendingValue[numQueued];
    std::atomic<bool>  pendingSet[numQueued];
    void handleAsyncUpdate() override;
    float mapCC (int knob, float raw01);

    omi::Tracker tracker;
    omi::Event events[omi::Tracker::maxEvents];
    juce::MidiBuffer passThru;
    juce::HeapBlock<float> mono;
    int monoSize = 0;
    double sr = 48000.0;
    bool wasActive = true;
    int lastChannel = 1;

    struct ExprRamp { float lo = 1.0f, hi = 0.0f; int msb = -1; } ramps[numKnobs];
    int prevFsCC[numLearn] {};
    std::atomic<int> pendingPreset { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmiAudioProcessor)
};
