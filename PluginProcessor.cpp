#include "PluginProcessor.h"
#include "NineHandsFolder.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Presets d'usine
//==============================================================================
static const OmiPreset factoryPresets[] =
{
    //  nom                          sens    dyn   bend  trans
    { "Bass Mono",                  -48.0f,  7.0f,  0,   -12 },   // la guitare joue la basse une octave plus bas, notes franches
    { "Bass Bend",                  -48.0f,  7.0f,  2,   -12 },   // idem avec glissements et vibratos en pitch bend (+/-2)
    { "8 Strings Sub",              -46.0f,  6.0f,  0,     0 },   // 8 cordes : la note jouee telle quelle (deja tres grave)
    { "Unison",                     -48.0f,  8.0f,  0,     0 },
    { "Synth Lead",                 -50.0f,  8.0f,  7,     0 },   // bends et vibratos de lead, plage +/-12
    { "Octave Up Pad",              -50.0f,  3.0f,  0,    12 },
    { "Fixed Velocity",             -48.0f,  0.0f,  0,     0 },   // velocite constante (100) : pour les sons sans dynamique
    { "Noisy Pickups",              -40.0f,  7.0f,  0,   -12 },   // seuil plus haut : micros simples, bruit de fond
};

int OmiAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }

const OmiPreset& OmiAudioProcessor::getFactoryPreset (int i)
{
    return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)];
}

const char* OmiAudioProcessor::knobId (int i)
{
    static const char* ids[numKnobs] = { "sens", "dyn", "bend", "trans" };
    return ids[jlimit (0, numKnobs - 1, i)];
}

const OmiAudioProcessor::LearnDef& OmiAudioProcessor::learnDef (int i)
{
    static const LearnDef defs[numLearn] =
    {
        { "sens",   "Sens",       learnContinuous },
        { "dyn",    "Dyn",        learnContinuous },
        { "bend",   "Bend",       learnContinuous },
        { "trans",  "Trans",      learnContinuous },
        { "active", "Footswitch", learnFootswitch },
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

String OmiAudioProcessor::noteName (int midi)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (midi < 0 || midi > 127) return "-";
    return String (n[midi % 12]) + String (midi / 12 - 1);
}

void OmiAudioProcessor::resetMidiMap()
{
    for (int i = 0; i < numLearn; ++i) ccMap[i].store (-1);     // aucune assignation par defaut : tout passe par le MIDI Learn
}

//==============================================================================
OmiAudioProcessor::OmiAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OMI", createLayout())
{
    for (int i = 0; i < numRaw; ++i) rawPtr[i] = apvts.getRawParameterValue (rawIds[i]);
    for (int i = 0; i < numQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    resetMidiMap();
    startTimerHz (60);
}

AudioProcessorValueTreeState::ParameterLayout OmiAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sens",
        NormalisableRange<float> (-72.0f, -12.0f, 0.1f), d.sens,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dyn", 1 }, "Dyn",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), d.dyn,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 10.0f)) + " %"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bend", 1 }, "Bend Range",
        NormalisableRange<float> (0.0f, (float) (numBendSteps - 1), 1.0f), (float) d.bend,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return bendText (roundToInt (v)); })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "trans", 1 }, "Transpose",
        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), (float) d.trans,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { const int t = roundToInt (v); return (t > 0 ? "+" : "") + String (t) + " st"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Footswitch", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "midiFsMode", 1 }, "MIDI Footswitch Mode",
                                                         StringArray { "Latching (127 / 0)", "Momentary (toggle)" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 1 }, "Play Feel", feelNames(), 1));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lowest", 1 }, "Lowest Note", lowestNames(), 4));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "thru", 1 }, "Guitar Audio", StringArray { "Thru", "Mute" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "fold", 1 }, "Lowest MIDI Note Out", foldNames(), 1));
    return { p.begin(), p.end() };
}

bool OmiAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == out;
}

void OmiAudioProcessor::setParamValue (const String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// Anti-gel : jamais d'appel a l'hote ni d'allocation depuis le thread audio
void OmiAudioProcessor::setParamFromAudio (int qi, float plain)
{
    if (qi < 0 || qi >= numQueued) return;
    pendingValue[qi].store (plain, std::memory_order_relaxed);
    pendingSet[qi].store (true, std::memory_order_release);
}

void OmiAudioProcessor::timerCallback()
{
    for (int i = 0; i < numQueued; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (learnDef (i).id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));

    // REAPER : Omi vient d'etre insere (pas recharge avec un projet) -> la piste d'instrument est creee d'office,
    // avec le dernier instrument utilise s'il est connu, sinon le navigateur d'effets s'ouvre pour le choisir
    if (++pollTicks >= 6) { pollTicks = 0; reaperBridge.poll(); }   // piste choisie dans la fenetre de REAPER -> reliee a Omi
    if (autoTrackTicks > 0 && --autoTrackTicks == 0)
        if (! restoredFromState && omi::ReaperBridge::autoCreate() && reaperBridge.available() && reaperBridge.receivers().isEmpty())
            reaperBridge.createInstrumentTrack ({});        // fenetre de choix de l'instrument de REAPER
}

//==============================================================================
void OmiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    tracker.prepare (sr);
    monoSize = jmax (512, samplesPerBlock * 2);
    mono.allocate ((size_t) monoSize, true);
    passThru.ensureSize (4096);
    setLatencySamples (0);
}

//==============================================================================
// MIDI entrant : MIDI Learn des commandes ; les messages non assignes passent vers le VST suivant
//==============================================================================
float OmiAudioProcessor::mapCC (int k, float raw01)
{
    auto& r = ramps[k];
    if (raw01 < r.lo) r.lo = raw01;
    if (raw01 > r.hi) r.hi = raw01;
    float v = raw01;
    if (r.hi - r.lo > 0.4f) v = (raw01 - r.lo) / (r.hi - r.lo);   // course de la pedale apprise
    constexpr float dz = 0.02f;
    return jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
}

void OmiAudioProcessor::handleMidi (MidiBuffer& midi, MidiBuffer& thru)
{
    const bool latching = rawAt (rMidiFsMode) < 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            const int lt = learnTarget.load();
            if (lt >= 0 && lt < numLearn && cc < 120)
            {
                for (int j = 0; j < numLearn; ++j)
                    if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);
                ccMap[lt].store (cc);
                if (lt < numKnobs) ramps[lt].msb = -1;
                if (learnDef (lt).type == learnFootswitch) prevFsCC[lt] = v;
                lastLearned.store (lt);
                learnTarget.store (-1);
                lastMidiCC = cc; lastMidiValue = v; ++midiCounter;
                continue;
            }
            int ctl = -1;
            for (int j = 0; j < numLearn; ++j)
                if (ccMap[j].load (std::memory_order_relaxed) == cc) { ctl = j; break; }
            int lsbOf = -1;
            if (ctl < 0)
                for (int k = 0; k < numKnobs; ++k)
                {
                    const int msbCC = ccMap[k].load (std::memory_order_relaxed);
                    if (msbCC >= 0 && msbCC < 32 && cc == msbCC + 32) { lsbOf = k; break; }
                }
            auto knobFromCC = [&] (int k, float raw01)
            {
                auto* prm = apvts.getParameter (knobId (k));
                setParamFromAudio (k, prm->convertFrom0to1 (mapCC (k, raw01)));
                currentPreset = -1;
            };
            if (ctl >= 0 && ctl < numKnobs)       { ramps[ctl].msb = v; knobFromCC (ctl, (float) v / 127.0f); }
            else if (lsbOf >= 0)                  { if (ramps[lsbOf].msb >= 0) knobFromCC (lsbOf, (float) (ramps[lsbOf].msb * 128 + v) / 16383.0f); }
            else if (ctl == 4)
            {
                int& prev = prevFsCC[ctl];
                const float cur = pendingSet[ctl].load() ? pendingValue[ctl].load() : rawAt (rActive);
                if (latching)                   setParamFromAudio (ctl, v >= 64 ? 1.0f : 0.0f);
                else if (v >= 64 && prev < 64)  setParamFromAudio (ctl, cur > 0.5f ? 0.0f : 1.0f);
                prev = v;
            }
            else { thru.addEvent (m, meta.samplePosition); continue; }    // CC non assigne : transmis au VST suivant
            lastMidiCC = cc; lastMidiValue = v; ++midiCounter;
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) { pendingPreset = pc; triggerAsyncUpdate(); }
            lastMidiProgram = pc; lastMidiCC = -2; ++midiCounter;
        }
        else thru.addEvent (m, meta.samplePosition);
    }
}

void OmiAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) applyFactoryPreset (p);
}

//==============================================================================
void OmiAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    passThru.clear();
    handleMidi (midi, passThru);
    midi.swapWith (passThru);                 // MIDI entrant non utilise : transmis tel quel
    if (n == 0) return;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    omi::Settings s;
    s.sensDb    = rawAt (rSens);
    s.dyn       = rawAt (rDyn) / 10.0f;
    s.bendRange = bendSemis (roundToInt (rawAt (rBend)));
    s.transpose = roundToInt (rawAt (rTrans));
    s.feel      = jlimit (0, 2, roundToInt (rawAt (rFeel)));
    s.channel   = jlimit (1, 16, roundToInt (rawAt (rChannel)));
    s.lowestHz  = lowestHz (roundToInt (rawAt (rLowest)));
    s.minOut    = foldNote (roundToInt (rawAt (rFold)));
    s.active    = rawAt (rActive) > 0.5f;

    // changement de canal : la note en cours est coupee sur l'ancien canal
    if (s.channel != lastChannel) { midi.addEvent (MidiMessage::allNotesOff (lastChannel), 0); lastChannel = s.channel; }

    // entree mono (moyenne des voies) analysee par blocs
    const int nCh = jmin (2, buffer.getNumChannels());
    for (int start = 0; start < n; start += monoSize)
    {
        const int len = jmin (monoSize, n - start);
        const float* l = buffer.getReadPointer (0, start);
        const float* r = nCh > 1 ? buffer.getReadPointer (1, start) : nullptr;
        for (int i = 0; i < len; ++i) mono[i] = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
        int nEv = 0;
        tracker.process (mono.get(), len, s, events, nEv);
        for (int e = 0; e < nEv; ++e)
        {
            const auto& ev = events[e];
            const int pos = jlimit (0, n - 1, start + ev.sample);
            switch (ev.type)
            {
                case omi::Event::NoteOn:  midi.addEvent (MidiMessage::noteOn (ev.a, ev.b / 128, (uint8) (ev.b % 128)), pos); ++uiNoteCounter; break;
                case omi::Event::NoteOff: midi.addEvent (MidiMessage::noteOff (ev.a, ev.b), pos); break;
                case omi::Event::Bend:    midi.addEvent (MidiMessage::pitchWheel (ev.a, ev.b), pos); break;
                case omi::Event::Rpn:
                    // RPN 0 : plage du pitch bend du VST recepteur
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 101, 0), pos);
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 100, 0), pos);
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 6, ev.b), pos);
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 38, 0), pos);
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 101, 127), pos);
                    midi.addEvent (MidiMessage::controllerEvent (ev.a, 100, 127), pos);
                    break;
            }
        }
    }
    if (! s.active && wasActive) midi.addEvent (MidiMessage::allNotesOff (s.channel), 0);   // footswitch : aucune note bloquee
    wasActive = s.active;

    if (rawAt (rThru) > 0.5f) buffer.clear();            // Guitar Audio : Mute

    uiDetected.store (tracker.detectedMidi, std::memory_order_relaxed);
    uiClarity.store (tracker.clarity, std::memory_order_relaxed);
    uiLevelDb.store (tracker.envDb, std::memory_order_relaxed);
    uiOutNote.store (tracker.sentNote, std::memory_order_relaxed);
    uiOutVel.store (tracker.lastVelocity, std::memory_order_relaxed);
    uiBend.store (tracker.lastBend, std::memory_order_relaxed);
    uiGate.store (tracker.gateOpen, std::memory_order_relaxed);
}

//==============================================================================
// Presets
//==============================================================================
int OmiAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void OmiAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParamValue ("sens", p.sens); setParamValue ("dyn", p.dyn);
    setParamValue ("bend", (float) p.bend); setParamValue ("trans", (float) p.trans);
    currentPreset = jlimit (0, getNumFactoryPresets() - 1, index);
    ++presetCounter;
}

File OmiAudioProcessor::getUserPresetFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile (nhd::folderName()).getChildFile ("Omi").getChildFile ("UserPresets.xml");
}

static const char* presetIds[] = { "sens", "dyn", "bend", "trans", "feel", "lowest", "fold" };

bool OmiAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return false;
    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<XmlElement> root;
    if (file.existsAsFile()) root = XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets")) root = std::make_unique<XmlElement> ("UserPresets");
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot) { root->removeChildElement (e, true); break; }
    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot", slot);
    for (auto* id : presetIds) e->setAttribute (id, (double) raw (id));
    const bool ok = root->writeTo (file);
    if (ok) { currentPreset = 100 + slot; ++presetCounter; }
    return ok;
}

bool OmiAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    auto root = XmlDocument::parse (file);
    if (root == nullptr) return false;
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot)
        {
            for (auto* id : presetIds)
                if (e->hasAttribute (id)) setParamValue (id, (float) e->getDoubleAttribute (id));
            currentPreset = 100 + slot; ++presetCounter;
            return true;
        }
    return false;
}

bool OmiAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    if (auto root = XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot) return true;
    return false;
}

//==============================================================================
void OmiAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    state.setProperty ("uiSize", uiSizeIdx.load(), nullptr);
    for (int i = 0; i < numLearn; ++i)
        state.setProperty (String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void OmiAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            uiSizeIdx = jlimit (-1, 2, (int) state.getProperty ("uiSize", -1));
            for (int i = 0; i < numLearn; ++i)
                ccMap[i].store (jlimit (-1, 127, (int) state.getProperty (String ("cc_") + learnDef (i).id, -1)));
            apvts.replaceState (state);
            restoredFromState = true;      // projet recharge, piste dupliquee, preset : jamais de piste creee d'office
            ++presetCounter;
        }
}

//==============================================================================
AudioProcessorEditor* OmiAudioProcessor::createEditor() { return new OmiAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OmiAudioProcessor(); }
