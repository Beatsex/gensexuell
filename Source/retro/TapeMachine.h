#pragma once

#include "Biquad.h"
#include "DelayLine.h"
#include "ModLFO.h"

#include <cmath>
#include <cstdint>

namespace retro
{

/**
    Tape-machine colour: transport wow/flutter, magnetic saturation with
    a small bias asymmetry, head bump, HF loss (bias) and hiss.
*/
class TapeMachine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            line[c].prepare (sr, 0.05);
            bump[c].reset();
            hf[c].reset();
        }
        wow.prepare (sr);     wow.setRate (0.7f);
        flutter.prepare (sr); flutter.setRate (6.3f);
        stateL = stateR = 0.0f;
        rng = 0x9e3779b9u;
        updateFilters();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { line[c].reset(); bump[c].reset(); hf[c].reset(); }
        stateL = stateR = 0.0f;
    }

    void setDrive (float d) noexcept { drive = d; }
    void setSpeed (int s) noexcept { speed = s; updateFilters(); }
    void setBias (float b) noexcept { biasLp = b; updateFilters(); }
    void setBump (float dB) noexcept { bumpDb = dB; updateFilters(); }
    void setWowFlutter (float w) noexcept { wowAmount = w; }
    void setNoise (float n) noexcept { noiseAmt = n; }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float base = 0.005f * (float) sr; // 5 ms transport delay
        const float mod = (wowAmount * 0.0025f * wow.process()
                         + wowAmount * 0.0004f * flutter.process()) * (float) sr;

        float dL = line[0].readCubic (base + mod);
        float dR = line[1].readCubic (base - mod);
        line[0].write (inL);
        line[1].write (inR);

        const float drv = 1.0f + drive * 6.0f;
        float yL = saturate (dL, drv, stateL);
        float yR = saturate (dR, drv, stateR);

        yL = hf[0].process (bump[0].process (yL));
        yR = hf[1].process (bump[1].process (yR));

        if (noiseAmt > 0.0f)
        {
            const float g = noiseAmt * 0.0015f;
            yL += g * white();
            yR += g * white();
        }

        outL = yL;
        outR = yR;
    }

private:
    void updateFilters() noexcept
    {
        static const float baseLp[3] = { 9000.0f, 14000.0f, 19000.0f };
        const int sp = speed < 0 ? 0 : (speed > 2 ? 2 : speed);
        float lp = baseLp[sp] * (1.0f - 0.75f * biasLp);
        if (lp < 600.0f) lp = 600.0f;

        for (int c = 0; c < 2; ++c)
        {
            bump[c].setLowShelf (sr, 60.0f, 0.7f, bumpDb);
            hf[c].setLowpass (sr, lp, 0.707f);
        }
    }

    static inline float saturate (float x, float drv, float& state) noexcept
    {
        state += 0.35f * (x - state);
        const float bias = 0.05f;
        return (std::tanh (state * drv + bias) - std::tanh (bias)) / drv;
    }

    inline float white() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * 4.6566129e-10f;
    }

    double sr = 44100.0;
    DelayLine line[2];
    Biquad bump[2], hf[2];
    ModLFO wow, flutter;
    int speed = 1;
    float drive = 0.35f, biasLp = 0.4f, bumpDb = 3.0f, wowAmount = 0.25f, noiseAmt = 0.1f;
    float stateL = 0.0f, stateR = 0.0f;
    uint32_t rng = 0x9e3779b9u;
};

} // namespace retro
