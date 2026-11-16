// Captures PNG de la fenetre reelle d'Omi (meme code que le plugin) + test de bout en bout (audio -> MIDI).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    OmiAudioProcessor proc;
    const double fs = 48000.0;
    proc.prepareToPlay (fs, 256);
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* oed = static_cast<OmiAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto pump = [] (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (40); };

    // corde grave synthetique (fondamentale faible), note MIDI attendue
    double phase[13] = {};
    auto play = [&] (int midiNote, double secs, float amp, int& notesOut, int& lastNote)
    {
        const double f = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
        const int total = (int) (secs * fs); int done = 0;
        while (done < total)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m;
            for (int i = 0; i < 256; ++i)
            {
                const double t = (done + i) / fs; double v = 0;
                for (int h = 1; h <= 10; ++h) { phase[h] += 2 * MathConstants<double>::pi * f * h * std::sqrt (1 + 0.0004 * h * h) / fs;
                                                v += (h == 1 ? 0.12 : 1.0 / h) * std::exp (-t * (1.0 + 0.5 * h)) * std::sin (phase[h]); }
                const float s = (float) (amp * jmin (1.0, t / 0.003) * v);
                b.setSample (0, i, s); b.setSample (1, i, s);
            }
            proc.processBlock (b, m);
            for (const auto meta : m) if (meta.getMessage().isNoteOn()) { ++notesOut; lastNote = meta.getMessage().getNoteNumber(); }
            done += 256;
        }
    };
    auto silence = [&] (double secs) { int n = 0, l = 0; play (0, 0, 0, n, l); for (int k = 0; k < (int) (secs * fs / 256); ++k) { AudioBuffer<float> b (2, 256); b.clear(); MidiBuffer m; proc.processBlock (b, m); } for (auto& p : phase) p = 0; };

    proc.applyFactoryPreset (0);                // Bass Mono : -12
    int ok = 0, tot = 0;
    for (int n : { 30, 28, 26, 21, 40, 52 })
    {
        int cnt = 0, last = -1;
        play (n, 0.6, 0.4f, cnt, last);
        ++tot; if (cnt == 1 && last == n - 12) ++ok;
        std::printf ("joue %s -> %d note(s), derniere %s\n", OmiAudioProcessor::noteName (n).toRawUTF8(), cnt, OmiAudioProcessor::noteName (last).toRawUTF8());
        silence (0.1);
    }
    std::printf ("bout en bout : %d / %d\n", ok, tot);

    auto shot = [&] (const String& name, int height, int note, bool status)
    {
        oed->setScaledHeight (height);
        oed->forceRenderScale (2.0f);
        int cnt = 0, last = -1;
        if (note > 0) play (note, 0.25, 0.4f, cnt, last); else silence (0.2);
        for (int i = 0; i < 20; ++i) oed->tickForCapture();
        if (status) proc.setParamValue ("sens", -44.0f);
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    proc.applyFactoryPreset (0);
    shot ("omi_note", OmiAudioProcessorEditor::refH, 31, false);
    shot ("omi_default", OmiAudioProcessorEditor::defaultH, 28, false);
    shot ("omi_status", OmiAudioProcessorEditor::refH, 26, true);
    proc.setParamValue ("active", 0.0f);
    shot ("omi_bypass", OmiAudioProcessorEditor::refH, 0, false);
    proc.setParamValue ("active", 1.0f);
    {
        oed->setScaledHeight (900); oed->forceRenderScale (2.0f);
        int c = 0, l = 0; proc.applyFactoryPreset (0); play (31, 0.25, 0.4f, c, l);
        for (int i = 0; i < 20; ++i) oed->tickForCapture();
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1800.0f / (float) ed->getHeight());
        File f (out + "/omi_exact.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();
    return ok == tot ? 0 : 1;
}
