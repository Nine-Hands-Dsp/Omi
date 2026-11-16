#pragma once
#include <juce_core/juce_core.h>

//==============================================================================
// Pont vers l'API de REAPER (VST3 : interface IReaperHostApplication, REAPER 5.02+).
// Permet a Omi de creer, sous sa propre piste, une piste d'instrument qui recoit son MIDI
// (receive Audio : None, MIDI : All -> All), puis d'ouvrir le navigateur d'effets pour choisir le VST
// d'instrument, ou d'y charger directement le dernier instrument utilise.
// Hors de REAPER (ou outil de rendu Linux), available() renvoie false et rien n'est propose.
//==============================================================================
namespace omi
{
class ReaperBridge
{
public:
    ReaperBridge();
    ~ReaperBridge();

    void setHost (void* funknownHostApplication);   // appele par VST3ClientExtensions::setIHostApplication
    bool available() const;

    struct Target { juce::String trackName, instrument; };
    // pistes qui recoivent deja le MIDI de la piste d'Omi (nom de la piste et premier effet)
    juce::Array<Target> receivers() const;
    // cree la piste d'instrument sous la piste d'Omi ; instrument vide = ouvre le navigateur d'effets
    bool createInstrumentTrack (const juce::String& instrument);
    // ouvre la fenetre de l'instrument de la n-ieme piste receptrice
    void showReceiver (int index);
    // a appeler regulierement (timer) : relie a Omi la piste creee par la fenetre de choix d'instrument de REAPER
    void poll();

    static juce::String lastInstrument();
    static bool autoCreate();                      // creer la piste d'office a l'insertion d'Omi (reglage commun, oui par defaut)
    static void setAutoCreate (bool on);
    static void setLastInstrument (const juce::String& name);

private:
    void wire (void* src, void* dst);
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace omi
