// Test hors plugin du suivi de hauteur d'Omi : cordes graves synthetiques (fondamentale faible, inharmonicite,
// decroissance, bruit), notes enchainees, re-attaques, 7 et 8 cordes. g++ -O2 -std=c++17 tests/tracker_test.cpp
#include "../OmiDSP.h"
#include <cstdio>
#include <vector>
#include <random>
static const char* nn[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static std::string name (int m) { return std::string (nn[m % 12]) + std::to_string (m / 12 - 1); }
int main()
{
    const double fs = 48000.0;
    std::mt19937 rng (3); std::normal_distribution<float> nd (0, 1);
    struct N { int midi; double dur; float amp; float fundGain; };
    // 8 cordes en F#1 / E1 / D1, 7 cordes en G1, puis aigus ; fondamentale tres faible sur les graves
    std::vector<N> seq = { {30,0.8,0.5f,0.15f},{28,0.8,0.5f,0.12f},{28,0.5,0.4f,0.12f},{26,0.9,0.5f,0.10f},{31,0.6,0.3f,0.2f},
                           {33,0.4,0.6f,0.2f},{24,1.0,0.5f,0.08f},{21,1.2,0.6f,0.08f},{20,1.2,0.6f,0.08f},{40,0.4,0.3f,0.6f},{52,0.3,0.2f,0.8f},{64,0.3,0.2f,0.9f},{88,0.3,0.15f,1.0f} };
    std::vector<float> x; std::vector<std::pair<double,int>> truth;
    for (auto& n : seq)
    {
        const double f = 440.0 * std::pow (2.0, (n.midi - 69) / 12.0);
        truth.push_back ({ x.size() / fs, n.midi });
        const int len = (int) (n.dur * fs);
        for (int i = 0; i < len; ++i)
        {
            const double t = i / fs; double v = 0;
            for (int h = 1; h <= 12; ++h)
            {
                const double B = 0.0004;                                  // inharmonicite des cordes epaisses
                const double fh = f * h * std::sqrt (1 + B * h * h);
                if (fh > 6000) break;
                const double g = (h == 1 ? n.fundGain : 1.0 / h) * std::exp (-t * (1.5 + 0.6 * h));
                v += g * std::sin (2 * M_PI * fh * t + h);
            }
            const double att = std::min (1.0, t / 0.003);
            x.push_back ((float) (n.amp * att * v * 0.5 + 0.0008 * nd (rng)));
        }
        for (int i = 0; i < (int) (0.08 * fs); ++i) x.push_back (0.0008f * nd (rng));   // petit silence
    }
    {
        truth.push_back ({ x.size() / fs, 30 });
        const int ms[3] = { 30, 32, 33 };
        double ph[13] = {}; const int len = (int) (0.45 * fs);
        for (int k = 0; k < 3; ++k)
        {
            if (k > 0) truth.push_back ({ x.size() / fs, ms[k] });
            const double f = 440.0 * std::pow (2.0, (ms[k] - 69) / 12.0);
            for (int i = 0; i < len; ++i)
            {
                const double t = (k * len + i) / fs; double v = 0;
                for (int h = 1; h <= 10; ++h) { ph[h] += 2 * M_PI * f * h * std::sqrt (1 + 0.0004 * h * h) / fs; v += (h == 1 ? 0.12 : 1.0 / h) * std::exp (-t * (0.8 + 0.4 * h)) * std::sin (ph[h]); }
                x.push_back ((float) (0.25 * std::min (1.0, t / 0.003) * v + 0.0008 * nd (rng)));
            }
        }
        for (int i = 0; i < (int) (0.08 * fs); ++i) x.push_back (0.0008f * nd (rng));
    }
    omi::Tracker tr; tr.prepare (fs);
    omi::Settings s; s.minOut = -1; s.sensDb = -40.0f; s.lowestHz = 25.0f; s.feel = 1;
    omi::Event ev[omi::Tracker::maxEvents]; int nEv = 0;
    const int B = 256;
    size_t ti = 0; int errors = 0, ons = 0;
    std::vector<int> onsThis;
    for (size_t p = 0; p < x.size(); p += B)
    {
        const int n = (int) std::min<size_t> (B, x.size() - p);
        tr.process (x.data() + p, n, s, ev, nEv);
        for (int e = 0; e < nEv; ++e)
            if (ev[e].type == omi::Event::NoteOn)
            {
                const double t = (p + ev[e].sample) / fs;
                const int m = ev[e].b / 128;
                while (ti + 1 < truth.size() && truth[ti + 1].first <= t) ++ti;
                const bool ok = m == truth[ti].second;
                ++ons; if (! ok) ++errors;
                std::printf ("%7.3f s  note on %-4s vel %3d   attendu %-4s  latence %5.1f ms %s\n", t, name (m).c_str(), ev[e].b % 128,
                             name (truth[ti].second).c_str(), (t - truth[ti].first) * 1000.0, ok ? "" : "  <-- ERREUR");
            }
    }
    std::printf ("%d notes, %d erreurs (attendu %zu notes)\n", ons, errors, seq.size() + 3);
    return errors;
}
