#pragma once

#include "DelayLine.h"
#include "OnePole.h"
#include "ModLFO.h"
#include "Allpass.h"

#include <cmath>
#include <cstdint>

namespace retro
{

/**
    Feedback delay network reverb with 21 voicings, input/late diffusion,
    attack shaping, and two-band damping (bass multiplier + HF shelf).

    Original design built from documented techniques; the 21 mode names and
    their character follow the observed control surface of the reference
    plug-in, but the delay lengths and coefficients here are our own.
*/
class FDN
{
public:
    static constexpr int N = 8;
    static constexpr int NUM_MODES = 21;

    void prepare (double sampleRate, float maxDelaySeconds)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        for (int i = 0; i < N; ++i)
        {
            lines[i].prepare (sr, maxDelaySeconds);
            dampLp[i].reset();
            bassLp[i].reset();
            lfoA[i].prepare (sr);
            lfoB[i].prepare (sr);
            noiseState[i] = 0.0f;
        }
        for (int i = 0; i < 4; ++i) { diffEarlyL[i].prepare (sr, 0.1); diffEarlyR[i].prepare (sr, 0.1); }
        for (int i = 0; i < 2; ++i) { diffLateL[i].prepare (sr, 0.1); diffLateR[i].prepare (sr, 0.1); }
        setupDiffusers();
        setMode (mode);
    }

    void reset()
    {
        for (int i = 0; i < N; ++i) { lines[i].reset(); dampLp[i].reset(); bassLp[i].reset(); noiseState[i] = 0.0f; }
        for (int i = 0; i < 4; ++i) { diffEarlyL[i].reset(); diffEarlyR[i].reset(); }
        for (int i = 0; i < 2; ++i) { diffLateL[i].reset(); diffLateR[i].reset(); }
        attackEnv = 0.0f;
    }

    void setMode (int m)
    {
        static const float base[N] = { 0.0239f, 0.0281f, 0.0337f, 0.0383f, 0.0439f, 0.0491f, 0.0547f, 0.0601f };
        struct ModeDef { float size, dampCut, shelf, modScale; int style; };
        static const ModeDef defs[NUM_MODES] = {
            { 1.15f,  6000.f, -6.f, 1.00f, 0 }, // 0 Concert Hall
            { 1.15f, 14000.f, -2.f, 1.00f, 0 }, // 1 Bright Hall
            { 1.15f,  5000.f, -8.f, 1.00f, 0 }, // 2 Dirty Hall
            { 1.15f,  5000.f, -8.f, 1.30f, 3 }, // 3 Chaotic Hall
            { 1.20f, 12000.f, -3.f, 1.20f, 1 }, // 4 Hall1984
            { 1.45f,  9000.f, -4.f, 1.00f, 2 }, // 5 Cathedral
            { 1.30f,  8000.f, -5.f, 1.00f, 0 }, // 6 Palace
            { 0.60f, 16000.f, -1.f, 1.10f, 0 }, // 7 Plate
            { 0.62f, 14000.f, -3.f, 1.00f, 0 }, // 8 Smooth Plate
            { 0.60f, 15000.f, -2.f, 1.20f, 0 }, // 9 Dirty Plate
            { 0.80f, 13000.f, -2.f, 1.00f, 0 }, // 10 Chamber
            { 0.82f, 10000.f, -3.f, 1.10f, 0 }, // 11 Chamber1979
            { 0.80f, 11000.f, -3.f, 1.30f, 3 }, // 12 Chaotic Chamber
            { 0.55f,  8000.f, -4.f, 1.00f, 0 }, // 13 Room
            { 0.55f, 10000.f, -3.f, 1.00f, 0 }, // 14 Smooth Room
            { 0.50f, 12000.f, -2.f, 1.00f, 1 }, // 15 Ambience
            { 1.05f,  9000.f, -4.f, 1.00f, 4 }, // 16 Sanctuary
            { 0.70f, 14000.f, -2.f, 1.00f, 0 }, // 17 Nonlin
            { 1.25f, 11000.f, -3.f, 1.20f, 1 }, // 18 Random Space
            { 1.25f, 12000.f, -3.f, 1.20f, 0 }, // 19 Chorus Space
            { 1.00f, 15000.f, -1.f, 1.20f, 3 }  // 20 Chaotic Neutral
        };

        const int m2 = (m < 0 ? 0 : (m >= NUM_MODES ? NUM_MODES - 1 : m));
        mode = m2;
        const ModeDef& d = defs[m2];

        for (int i = 0; i < N; ++i)
            baseSec[i] = base[i] * d.size;

        modeDampCut = d.dampCut;
        modeShelfDb = d.shelf;
        modeModScale = d.modScale;
        modStyle = d.style;

        recomputeDelays();
        recomputeFeedback();
        updateLfoRates();
    }

    void setSize (float size01)
    {
        sizeScale = 0.30f + 0.70f * clamp01 (size01);
        recomputeDelays();
        recomputeFeedback();
    }

    void setDecay (float decay01)
    {
        t60 = 0.5f * std::pow (40.0f, clamp01 (decay01));
        recomputeFeedback();
    }

    void setDecaySeconds (float seconds)
    {
        t60 = seconds < 0.1f ? 0.1f : (seconds > 100.0f ? 100.0f : seconds);
        recomputeFeedback();
    }

    void setDamping (float damp01)
    {
        // damp01 raises damping (lowers HF cutoff) around the mode's base cutoff
        const float cut = modeDampCut * std::pow (0.10f, clamp01 (damp01));
        dampCut = cut < 800.0f ? 800.0f : cut;
        for (int i = 0; i < N; ++i)
            dampLp[i].setCutoff (dampCut, sr);
    }

    void setHighShelf (float db)
    {
        const float d = db > 0.0f ? 0.0f : (db < -24.0f ? -24.0f : db);
        shelfGain = std::pow (10.0f, (modeShelfDb + d) / 20.0f);
        recomputeFeedback();
    }

    void setBassMult (float m01)
    {
        bassFactor = 0.6f + 0.8f * clamp01 (m01); // 0.6..1.4
        recomputeFeedback();
    }

    // Three-band decay: bass, middle and highs each get their own decay. Off until first used.
    void setBassCrossover (float hz)
    {
        threeBand = true;
        for (int i = 0; i < N; ++i)
            bassLp[i].setCutoff (hz, sr);
    }

    /** Bass decay as a multiple of the main decay. */
    void setBassMultiple (float multiple)
    {
        bassMultiple = multiple < 0.1f ? 0.1f : multiple;
        recomputeFeedback();
    }

    /** Where the highs start to decay faster, in Hz. */
    void setHighCrossover (float hz)
    {
        dampCut = hz;
        for (int i = 0; i < N; ++i)
            dampLp[i].setCutoff (hz, sr);
        recomputeFeedback();
    }

    /** How much faster: the highs' loss per pass, in dB. 0 turns it off. */
    void setHighLossDb (float db)
    {
        const float d = db > 0.0f ? 0.0f : (db < -24.0f ? -24.0f : db);
        shelfGain = std::pow (10.0f, d / 20.0f);
        recomputeFeedback();
    }

    void setEarlyDiffusion (float d01)
    {
        const float g = 0.70f * clamp01 (d01);
        for (int i = 0; i < 4; ++i) { diffEarlyL[i].setCoef (g); diffEarlyR[i].setCoef (g); }
    }

    void setLateDiffusion (float d01)
    {
        const float g = 0.70f * clamp01 (d01);
        for (int i = 0; i < 2; ++i) { diffLateL[i].setCoef (g); diffLateR[i].setCoef (g); }
    }

    void setAttack (float a01)
    {
        const float t = clamp01 (a01) * 0.40f; // 0..400 ms
        attackCoef = std::exp (-1.0f / ((t > 1e-4f ? t : 1e-4f) * (float) sr));
    }

    void setModDepth (float d01) { modDepthSamples = clamp01 (d01) * 25.0f; }

    void setModRate (float hz) { modRateHz = hz; updateLfoRates(); }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        // input (early) diffusion
        float xl = inL, xr = inR;
        for (int i = 0; i < 4; ++i) { xl = diffEarlyL[i].process (xl); xr = diffEarlyR[i].process (xr); }

        // attack shaping on the excitation
        attackEnv = attackEnv + (1.0f - attackEnv) * (1.0f - attackCoef);
        xl *= attackEnv;
        xr *= attackEnv;

        float x[N];
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float m = modValue (i) * modDepthSamples * modeModScale;
            float d = threeBand ? lines[i].readCubic (delaySamples[i] + m) : lines[i].readLinear (delaySamples[i] + m);
            if (threeBand)
            {
                // Two first-order shelves in a row: neither can lift anything past its own gain,
                // so the loop stays below 1 at every frequency.
                const float low = dampLp[i].process (d);
                d = low + shelfGain * (d - low);
                const float bass = bassLp[i].process (d);
                d = (bass * bassRatio[i] + (d - bass)) * gMiddle[i];
            }
            else
            {
                const float low = dampLp[i].process (d);
                const float high = d - low;
                d = low * gBass[i] + high * gHigh[i];
            }
            x[i] = d;
            sum += d;
        }

        const float k = 2.0f / (float) N;
        const float inj = 0.40f;
        for (int i = 0; i < N; ++i)
        {
            const float mixed = x[i] - k * sum;
            const float src = (i < 4) ? xl : xr;
            const float sign = ((i & 1) == 0) ? 1.0f : -1.0f;
            lines[i].write (mixed + src * inj * sign);
        }

        static const float sl[N]  = {  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f };
        static const float srT[N] = {  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f };
        float ol = 0.0f, orr = 0.0f;
        for (int i = 0; i < N; ++i) { ol += x[i] * sl[i]; orr += x[i] * srT[i]; }
        ol *= 0.22f;
        orr *= 0.22f;

        // late diffusion (output density)
        for (int i = 0; i < 2; ++i) { ol = diffLateL[i].process (ol); orr = diffLateR[i].process (orr); }

        outL = ol;
        outR = orr;
    }

private:
    /** Where the decay control is meant to be true: the middle, or below the highs if they start lower. */
    static inline float referenceHz (float highCrossover) noexcept
    {
        const float below = highCrossover / 3.0f;
        return below < 650.0f ? below : 650.0f;
    }

    static inline float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    void setupDiffusers()
    {
        static const float earlyMs[4] = { 4.7f, 6.1f, 8.3f, 11.9f };
        static const float lateMs[2]  = { 7.3f, 10.7f };
        for (int i = 0; i < 4; ++i)
        {
            diffEarlyL[i].setDelaySamples (earlyMs[i] * 0.001f * (float) sr);
            diffEarlyR[i].setDelaySamples (earlyMs[i] * 1.07f * 0.001f * (float) sr);
        }
        for (int i = 0; i < 2; ++i)
        {
            diffLateL[i].setDelaySamples (lateMs[i] * 0.001f * (float) sr);
            diffLateR[i].setDelaySamples (lateMs[i] * 1.11f * 0.001f * (float) sr);
        }
    }

    void recomputeDelays()
    {
        for (int i = 0; i < N; ++i)
        {
            float d = baseSec[i] * sizeScale * (float) sr;
            delaySamples[i] = d < 4.0f ? 4.0f : d;
        }
    }

    void recomputeFeedback()
    {
        for (int i = 0; i < N; ++i)
        {
            float g = std::pow (10.0f, -3.0f * delaySamples[i] / ((float) sr * t60));
            if (threeBand)
            {
                // The high shelf reaches a little into the middle. Make up for it there, so the decay
                // control is true in the band it speaks for.
                const float x = referenceHz (dampCut) / dampCut;
                const float re = 1.0f + (shelfGain - 1.0f) * x * x / (1.0f + x * x);
                const float im = (shelfGain - 1.0f) * x / (1.0f + x * x);
                const float atReference = std::sqrt (re * re + im * im);

                const float limit = 0.99995f;
                const float bass = std::pow (10.0f, -3.0f * delaySamples[i] / ((float) sr * t60 * bassMultiple));
                const float middle = g / atReference;
                gMiddle[i] = middle > limit ? limit : middle;
                const float ratio = bass / g;
                bassRatio[i] = gMiddle[i] * ratio > limit ? limit / gMiddle[i] : ratio;
                continue;
            }

            float gb = g * bassFactor;
            const float limit = 0.9995f;
            float gh = g * shelfGain;
            gBass[i] = gb > limit ? limit : gb;
            gHigh[i] = gh > limit ? limit : gh;
        }
    }

    void updateLfoRates()
    {
        for (int i = 0; i < N; ++i)
        {
            lfoA[i].setRate (modRateHz * (0.70f + 0.07f * (float) i));
            lfoB[i].setRate (modRateHz * (0.83f + 0.05f * (float) i));
        }
    }

    inline float noiseNext() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * 4.6566129e-10f;
    }

    inline float modValue (int i) noexcept
    {
        const float a = lfoA[i].process();
        const float b = lfoB[i].process();
        switch (modStyle)
        {
            case 1: case 3: // random / chaotic
            {
                const float n = noiseNext();
                noiseState[i] += 0.05f * (n - noiseState[i]);
                return modStyle == 3 ? noiseState[i] * 1.3f : noiseState[i];
            }
            case 2: return 0.6f * a + 0.4f * b; // ensemble
            case 4: return 0.5f * (a + b);      // detuned
            default: return a;                  // chorus
        }
    }

    double sr = 44100.0;
    DelayLine lines[N];
    OnePoleLP dampLp[N], bassLp[N];
    ModLFO lfoA[N], lfoB[N];
    Allpass diffEarlyL[4], diffEarlyR[4], diffLateL[2], diffLateR[2];

    float baseSec[N] {};
    float delaySamples[N] {};
    float gBass[N] {};
    float gMiddle[N] {};
    float bassRatio[N] {};
    float gHigh[N] {};
    float noiseState[N] {};

    int mode = 0;
    int modStyle = 0;
    float modeDampCut = 12000.0f;
    float modeShelfDb = -3.0f;
    float modeModScale = 1.0f;
    float dampCut = 12000.0f;
    float sizeScale = 0.65f;
    float t60 = 2.0f;
    float bassFactor = 1.0f;
    float bassMultiple = 1.0f;
    bool threeBand = false;
    float shelfGain = 1.0f;
    float modDepthSamples = 0.0f;
    float modRateHz = 0.5f;
    float attackCoef = 0.0f;
    float attackEnv = 0.0f;
    uint32_t rng = 0x9e3779b9u;
};

} // namespace retro
