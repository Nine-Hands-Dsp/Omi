#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <array>

//==============================================================================
// OMI (NINE HANDS | DSP) - convertisseur guitare -> MIDI monophonique, dans l'esprit des interfaces GK-MIDI Roland (GI-20).
//
// Detection de hauteur renforcee pour les accordages tres graves (7 / 8 cordes, jusqu'a ~25 Hz) :
//  1. pre-traitement : passe-haut 18 Hz, passe-bas 6e ordre a 1,4 kHz (retire les harmoniques aigues qui trompent
//     l'analyse), decimation vers ~8-9 kHz ;
//  2. methode McLeod (NSDF) avec une fenetre propre a chaque decalage (1,25 periode, au moins 12 ms) : une note aigue
//     est detectee en quelques ms, une note grave dispose de la fenetre dont elle a besoin ;
//  3. correction d'octave : sur les cordes graves, la fondamentale captee par le micro est faible et l'harmonique 2
//     domine. Une periode double (ou triple) nettement plus coherente est preferee : pas de note a l'octave du dessus ;
//  4. stabilisation : mediane glissante des estimations, confirmation sur plusieurs analyses avant une nouvelle note,
//     saut d'octave exige deux fois plus de confirmations, hysteresis de 0,6 demi-ton entre deux notes ;
//  5. tenue : une analyse ratee (fin de note, bruit) ne coupe jamais la note ; seule la porte de niveau (seuil SENS
//     avec 6 dB d'hysteresis) ou une nouvelle note confirmee y mettent fin ; nouvelle attaque sur la meme note
//     reconnue par l'enveloppe (re-declenchement).
// Aucune allocation, aucun verrou : tout est dimensionne dans prepare().
//==============================================================================
namespace omi
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fs, double fc, double q)  { set (fs, fc, q, true); }
    void highpass (double fs, double fc, double q) { set (fs, fc, q, false); }
    void set (double fs, double fc, double q, bool lp)
    {
        const double w = 2.0 * 3.14159265358979323846 * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        const double a0 = 1.0 + al;
        if (lp) { b0 = (float) ((1 - c) / 2 / a0); b1 = (float) ((1 - c) / a0); b2 = b0; }
        else    { b0 = (float) ((1 + c) / 2 / a0); b1 = (float) (-(1 + c) / a0); b2 = b0; }
        a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
    }
    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Settings
{
    float sensDb = -48.0f;    // seuil de detection (niveau crete d'entree, dBFS)
    float dyn = 0.7f;         // 0 = velocite fixe (100), 1 = toute la dynamique du jeu
    int   bendRange = 0;      // 0 = chromatique (pas de pitch bend), sinon plage en demi-tons
    int   transpose = 0;      // demi-tons
    int   feel = 1;           // 0 rapide, 1 normal, 2 stable (comme le PLAY FEEL du GI-20)
    int   channel = 1;        // 1-16
    float lowestHz = 25.0f;   // note la plus grave suivie
    int   minOut = 23;        // note MIDI la plus grave envoyee (-1 = libre) : en dessous, remontee d'une octave
                              // (tessiture du VST recepteur : B0 = 23 basse 5 cordes, E1 = 28 basse 4 cordes)
    bool  active = true;
};

// evenement MIDI produit (le processeur le place dans le MidiBuffer)
struct Event { enum Type { NoteOn, NoteOff, Bend, Rpn } type; int sample, a, b; };

class Tracker
{
public:
    static constexpr int ringSize = 8192;          // ~0,9 s a 9 kHz
    static constexpr int hop = 32;                 // une analyse toutes les ~3,6 ms
    static constexpr int maxEvents = 64;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        decim = std::max (1, (int) std::floor (fs / 8000.0));
        fsd = fs / decim;
        hp.highpass (fs, 18.0, 0.707);
        const double qs[3] = { 0.5176, 0.7071, 1.9319 };          // Butterworth 6e ordre
        for (int i = 0; i < 3; ++i) lp[i].lowpass (fs, 1400.0, qs[i]);
        reset();
    }

    void reset()
    {
        hp.reset(); for (auto& f : lp) f.reset();
        std::memset (ring, 0, sizeof (ring));
        writePos = 0; decCount = 0; hopCount = 0;
        env = 0.0f; fastE = slowE = fastPrev1 = fastPrev2 = 0.0f; envDb = -120.0f; gateOpen = false; gateLowTime = 0;
        onsetFlag = false; onsetPeak = -120.0f; onsetAge = 1 << 20;
        histCount = 0; candNote = -1; candCount = 0;
        note = -1; sentNote = -1; lastBend = 8192; bendSm = 0.0f;
        detectedMidi = -1.0f; clarity = 0.0f; rpnSentFor = -1;
    }

    // processus : x = entree mono, n echantillons ; les evenements sont ajoutes a ev (nEv)
    void process (const float* x, int n, const Settings& s, Event* ev, int& nEv)
    {
        nEv = 0;
        if (! s.active)
        {
            if (sentNote >= 0) stopNote (0, s, ev, nEv);
            note = -1; candNote = -1; candCount = 0; histCount = 0;
        }
        else if (rpnSentFor != s.bendRange && s.bendRange > 0)
        {
            push (ev, nEv, { Event::Rpn, 0, s.channel, s.bendRange });
            rpnSentFor = s.bendRange;
        }

        const float relCoef = std::exp (-1.0f / (0.060f * (float) fs));   // enveloppe : attaque immediate, retombee 60 ms
        const float fastCoef = std::exp (-1.0f / (0.004f * (float) fs));    // energie rapide (4 ms)
        const float refCoef  = std::exp (-1.0f / (0.100f * (float) fs));    // energie de reference (100 ms)
        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            const float a = std::abs (in);
            env = a > env ? a : env * relCoef;
            fastE = fastE * fastCoef + in * in * (1.0f - fastCoef);
            slowE = slowE * refCoef + fastE * (1.0f - refCoef);
            if (++snapCount >= (int) (0.005 * fs)) { snapCount = 0; fastPrev2 = fastPrev1; fastPrev1 = fastE; }
            // attaque (nouveau coup de mediator) : energie +6 dB au-dessus de la reference et +5 dB en 10 ms
            if (fastE > 4.0f * slowE + 1.0e-9f && fastE > 3.0f * fastPrev2 + 1.0e-9f && env > 1.0e-3f && onsetAge > (int) (0.080 * fs))
                { onsetFlag = true; onsetAge = 0; onsetPeak = -120.0f; }
            ++onsetAge;
            if (onsetAge < (int) (0.030 * fs)) onsetPeak = std::max (onsetPeak, toDb (env));

            float y = hp.process (in);
            for (auto& f : lp) y = f.process (y);
            if (++decCount >= decim)
            {
                decCount = 0;
                ring[writePos] = y;
                writePos = (writePos + 1) & (ringSize - 1);
                if (++hopCount >= hop)
                {
                    hopCount = 0;
                    analyse (i, s, ev, nEv);
                }
            }
        }
    }

    // informations pour l'ecran (lues par l'interface)
    float detectedMidi = -1.0f, clarity = 0.0f, envDb = -120.0f;
    int sentNote = -1, lastVelocity = 0, lastBend = 8192;
    bool gateOpen = false;

private:
    static float toDb (float a) { return a > 1.0e-6f ? 20.0f * std::log10 (a) : -120.0f; }
    static void push (Event* ev, int& nEv, Event e) { if (nEv < maxEvents) ev[nEv++] = e; }
    inline float at (int back) const { return ring[(writePos - 1 - back) & (ringSize - 1)]; }

    void analyse (int sample, const Settings& s, Event* ev, int& nEv)
    {
        envDb = toDb (env);
        // porte de niveau : ouverture au seuil SENS, fermeture 6 dB plus bas pendant 40 ms
        if (envDb >= s.sensDb) { gateOpen = true; gateLowTime = 0; }
        else if (envDb < s.sensDb - 6.0f)
        {
            gateLowTime += hop * decim;
            if (gateLowTime > (int) (0.040 * fs)) gateOpen = false;
        }

        if (! gateOpen || ! s.active)
        {
            if (sentNote >= 0) stopNote (sample, s, ev, nEv);
            note = -1; candNote = -1; candCount = 0; histCount = 0; detectedMidi = -1.0f; clarity = 0.0f;
            onsetFlag = false;
            return;
        }

        float f0 = 0.0f, clar = 0.0f;
        const bool ok = detect (s, f0, clar);
        clarity = clar;
        const float minClarity = s.feel == 0 ? 0.55f : (s.feel == 1 ? 0.62f : 0.70f);
        if (! ok || clar < minClarity) return;                    // analyse ratee : la note en cours est tenue

        noteAge += hop * decim;
        if (onsetFlag && note >= 0 && noteAge < (int) (0.120 * fs)) onsetFlag = false;   // pas de re-attaque juste apres une note
        const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);
        // mediane glissante des dernieres estimations
        const int histLen = s.feel == 0 ? 3 : (s.feel == 1 ? 5 : 7);
        if (onsetFlag) histCount = 0;                             // nouvelle attaque : on repart de zero
        for (int k = std::min (histCount, histMax - 1); k > 0; --k) hist[k] = hist[k - 1];
        hist[0] = midi; histCount = std::min (histCount + 1, histMax);
        const int m = std::min (histCount, histLen);
        float tmp[histMax];
        std::copy (hist, hist + m, tmp);
        std::nth_element (tmp, tmp + m / 2, tmp + m);
        const float med = tmp[m / 2];
        detectedMidi = med;

        const int confirmBase = s.feel == 0 ? 2 : (s.feel == 1 ? 3 : 5);
        const int c = (int) std::lround (med);

        if (note < 0 || onsetFlag)
        {
            // nouvelle note : confirmation sur plusieurs analyses
            if (c == candNote) ++candCount; else { candNote = c; candCount = 1; }
            if (candCount >= confirmBase)
            {
                const bool retrig = onsetFlag;
                onsetFlag = false;
                if (sentNote >= 0 && (retrig || c != note)) stopNote (sample, s, ev, nEv);
                startNote (c, med, sample, s, ev, nEv);
            }
            return;
        }

        // une note est en cours
        const float dev = med - (float) note;
        if (s.bendRange > 0 && std::abs (dev) <= (float) s.bendRange - 0.5f)
        {
            // mode BEND : le glissement suit en pitch bend (lisse)
            bendSm += (dev - bendSm) * 0.5f;
            sendBend (bendSm, sample, s, ev, nEv);
            candNote = note; candCount = 0;
            return;
        }
        if (std::abs (med - (float) note) < 0.6f) { candNote = note; candCount = 0; return; }   // hysteresis

        // changement de note (legato, hammer-on, slide en mode chromatique)
        const int interval = std::abs (c - note);
        const bool octaveJump = interval == 12 || interval == 24 || interval == 19;
        const int need = octaveJump ? confirmBase * 2 : confirmBase;
        if (c == candNote) ++candCount; else { candNote = c; candCount = 1; }
        if (candCount >= need)
        {
            stopNote (sample, s, ev, nEv);
            startNote (c, med, sample, s, ev, nEv);
        }
    }

    // ------------------------------------------------------------------ hauteur : NSDF a fenetre par decalage + correction d'octave
    bool detect (const Settings& s, float& f0, float& clar)
    {
        const int tauMin = std::max (2, (int) std::floor (fsd / 1400.0));
        const int tauMax = std::min (maxLag - 2, (int) std::ceil (fsd / std::max (20.0f, s.lowestHz * 0.94f)));
        const int wMin = (int) (0.012 * fsd);
        // copie lineaire des derniers echantillons (le plus recent en dernier)
        const int need = std::min (ringSize - 1, (int) (tauMax * 1.25f) + tauMax + 8 + wMin);
        for (int i = 0; i < need; ++i) lin[need - 1 - i] = at (i);
        const int end = need;                                   // indice apres le dernier echantillon

        float best = 0.0f;
        for (int tau = 0; tau <= tauMax + 1; ++tau) nsdf[tau] = 0.0f;
        for (int tau = 1; tau <= tauMax + 1; ++tau)
            nsdf[tau] = nsdfAt (tau, end, wMin);

        // maxima cles : un maximum par lobe positif, apres le premier passage sous zero
        int nKeys = 0;
        int t = 1;
        while (t <= tauMax && nsdf[t] > 0.0f) ++t;              // sortir du lobe du decalage nul
        while (t <= tauMax)
        {
            while (t <= tauMax && nsdf[t] <= 0.0f) ++t;
            int bi = -1; float bv = 0.0f;
            while (t <= tauMax && nsdf[t] > 0.0f) { if (nsdf[t] > bv) { bv = nsdf[t]; bi = t; } ++t; }
            if (bi >= tauMin && nKeys < maxKeys) { keyIdx[nKeys++] = bi; best = std::max (best, bv); }
        }
        if (nKeys == 0 || best < 0.3f) return false;

        const float k = 0.88f;
        int sel = keyIdx[0];
        for (int i = 0; i < nKeys; ++i) if (nsdf[keyIdx[i]] >= k * best) { sel = keyIdx[i]; break; }

        // correction d'octave : une periode multiple nettement plus coherente l'emporte (fondamentale faible des cordes graves)
        for (int mult = 2; mult <= 3; ++mult)
        {
            const int cand = sel * mult;
            if (cand > tauMax) break;
            int ci = cand; float cv = nsdf[cand];
            for (int d = -3; d <= 3; ++d)
                if (cand + d >= tauMin && cand + d <= tauMax && nsdf[cand + d] > cv) { cv = nsdf[cand + d]; ci = cand + d; }
            if (cv > 0.80f && cv > nsdf[sel] + 0.045f) { sel = ci; break; }
        }

        // interpolation parabolique
        float tauF = (float) sel;
        const float a = nsdf[sel - 1], b = nsdf[sel], c = nsdf[sel + 1];
        const float den = a - 2.0f * b + c;
        if (std::abs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        f0 = (float) (fsd / tauF);
        clar = b;
        return f0 >= s.lowestHz * 0.94f && f0 <= 1500.0f;
    }

    float nsdfAt (int tau, int end, int wMin) const
    {
        const int w = std::max (wMin, (int) (tau * 1.25f));
        const int i0 = end - w;
        if (i0 - tau < 0) return 0.0f;
        double r = 0.0, m = 0.0;
        const float* p = lin + i0;
        const float* q = lin + i0 - tau;
        for (int i = 0; i < w; ++i) { r += (double) p[i] * q[i]; m += (double) p[i] * p[i] + (double) q[i] * q[i]; }
        return m > 1.0e-12 ? (float) (2.0 * r / m) : 0.0f;
    }

    // ------------------------------------------------------------------ MIDI
    void startNote (int c, float med, int sample, const Settings& s, Event* ev, int& nEv)
    {
        note = c; noteAge = 0;
        int out = c + s.transpose;
        if (s.minOut >= 0) while (out < s.minOut) out += 12;          // note sous la tessiture du VST : une octave plus haut
        out = std::clamp (out, 0, 127);
        // velocite : crete des 30 premieres ms au-dessus du seuil (36 dB de dynamique)
        const float pk = std::max (onsetPeak, envDb);
        float v = std::clamp ((pk - s.sensDb) / 36.0f, 0.0f, 1.0f);
        v = std::sqrt (v);
        const float vel = 100.0f + (1.0f + 126.0f * v - 100.0f) * std::clamp (s.dyn, 0.0f, 1.0f);
        lastVelocity = std::clamp ((int) std::lround (vel), 1, 127);
        bendSm = s.bendRange > 0 ? (med - (float) c) : 0.0f;
        if (s.bendRange > 0) sendBend (bendSm, sample, s, ev, nEv, true);
        else if (lastBend != 8192) { push (ev, nEv, { Event::Bend, sample, s.channel, 8192 }); lastBend = 8192; }
        push (ev, nEv, { Event::NoteOn, sample, s.channel, out * 128 + lastVelocity });
        sentNote = out; sentChannel = s.channel;
        candCount = 0; candNote = c;
    }

    void stopNote (int sample, const Settings&, Event* ev, int& nEv)
    {
        if (sentNote >= 0) push (ev, nEv, { Event::NoteOff, sample, sentChannel, sentNote });
        sentNote = -1;
    }

    void sendBend (float semis, int sample, const Settings& s, Event* ev, int& nEv, bool force = false)
    {
        const int v = std::clamp (8192 + (int) std::lround (semis / (float) s.bendRange * 8191.0f), 0, 16383);
        if (force || std::abs (v - lastBend) >= 8) { push (ev, nEv, { Event::Bend, sample, s.channel, v }); lastBend = v; }
    }

    double fs = 48000.0, fsd = 8000.0;
    int decim = 6, decCount = 0, hopCount = 0, writePos = 0;
    Biquad hp, lp[3];
    float ring[ringSize] {};
    static constexpr int maxLag = 1400;
    float lin[ringSize] {};
    float nsdf[maxLag + 4] {};
    static constexpr int maxKeys = 64;
    int keyIdx[maxKeys] {};

    float fastPrev1 = 0.0f, fastPrev2 = 0.0f, fastE = 0.0f, slowE = 0.0f; int snapCount = 0;
    float env = 0.0f, onsetPeak = -120.0f;
    int gateLowTime = 0, onsetAge = 1 << 20;
    bool onsetFlag = false;
    static constexpr int histMax = 8;
    float hist[histMax] {};
    int histCount = 0, candNote = -1, candCount = 0;
    int note = -1, sentChannel = 1, rpnSentFor = -1, noteAge = 0;
    float bendSm = 0.0f;
};
} // namespace omi
