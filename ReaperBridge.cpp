#include "ReaperBridge.h"
#include "NineHandsFolder.h"
#include <vector>
#include <algorithm>

#if JucePlugin_Build_VST3
 #include <juce_audio_processors/juce_audio_processors.h>
 JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wshadow-field-in-constructor", "-Wnon-virtual-dtor", "-Wpragma-pack", "-Wunused-parameter")
 #include <pluginterfaces/base/ftypes.h>
 #include <pluginterfaces/base/funknown.h>
 JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace reaper
{
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wzero-as-null-pointer-constant", "-Wunused-parameter", "-Wnon-virtual-dtor")
    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4100)
    using namespace Steinberg;
    // interface publiee par Cockos (reaper_vst3_interfaces.h, REAPER SDK)
    class IReaperHostApplication : public FUnknown
    {
    public:
        virtual void* PLUGIN_API getReaperApi (CStringA funcname) = 0;
        virtual void* PLUGIN_API getReaperParent (uint32 w) = 0;   // piste (1), prise (2), projet (3)
        virtual void* PLUGIN_API reaperExtended (uint32 call, void* parm1, void* parm2, void* parm3) = 0;
        static const FUID iid;
    };
    DECLARE_CLASS_IID (IReaperHostApplication, 0x79655E36, 0x77EE4267, 0xA573FEF7, 0x4912C27C)
    DEF_CLASS_IID (IReaperHostApplication)
    JUCE_END_IGNORE_WARNINGS_MSVC
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
}
#endif

namespace omi
{
struct ReaperBridge::Impl
{
#if JucePlugin_Build_VST3
    reaper::IReaperHostApplication* host = nullptr;

    // fonctions de l'API REAPER utilisees
    double (*GetMediaTrackInfo_Value) (void*, const char*) = nullptr;
    void   (*InsertTrackAtIndex) (int, bool) = nullptr;
    void*  (*GetTrack) (void*, int) = nullptr;
    bool   (*GetSetMediaTrackInfo_String) (void*, const char*, char*, bool) = nullptr;
    int    (*CreateTrackSend) (void*, void*) = nullptr;
    bool   (*SetTrackSendInfo_Value) (void*, int, int, const char*, double) = nullptr;
    int    (*GetTrackNumSends) (void*, int) = nullptr;
    void*  (*GetSetTrackSendInfo) (void*, int, int, const char*, void*) = nullptr;
    int    (*TrackFX_AddByName) (void*, const char*, bool, int) = nullptr;
    int    (*TrackFX_GetCount) (void*) = nullptr;
    bool   (*TrackFX_GetFXName) (void*, int, char*, int) = nullptr;
    void   (*TrackFX_Show) (void*, int, int) = nullptr;
    void   (*SetOnlyTrackSelected) (void*) = nullptr;
    void   (*Main_OnCommand) (int, int) = nullptr;
    void   (*Undo_BeginBlock2) (void*) = nullptr;
    void   (*Undo_EndBlock2) (void*, const char*, int) = nullptr;
    void   (*PreventUIRefresh) (int) = nullptr;
    void   (*TrackList_AdjustWindows) (bool) = nullptr;
    int    (*CountTracks) (void*) = nullptr;
    bool   (*SetMediaTrackInfo_Value) (void*, const char*, double) = nullptr;

    // choix de l'instrument par la fenetre de REAPER (Insert > Virtual instrument on new track) : la piste qu'elle cree
    // est reperee puis reliee a Omi (receive MIDI) des qu'elle apparait
    std::vector<void*> before;
    void* watchSrc = nullptr;
    double watchUntil = 0.0;
    void* hideTrack = nullptr;            // apres creation : chaine FX fermee, fenetre de l'instrument seule (comme REAPER l'ouvre d'habitude)
    double hideUntil = 0.0;
    void hideWindows (void* t)
    {
        TrackFX_Show (t, 0, 0);                                  // fenetre de la chaine FX : fermee
        if (TrackFX_GetCount (t) > 0) TrackFX_Show (t, 0, 3);    // fenetre de l'instrument (Kontakt...) : ouverte, flottante
    }

    template <typename F> void get (F& f, const char* name)
    {
        f = reinterpret_cast<F> (host->getReaperApi (name));
    }

    bool ok() const
    {
        return host != nullptr && GetMediaTrackInfo_Value && InsertTrackAtIndex && GetTrack && GetSetMediaTrackInfo_String
            && CreateTrackSend && SetTrackSendInfo_Value && GetTrackNumSends && GetSetTrackSendInfo && TrackFX_AddByName
            && TrackFX_GetCount && TrackFX_GetFXName && TrackFX_Show && SetOnlyTrackSelected && Main_OnCommand
            && CountTracks && SetMediaTrackInfo_Value;
    }
    void* track() const   { return host != nullptr ? host->getReaperParent (1) : nullptr; }
    void* project() const { return host != nullptr ? host->getReaperParent (3) : nullptr; }

    juce::String trackName (void* tr) const
    {
        char buf[512] = {};
        GetSetMediaTrackInfo_String (tr, "P_NAME", buf, false);
        return juce::String::fromUTF8 (buf);
    }
    juce::String fxName (void* tr, int i) const
    {
        char buf[512] = {};
        if (! TrackFX_GetFXName (tr, i, buf, (int) sizeof (buf))) return {};
        return juce::String::fromUTF8 (buf);
    }
#endif
};

ReaperBridge::ReaperBridge() : impl (std::make_unique<Impl>()) {}

ReaperBridge::~ReaperBridge()
{
   #if JucePlugin_Build_VST3
    if (impl->host != nullptr) impl->host->release();
   #endif
}

void ReaperBridge::setHost (void* p)
{
   #if JucePlugin_Build_VST3
    if (p == nullptr || impl->host != nullptr) return;
    auto* unk = static_cast<Steinberg::FUnknown*> (p);
    void* obj = nullptr;
    if (unk->queryInterface (reaper::IReaperHostApplication::iid, &obj) != Steinberg::kResultOk || obj == nullptr) return;
    auto& m = *impl;
    m.host = static_cast<reaper::IReaperHostApplication*> (obj);
    m.get (m.GetMediaTrackInfo_Value, "GetMediaTrackInfo_Value");
    m.get (m.InsertTrackAtIndex, "InsertTrackAtIndex");
    m.get (m.GetTrack, "GetTrack");
    m.get (m.GetSetMediaTrackInfo_String, "GetSetMediaTrackInfo_String");
    m.get (m.CreateTrackSend, "CreateTrackSend");
    m.get (m.SetTrackSendInfo_Value, "SetTrackSendInfo_Value");
    m.get (m.GetTrackNumSends, "GetTrackNumSends");
    m.get (m.GetSetTrackSendInfo, "GetSetTrackSendInfo");
    m.get (m.TrackFX_AddByName, "TrackFX_AddByName");
    m.get (m.TrackFX_GetCount, "TrackFX_GetCount");
    m.get (m.TrackFX_GetFXName, "TrackFX_GetFXName");
    m.get (m.TrackFX_Show, "TrackFX_Show");
    m.get (m.SetOnlyTrackSelected, "SetOnlyTrackSelected");
    m.get (m.Main_OnCommand, "Main_OnCommand");
    m.get (m.Undo_BeginBlock2, "Undo_BeginBlock2");
    m.get (m.Undo_EndBlock2, "Undo_EndBlock2");
    m.get (m.PreventUIRefresh, "PreventUIRefresh");
    m.get (m.TrackList_AdjustWindows, "TrackList_AdjustWindows");
    m.get (m.CountTracks, "CountTracks");
    m.get (m.SetMediaTrackInfo_Value, "SetMediaTrackInfo_Value");
   #else
    juce::ignoreUnused (p);
   #endif
}

bool ReaperBridge::available() const
{
   #if JucePlugin_Build_VST3
    return impl->ok() && impl->track() != nullptr;
   #else
    return false;
   #endif
}

juce::Array<ReaperBridge::Target> ReaperBridge::receivers() const
{
    juce::Array<Target> out;
   #if JucePlugin_Build_VST3
    if (! available()) return out;
    auto& m = *impl;
    void* src = m.track();
    const int n = m.GetTrackNumSends (src, 0);
    for (int i = 0; i < n; ++i)
        if (void* dst = m.GetSetTrackSendInfo (src, 0, i, "P_DESTTRACK", nullptr))
        {
            Target t;
            t.trackName = m.trackName (dst);
            if (t.trackName.isEmpty()) t.trackName = "Track " + juce::String ((int) m.GetMediaTrackInfo_Value (dst, "IP_TRACKNUMBER"));
            if (m.TrackFX_GetCount (dst) > 0) t.instrument = m.fxName (dst, 0);
            out.add (t);
        }
   #endif
    return out;
}

bool ReaperBridge::createInstrumentTrack (const juce::String& instrument)
{
   #if JucePlugin_Build_VST3
    if (! available()) return false;
    auto& m = *impl;
    void* proj = m.project();
    void* src = m.track();
    const int srcNum = (int) m.GetMediaTrackInfo_Value (src, "IP_TRACKNUMBER");     // 1 = premiere piste
    if (srcNum <= 0) return false;

    if (instrument.isNotEmpty())
    {
        // dernier instrument utilise : piste creee et reliee directement, sans fenetre de choix
        if (m.Undo_BeginBlock2) m.Undo_BeginBlock2 (proj);
        if (m.PreventUIRefresh) m.PreventUIRefresh (1);
        m.InsertTrackAtIndex (srcNum, true);                                         // juste sous la piste d'Omi
        void* dst = m.GetTrack (proj, srcNum);
        bool loaded = false;
        if (dst != nullptr)
        {
            loaded = m.TrackFX_AddByName (dst, instrument.toRawUTF8(), false, -1) >= 0 && m.TrackFX_GetCount (dst) > 0;
            if (loaded) wire (src, dst);
            else        m.GetSetMediaTrackInfo_String (dst, "P_NAME", const_cast<char*> ("Omi - MIDI"), true);
        }
        if (m.PreventUIRefresh) m.PreventUIRefresh (-1);
        if (m.TrackList_AdjustWindows) m.TrackList_AdjustWindows (false);
        if (m.Undo_EndBlock2) m.Undo_EndBlock2 (proj, "Omi: create MIDI instrument track", -1);
        if (loaded) { m.SetOnlyTrackSelected (dst); m.hideWindows (dst); m.hideTrack = dst; m.hideUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0; return true; }
        if (dst != nullptr)
        {
            // instrument introuvable : piste vide reliee, fenetre de choix de REAPER sur cette piste
            wire (src, dst);
            m.SetOnlyTrackSelected (dst);
            m.TrackFX_Show (dst, 0, 1);
            return true;
        }
        return false;
    }

    // choix de l'instrument : fenetre de REAPER « Insert virtual instrument on new track » (sous la piste d'Omi) ;
    // la piste qu'elle cree est reliee a Omi des qu'elle apparait (poll(), appele par le timer du plugin)
    m.before.clear();
    const int n = m.CountTracks (proj);
    for (int i = 0; i < n; ++i) m.before.push_back (m.GetTrack (proj, i));
    m.watchSrc = src;
    m.watchUntil = juce::Time::getMillisecondCounterHiRes() + 180000.0;
    m.SetOnlyTrackSelected (src);
    m.Main_OnCommand (40701, 0);          // Insert > Virtual instrument on new track...
    poll();                               // la fenetre est modale : la piste existe deja si l'instrument a ete choisi
    return true;
   #else
    juce::ignoreUnused (instrument);
    return false;
   #endif
}

void ReaperBridge::wire (void* src, void* dst)
{
   #if JucePlugin_Build_VST3
    auto& m = *impl;
    const int s = m.CreateTrackSend (src, dst);
    if (s >= 0)
    {
        m.SetTrackSendInfo_Value (src, 0, s, "I_SRCCHAN", -1.0);    // Audio : None
        m.SetTrackSendInfo_Value (src, 0, s, "I_MIDIFLAGS", 0.0);   // MIDI : All -> All
    }
    m.SetMediaTrackInfo_Value (dst, "I_RECARM", 0.0);               // l'instrument ne joue que le MIDI d'Omi
    juce::String name ("Omi - MIDI");
    if (m.TrackFX_GetCount (dst) > 0)
    {
        const auto fx = m.fxName (dst, 0);
        setLastInstrument (fx);
        name = "Omi - " + fx.fromFirstOccurrenceOf (": ", false, false).upToFirstOccurrenceOf (" (", false, false);
    }
    m.GetSetMediaTrackInfo_String (dst, "P_NAME", const_cast<char*> (name.toRawUTF8()), true);
   #else
    juce::ignoreUnused (src, dst);
   #endif
}

void ReaperBridge::poll()
{
   #if JucePlugin_Build_VST3
    auto& m = *impl;
    if (m.hideTrack != nullptr)
    {
        if (available() && juce::Time::getMillisecondCounterHiRes() < m.hideUntil) m.hideWindows (m.hideTrack);
        else m.hideTrack = nullptr;
    }
    if (m.watchSrc == nullptr) return;
    if (! available() || juce::Time::getMillisecondCounterHiRes() > m.watchUntil) { m.watchSrc = nullptr; return; }
    void* proj = m.project();
    const int n = m.CountTracks (proj);
    for (int i = 0; i < n; ++i)
    {
        void* t = m.GetTrack (proj, i);
        if (t == nullptr || t == m.watchSrc || std::find (m.before.begin(), m.before.end(), t) != m.before.end()) continue;
        if (m.TrackFX_GetCount (t) <= 0) continue;               // dossier d'une sortie multiple : on prend la piste de l'instrument
        if (m.Undo_BeginBlock2) m.Undo_BeginBlock2 (proj);
        wire (m.watchSrc, t);
        if (m.TrackList_AdjustWindows) m.TrackList_AdjustWindows (false);
        if (m.Undo_EndBlock2) m.Undo_EndBlock2 (proj, "Omi: route MIDI to instrument", -1);
        m.hideWindows (t);
        m.hideTrack = t; m.hideUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
        m.watchSrc = nullptr;
        return;
    }
   #endif
}

void ReaperBridge::showReceiver (int index)
{
   #if JucePlugin_Build_VST3
    if (! available()) return;
    auto& m = *impl;
    void* src = m.track();
    void* dst = m.GetSetTrackSendInfo (src, 0, index, "P_DESTTRACK", nullptr);
    if (dst == nullptr) return;
    m.SetOnlyTrackSelected (dst);
    if (m.TrackFX_GetCount (dst) > 0) m.TrackFX_Show (dst, 0, 3);
    else                              m.TrackFX_Show (dst, 0, 1);
   #else
    juce::ignoreUnused (index);
   #endif
}

static juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile (nhd::folderName()).getChildFile ("Omi").getChildFile ("Instrument.txt");
}

static juce::File autoFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile (nhd::folderName()).getChildFile ("Omi").getChildFile ("AutoTrack.txt");
}

bool ReaperBridge::autoCreate()
{
    const auto f = autoFile();
    return ! f.existsAsFile() || f.loadFileAsString().trim() != "0";
}

void ReaperBridge::setAutoCreate (bool on)
{
    auto f = autoFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (on ? "1" : "0");
}

juce::String ReaperBridge::lastInstrument()
{
    const auto f = settingsFile();
    return f.existsAsFile() ? f.loadFileAsString().trim() : juce::String();
}

void ReaperBridge::setLastInstrument (const juce::String& name)
{
    if (name.isEmpty() || name == lastInstrument()) return;
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (name);
}
} // namespace omi
