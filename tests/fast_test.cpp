// Bruit de mediator (~4 ms) a chaque attaque, comme sur une vraie guitare.
// Jeu rapide d'une corde a l'autre, cordes NON etouffees (l'ancienne corde continue de sonner sous la nouvelle) :
// 7 cordes en G (G1 D2 G2 C3 F3 A3 D4) et 8 cordes, croches et doubles croches. g++ -O2 -std=c++17 tests/fast_test.cpp
#include "../OmiDSP.h"
#include <cstdio>
#include <vector>
#include <random>
#include <string>
static const char* nn[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static std::string name (int m) { return std::string (nn[m % 12]) + std::to_string (m / 12 - 1); }
struct Pluck { int string, midi; };
int run (const char* title, std::vector<Pluck> seq, double step, float lowestHz, int feel, bool mute)
{
    const double fs = 48000.0;
    std::mt19937 rng (5); std::normal_distribution<float> nd (0, 1);
    struct Voice { double f = 0, t0 = -1, tOff = 1e9; float fund = 0.1f; double ph[13] {}; } v[8];
    const int stepN = (int) (step * fs);
    std::vector<float> x; std::vector<std::pair<double,int>> truth; float lastR = 0;
    for (size_t k = 0; k < seq.size(); ++k)
    {
        if (mute) for (int j = 0; j < 8; ++j) if (j != seq[k].string && v[j].t0 >= 0 && v[j].tOff > 1e8) v[j].tOff = x.size() / fs;   // main gauche / paume : l'ancienne corde est etouffee
        auto& vo = v[seq[k].string];
        vo.tOff = 1e9;
        vo.f = 440.0 * std::pow (2.0, (seq[k].midi - 69) / 12.0); vo.t0 = x.size() / fs; vo.fund = seq[k].midi < 40 ? 0.12f : 0.5f;
        truth.push_back ({ vo.t0, seq[k].midi });
        for (int i = 0; i < stepN; ++i)
        {
            const double t = x.size() / fs; double s = 0;
            for (auto& w : v)
            {
                if (w.t0 < 0) continue;
                const double tt = t - w.t0;
                for (int h = 1; h <= 10; ++h)
                {
                    w.ph[h] += 2 * M_PI * w.f * h * std::sqrt (1 + 0.0004 * h * h) / fs;
                    const double damp = t > w.tOff ? std::exp (-(t - w.tOff) / 0.025) : 1.0;     // etouffee en ~25 ms
                    s += damp * (h == 1 ? w.fund : 1.0 / h) * std::exp (-tt * (1.2 + 0.5 * h)) * std::sin (w.ph[h]) * std::min (1.0, tt / 0.002);
                }
            }
            { const double tp = t - vo.t0; double pk = 0; if (tp < 0.004) { const float r = nd (rng); pk = (r - lastR) * 0.05 * std::exp (-tp / 0.0012); lastR = r; }
              x.push_back ((float) (0.25 * s + pk + 0.0006 * nd (rng))); }
        }
    }
    omi::Tracker tr; tr.prepare (fs);
    omi::Settings s; s.minOut = -1; s.sensDb = -40.0f; s.lowestHz = lowestHz; s.feel = feel;
    omi::Event ev[omi::Tracker::maxEvents]; int nEv = 0;
    std::vector<int> got (seq.size(), -1); std::vector<double> lat (seq.size(), 0); int extra = 0;
    for (size_t p = 0; p < x.size(); p += 128)
    {
        const int n = (int) std::min<size_t> (128, x.size() - p);
        tr.process (x.data() + p, n, s, ev, nEv);
        for (int e = 0; e < nEv; ++e)
            if (ev[e].type == omi::Event::NoteOn)
            {
                const double t = (p + ev[e].sample) / fs;
                size_t k = 0; while (k + 1 < truth.size() && truth[k + 1].first <= t) ++k;
                if (got[k] < 0) { got[k] = ev[e].b / 128; lat[k] = (t - truth[k].first) * 1000; } else ++extra;
            }
    }
    int ok = 0; double latSum = 0;
    std::string line;
    for (size_t k = 0; k < seq.size(); ++k)
    {
        const bool good = got[k] == seq[k].midi;
        ok += good; if (good) latSum += lat[k];
        line += name (seq[k].midi) + (good ? "" : "->" + (got[k] < 0 ? std::string ("rien") : name (got[k]))) + " ";
    }
    std::printf ("%-34s %2d/%2zu justes, %d en trop, latence moy. %.1f ms\n   %s\n", title, ok, seq.size(), extra, ok ? latSum / ok : 0.0, line.c_str());
    return (int) seq.size() - ok + extra;
}
int main()
{
    // 7 cordes en G : cordes 0..6 = G1 D2 G2 C3 F3 A3 D4 (31 38 43 48 53 57 62)
    std::vector<Pluck> riff = { {0,31},{1,38},{0,31},{1,38},{1,40},{0,31},{1,41},{0,31},{2,43},{1,38},{0,31},{1,38},{0,33},{1,38},{0,34},{1,38} };
    std::vector<Pluck> run8 = { {0,30},{1,35},{0,30},{1,37},{2,40},{1,35},{0,28},{1,35},{0,30},{0,31},{1,35},{0,28} };   // 8 cordes F#1 B1 E2
    int bad = 0;
    for (int mute : { 1, 0 })
    for (int feel : { 0, 1 })
    {
        std::printf ("--- %s, play feel %s ---\n", mute ? "cordes etouffees" : "cordes qui sonnent encore", feel == 0 ? "Fast" : "Normal");
        bad += run ("7 cordes en G, croches 150 bpm", riff, 0.200, 49.0f, feel, mute);
        bad += run ("7 cordes en G, doubles 150 bpm", riff, 0.100, 49.0f, feel, mute);
        bad += run ("7 cordes, Lowest = E1 (defaut)", riff, 0.120, 41.2f, feel, mute);
        bad += run ("7 cordes, Lowest = A0", riff, 0.120, 27.5f, feel, mute);
        bad += run ("8 cordes F#1, croches", run8, 0.160, 41.2f, feel, mute);
    }
    std::printf ("total erreurs : %d\n", bad);
    return 0;
}
