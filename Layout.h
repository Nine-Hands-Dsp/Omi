// genere par tools/make_layout.py - ne pas modifier a la main (repere 617.5 x 900.0 = fond 1235 x 1800 px / 2)
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace layout
{
    constexpr float frameW = 617.50f, frameH = 900.00f;
    // ordre : sens, dyn, bend, trans
    const juce::Point<float> knobPos[4] = { { 114.59f, 307.42f }, { 244.03f, 307.42f }, { 373.47f, 307.42f }, { 502.91f, 307.42f } };
    const float knobR[4] = { 46.52f, 46.52f, 46.52f, 46.52f };
    const juce::Rectangle<float> screenRect { 65.98f, 32.36f, 485.53f, 169.89f };
    const juce::Point<float> ledPos { 308.75f, 681.57f }; const float ledS = 30.34f;
    const juce::Rectangle<float> glowRect { 197.51f, 570.34f, 222.47f, 222.47f };
    const juce::Point<float> fsPos { 308.75f, 778.65f }; const float footR = 38.31f, fsS = 192.13f;
    const juce::Rectangle<float> presetRect { 108.75f, 560.22f, 400.00f, 38.00f };   // pastille de reference 400 x 38
    const float presetFont = 23.0f;
    const float jackY = 558.00f;
    struct Txt { const char* s; int font; float em, track, x, y, rot; };   // police : 0 medium, 1 bold, 2 light
    const Txt texts[] = {
        { "SENS", 0, 21.00f, 3.00f, 114.59f, 230.90f, 0.00f },
        { "DYN", 0, 21.00f, 3.00f, 244.03f, 230.90f, 0.00f },
        { "BEND", 0, 21.00f, 3.00f, 373.47f, 230.90f, 0.00f },
        { "TRANS", 0, 21.00f, 3.00f, 502.91f, 230.90f, 0.00f },
        { "OMI", 1, 62.00f, 14.00f, 308.75f, 440.90f, 0.00f },
        { "GUITAR TO MIDI", 2, 18.00f, 7.00f, 308.75f, 493.48f, 0.00f },
        { "DRY", 0, 15.00f, 2.50f, 148.97f, 726.37f, 0.00f },
        { "MIDI", 0, 15.00f, 2.50f, 148.97f, 830.93f, 0.00f },
        { "OUT", 0, 16.00f, 3.00f, 47.78f, 558.00f, 90.00f },
        { "IN", 0, 16.00f, 3.00f, 569.72f, 558.00f, 90.00f }
    };
    struct Dot { float x, y, r; };
    const Dot dots[] = { { 73.92f, 348.09f, 1.80f }, { 59.89f, 325.19f, 1.80f }, { 57.78f, 298.42f, 1.80f }, { 68.06f, 273.61f, 1.80f }, { 88.48f, 256.17f, 1.80f }, { 114.59f, 249.90f, 1.80f }, { 140.70f, 256.17f, 1.80f }, { 161.12f, 273.61f, 1.80f }, { 171.40f, 298.42f, 1.80f }, { 169.29f, 325.19f, 1.80f }, { 155.26f, 348.09f, 1.80f }, { 203.36f, 348.09f, 1.80f }, { 189.33f, 325.19f, 1.80f }, { 187.22f, 298.42f, 1.80f }, { 197.50f, 273.61f, 1.80f }, { 217.92f, 256.17f, 1.80f }, { 244.03f, 249.90f, 1.80f }, { 270.14f, 256.17f, 1.80f }, { 290.56f, 273.61f, 1.80f }, { 300.84f, 298.42f, 1.80f }, { 298.73f, 325.19f, 1.80f }, { 284.70f, 348.09f, 1.80f }, { 332.80f, 348.09f, 1.80f }, { 318.77f, 325.19f, 1.80f }, { 316.66f, 298.42f, 1.80f }, { 326.94f, 273.61f, 1.80f }, { 347.36f, 256.17f, 1.80f }, { 373.47f, 249.90f, 1.80f }, { 399.58f, 256.17f, 1.80f }, { 420.00f, 273.61f, 1.80f }, { 430.28f, 298.42f, 1.80f }, { 428.17f, 325.19f, 1.80f }, { 414.14f, 348.09f, 1.80f }, { 462.24f, 348.09f, 1.80f }, { 448.21f, 325.19f, 1.80f }, { 446.10f, 298.42f, 1.80f }, { 456.38f, 273.61f, 1.80f }, { 476.80f, 256.17f, 1.80f }, { 502.91f, 249.90f, 1.80f }, { 529.02f, 256.17f, 1.80f }, { 549.44f, 273.61f, 1.80f }, { 559.72f, 298.42f, 1.80f }, { 557.61f, 325.19f, 1.80f }, { 543.58f, 348.09f, 1.80f } };
    const juce::Rectangle<float> rules[] = { { 133.61f, 492.58f, 50.00f, 1.80f }, { 433.89f, 492.58f, 50.00f, 1.80f } };
    const juce::Point<float> togPos { 148.97f, 778.65f }; const float togS = 80.09f, togR = 22.25f;
    const juce::Point<float> logoPos { 308.75f, 871.69f }; const float logoW = 198.20f;
}
