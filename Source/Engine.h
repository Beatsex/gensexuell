// gensexuell voice engine: plain C++ (no JUCE), shared by the plugin, the tests and the dataset generator.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace seedling {

constexpr float PI = 3.14159265358979f;
// MIDI channels are 1..16; the on-screen keyboard and the live input get channels of their own above that
constexpr int NB = 12, MAXV = 32, UI_CHANNEL = 17, LIVE_CHANNEL = 18, NCH = 19;

// ---------------------------------------------------------------- genes
enum Gene
{
    waveA, waveB, ratioB, fm, mix, noise, cutoff, reso, fltEnv, attack, decay, sustain, modDecay, pitchMod, lfoRate, lfoAmt,
    volFade, envLoop, envKey, modAttack, modVel, lfoShape, lfoDelay, noiseColor, sub, fltType, fltSpread, fltKey, drive,
    space, damp, chorus, bass, treble, spread,
    // 54-gene expansion (P1a): appended, so existing indices/patches stay valid. Not yet seeded from the
    // random stream and not yet wired into the voice until P1b (keeps index.html parity + net stable).
    envTime, envTilt, modHold, lfoBalance, pitchA, noiseB, modB, holdB, fmMod, mixMod, shaper,
    rvbSize, rvbAttack, rvbLength, fxMix, fxSize, fxLength, fxDamp, fxChorus,
    // E2 pitch split, appended for index stability; pitchA above is retired (silent) so old patches keep their pitch
    octA, semiA, centA, semiB, centB,
    modSH, // E4: sample & hold on the mod envelope
    NP
};
// genes that existed before the expansion; the seed/dirs stream covers exactly these
inline constexpr int BASE_GENES = 35;
inline const char* const KEYS[NP] = {
    "waveA", "waveB", "ratioB", "fm", "mix", "noise", "cutoff", "reso", "fltEnv", "attack", "decay", "sustain", "modDecay", "pitchMod", "lfoRate", "lfoAmt",
    "volFade", "envLoop", "envKey", "modAttack", "modVel", "lfoShape", "lfoDelay", "noiseColor", "sub", "fltType", "fltSpread", "fltKey", "drive",
    "space", "damp", "chorus", "bass", "treble", "spread",
    "envTime", "envTilt", "modHold", "lfoBalance", "pitchA", "noiseB", "modB", "holdB", "fmMod", "mixMod", "shaper",
    "rvbSize", "rvbAttack", "rvbLength", "fxMix", "fxSize", "fxLength", "fxDamp", "fxChorus",
    "octA", "semiA", "centA", "semiB", "centB",
    "modSH"
};
// effect genes act on the shared master bus, so they always come from the seed, never from a branch
inline bool isMasterGene (int k)
{
    switch (k)
    {
        case space: case damp: case chorus: case bass: case treble:
        case rvbSize: case rvbAttack: case rvbLength: case fxMix: case fxSize: case fxLength: case fxDamp: case fxChorus:
            return true;
        default: return false;
    }
}

using Params = std::array<float, NP>;

enum BulbMode { bulbStandard, bulbVelocity, bulbRanges, bulbLayered };
enum VoiceMode { voicePoly, voiceMono, voiceLegato };
enum WheelTarget { growthUp, growthDown, filterUp, filterDown, envUp, envDown, fmAmount, lfoAmount, effectAmount, volumeAmount };
inline constexpr float SYNC_BEATS[] = { 0, 2, 1.5f, 4.0f / 3, 1, 0.75f, 2.0f / 3, 0.5f }; // off 1/2 1/4D 1/2T 1/4 1/8D 1/4T 1/8

struct Genome // everything about a patch that isn't a host parameter
{
    std::array<Params, NB> dir {};
    std::array<float, NB> off {}, gain {};
    std::array<bool, NB> layer {};
    Genome() { gain.fill (1); layer.fill (true); }
};

struct Controls // host parameters, snapshotted once per block
{
    Params base {};
    std::array<float, NB> len {};
    float tuning = 0, effect = 35, volume = -8, release = 40, atonality = 20;
    float velSens = 100, glide = 0, wheel = 0, wheelScale = 100;
    int wheelTarget = growthUp, bulbMode = bulbStandard, voiceMode = voicePoly, rotation = 0, tempoSync = 0;
    float bpm = 120;
    bool lowCpu = false; // voices run at the host rate, without oversampling
};

struct Settings // per-instance MIDI settings
{
    int inChannel = 0; // 0 = omni
    int velCurve = 1;  // low normal high fixed
    int pressure = 3;  // soft normal hard off
    int minPoly = 16, maxPoly = 32;
    float pbRange = 2, masterTune = 440;
    bool pbHeldOnly = false, programChange = true;
    bool mpe = false;     // MPE lower zone: channel 1 is the master, 2..16 carry one note each
    float mpeRange = 48;  // bend range of the note channels, in semitones
};

// same generator as mulberry32 in index.html, so seeds match the UI
struct Mulberry32
{
    uint32_t a;
    float operator()()
    {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return float (double (t ^ (t >> 14)) / 4294967296.0);
    }
};

inline float clamp01 (float x) { return std::clamp (x, 0.0f, 1.0f); }

inline float snap (float x)
{
    static constexpr float harm[] = { 0.0f, 0.5f, 0.75f, 0.5f + 0.39624813f, 1.0f }; // 0.5x 1x 2x 3x 4x
    float best = harm[0];
    for (float h : harm)
        if (std::abs (h - x) < std::abs (best - x)) best = h;
    return best;
}

// keep in sync with seedParams() in index.html
inline Params seedParams (uint32_t n, float atonality)
{
    Mulberry32 r { n };
    Params p {};
    for (int k = 0; k < NP; ++k) p[size_t (k)] = r();
    p[attack] = std::pow (p[attack], 2.2f);
    p[noise] *= p[noise] * 0.6f;
    p[pitchMod] = 0.5f + (p[pitchMod] - 0.5f) * 0.25f;
    p[lfoAmt] *= 0.5f;
    p[reso] *= 0.8f;
    p[fm] *= 0.7f;
    p[volFade] *= 0.8f;
    p[envLoop] = 1.0f; // seeds start without an envelope loop (branches can grow into it)
    p[modAttack] *= 0.6f;
    p[lfoShape] *= 0.7f;
    p[sub] = 0.3f + p[sub] * 0.5f;
    p[fltType] = 0.3f + p[fltType] * 0.4f;
    p[fltSpread] = 0.5f + (p[fltSpread] - 0.5f) * 0.6f;
    p[fltKey] = 0.35f + p[fltKey] * 0.3f;
    p[drive] = 0.3f + p[drive] * 0.4f;
    p[bass] = 0.3f + p[bass] * 0.4f;
    p[treble] = 0.3f + p[treble] * 0.4f;
    p[spread] = 0.3f + p[spread] * 0.5f;
    for (int k = octA; k <= centB; ++k) p[size_t (k)] = 0.5f; // seeds start in tune
    p[modSH] = 0.5f;                                                // and with a smooth mod envelope
    if (atonality < 50) p[ratioB] = snap (p[ratioB]);
    return p;
}

// keep in sync with dirsFor() in index.html (the UI also draws `bend` from the same stream)
inline void dirsFor (uint32_t n, Genome& g)
{
    Mulberry32 r { n ^ 0xA5A5A5A5u };
    for (int i = 0; i < NB; ++i)
    {
        for (int k = 0; k < BASE_GENES; ++k) g.dir[size_t (i)][size_t (k)] = r() * 2 - 1;
        g.off[size_t (i)] = (r() - 0.5f) * 0.36f;
        r(); // bend
    }
    // new genes consume in a second pass after every branch, so all existing dirs/offs keep their values (UI parity)
    for (int i = 0; i < NB; ++i)
        for (int k = BASE_GENES; k < NP; ++k) g.dir[size_t (i)][size_t (k)] = r() * 2 - 1;
}

inline std::array<float, NB> freshLens (uint32_t n)
{
    Mulberry32 r { n ^ 0x1234567u };
    std::array<float, NB> l;
    for (auto& v : l) v = 0.15f + r() * 0.45f;
    return l;
}

inline float wheelAmount (const Controls& c) { return clamp01 (c.wheel) * c.wheelScale / 100; }

inline float effectiveLen (const Controls& c, int i)
{
    const float l = c.len[size_t (i)], w = wheelAmount (c);
    if (c.wheelTarget == growthUp) return l + (1 - l) * w;
    if (c.wheelTarget == growthDown) return l * (1 - w);
    return l;
}

// pitch genes only drift with atonality (keep in sync with branchParams() in index.html)
inline bool isPitchGene (int k) { return k == pitchMod || (k >= octA && k <= centB); }
// whole steps: floor (x + 0.5) rounds the same way as pitchSteps() in index.html
inline float pitchSteps (float p, float range) { return std::floor ((p - 0.5f) * range + 0.5f); }

inline Params branchParams (const Genome& g, const Controls& c, int i)
{
    const float a = c.atonality / 100, l = effectiveLen (c, i), amt = l * l * (0.4f + a);
    Params p;
    for (int k = 0; k < NP; ++k)
        p[size_t (k)] = isMasterGene (k) ? c.base[size_t (k)]
                                         : clamp01 (c.base[size_t (k)] + g.dir[size_t (i)][size_t (k)] * amt * (isPitchGene (k) ? a : 1.0f));
    if (a < 0.5f && amt > 0) p[ratioB] = snap (p[ratioB]);
    if (a < 0.5f) for (int k : { octA, semiA, semiB }) p[size_t (k)] = c.base[size_t (k)]; // tonal: whole steps stay put
    return p;
}

// which branches a note plays; returns how many were written to `out`
inline int branchesFor (int note, int vel127, const Genome& g, const Controls& c, std::array<int, NB>& out)
{
    const int rot = c.rotation;
    switch (c.bulbMode)
    {
        case bulbVelocity: out[0] = (6 + (127 - vel127) * NB / 128 + rot) % NB; return 1; // bottom branch = hardest hits
        case bulbRanges:   out[0] = (std::clamp ((note - 24) / 6, 0, NB - 1) + rot) % NB; return 1;
        case bulbLayered:
        {
            int n = 0;
            for (int i = 0; i < NB; ++i)
                if (g.layer[size_t (i)]) out[size_t (n++)] = i;
            return n;
        }
        default: out[0] = (note % NB + rot) % NB; return 1;
    }
}

inline float ratioOf (float x) { return x < 0.5f ? std::exp2 (2 * (x - 0.5f)) : std::exp2 (4 * (x - 0.5f)); }
inline float branchAngle (const Genome& g, int i) { return (-90.0f + i * 30.0f) * PI / 180.0f + g.off[size_t (i)]; }

inline float syncSeconds (float seconds, const Controls& c)
{
    if (c.tempoSync <= 0 || c.bpm <= 0) return seconds;
    const float unit = SYNC_BEATS[c.tempoSync] * 60 / c.bpm;
    return unit * std::exp2 (std::round (std::log2 (std::max (seconds, 1e-4f) / unit)));
}

// ---------------------------------------------------------------- wavetables
// band-limited morph tables: sine 0 > saw 0.57 > square 0.81 > pulse 1 (the width narrows from
// 50 % to 12.5 % over the last segment), 101 morph steps so every break point has its own table
// ponytail: FM sidebands are not band-limited; the 2x oversampled voice helps, extreme fm on high notes still aliases
class Tables
{
public:
    static constexpr int SIZE = 2048, MORPHS = 101;
    static constexpr float SAW = 0.57f, SQUARE = 0.81f, PULSE_MIN = 0.125f;
    static constexpr int HARMS[] = { 40, 28, 20, 14, 10, 7, 5, 3, 2, 1 };
    static constexpr int BANDS = int (std::size (HARMS));

    static const Tables& get() { static const Tables t; return t; }

    const float* table (float morph, float freq, float sampleRate) const
    {
        const int m = int (std::lround (clamp01 (morph) * (MORPHS - 1)));
        const float maxH = 0.5f * sampleRate / std::max (freq, 1.0f);
        int b = 0;
        while (b < BANDS - 1 && HARMS[b] > maxH) ++b;
        return data[size_t (m * BANDS + b)].data();
    }

private:
    std::vector<std::array<float, SIZE>> data;

public:
    // sine / cosine amplitude of harmonic n at a morph position (mirrored in index.html's wave preview)
    static void harmonic (float morph, int n, float& sn, float& cs)
    {
        const float saw = 2.0f / (PI * n) * (n % 2 ? 1.0f : -1.0f), square = n % 2 ? 4.0f / (PI * n) : 0.0f;
        cs = 0;
        if (morph <= SAW)
        {
            const float f = morph / SAW;
            sn = (n == 1 ? 1 - f : 0.0f) + saw * f;
        }
        else if (morph <= SQUARE)
        {
            const float f = (morph - SAW) / (SQUARE - SAW);
            sn = saw * (1 - f) + square * f;
        }
        else // pulse of width d, aligned so that d = 0.5 is the square above
        {
            const float d = 0.5f - (0.5f - PULSE_MIN) * (morph - SQUARE) / (1 - SQUARE);
            sn = 2.0f / (PI * n) * (1 - std::cos (2 * PI * n * d));
            cs = 2.0f / (PI * n) * std::sin (2 * PI * n * d);
        }
    }

private:
    Tables() : data (size_t (MORPHS * BANDS))
    {
        std::array<float, SIZE> sine;
        for (int s = 0; s < SIZE; ++s) sine[size_t (s)] = std::sin (2 * PI * s / SIZE);
        const int squareM = int (std::lround (SQUARE * (MORPHS - 1)));
        std::array<float, BANDS> squareRms {};
        for (int m = 0; m < MORPHS; ++m)
        {
            float sn[HARMS[0] + 1], cs[HARMS[0] + 1];
            for (int n = 1; n <= HARMS[0]; ++n) harmonic (m / float (MORPHS - 1), n, sn[n], cs[n]);
            for (int b = 0; b < BANDS; ++b)
            {
                auto& t = data[size_t (m * BANDS + b)];
                float peak = 0;
                for (int s = 0; s < SIZE; ++s)
                {
                    float v = 0;
                    for (int n = 1; n <= HARMS[b]; ++n)
                        v += sn[n] * sine[size_t ((n * s) & (SIZE - 1))] + cs[n] * sine[size_t ((n * s + SIZE / 4) & (SIZE - 1))];
                    t[size_t (s)] = v;
                    peak = std::max (peak, std::abs (v));
                }
                for (auto& v : t) v /= std::max (peak, 1e-6f);
                // a narrowing pulse keeps the square's loudness (peak-normalised it would fall ~8 dB);
                // the price is a higher peak (about 2x the square's at 12.5 %), caught by the master soft clipper
                float sum = 0;
                for (float v : t) sum += v * v;
                const float rms = std::sqrt (sum / SIZE);
                if (m == squareM) squareRms[size_t (b)] = rms;
                else if (m > squareM) for (auto& v : t) v *= squareRms[size_t (b)] / std::max (rms, 1e-6f);
            }
        }
    }
};

inline float lookup (const float* t, float phase)
{
    const float x = phase * Tables::SIZE;
    const int i = int (x);
    const float f = x - i;
    return t[i & (Tables::SIZE - 1)] * (1 - f) + t[(i + 1) & (Tables::SIZE - 1)] * f;
}

inline float wrap (float p) { return p - std::floor (p); }

// TPT state variable filter (Simper); returns lowpass, bandpass, highpass
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void process (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1 / (1 + g * (g + k)), v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3, v2 = ic2 + g * v1;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

// live modulation applied to sounding voices each block
struct Live
{
    float filterMul = 1, fmAdd = 0, lfoAdd = 0, bendSemis = 0;
};

// ---------------------------------------------------------------- oversampling
// the voice runs at 2^OS_STAGES times the host rate; each half-band stage halves it again
inline constexpr int OS_STAGES = 1, OS = 1 << OS_STAGES;
// 63-tap half-band low-pass (Blackman-windowed sinc): flat to ~0.2, stops from ~0.3 of the rate it runs at
inline const std::array<float, 63> halfBandTaps = [] {
    std::array<float, 63> t {};
    double sum = 0;
    for (int i = 0; i < 63; ++i)
    {
        const int m = i - 31;
        const double w = 0.42 - 0.5 * std::cos (2 * PI * i / 62) + 0.08 * std::cos (4 * PI * i / 62);
        t[size_t (i)] = float ((m == 0 ? 0.5 : std::sin (PI * m / 2) / (PI * m)) * w);
        sum += t[size_t (i)];
    }
    for (auto& x : t) x = float (x / sum);
    return t;
}();
struct HalfBand
{
    float buf[126] {};
    int pos = 0;
    void push (float v) { buf[pos] = buf[pos + 63] = v; pos = pos == 62 ? 0 : pos + 1; }
    float out() const
    {
        const float* x = buf + pos;
        float y = halfBandTaps[31] * x[31];
        for (int i = 0; i < 63; i += 2) y += halfBandTaps[size_t (i)] * x[i]; // the other odd taps are zero
        return y;
    }
};

// ---------------------------------------------------------------- voice
class Voice
{
public:
    bool active = false, held = false;
    int note = -1, channel = 0, branch = 0;
    uint64_t age = 0;
    float frozenBend = 0;

    void start (int midi, int ch, int br, float vel, const Genome& g, const Controls& c, const Settings& s, float sampleRate,
                uint32_t& rng, float glideFrom, float layerCents)
    {
        // a sounding voice taken over (voice steal, mono retrigger) keeps its phases, filters and decimator and
        // ramps from its current level, so it does not click; a free voice starts exactly as before
        const bool steal = active && osStages == (c.lowCpu ? 0 : OS_STAGES);
        osStages = c.lowCpu ? 0 : OS_STAGES;
        os = 1 << osStages;
        sr = sampleRate * float (os);
        if (! steal) for (auto& h : down) h = {};
        note = midi;
        channel = ch;
        branch = br;
        p = branchParams (g, c, br);
        const float a = c.atonality / 100;
        tune = c.tuning * 100 + layerCents;
        if (a > 0.5f) tune += (rand (rng) * 2 - 1) * (a - 0.5f) * 100;
        masterTune = s.masterTune;
        targetSemi = float (midi);
        semi = glideFrom >= 0 ? glideFrom : targetSemi;
        setGlide (c.glide);

        const float velEff = 1 - c.velSens / 100 * (1 - vel);
        const float modVelAmt = clamp01 ((p[modVel] - 0.1f) / 0.9f);
        modScale = 1 - modVelAmt * (1 - vel);

        // envelope times, scaled by key follow and the wheel's env-time targets
        float timeMul = std::exp2 (-(p[envKey] - 0.5f) * 2 * (midi - 60) / 12.0f);
        timeMul *= std::exp2 ((p[envTime] - 0.5f) * 2.0f); // expansion gene: overall envelope time (1 at 0.5)
        const float w = wheelAmount (c);
        if (c.wheelTarget == envUp) timeMul *= std::exp2 (w * 2);
        if (c.wheelTarget == envDown) timeMul *= std::exp2 (-w * 2);

        ratio = ratioOf (p[ratioB]);
        const float md = 0.01f * std::pow (800.0f, p[modDecay]) * timeMul;
        modCoef = std::exp (-1.0f / (md / 3 * sr));
        modAtkS = p[modAttack] > 0.5f ? 0.001f * std::pow (2000.0f, (p[modAttack] - 0.5f) * 2) * timeMul * sr : 0.0f;
        sweep = (p[pitchMod] - 0.5f) * 4800;
        lfoHz = 1 / syncSeconds (1 / (0.05f * std::pow (400.0f, p[lfoRate])), c);
        // above every seed's range: tremolo gives way to stepped random pitch (sample & hold)
        shMix = p[lfoShape] > 0.85f ? (p[lfoShape] - 0.85f) / 0.15f : 0.0f;
        trem = p[lfoShape] > 0.5f ? (std::min (p[lfoShape], 0.85f) - 0.5f) * 2 * (1 - shMix) : 0.0f;
        lfoDelayS = p[lfoDelay] > 0.5f ? (p[lfoDelay] - 0.5f) * 2 * 2 * sr : 0.0f;
        noiseAmt = std::pow (p[noise], 1.5f) * 0.8f;
        noiseLp = 1 - std::pow (1 - (p[noiseColor] < 0.5f ? 0.02f + 1.96f * p[noiseColor] : 1.0f), 1.0f / float (os)); // same colour at the oversampled rate
        noiseSlow = 1 - std::pow (0.95f, 1.0f / float (os));
        noiseHp = p[noiseColor] > 0.5f ? (p[noiseColor] - 0.5f) * 2 : 0.0f;
        subAmt = p[sub] > 0.5f ? (p[sub] - 0.5f) * 1.4f : 0.0f;
        ringAmt = p[sub] < 0.3f ? (0.3f - p[sub]) / 0.3f : 0.0f; // below every seed's range: ring modulation, 0.3 .. 0.5 is neither
        spreadAmt = std::abs (p[fltSpread] - 0.5f) * 1.4f;
        spreadMul = std::exp2 ((p[fltSpread] - 0.5f) * 6);
        driveGain = 1 + std::max (0.0f, p[drive] - 0.5f) * 16;
        driveRoot = std::sqrt (driveGain);
        osRoot = std::sqrt (float (os));
        // cutoff follows the key by fltKey (0.5 = fully, 0 = fixed at middle C); the per-sample part is pow (f, keyAmt)
        keyAmt = clamp01 (p[fltKey] * 2);
        keyFixed = std::pow (261.63f, 1 - keyAmt);
        cutoffMul = std::pow (40.0f, p[cutoff]);
        fltEnv6 = p[fltEnv] * 6;
        foldAmt = p[drive] > 0.8f ? (p[drive] - 0.8f) / 0.2f : 0.0f; // above every seed's range: wavefolding on top of the saturation
        k = 1.0f / std::max (0.707f, std::pow (10.0f, 0.5f * std::pow (30.0f, p[reso]) / 20)); // resonance in dB, like Web Audio

        envTiltVal = (p[envTilt] - 0.5f) * 2.0f; // before attack and decay use it
        atkS = std::max (1.0f, 0.002f * std::pow (2000.0f, p[attack]) * timeMul * sr * std::exp2 (envTiltVal * 0.7f));
        const float dec = 0.01f * std::pow (800.0f, p[decay]) * timeMul * std::exp2 (-envTiltVal * 0.7f);
        decCoef = std::exp (-1.0f / (dec / 3 * sr));
        sus = p[sustain] < 0.08f ? 0.0f : p[sustain];
        peak = velEff * 0.22f * g.gain[size_t (br)];
        fadeCoef = p[volFade] > 0.75f ? std::exp (-1.0f / ((10.0f - (p[volFade] - 0.75f) * 4 * 9.5f) * sr)) : 1.0f;
        const float loopSec = p[envLoop] < 0.98f ? syncSeconds (0.05f * std::pow (160.0f, p[envLoop]) * timeMul, c) : 0.0f;
        loopS = loopSec > 0 && loopSec * sr < atkS + dec * 2 * sr ? loopSec * sr : 0.0f;
        oneShotEnd = sus > 0 || loopS > 0 ? -1.0f : atkS + dec * 2.5f * sr;

        const float pan = std::cos (branchAngle (g, br)) * c.effect / 100 * p[spread] * 2;
        const float x = (std::clamp (pan, -1.0f, 1.0f) + 1) * 0.25f * PI; // equal power, like StereoPannerNode
        gl = std::cos (x);
        gr = std::sin (x);

        // 54-gene expansion voice controls (neutral at 0.5 where possible)
        pitchACents = pitchSteps (p[octA], 4) * 1200 + pitchSteps (p[semiA], 24) * 100 + (p[centA] - 0.5f) * 100; // -2..+2 oct, +-12 st, +-50 ct
        ratio *= std::exp2 (pitchSteps (p[semiB], 24) / 12 + (p[centB] - 0.5f) / 12);                         // B relative to A
        fmModAmt = std::max (0.0f, p[fmMod] - 0.5f) * 2.0f;
        mixModAmt = (p[mixMod] - 0.5f) * 2.0f;
        noiseBAmt = std::max (0.0f, p[noiseB] - 0.5f) * 2.0f;
        shaperAmt = std::max (0.0f, p[shaper] - 0.5f) * 2.0f;
        modHoldLevel = std::max (0.0f, p[modHold] - 0.5f) * 2.0f;
        modShS = p[modSH] > 0.6f ? sr / std::pow (64.0f, (p[modSH] - 0.6f) / 0.4f) : 0.0f; // above 0.6: 1 .. 64 steps per second
        lfoBal = (p[lfoBalance] - 0.5f) * 2.0f;
        modBAmt = (p[modB] - 0.5f) * 2.0f;
        holdBAmt = std::max (0.0f, p[holdB] - 0.5f) * 2.0f;

        shHold = shB = 0;
        mod = modEnv = modAtkS > 0 ? 0.0f : 1.0f;
        modShT = 0;
        t = tAll = 0;
        if (steal)
            amp = ampStart = amp * fade;
        else
        {
            phA = phB = phL = phS = 0;
            amp = ampStart = 0;
            f1 = {};
            f2 = {};
            f3 = {};
            f4 = {};
            nLp = nSlow = 0;
        }
        fade = 1;
        releasing = false;
        active = held = true;
        frozenBend = 0;
    }

    void setGlide (float seconds) { glideCoef = seconds > 0 ? 1 - std::exp (-1.0f / (seconds / 3 * sr)) : 1.0f; }

    // legato: move pitch without retriggering envelopes
    void legatoTo (int midi, float glideSeconds)
    {
        note = midi;
        targetSemi = float (midi);
        setGlide (glideSeconds);
        held = true;
    }

    void release (float seconds, float bendNow)
    {
        held = false;
        releasing = true;
        frozenBend = bendNow;
        relCoef = std::exp (-1.0f / (seconds / 4 * sr));
    }

    void render (float* L, float* R, int n, uint32_t& rng, const Live& live)
    {
        const auto& T = Tables::get();
        const float fNow = masterTune * std::exp2 ((semi - 69) / 12);
        const float* ta = T.table (p[waveA], fNow * std::exp2 (pitchACents / 1200) * 2, sr); // headroom for vibrato, sweep and glide
        const float* tb = T.table (p[waveB], fNow * std::exp2 (pitchACents / 1200) * ratio * 2, sr);
        const float fmIndex = std::pow (clamp01 (p[fm] + live.fmAdd), 2.0f) * 10;
        const float lfoA = clamp01 (p[lfoAmt] + live.lfoAdd), lfoCents = lfoA * lfoA * 100 * (1 - trem) * (1 - 0.5f * lfoBal), tremDepth = lfoA * trem * 0.5f * (1 + 0.5f * lfoBal);
        const float fltT = p[fltType];

        for (int s = 0; s < n * os; ++s)
        {
            if (loopS > 0 && t >= loopS && ! releasing)
            {
                t = 0;
                ampStart = amp;
                mod = modEnv = modAtkS > 0 ? 0.0f : 1.0f;
                modShT = 0;
            }

            if (releasing)
                amp *= relCoef;
            else if (t < atkS)
                amp = ampStart + (peak - ampStart) * (t + 1) / atkS;
            else
            {
                amp = peak * sus + (amp - peak * sus) * decCoef;
                fade *= fadeCoef;
            }

            if (modAtkS > 0 && t < modAtkS) modEnv = (t + 1) / modAtkS;
            else modEnv *= modCoef;
            if (modEnv < modHoldLevel) modEnv = modHoldLevel; // expansion gene: mod-envelope hold floor
            if (modShS <= 0) mod = modEnv;
            else if (++modShT >= modShS) { modShT -= modShS; mod = modEnv; } // sample & hold: mod moves in steps

            semi += (targetSemi - semi) * glideCoef;
            const float lfoFade = lfoDelayS > 0 ? std::min (1.0f, tAll / lfoDelayS) : 1.0f;
            const float lfo = (std::sin (2 * PI * phL) * (1 - shMix) + shHold * shMix) * lfoFade;
            const float cents = (semi - 69) * 100 + tune + pitchACents + sweep * mod * modScale + lfo * lfoCents + live.bendSemis * 100;
            const float f = masterTune * std::exp2 (cents / 1200);

            const float b = lookup (tb, phB);
            phA = wrap (phA + (f + fmIndex * (1 + fmModAmt * mod) * f * ratio * (0.3f + 0.7f * mod * modScale) * b) / sr);
            phB = wrap (phB + f * ratio * (1 + modBAmt * mod * 0.15f) * std::exp2 (shB * holdBAmt / 12.0f) / sr);
            phS = wrap (phS + f * 0.5f / sr);
            const float phLNext = wrap (phL + lfoHz / sr);
            if ((shMix > 0 || holdBAmt > 0) && phLNext < phL)
            {
                if (shMix > 0) shHold = rand (rng) * 2 - 1;    // only here, so the noise of every other patch stays the same
                if (holdBAmt > 0) shB = rand (rng) * 2 - 1;    // expansion gene: sample & hold on oscillator B
            }
            phL = phLNext;

            const float w = rand (rng) * 2 - 1;
            nLp += (w - nLp) * noiseLp;
            nSlow += (w - nSlow) * noiseSlow;
            const float nz = (nLp - noiseHp * nSlow) * osRoot; // decimation keeps 1/os of the white noise power

            const float a = lookup (ta, phA);
            const float mixB = clamp01 (p[mix] + mixModAmt * mod * 0.5f), mixA = 1 - mixB; // expansion gene: mix can follow the mod envelope
            const float x = (a * mixA + b * mixB) * (1 - ringAmt) + a * b * ringAmt + subAmt * std::sin (2 * PI * phS) + noiseAmt * nz + noiseBAmt * nz * mixB;

            const float base = std::pow (f, keyAmt) * keyFixed * 0.5f * cutoffMul;
            const float fc = std::clamp (base * std::exp2 (fltEnv6 * mod * modScale) * live.filterMul, 20.0f, 0.45f * sr);
            float lp, bp, hp;
            f1.process (x, std::tan (PI * fc / sr), k, lp, bp, hp);
            // filter type morphs band-pass (0) -> serial low-pass (1/3) -> parallel low-pass (2/3) -> notch (1)
            const float g1 = std::tan (PI * fc / sr), seg = std::min (fltT * 3, 2.999f), u = seg - std::floor (seg);
            float serial = 0, parallel = 0;
            if (seg < 2) { float l, b2, h; f3.process (lp, g1, 1.414f, l, b2, h); serial = l; }    // second stage flat (no extra peak): 24 dB/oct
            if (seg >= 1) { float l, b2, h; f4.process (x, std::tan (PI * std::min (fc * 2, 0.45f * sr) / sr), k, l, b2, h); parallel = 0.5f * (lp + l); } // second peak an octave up
            float y = seg < 1 ? bp * k * (1 - u) + serial * u
                    : seg < 2 ? serial * (1 - u) + parallel * u
                              : parallel * (1 - u) + (lp + hp) * u;
            if (spreadAmt > 0.01f)
            {
                float lp2, bp2, hp2;
                f2.process (x, std::tan (PI * std::min (fc * spreadMul, 0.45f * sr) / sr), k, lp2, bp2, hp2);
                y += bp2 * k * spreadAmt;
            }
            if (driveGain > 1)
            {
                // ponytail: oversampled by OS only; at full fold on very high notes some aliasing is left, raise OS_STAGES if audible
                const float sat = std::tanh (y * driveGain);
                y = (foldAmt > 0 ? sat + foldAmt * (std::sin (y * driveGain * 1.5f) - sat) : sat) / driveRoot;
            }
            if (shaperAmt > 0.001f)
                y = std::tanh (y * (1 + shaperAmt * 3)); // expansion gene: extra wave shaping

            const float out = y * amp * fade * (1 - tremDepth * (1 + std::sin (2 * PI * phL)));
            ++t;
            ++tAll;
            // decimate back to the host rate: every stage keeps every second sample
            float d = out;
            bool ready = true;
            for (int st = 0, m = s; st < osStages && ready; ++st, m >>= 1)
            {
                down[size_t (st)].push (d);
                ready = (m & 1) != 0;
                if (ready) d = down[size_t (st)].out();
            }
            if (ready)
            {
                L[s >> osStages] += d * gl;
                R[s >> osStages] += d * gr;
            }
        }

        if ((releasing && amp < 1e-5f) || (oneShotEnd > 0 && t > oneShotEnd) || (fade < 1e-4f) || ! std::isfinite (f1.ic1 + f1.ic2 + f2.ic1 + f3.ic1 + f4.ic1))
            active = false;
    }

    float level() const { return amp * fade; }

private:
    static float rand (uint32_t& s)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s / 4294967296.0f;
    }

    Params p {};
    Svf f1, f2, f3, f4;
    std::array<HalfBand, OS_STAGES> down;
    int os = OS, osStages = OS_STAGES;
    float sr = 48000, masterTune = 440, tune = 0, semi = 60, targetSemi = 60, glideCoef = 1, ratio = 1, sweep = 0;
    float lfoHz = 1, trem = 0, lfoDelayS = 0, noiseAmt = 0, noiseLp = 1, noiseSlow = 0.05f, noiseHp = 0, nLp = 0, nSlow = 0, subAmt = 0;
    float spreadAmt = 0, spreadMul = 1, driveGain = 1, k = 1, modCoef = 0, modAtkS = 0, mod = 1, modEnv = 1, modShS = 0, modShT = 0, modScale = 1;
    float decCoef = 0, relCoef = 0, sus = 0, peak = 0, amp = 0, ampStart = 0, fade = 1, fadeCoef = 1, loopS = 0;
    float driveRoot = 1, osRoot = 1, keyAmt = 0, keyFixed = 1, cutoffMul = 1, fltEnv6 = 0; // per-note constants of the sample loop
    float atkS = 1, oneShotEnd = -1, gl = 0.7f, gr = 0.7f, phA = 0, phB = 0, phL = 0, phS = 0, t = 0, tAll = 0;
    float shMix = 0, shHold = 0, ringAmt = 0, foldAmt = 0;
    float pitchACents = 0, fmModAmt = 0, mixModAmt = 0, noiseBAmt = 0, shaperAmt = 0;
    float envTiltVal = 0, modHoldLevel = 0, lfoBal = 0, modBAmt = 0, holdBAmt = 0, shB = 0;
    bool releasing = false;
};

// ---------------------------------------------------------------- engine
class Engine
{
public:
    void prepare (float sampleRate)
    {
        Tables::get(); // build the wavetables here, not on the audio thread at the first note
        sr = sampleRate; for (auto& v : voices) v.active = false; stack.clear();
    }

    // forcedBranch >= 0: a note played on the UI, which names its branch directly
    void noteOn (int ch, int midi, float vel, int forcedBranch, const Genome& g, const Controls& c, const Settings& s)
    {
        std::array<int, NB> brs {};
        const int nb = forcedBranch >= 0 ? (brs[0] = forcedBranch, 1) : branchesFor (midi, int (vel * 127), g, c, brs);
        if (nb == 0) return;
        const bool layered = c.bulbMode == bulbLayered && forcedBranch < 0;
        const float a = c.atonality / 100;

        if (c.voiceMode != voicePoly)
        {
            const bool anyHeld = ! stack.empty();
            stack.erase (std::remove (stack.begin(), stack.end(), midi), stack.end());
            stack.push_back (midi);
            if (c.voiceMode == voiceLegato && anyHeld && monoActive())
            {
                for (int j = 0; j < nb; ++j) voices[size_t (j)].legatoTo (midi, c.glide);
                lastNote = midi;
                return;
            }
            const float from = c.glide > 0 && anyHeld && monoActive() ? float (lastNote) : -1.0f; // glide only while a key is held
            for (int j = nb; j < MAXV; ++j) // fewer layers than before: fade the extra voices out instead of cutting them
                if (voices[size_t (j)].active && voices[size_t (j)].held) voices[size_t (j)].release (0.015f, bend[size_t (voices[size_t (j)].channel)]);
            for (int j = 0; j < nb; ++j)
                voices[size_t (j)].start (midi, ch, brs[size_t (j)], vel, g, c, s, sr, rng, from, layered ? layerCents (g, brs[size_t (j)], a) : 0.0f),
                voices[size_t (j)].age = ++clock;
            lastNote = midi;
            return;
        }

        for (auto& v : voices) // retrigger: release this key's previous voices fast
            if (v.active && v.held && v.note == midi && v.channel == ch) v.release (0.015f, bend[size_t (ch)]);

        const int limit = std::min (MAXV, layered ? std::min (s.maxPoly, s.minPoly * nb) : s.minPoly);
        const bool keyHeld = std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.active && v.held; });
        const float from = c.glide > 0 && keyHeld && lastNote >= 0 ? float (lastNote) : -1.0f; // glide only while a key is held
        for (int j = 0; j < nb; ++j)
        {
            Voice* pick = oldestOrFree (limit);
            pick->start (midi, ch, brs[size_t (j)], vel, g, c, s, sr, rng, from, layered ? layerCents (g, brs[size_t (j)], a) : 0.0f);
            pick->age = ++clock;
        }
        lastNote = midi;
    }

    void noteOff (int ch, int midi, const Controls& c)
    {
        const float rel = 0.02f * std::pow (200.0f, c.release / 100);
        if (c.voiceMode != voicePoly)
        {
            stack.erase (std::remove (stack.begin(), stack.end(), midi), stack.end());
            if (! stack.empty() && voices[0].active && voices[0].note == midi) // fall back to the last held key
            {
                for (auto& v : voices) if (v.active && v.held) v.legatoTo (stack.back(), c.glide);
                lastNote = stack.back();
                return;
            }
            if (! stack.empty()) return;
            for (auto& v : voices) if (v.active && v.held) v.release (rel, bend[size_t (v.channel)]);
            return;
        }
        for (auto& v : voices)
            if (v.active && v.held && v.note == midi && v.channel == ch) v.release (rel, bend[size_t (v.channel)]);
    }

    void allOff() { stack.clear(); for (auto& v : voices) if (v.active) v.release (0.015f, 0); }
    void pitchBend (int ch, float value) { bend[size_t (std::clamp (ch, 0, NCH - 1))] = value; }
    // MPE per-note expression: pressure 0..1 (Z), slide -1..1 around CC74 = 64 (Y)
    void pressure (int ch, float value) { press[size_t (std::clamp (ch, 0, NCH - 1))] = value; }
    void slide (int ch, float value) { slideY[size_t (std::clamp (ch, 0, NCH - 1))] = value; }

    void render (float* L, float* R, int n, const Controls& c, const Settings& s)
    {
        const float w = wheelAmount (c);
        Live live;
        if (c.wheelTarget == filterUp) live.filterMul = std::exp2 (w * 4);
        if (c.wheelTarget == filterDown) live.filterMul = std::exp2 (-(1 - w) * 6);
        if (c.wheelTarget == fmAmount) live.fmAdd = w;
        if (c.wheelTarget == lfoAmount) live.lfoAdd = w;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            Live lv = live;
            const auto ch = size_t (v.channel);
            if (s.mpe && v.channel >= 2 && v.channel <= 16) // MPE note channel: its own bend (also after release), pressure and slide
            {
                lv.bendSemis = bend[ch] * s.mpeRange + bend[1] * s.pbRange;
                lv.filterMul *= std::exp2 (press[ch] * 3 + std::min (0.0f, slideY[ch]) * 2); // pressure opens up to 3 octaves, slide down closes 2
                lv.fmAdd += std::max (0.0f, slideY[ch]) * 0.6f;                            // slide up adds FM brightness
            }
            else
                lv.bendSemis = (s.pbHeldOnly && ! v.held ? v.frozenBend : bend[ch]) * s.pbRange + (s.mpe && v.channel != 1 ? bend[1] * s.pbRange : 0.0f);
            v.render (L, R, n, rng, lv);
        }
    }

    int activeVoices() const { int a = 0; for (auto& v : voices) a += v.active; return a; }

    std::array<Voice, MAXV> voices;

private:
    bool monoActive() const { return voices[0].active; }

    // layers detune by atonality even at length 0, each branch its own fixed direction
    static float layerCents (const Genome& g, int br, float a) { return g.off[size_t (br)] / 0.18f * a * 25; }

    // a free voice while under the limit, else the quietest released voice, else the oldest held one
    Voice* oldestOrFree (int limit)
    {
        int sounding = 0;
        for (auto& v : voices) sounding += v.active;
        if (sounding < limit)
            for (auto& v : voices)
                if (! v.active) return &v;
        Voice *quietest = nullptr, *oldest = nullptr;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (! v.held && (quietest == nullptr || v.level() < quietest->level())) quietest = &v;
            if (oldest == nullptr || v.age < oldest->age) oldest = &v;
        }
        return quietest != nullptr ? quietest : oldest != nullptr ? oldest : &voices[0];
    }

    float sr = 48000;
    uint32_t rng = 0x5EED1E55u;
    uint64_t clock = 0;
    int lastNote = -1;
    std::vector<int> stack;
    std::array<float, NCH> bend {}, press {}, slideY {};
};

inline Genome defaultGenome (uint32_t seed)
{
    Genome g;
    dirsFor (seed, g);
    return g;
}

// ---------------------------------------------------------------- offline analysis
// renders one note of one branch, mono, no master effects
inline std::vector<float> renderNote (const Genome& g, Controls c, int branch, int midi, float seconds, float sampleRate, float holdSeconds = -1)
{
    Engine e;
    e.prepare (sampleRate);
    Settings s;
    c.voiceMode = voicePoly;
    e.noteOn (UI_CHANNEL, midi, 0.85f, branch, g, c, s);
    const int n = int (seconds * sampleRate), hold = holdSeconds < 0 ? n : int (holdSeconds * sampleRate);
    std::vector<float> out (static_cast<size_t> (n)), L (256), R (256);
    for (int pos = 0; pos < n; pos += 256)
    {
        const int m = std::min (256, n - pos);
        if (pos <= hold && pos + m > hold) e.noteOff (UI_CHANNEL, midi, c);
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        e.render (L.data(), R.data(), m, c, s);
        for (int i = 0; i < m; ++i) out[size_t (pos + i)] = 0.5f * (L[size_t (i)] + R[size_t (i)]);
    }
    return out;
}

inline float rms (const std::vector<float>& x)
{
    double s = 0;
    for (float v : x) s += double (v) * v;
    return x.empty() ? 0.0f : float (std::sqrt (s / double (x.size())));
}

// fundamental by normalised autocorrelation (40 Hz .. 2 kHz); 0 if nothing periodic.
// `strength` (optional) receives the best correlation, 0..1 = how periodic the sound is.
inline float detectPitch (const float* x, int n, float sampleRate, float* strength = nullptr)
{
    if (strength != nullptr) *strength = 0;
    const int start = n / 8, len = std::min (n - start, int (sampleRate * 0.25f));
    const int minLag = int (sampleRate / 2000), maxLag = std::min (int (sampleRate / 40), len / 2);
    if (len < 2 * maxLag || maxLag <= minLag) return 0;
    std::vector<float> r (size_t (maxLag + 2), 0.0f);
    double e0 = 0;
    for (int i = 0; i < len - maxLag; ++i) e0 += double (x[start + i]) * x[start + i];
    for (int lag = minLag; lag <= maxLag; ++lag) // one lag further would read past the end of a short input
    {
        double acc = 0, e1 = 0;
        for (int i = 0; i < len - maxLag; ++i)
        {
            acc += double (x[start + i]) * x[start + i + lag];
            e1 += double (x[start + i + lag]) * x[start + i + lag];
        }
        r[size_t (lag)] = float (acc / std::sqrt (std::max (e0 * e1, 1e-20)));
    }
    // first peak that comes close to the best one (avoids octave errors)
    float best = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, r[size_t (lag)]);
    if (strength != nullptr) *strength = std::max (0.0f, best);
    if (best < 0.5f) return 0;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (r[size_t (lag)] > 0.9f * best && r[size_t (lag)] >= r[size_t (lag - 1)] && r[size_t (lag)] >= r[size_t (lag + 1)])
        {
            const float a = r[size_t (lag - 1)], b = r[size_t (lag)], c = r[size_t (lag + 1)];
            const float d = 0.5f * (a - c) / std::max (a - 2 * b + c, -1e9f) ;
            return sampleRate / (lag + (std::abs (a - 2 * b + c) > 1e-9f ? d : 0.0f));
        }
    return 0;
}

} // namespace seedling
