// Tenue d'octave : G1 qui sonne encore + G2 joue par-dessus -> G1 garde ; G1 etouffe puis G2 -> G2 envoye.
// g++ -O2 -std=c++17 tests/octave_test.cpp
#include "../OmiDSP.h"
#include <cstdio>
#include <vector>
#include <cmath>
static int run (int first, bool muteFirst, int second, bool hold)
{
    const double fs = 48000.0; std::vector<float> x;
    struct V { double f, t0, tOff; float fund; double ph[11] = {}; } v[2] = { { 440 * std::pow (2.0, (first - 69) / 12.0), 0.0, 1e9, 0.12f },
                                                                            { 440 * std::pow (2.0, (second - 69) / 12.0), 0.5, 1e9, 0.3f } };
    if (muteFirst) v[0].tOff = 0.5;
    for (int i = 0; i < (int) (1.1 * fs); ++i)
    {
        const double t = i / fs; double s = 0;
        for (auto& w : v)
        {
            if (t < w.t0) continue;
            const double tt = t - w.t0, damp = t > w.tOff ? std::exp (-(t - w.tOff) / 0.025) : 1.0;
            for (int h = 1; h <= 10; ++h) { w.ph[h] += 2 * M_PI * w.f * h * std::sqrt (1 + 0.0004 * h * h) / fs;
                                            s += damp * (h == 1 ? w.fund : 1.0 / h) * std::exp (-tt * (1.0 + 0.5 * h)) * std::sin (w.ph[h]) * std::min (1.0, tt / 0.002); }
        }
        x.push_back ((float) (0.25 * s));
    }
    omi::Tracker tr; tr.prepare (fs); 
    omi::Settings st; st.minOut = -1; st.sensDb = -40; st.lowestHz = 40.0f; st.octaveHold = hold;
    omi::Event ev[64]; int n = 0, last = -1, count = 0;
    for (size_t p = 0; p < x.size(); p += 128)
    {
        tr.process (x.data() + p, 128, st, ev, n);
        for (int e = 0; e < n; ++e) if (ev[e].type == omi::Event::NoteOn) { last = ev[e].b / 128; ++count; }
    }
    std::printf ("%d %s, puis %d (%s) : %d note(s), derniere %d\n", first, muteFirst ? "etouffe" : "qui sonne", second, hold ? "tenue d'octave" : "sans tenue", count, last);
    return last;
}
int main()
{
    int bad = 0;
    for (int f : { 31, 28, 38, 45 })
    {
        bad += run (f, false, f + 12, true) != f;     // octave par-dessus : premiere note gardee
        bad += run (f, false, f + 24, true) != f;     // deux octaves
        bad += run (f, true,  f + 12, true) != f + 12;// premiere etouffee : octave envoyee
        bad += run (f, false, f + 7,  true) != f + 7; // quinte : nouvelle note
    }
    run (31, false, 43, false);
    run (45, false, 52, false);
    std::printf ("erreurs : %d\n", bad);
    return bad;
}
