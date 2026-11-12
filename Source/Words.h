// Text -> seed: describe how a patch sounds with 9 descriptors, read a German or English description into
// target descriptors, then search for seeds that sound like that. Words describe the sound, never the genes.
#pragma once
#include "Analysis.h"
#include <cctype>
#include <string>

namespace seedling {

// ---------------------------------------------------------------- descriptors
enum Descriptor { dBright, dAttack, dSustain, dLength, dNoise, dTonal, dInharm, dMotion, dBass, ND };
inline const char* const DESCRIPTOR_NAMES[ND] = { "bright", "attack", "sustain", "length", "noise", "tonal", "inharm", "motion", "bass" };
using Descriptors = std::array<float, ND>;

constexpr float DESC_SECONDS = 1.5f, DESC_HOLD = DESC_SECONDS; // held to the end, so a sustained sound can reach full length

// all values 0..1, from audio at FEAT_SR
inline Descriptors describe (const std::vector<float>& x)
{
    Descriptors d {};
    const int hop = int (FEAT_SR * 0.01f), frames = int (x.size()) / hop;
    if (frames < 4) return d;
    std::vector<float> env (static_cast<size_t> (frames));
    float peak = 1e-9f;
    int peakAt = 0;
    for (int f = 0; f < frames; ++f)
    {
        double s = 0;
        for (int i = 0; i < hop; ++i) s += double (x[size_t (f * hop + i)]) * x[size_t (f * hop + i)];
        env[size_t (f)] = float (std::sqrt (s / hop));
        if (env[size_t (f)] > peak) { peak = env[size_t (f)]; peakAt = f; }
    }
    int atk = 0;
    while (atk < frames && env[size_t (atk)] < 0.9f * peak) ++atk;
    d[dAttack] = clamp01 (std::log10 (atk * 10.0f + 1) / 3); // 0 ms -> 0, 1 s -> 1
    d[dSustain] = clamp01 (env[size_t (std::min (frames - 1, int (0.6f / 0.01f)))] / peak);
    int last = frames - 1;
    while (last > peakAt && env[size_t (last)] < peak * 0.01f) --last;
    d[dLength] = clamp01 (last * 0.01f / DESC_SECONDS);

    // averaged spectrum over the first half second, plus spectral flux between frames
    const int specFrames = 6;
    std::vector<float> power (FFT_N / 2 + 1, 0.0f), prev (FFT_N / 2 + 1, 0.0f);
    std::vector<std::complex<float>> buf (FFT_N);
    float flux = 0, fluxNorm = 1e-9f;
    for (int j = 0; j < specFrames; ++j)
    {
        const int start = j * int (FEAT_SR * 0.08f);
        for (int i = 0; i < FFT_N; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2 * PI * i / (FFT_N - 1));
            buf[size_t (i)] = { start + i < int (x.size()) ? x[size_t (start + i)] * w : 0.0f, 0.0f };
        }
        fft (buf);
        for (int k = 0; k <= FFT_N / 2; ++k)
        {
            const float m = std::abs (buf[size_t (k)]);
            power[size_t (k)] += m * m;
            if (j > 0) { flux += std::abs (m - prev[size_t (k)]); fluxNorm += m + prev[size_t (k)]; }
            prev[size_t (k)] = m;
        }
    }
    double total = 1e-20, weighted = 0, logSum = 0, bassE = 0;
    for (int k = 1; k <= FFT_N / 2; ++k)
    {
        const double p = power[size_t (k)] + 1e-20, hz = k * FEAT_SR / FFT_N;
        total += p;
        weighted += p * hz;
        logSum += std::log (p);
        if (hz < 200) bassE += p;
    }
    const double centroid = weighted / total, nBins = FFT_N / 2;
    d[dBright] = clamp01 (float (std::log2 (std::max (centroid, 100.0) / 100) / 7)); // 100 Hz -> 0, 12.8 kHz -> 1
    d[dNoise] = clamp01 (float (std::exp (logSum / nBins) / (total / nBins)) * 3);   // spectral flatness
    d[dBass] = clamp01 (float (bassE / total) * 1.5f);
    d[dMotion] = clamp01 (flux / fluxNorm * 4);

    float strength = 0;
    const float f0 = detectPitch (x.data(), int (x.size()), FEAT_SR, &strength);
    d[dTonal] = clamp01 ((strength - 0.3f) / 0.65f);
    if (f0 > 0) // energy away from the harmonic series
    {
        double harmonic = 0;
        for (int h = 1; h * f0 < FEAT_SR / 2 && h <= 40; ++h)
        {
            const int c = int (std::lround (h * f0 * FFT_N / FEAT_SR)), w = std::max (1, int (c * 0.03f));
            for (int k = std::max (1, c - w); k <= std::min (FFT_N / 2, c + w); ++k) harmonic += power[size_t (k)];
        }
        d[dInharm] = clamp01 (float (1 - harmonic / total) * 1.4f);
    }
    else
        d[dInharm] = 0.5f;
    return d;
}

inline Descriptors describePatch (const Params& genes, int midi)
{
    static const Genome g;
    Controls c;
    c.base = genes;
    c.len.fill (0);
    c.atonality = 0;
    return describe (renderNote (g, c, 0, midi, DESC_SECONDS, FEAT_SR, DESC_HOLD));
}

// ---------------------------------------------------------------- lexicon
struct Target { Descriptor d; float value, weight; };
struct GeneRange { Gene g; float lo, hi; };

struct Word
{
    const char* stems;                       // '|'-separated stems, German and English, umlauts as ae/oe/ue/ss
    std::vector<Target> targets;
    std::vector<GeneRange> genes = {};       // category templates: where random seeds start
    int note = 0;                            // category register (0 = keep)
};

inline const std::vector<Word>& lexicon()
{
    static const std::vector<Word> words = {
        // categories
        { "pad|flaeche|teppich|strings", { { dAttack, 0.7f, 1 }, { dSustain, 0.8f, 1 }, { dLength, 0.9f, 1 } }, { { attack, 0.5f, 0.8f }, { sustain, 0.6f, 0.95f }, { decay, 0.6f, 0.95f } }, 60 },
        { "bass|bas", { { dBass, 0.8f, 1.5f }, { dBright, 0.3f, 0.7f } }, { { sub, 0.6f, 1 }, { cutoff, 0.15f, 0.45f }, { attack, 0, 0.05f } }, 36 },
        { "sub|subbass|808", { { dBass, 1, 2 }, { dBright, 0.1f, 1 }, { dTonal, 0.9f, 1 } }, { { sub, 0.75f, 1 }, { waveA, 0, 0.09f }, { cutoff, 0.1f, 0.3f }, { noise, 0, 0.05f } }, 36 },
        { "pluck|zupf|gezupft|pizz", { { dAttack, 0, 1 }, { dSustain, 0, 1 }, { dLength, 0.25f, 1 } }, { { attack, 0, 0.05f }, { sustain, 0, 0.07f }, { decay, 0.3f, 0.55f }, { fltEnv, 0.4f, 0.8f } }, 60 },
        { "lead|solo|melodie", { { dSustain, 0.7f, 1 }, { dTonal, 0.9f, 1 }, { dBright, 0.6f, 0.7f } }, { { sustain, 0.6f, 0.9f }, { attack, 0, 0.15f } }, 67 },
        { "glocke|bell|glockenspiel|chime", { { dAttack, 0, 1 }, { dInharm, 0.65f, 1 }, { dLength, 0.7f, 1 }, { dSustain, 0.15f, 0.7f } }, { { fm, 0.25f, 0.6f }, { sustain, 0, 0.07f }, { decay, 0.6f, 0.85f }, { attack, 0, 0.03f } }, 72 },
        { "mallet|marimba|xylophon|vibraphon", { { dAttack, 0, 1 }, { dLength, 0.45f, 1 }, { dTonal, 0.85f, 0.7f } }, { { sustain, 0, 0.07f }, { decay, 0.4f, 0.65f }, { attack, 0, 0.02f } }, 64 },
        { "kick|bassdrum|bd", { { dAttack, 0, 1 }, { dBass, 1, 2 }, { dLength, 0.25f, 1 } }, { { pitchMod, 0.1f, 0.3f }, { modDecay, 0.15f, 0.35f }, { sustain, 0, 0.07f }, { decay, 0.3f, 0.5f }, { waveA, 0, 0.13f } }, 36 },
        { "snare|clap|rim", { { dAttack, 0, 1 }, { dNoise, 0.7f, 1.5f }, { dLength, 0.2f, 1 } }, { { noise, 0.6f, 1 }, { sustain, 0, 0.07f }, { decay, 0.2f, 0.4f } }, 50 },
        { "hat|hihat|hi-hat|cymbal|becken", { { dAttack, 0, 1 }, { dNoise, 0.9f, 1.5f }, { dBright, 0.85f, 1.5f }, { dLength, 0.1f, 1 } }, { { noise, 0.75f, 1 }, { noiseColor, 0.7f, 1 }, { sustain, 0, 0.07f }, { decay, 0.05f, 0.3f }, { cutoff, 0.8f, 1 } }, 72 },
        { "drone|brumm", { { dSustain, 0.95f, 1 }, { dLength, 1, 1 }, { dMotion, 0.6f, 0.5f } }, { { sustain, 0.8f, 1 }, { attack, 0.4f, 0.9f }, { lfoAmt, 0.2f, 0.5f } }, 48 },
        { "fx|effekt|riser|sweep|whoosh", { { dMotion, 0.8f, 1 }, { dNoise, 0.6f, 0.7f } }, { { pitchMod, 0.6f, 0.9f }, { noise, 0.3f, 0.8f }, { modDecay, 0.6f, 0.9f } }, 60 },
        { "textur|texture|atmo|ambient|soundscape", { { dMotion, 0.7f, 1 }, { dLength, 0.9f, 0.7f }, { dAttack, 0.6f, 0.6f } }, { { attack, 0.4f, 0.8f }, { sustain, 0.5f, 0.9f }, { lfoAmt, 0.15f, 0.5f } }, 60 },
        { "orgel|organ", { { dAttack, 0.05f, 0.7f }, { dSustain, 1, 1 }, { dTonal, 1, 1 } }, { { sustain, 0.85f, 1 }, { attack, 0, 0.1f }, { waveA, 0, 0.26f } }, 60 },
        { "chord|akkord|stab", { { dAttack, 0, 0.8f }, { dSustain, 0.3f, 0.6f }, { dBright, 0.6f, 0.5f } }, { { attack, 0, 0.05f }, { decay, 0.35f, 0.6f } }, 60 },
        // properties
        { "hell|bright|brillant|crisp|knackig|spitz|strahlend", { { dBright, 0.85f, 1 } } },
        { "dunkel|dunkl|dark|dumpf|muffled|gedaempft|finster", { { dBright, 0.15f, 1 } } },
        { "warm|waerm|weich|soft|sanft|smooth|rund|mellow", { { dBright, 0.35f, 0.8f }, { dNoise, 0.1f, 0.6f }, { dAttack, 0.35f, 0.4f } } },
        { "hart|haert|hard|aggressiv|aggressive|harsh|scharf|schaerf", { { dBright, 0.8f, 0.8f }, { dAttack, 0, 0.7f }, { dNoise, 0.5f, 0.4f } } },
        { "glasig|glassy|glas|glass|kristall|crystal", { { dBright, 0.75f, 0.8f }, { dTonal, 0.9f, 0.8f }, { dInharm, 0.55f, 0.6f }, { dNoise, 0, 0.6f } } },
        { "metallisch|metallic|metall|metal|blech", { { dInharm, 0.85f, 1 }, { dBright, 0.7f, 0.5f } } },
        { "dreckig|dirty|schmutzig|gritty|kaputt|broken|lofi|lo-fi", { { dNoise, 0.6f, 1 }, { dInharm, 0.6f, 0.6f } } },
        { "verzerrt|distorted|distortion|crunch|zerre", { { dNoise, 0.5f, 0.7f }, { dBright, 0.75f, 0.6f } }, { { drive, 0.75f, 1 } } },
        { "sauber|clean|pur|pure|rein", { { dNoise, 0, 1 }, { dInharm, 0.1f, 0.8f }, { dTonal, 1, 0.7f } } },
        { "kurz|kuerz|short|knapp|staccato|perkussiv|percussive|snappy|tight", { { dLength, 0.15f, 1 }, { dAttack, 0, 0.6f } } },
        { "lang|laeng|long|endlos|endless|ausklingend|sustained", { { dLength, 0.95f, 1 }, { dSustain, 0.6f, 0.5f } } },
        { "langsam|slow|schwellend|swell", { { dAttack, 0.8f, 1 } } },
        { "schnell|fast|direkt|immediate", { { dAttack, 0, 1 } } },
        { "luftig|airy|hauchig|breathy|rauschig|noisy", { { dNoise, 0.7f, 1 }, { dBright, 0.7f, 0.4f } } },
        { "hohl|hollow|nasal", { { dBright, 0.5f, 0.4f }, { dTonal, 0.85f, 0.5f } }, { { waveA, 0.7f, 0.81f }, { fltType, 0, 0.2f } } },
        { "wobbel|wobble|wabern|wobbly|schwebend|wavy|vibrato", { { dMotion, 0.8f, 1 } }, { { lfoAmt, 0.4f, 0.9f } } },
        { "bewegt|moving|lebendig|evolving|organisch|organic", { { dMotion, 0.7f, 1 } } },
        { "statisch|static|steady", { { dMotion, 0.05f, 1 } } },
        { "verstimmt|detuned|schief|schraeg|wonky|atonal", { { dInharm, 0.7f, 0.8f }, { dTonal, 0.5f, 0.6f } } },
        { "tonal|melodisch|melodic|musikalisch", { { dTonal, 1, 1 }, { dInharm, 0.15f, 0.6f } } },
        { "fett|fat|dick|thick|voll|full|breit|wide", { { dBass, 0.6f, 0.7f }, { dBright, 0.5f, 0.3f } }, { { sub, 0.6f, 0.9f }, { chorus, 0.6f, 0.9f } } },
        { "duenn|thin|schmal|schmael", { { dBass, 0.05f, 1 }, { dBright, 0.7f, 0.4f } } },
        { "tief|low|deep|deeper", { { dBass, 0.8f, 1 }, { dBright, 0.25f, 0.5f } } },
        { "hoch|hoh|hohe|hoeh|high", { { dBright, 0.8f, 0.6f }, { dBass, 0.05f, 0.6f } } },
        { "raum|hall|reverb|spacey|spacig|weit|roomy", {}, { { space, 0.6f, 1 } } },
        { "trocken|dry|nah|nahe|close", {}, { { space, 0, 0.2f } } },
        { "fm|digital|80er|80s", { { dInharm, 0.5f, 0.5f } }, { { fm, 0.3f, 0.7f } } },
        { "analog|vintage|retro|alt|alte|aelter|old", { { dBright, 0.45f, 0.5f } }, { { waveA, 0.47f, 0.63f }, { drive, 0.55f, 0.7f } } },
        { "acid|303|squelch", { { dMotion, 0.6f, 0.6f } }, { { reso, 0.65f, 0.9f }, { fltEnv, 0.5f, 0.9f }, { waveA, 0.51f, 0.63f } } },
        { "chorus|phaser|flanger", { { dMotion, 0.6f, 0.7f } }, { { chorus, 0.6f, 0.95f } } },
        // synth archetypes
        { "cs-80|cs80|vangelis|bladerunner", { { dAttack, 0.55f, 0.8f }, { dSustain, 0.85f, 1 }, { dBright, 0.55f, 0.7f }, { dMotion, 0.6f, 0.7f } }, { { waveA, 0.47f, 0.67f }, { chorus, 0.5f, 0.85f }, { space, 0.5f, 0.9f }, { fltEnv, 0.35f, 0.7f } } },
        { "juno|juno-60|juno-106|jupiter|polysix", { { dSustain, 0.75f, 0.8f }, { dBright, 0.5f, 0.7f }, { dMotion, 0.55f, 0.7f } }, { { chorus, 0.65f, 0.95f }, { sub, 0.5f, 0.8f }, { waveA, 0.43f, 0.63f } } },
        { "moog|minimoog|sub37|taurus|model-d", { { dBass, 0.85f, 1.2f }, { dBright, 0.4f, 0.7f }, { dTonal, 0.9f, 0.8f } }, { { sub, 0.65f, 0.95f }, { reso, 0.35f, 0.65f }, { fltEnv, 0.4f, 0.75f }, { drive, 0.45f, 0.7f } }, 36 },
        { "prophet|oberheim|ob-xa|ob-8", { { dBright, 0.55f, 0.7f }, { dSustain, 0.7f, 0.7f } }, { { waveA, 0.47f, 0.7f }, { spread, 0.55f, 0.85f }, { fltSpread, 0.55f, 0.8f } } },
        { "dx7|tx81z|synclavier|operator|fm-piano|epiano|e-piano|rhodes|wurlitzer|wurli", { { dAttack, 0, 0.9f }, { dSustain, 0.35f, 0.7f }, { dTonal, 0.9f, 0.9f }, { dInharm, 0.35f, 0.7f } }, { { fm, 0.2f, 0.5f }, { attack, 0, 0.04f }, { decay, 0.45f, 0.75f }, { sustain, 0.1f, 0.35f } }, 60 },
        { "ms-20|ms20|polivoks|screaming", { { dBright, 0.75f, 0.8f }, { dNoise, 0.45f, 0.7f } }, { { reso, 0.7f, 0.95f }, { drive, 0.75f, 1 } } },
        { "buchla|vactrol|lpg|west-coast|westcoast", { { dAttack, 0, 1 }, { dSustain, 0, 1 }, { dLength, 0.25f, 0.9f }, { dInharm, 0.45f, 0.7f } }, { { fm, 0.2f, 0.55f }, { drive, 0.75f, 0.95f }, { fltEnv, 0.6f, 0.9f }, { attack, 0, 0.03f }, { sustain, 0, 0.05f }, { decay, 0.2f, 0.45f } }, 60 },
        { "mellotron|optigan", { { dAttack, 0.45f, 0.8f }, { dSustain, 0.75f, 0.8f }, { dBright, 0.35f, 0.8f }, { dNoise, 0.35f, 0.7f }, { dMotion, 0.55f, 0.7f } }, { { lfoAmt, 0.2f, 0.45f }, { cutoff, 0.25f, 0.5f }, { noise, 0.15f, 0.35f } } },
        { "supersaw|hypersaw|virus|jp-8000|trance", { { dBright, 0.8f, 1 }, { dSustain, 0.8f, 0.8f }, { dMotion, 0.65f, 0.8f } }, { { waveA, 0.56f, 0.74f }, { chorus, 0.75f, 1 }, { spread, 0.7f, 1 }, { cutoff, 0.65f, 0.95f } } },
        { "chiptune|8-bit|8bit|sid|c64|gameboy|nes|arcade", { { dAttack, 0, 1 }, { dBright, 0.75f, 0.8f }, { dTonal, 0.9f, 0.8f } }, { { waveA, 0.67f, 0.81f }, { lfoShape, 0.7f, 1 }, { space, 0, 0.15f }, { attack, 0, 0.02f } } },
        // acoustic instruments
        { "kalimba|mbira|handpan|hang|tongue-drum", { { dAttack, 0, 1 }, { dSustain, 0.05f, 1 }, { dLength, 0.45f, 0.9f }, { dTonal, 0.95f, 1 }, { dBright, 0.45f, 0.8f } }, { { attack, 0, 0.02f }, { sustain, 0, 0.06f }, { decay, 0.4f, 0.65f }, { waveA, 0, 0.21f } }, 64 },
        { "harfe|harp|gitarre|guitar|banjo|mandoline|mandolin|ukulele|koto|guzheng|shamisen|sitar|zither|oud", { { dAttack, 0, 1 }, { dSustain, 0.1f, 0.9f }, { dLength, 0.55f, 0.8f }, { dBright, 0.65f, 0.7f }, { dTonal, 0.9f, 0.8f } }, { { attack, 0, 0.03f }, { sustain, 0, 0.08f }, { decay, 0.45f, 0.75f }, { fltEnv, 0.45f, 0.8f } }, 60 },
        { "cello|violine|geige|violin|viola|bratsche|arco|bowed|gestrichen|gambe", { { dAttack, 0.55f, 0.9f }, { dSustain, 0.85f, 1 }, { dTonal, 0.95f, 1 }, { dMotion, 0.5f, 0.7f } }, { { attack, 0.35f, 0.7f }, { sustain, 0.7f, 0.95f }, { waveA, 0.47f, 0.67f } }, 60 },
        { "floete|flute|shakuhachi|panfloete|ocarina|whistle|duduk|geblasen|blown", { { dAttack, 0.3f, 0.8f }, { dSustain, 0.8f, 0.9f }, { dNoise, 0.45f, 0.8f }, { dTonal, 0.85f, 0.8f }, { dBright, 0.45f, 0.7f } }, { { noise, 0.2f, 0.45f }, { waveA, 0, 0.26f }, { attack, 0.15f, 0.4f }, { sustain, 0.65f, 0.9f } }, 67 },
        { "brass|blaeser|trompete|trumpet|horn|waldhorn|posaune|trombone|tuba|braaam", { { dAttack, 0.2f, 0.8f }, { dSustain, 0.75f, 0.9f }, { dBright, 0.6f, 0.8f }, { dTonal, 0.95f, 0.9f } }, { { waveA, 0.47f, 0.67f }, { fltEnv, 0.5f, 0.85f }, { modAttack, 0.55f, 0.8f }, { attack, 0.08f, 0.25f } }, 55 },
        { "chor|choir|vocal|stimme|voice|formant|vowel|aah|ooh", { { dAttack, 0.55f, 0.8f }, { dSustain, 0.85f, 0.9f }, { dTonal, 0.9f, 0.8f }, { dBright, 0.45f, 0.7f } }, { { fltType, 0.15f, 0.33f }, { fltSpread, 0.65f, 0.9f }, { reso, 0.5f, 0.8f }, { chorus, 0.55f, 0.9f }, { attack, 0.35f, 0.7f } }, 60 },
        { "gong|gamelan|klangschale|singing-bowl|bowl|waterphone|cristal|baschet|crotales", { { dAttack, 0.05f, 0.8f }, { dLength, 0.9f, 1 }, { dInharm, 0.75f, 1 }, { dSustain, 0.45f, 0.7f } }, { { fm, 0.35f, 0.75f }, { sub, 0, 0.25f }, { decay, 0.75f, 0.98f }, { space, 0.55f, 0.95f } }, 64 },
        // materials and effects
        { "holzig|holz|wood|wooden|bambus|bamboo", { { dBright, 0.35f, 0.8f }, { dInharm, 0.35f, 0.6f }, { dLength, 0.3f, 0.6f } }, { { cutoff, 0.25f, 0.5f }, { reso, 0.3f, 0.55f } } },
        { "eisig|eis|icy|ice|frost|frostig|kalt|cold|kuehl|cool|glacial", { { dBright, 0.8f, 0.9f }, { dInharm, 0.5f, 0.6f }, { dBass, 0.1f, 0.7f } }, { { fm, 0.25f, 0.6f }, { treble, 0.65f, 0.95f }, { cutoff, 0.7f, 1 } } },
        { "tape|band|cassette|kassette|vinyl|crackle|knistern|hiss|wow|flutter|leiern|nostalg", { { dBright, 0.3f, 0.8f }, { dNoise, 0.35f, 0.7f }, { dMotion, 0.6f, 0.7f } }, { { cutoff, 0.25f, 0.55f }, { lfoAmt, 0.2f, 0.45f }, { drive, 0.5f, 0.65f }, { damp, 0.55f, 0.85f } } },
        { "shimmer|kathedrale|cathedral|kirche|church|blackhole|haze", { { dLength, 0.95f, 1 }, { dSustain, 0.8f, 0.8f }, { dBright, 0.65f, 0.6f } }, { { space, 0.8f, 1 }, { damp, 0.1f, 0.4f } } },
        { "laser|zap|blaster|scifi|sci-fi|sonar|ping", { { dAttack, 0, 1 }, { dMotion, 0.85f, 1 }, { dLength, 0.35f, 0.8f } }, { { pitchMod, 0.75f, 1 }, { modDecay, 0.2f, 0.5f }, { reso, 0.7f, 0.95f } } },
        // more words
        { "klavier|piano|fluegel|keys|tasten", { { dAttack, 0, 1 }, { dSustain, 0.25f, 0.8f }, { dTonal, 0.95f, 1 }, { dInharm, 0.25f, 0.6f } }, { { attack, 0, 0.02f }, { decay, 0.55f, 0.8f }, { sustain, 0.1f, 0.3f }, { fltEnv, 0.5f, 0.8f } }, 60 },
        { "perc|percussion|perkussion|drum|drums|trommel|tom|toms|conga|bongo|tabla", { { dAttack, 0, 1 }, { dLength, 0.2f, 1 }, { dSustain, 0, 0.8f } }, { { attack, 0, 0.02f }, { sustain, 0, 0.06f }, { decay, 0.2f, 0.45f }, { pitchMod, 0.25f, 0.45f } }, 48 },
        { "arp|arpeggio|arpeggiator|sequenz|sequence|sequencer", { { dAttack, 0, 0.9f }, { dLength, 0.3f, 0.9f }, { dTonal, 0.9f, 0.7f } }, { { attack, 0, 0.03f }, { sustain, 0, 0.1f }, { decay, 0.3f, 0.5f } }, 60 },
        { "reese|growl|knurr|neuro", { { dBass, 0.7f, 1 }, { dMotion, 0.7f, 1 }, { dBright, 0.45f, 0.5f } }, { { waveA, 0.53f, 0.63f }, { chorus, 0.6f, 0.9f }, { drive, 0.6f, 0.8f }, { sustain, 0.75f, 1 } }, 36 },
        { "sinus|sine|sinusoid|sinuston", { { dTonal, 1, 1 }, { dBright, 0.1f, 0.8f }, { dNoise, 0, 0.8f }, { dInharm, 0, 0.6f } }, { { waveA, 0, 0.04f }, { mix, 0, 0.15f }, { fm, 0, 0.1f }, { noise, 0, 0.03f } } },
        { "saege|saegezahn|saw|saws|sawtooth", { { dBright, 0.65f, 0.6f }, { dTonal, 0.9f, 0.6f } }, { { waveA, 0.53f, 0.61f } } },
        { "rechteck|square|squarewave", { { dTonal, 0.9f, 0.6f }, { dBright, 0.55f, 0.4f } }, { { waveA, 0.78f, 0.83f } } },
        { "pulse|puls|pulswelle|pwm", { { dTonal, 0.9f, 0.6f }, { dBright, 0.7f, 0.5f } }, { { waveA, 0.86f, 1 } } },
        { "glitch|glitchy|stotter|stutter|stuttering|gated|zerhackt|choppy", { { dMotion, 0.95f, 1 }, { dNoise, 0.45f, 0.4f } }, { { modSH, 0.75f, 1 }, { lfoAmt, 0.4f, 0.8f } } },
        { "pulsierend|pulsing|rhythmisch|rhythmic|tremolo|pumpend|pumping", { { dMotion, 0.85f, 1 } }, { { lfoShape, 0.55f, 0.8f }, { lfoAmt, 0.45f, 0.85f } } },
        { "stufig|gestuft|stepped|sample-and-hold|treppe", { { dMotion, 0.8f, 1 } }, { { modSH, 0.7f, 0.95f } } },
        { "wiederholend|repeating|looping|loop|schleife", { { dMotion, 0.7f, 0.7f } }, { { envLoop, 0.2f, 0.6f } } },
        { "himmlisch|heavenly|engel|angel|aetherisch|ethereal|traum|dreamy|vertraeumt|celestial", { { dAttack, 0.65f, 0.8f }, { dSustain, 0.85f, 0.8f }, { dLength, 1, 0.8f }, { dBright, 0.6f, 0.6f }, { dMotion, 0.55f, 0.5f }, { dNoise, 0.15f, 0.4f } }, { { space, 0.65f, 1 }, { chorus, 0.5f, 0.85f }, { attack, 0.45f, 0.8f } } },
        { "gruselig|creepy|unheimlich|scary|horror|spooky|duester|bedrohlich|ominous", { { dInharm, 0.7f, 0.8f }, { dBright, 0.3f, 0.7f }, { dMotion, 0.7f, 0.7f }, { dTonal, 0.4f, 0.5f } }, { { space, 0.5f, 0.9f } } },
        { "druckvoll|punchy|knallig|wuchtig|punch", { { dAttack, 0, 1 }, { dBass, 0.7f, 0.8f }, { dLength, 0.35f, 0.5f } }, { { attack, 0, 0.02f }, { drive, 0.5f, 0.7f } } },
        { "rau|rauh|rough|raspy|kratzig|grob|coarse", { { dNoise, 0.55f, 1 }, { dInharm, 0.5f, 0.6f }, { dBright, 0.6f, 0.4f } }, { { drive, 0.6f, 0.85f } } },
        { "funkel|funkelnd|sparkle|sparkly|glitzer|glitzernd|shiny|schimmernd|glaenzend", { { dBright, 0.8f, 1 }, { dInharm, 0.45f, 0.5f }, { dAttack, 0, 0.4f } }, { { treble, 0.6f, 0.9f } } },
        { "zart|delicate|fragile|zerbrechlich|filigran|fein", { { dBright, 0.6f, 0.5f }, { dBass, 0.05f, 0.7f }, { dNoise, 0.1f, 0.5f } } },
        { "rauschen|noise|white-noise|static-noise", { { dNoise, 1, 1.2f }, { dTonal, 0.1f, 0.8f } }, { { noise, 0.75f, 1 } } },
    };
    return words;
}

// length of the longest stem in `list` that `token` starts with (0 = none)
inline size_t stemMatch (const std::string& token, const char* list)
{
    std::string all (list);
    size_t pos = 0, best = 0;
    while (pos <= all.size())
    {
        const size_t bar = std::min (all.find ('|', pos), all.size());
        const std::string stem = all.substr (pos, bar - pos);
        // stems match German endings (warm -> waermeres) but never across a hyphen, short stems match exactly or as a plural (pads)
        if (! stem.empty() && token.rfind (stem, 0) == 0)
        {
            const size_t extra = token.size() - stem.size();
            if (stem.size() >= 4 ? extra <= 5 && token.find ('-', stem.size()) == std::string::npos
                                 : extra == 0 || (stem.size() == 3 && extra == 1 && token.back() == 's'))
                best = std::max (best, stem.size());
        }
        pos = bar + 1;
    }
    return best;
}

inline bool isWordIn (const std::string& token, const char* list) { return stemMatch (token, list) > 0; }

// the word with the longest stem that `token` starts with
inline const Word* findWord (const std::string& token, size_t& bestLen)
{
    const Word* hit = nullptr;
    bestLen = 0;
    for (auto& word : lexicon())
        if (const size_t len = stemMatch (token, word.stems); len > bestLen) { bestLen = len; hit = &word; }
    return hit;
}

inline int editDistance (const std::string& a, const std::string& b)
{
    std::vector<int> row (b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) row[j] = int (j);
    for (size_t i = 1; i <= a.size(); ++i)
    {
        int diag = row[0];
        row[0] = int (i);
        for (size_t j = 1; j <= b.size(); ++j)
        {
            const int up = row[j];
            row[j] = std::min ({ row[j] + 1, row[j - 1] + 1, diag + (a[i - 1] == b[j - 1] ? 0 : 1) });
            diag = up;
        }
    }
    return row[b.size()];
}

// lower-case, umlauts and ß folded to ae/oe/ue/ss, split on anything that isn't a letter or digit; ',' and ';' become a "," token
inline std::vector<std::string> tokenize (const std::string& utf8)
{
    std::string s;
    for (size_t i = 0; i < utf8.size(); ++i)
    {
        const unsigned char c = (unsigned char) utf8[i];
        if (c == 0xC3 && i + 1 < utf8.size())
        {
            const unsigned char d = (unsigned char) utf8[++i];
            switch (d)
            {
                case 0xA4: case 0x84: s += "ae"; break;
                case 0xB6: case 0x96: s += "oe"; break;
                case 0xBC: case 0x9C: s += "ue"; break;
                case 0x9F: s += "ss"; break;
                default: s += ' ';
            }
        }
        else if (std::isalnum (c) || c == '-') s += char (std::tolower (c));
        else if (c == ',' || c == ';') s += " , ";
        else s += ' ';
    }
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < s.size())
    {
        const size_t sp = std::min (s.find (' ', pos), s.size());
        if (sp > pos) out.push_back (s.substr (pos, sp - pos));
        pos = sp + 1;
    }
    return out;
}

struct Wish
{
    Descriptors target {}, weight {};
    std::vector<GeneRange> genes;
    int note = 60;
    std::vector<std::string> known, unknown;
    bool any() const { for (float w : weight) if (w > 0) return true; return ! genes.empty(); }
};

inline Wish parseWish (const std::string& text)
{
    static const char* const NEG = "nicht|kein|not|no|without|ohne|non|never|weder|nie";
    static const char* const MORE = "sehr|very|super|extrem|extremely|mega|richtig|really|ganz|total|viel|viele|lots|much|more|mehr|ultra|hyper|krass|massiv|heavily|stark|strong";
    static const char* const LESS = "etwas|slightly|bisschen|leicht|bit|little|kaum|somewhat|wenig|weniger|dezent|mild|mildly|subtly|less";
    static const char* const TOO = "zu|too";
    static const char* const FILLER = "ein|eine|einen|einem|einer|der|die|das|den|dem|und|mit|a|an|the|and|with|of|von|wie|like|sound|klang|ton|but|aber";
    // genes that mean "how much", so "ohne", "wenig" and "nicht zu" have an obvious meaning for them
    static const auto isAmount = [] (Gene g) { return g == space || g == drive || g == fm || g == lfoAmt || g == chorus; };
    Wish w;
    std::array<double, ND> sum {}, wsum {};
    bool negate = false, mild = false; // mild: "nicht zu ...", a moderate value instead of the opposite
    float scale = 1;

    // a hyphenated token that isn't a word itself (hi-hat, cs-80) is its parts (808-kick, analog-pad)
    std::vector<std::string> tokens;
    for (const auto& tok : tokenize (text))
    {
        size_t len = 0;
        if (tok.find ('-') == std::string::npos || findWord (tok, len) != nullptr) { tokens.push_back (tok); continue; }
        for (size_t pos = 0; pos < tok.size();)
        {
            const size_t dash = std::min (tok.find ('-', pos), tok.size());
            if (dash > pos) tokens.push_back (tok.substr (pos, dash - pos));
            pos = dash + 1;
        }
    }

    for (const auto& tok : tokens)
    {
        if (tok == ",") { negate = mild = false; scale = 1; continue; }
        size_t bestLen = 0;
        const Word* hit = findWord (tok, bestLen);
        // a longer word wins over a modifier inside it (supersaw is not "super saw")
        if (stemMatch (tok, NEG) > bestLen) { negate = true; w.known.push_back (tok); continue; }
        if (stemMatch (tok, MORE) > bestLen) { scale = 1.6f; w.known.push_back (tok); continue; }
        if (stemMatch (tok, LESS) > bestLen) { scale = 0.5f; w.known.push_back (tok); continue; }
        if (stemMatch (tok, TOO) > bestLen)
        {
            if (negate) { negate = false; mild = true; scale = 0.7f; }
            else scale = 1.4f;
            w.known.push_back (tok);
            continue;
        }
        if (hit == nullptr && tok.size() >= 5) // tolerate one typo
            for (auto& word : lexicon())
            {
                for (const auto& stem : tokenize (std::string (word.stems))) // stems contain '|', tokenize splits on it
                    if (stem.size() >= 5 && editDistance (tok, stem) <= 1) { hit = &word; break; }
                if (hit != nullptr) break;
            }
        if (hit == nullptr)
        {
            if (! isWordIn (tok, FILLER)) w.unknown.push_back (tok);
            continue;
        }
        w.known.push_back (tok);
        for (const auto& t : hit->targets)
        {
            float v = negate ? 1 - t.value : t.value;
            if (mild) v = 0.5f + (v - 0.5f) * 0.25f;
            const float k = t.weight * scale;
            sum[size_t (t.d)] += double (v) * k;
            wsum[size_t (t.d)] += k;
        }
        for (GeneRange r : hit->genes)
        {
            if (! isAmount (r.g))
            {
                if (! negate && ! mild) w.genes.push_back (r);
                continue;
            }
            if (negate)
            {
                if (hit->note != 0) continue; // "ohne Pad" says nothing about the amount of anything
                r = { r.g, 1 - r.hi, 1 - r.lo };
            }
            else if (mild) r = { r.g, 0.5f + (r.lo - 0.5f) * 0.25f, 0.5f + (r.hi - 0.5f) * 0.25f };
            else if (scale > 1.2f) r.lo += 0.4f * (r.hi - r.lo);
            else if (scale < 0.8f) r = { r.g, r.lo * 0.5f, r.lo };
            w.genes.push_back (r);
        }
        if (! negate && hit->note != 0) w.note = hit->note;
        negate = mild = false;
        scale = 1;
    }
    for (int d = 0; d < ND; ++d)
        if (wsum[size_t (d)] > 0)
        {
            w.target[size_t (d)] = clamp01 (float (sum[size_t (d)] / wsum[size_t (d)]));
            w.weight[size_t (d)] = float (wsum[size_t (d)]);
        }
    return w;
}

// 0 = perfect fit
inline float wishDistance (const Wish& w, const Descriptors& d)
{
    double s = 0, ws = 1e-9;
    for (int i = 0; i < ND; ++i)
    {
        const double e = d[size_t (i)] - w.target[size_t (i)];
        s += w.weight[size_t (i)] * e * e;
        ws += w.weight[size_t (i)];
    }
    return float (std::sqrt (s / ws));
}

struct Take
{
    Params genes;
    float fit = 0; // 1 = matches every described quality
    int note = 60;
};

// draw `count` candidate seeds, keep the best distinct ones and refine each by a short evolution
inline std::vector<Take> textTakes (const Wish& w, uint32_t seed, int takes = 4, int count = 240, int generations = 10)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (0, 1);
    auto draw = [&] (uint32_t s) {
        Params p = seedParams (s, 20);
        for (auto& r : w.genes) p[size_t (r.g)] = r.lo + (r.hi - r.lo) * u (rng);
        return p;
    };
    std::vector<Params> cand (static_cast<size_t> (count));
    for (auto& c : cand) c = draw (uint32_t (rng()));
    std::vector<float> dist (static_cast<size_t> (count));
    {
        std::atomic<int> next { 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < 4; ++t)
            pool.emplace_back ([&] { for (int i; (i = next++) < count;) dist[size_t (i)] = w.any() ? wishDistance (w, describePatch (cand[size_t (i)], w.note)) : 0; });
        for (auto& t : pool) t.join();
    }
    std::vector<int> order (static_cast<size_t> (count));
    for (int i = 0; i < count; ++i) order[size_t (i)] = i;
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return dist[size_t (a)] < dist[size_t (b)]; });

    std::vector<Take> out;
    for (int i : order) // distinct starting points: skip near-duplicates of a take already chosen
    {
        bool close = false;
        for (auto& t : out)
        {
            float diff = 0;
            for (int k : netGenes()) diff += std::abs (t.genes[size_t (k)] - cand[size_t (i)][size_t (k)]);
            close |= diff < 2.0f;
        }
        if (! close) out.push_back ({ cand[size_t (i)], dist[size_t (i)], w.note });
        if (int (out.size()) == takes) break;
    }
    if (! w.any()) { for (auto& t : out) t.fit = 0; return out; }

    std::vector<std::thread> pool;
    for (size_t t = 0; t < out.size(); ++t)
        pool.emplace_back ([&, t] {
            std::mt19937 r (seed + uint32_t (t) * 977u);
            std::normal_distribution<float> n (0, 1);
            float sigma = 0.08f;
            for (int gen = 0; gen < generations; ++gen)
            {
                Params child = out[t].genes;
                for (int k : netGenes()) // words describe the sound: pitch, LOOP and MOD S&H stay as the seed has them
                    if (! isPitchGene (k) && k != envLoop && k != modSH) child[size_t (k)] = clamp01 (child[size_t (k)] + n (r) * sigma);
                const float d = wishDistance (w, describePatch (child, w.note));
                if (d < out[t].fit) { out[t].genes = child; out[t].fit = d; sigma *= 1.2f; }
                else sigma *= 0.85f;
            }
        });
    for (auto& t : pool) t.join();
    for (auto& t : out) t.fit = clamp01 (1 - t.fit / 0.35f);
    std::sort (out.begin(), out.end(), [] (const Take& a, const Take& b) { return a.fit > b.fit; }); // take 1 = best
    return out;
}

} // namespace seedling
