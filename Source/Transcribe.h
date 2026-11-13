// Performance -> seed: turn a monophonic recording into notes (timing, pitch, velocity, bends) and play them
// with gensexuell, offline; LiveTracker does the same in real time for the sidechain input.
#pragma once
#include "Engine.h"
#include <functional>

namespace seedling {

// pitch of one short window (normalised autocorrelation, 50 Hz .. 2 kHz); strength = periodicity 0..1.
// r: the caller's scratch of at least n / 2 + 2 floats, so the live tracker does not allocate on the audio thread
inline float windowPitch (const float* x, int n, float sampleRate, float& strength, float* r)
{
    strength = 0;
    const int minLag = std::max (2, int (sampleRate / 2000)), maxLag = std::min (n / 2, int (sampleRate / 50));
    if (maxLag <= minLag + 2) return 0;
    const int len = n - maxLag;
    double e0 = 0;
    for (int i = 0; i < len; ++i) e0 += double (x[i]) * x[i];
    if (e0 < 1e-12) return 0;
    float best = 0;
    int bestLag = 0;
    std::fill (r, r + maxLag + 2, 0.0f);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double acc = 0, e1 = 0;
        for (int i = 0; i < len; ++i)
        {
            acc += double (x[i]) * x[i + lag];
            e1 += double (x[i + lag]) * x[i + lag];
        }
        r[size_t (lag)] = float (acc / std::sqrt (std::max (e0 * e1, 1e-20)));
        if (r[size_t (lag)] > best) { best = r[size_t (lag)]; bestLag = lag; }
    }
    strength = std::max (0.0f, best);
    if (bestLag == 0) return 0; // nothing correlates (a lone click)
    // prefer the shortest lag that is nearly as good (octave errors)
    for (int lag = minLag + 1; lag < bestLag; ++lag)
        if (r[size_t (lag)] > 0.92f * best && r[size_t (lag)] >= r[size_t (lag - 1)] && r[size_t (lag)] >= r[size_t (lag + 1)]) { bestLag = lag; break; }
    const float a = r[size_t (bestLag - 1)], b = r[size_t (bestLag)], c = r[size_t (std::min (bestLag + 1, maxLag))];
    const float den = a - 2 * b + c;
    return sampleRate / (bestLag + (std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f));
}

inline float hzToSemi (float hz) { return 69 + 12 * std::log2 (hz / 440); }

struct NoteEvent
{
    double start = 0, end = 0;       // seconds
    int midi = 60;
    float velocity = 0.8f;
    std::vector<float> bend;         // semitones from `midi`, one value per 10 ms frame from `start`
};

struct TranscribeOptions
{
    float smoothing = 0.5f;          // 0..1, median filter length
    float minNote = 0.06f;           // seconds
    float gateDb = -40;              // relative to the loudest frame
    bool keepBends = true;
    int octave = 0;
};

// monophonic transcription at 10 ms resolution
inline std::vector<NoteEvent> transcribe (const float* x, int n, float sampleRate, const TranscribeOptions& o = {})
{
    const int hop = int (sampleRate * 0.01f), win = int (sampleRate * 0.04f), frames = (n - win) / hop;
    std::vector<NoteEvent> notes;
    if (frames < 3) return notes;
    std::vector<float> semi (size_t (frames), -1), level (size_t (frames), 0);
    float peak = 1e-9f;
    std::vector<float> scratch (size_t (win / 2 + 2));
    for (int f = 0; f < frames; ++f)
    {
        const float* w = x + f * hop;
        double e = 0;
        for (int i = 0; i < win; ++i) e += double (w[i]) * w[i];
        level[size_t (f)] = float (std::sqrt (e / win));
        peak = std::max (peak, level[size_t (f)]);
        float strength = 0;
        const float hz = windowPitch (w, win, sampleRate, strength, scratch.data());
        if (hz > 0 && strength > 0.6f) semi[size_t (f)] = hzToSemi (hz);
    }
    const float gate = peak * std::pow (10.0f, o.gateDb / 20);
    // median filter over voiced frames
    const int half = int (1 + o.smoothing * 4);
    std::vector<float> sm (semi);
    for (int f = 0; f < frames; ++f)
    {
        if (semi[size_t (f)] < 0 || level[size_t (f)] < gate) { sm[size_t (f)] = -1; continue; }
        std::vector<float> v;
        for (int k = std::max (0, f - half); k <= std::min (frames - 1, f + half); ++k)
            if (semi[size_t (k)] >= 0) v.push_back (semi[size_t (k)]);
        std::nth_element (v.begin(), v.begin() + ptrdiff_t (v.size() / 2), v.end());
        sm[size_t (f)] = v[v.size() / 2];
    }

    auto close = [&] (int from, int to) // frames [from, to)
    {
        if ((to - from) * 0.01f < o.minNote) return;
        std::vector<float> v (sm.begin() + from, sm.begin() + to);
        std::nth_element (v.begin(), v.begin() + ptrdiff_t (v.size() / 2), v.end());
        NoteEvent ev;
        ev.midi = std::clamp (int (std::lround (v[v.size() / 2])) + 12 * o.octave, 0, 127);
        ev.start = from * 0.01;
        ev.end = to * 0.01 + win / sampleRate * 0.5;
        float top = 0;
        for (int f = from; f < to; ++f) top = std::max (top, level[size_t (f)]);
        ev.velocity = std::clamp (std::sqrt (top / peak), 0.15f, 1.0f);
        if (o.keepBends)
            for (int f = from; f < to; ++f) ev.bend.push_back (sm[size_t (f)] + 12 * o.octave - ev.midi);
        notes.push_back (std::move (ev));
    };

    int start = -1;
    float anchor = 0;
    int away = 0;
    for (int f = 0; f <= frames; ++f)
    {
        const bool voiced = f < frames && sm[size_t (f)] >= 0;
        const bool onset = voiced && f > 2 && level[size_t (f)] > 2.2f * level[size_t (f - 3)] && level[size_t (f)] > gate * 3;
        if (start >= 0)
        {
            away = voiced && std::abs (sm[size_t (f)] - anchor) > 0.8f ? away + 1 : 0;
            if (! voiced || away >= 3 || onset)
            {
                const int end = away >= 3 ? f - 2 : f;
                close (start, end);
                start = -1;
                if (voiced) { start = end; anchor = sm[size_t (f)]; away = 0; }
                continue;
            }
        }
        else if (voiced)
        {
            start = f;
            anchor = sm[size_t (f)];
            away = 0;
        }
    }
    // a note's tail must not reach into the next one: the late note-off would release a repeated key's new voice
    for (size_t i = 0; i + 1 < notes.size(); ++i) notes[i].end = std::min (notes[i].end, notes[i + 1].start);
    return notes;
}

// plays a note list through a fresh engine; returns stereo (interleaved L/R) at sampleRate.
// Pitch bends ride on a 2-semitone wheel range when they fit, wider bends switch to a 12-semitone range.
inline std::vector<float> renderPerformance (const std::vector<NoteEvent>& notes, double seconds, const Genome& g, Controls c, float sampleRate)
{
    Engine e;
    e.prepare (sampleRate);
    Settings s;
    s.pbRange = 12;
    c.voiceMode = voicePoly;
    c.bulbMode = bulbStandard;
    c.wheel = 0;
    const int block = int (sampleRate * 0.01f), total = int (seconds * sampleRate);
    std::vector<float> out (static_cast<size_t> (total) * 2), L (static_cast<size_t> (block)), R (static_cast<size_t> (block));
    size_t next = 0;
    std::vector<const NoteEvent*> sounding;
    for (int pos = 0, frame = 0; pos < total; pos += block, ++frame)
    {
        const double t = pos / double (sampleRate);
        for (auto it = sounding.begin(); it != sounding.end();)
            if ((*it)->end <= t) { e.noteOff (1, (*it)->midi, c); it = sounding.erase (it); }
            else ++it;
        while (next < notes.size() && notes[next].start <= t)
        {
            e.noteOn (1, notes[next].midi, notes[next].velocity, -1, g, c, s);
            sounding.push_back (&notes[next]);
            ++next;
        }
        if (! sounding.empty()) // bend follows the newest note
        {
            const auto* n = sounding.back();
            const int k = int ((t - n->start) / 0.01);
            e.pitchBend (1, k >= 0 && k < int (n->bend.size()) ? std::clamp (n->bend[size_t (k)] / s.pbRange, -1.0f, 1.0f) : 0.0f);
        }
        const int m = std::min (block, total - pos);
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        e.render (L.data(), R.data(), m, c, s);
        for (int i = 0; i < m; ++i)
        {
            out[size_t (pos + i) * 2] = L[size_t (i)];
            out[size_t (pos + i) * 2 + 1] = R[size_t (i)];
        }
    }
    return out;
}

// ---------------------------------------------------------------- live
// Follows a monophonic input in real time and reports note on / off / bend. Runs on the audio thread:
// the input is decimated to ~24 kHz, pitch is measured every 128 decimated samples on a 1024 window (~45 ms).
class LiveTracker
{
public:
    std::function<void (int note, float vel)> onNote;
    std::function<void (int note)> offNote;
    std::function<void (float semis)> onBend; // offset from the sounding note

    void prepare (double sampleRate)
    {
        decim = std::max (1, int (std::lround (sampleRate / 24000)));
        rate = float (sampleRate / decim);
        ring.assign (WIN, 0.0f);
        pos = sinceHop = 0;
        acc = 0;
        accN = 0;
        current = -1;
        quiet = voicedRun = away = 0;
        level = confidence = 0;
    }

    // forget the sounding note and the buffered input without reporting anything (live input switched off)
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pos = sinceHop = 0;
        acc = 0;
        accN = 0;
        current = -1;
        quiet = voicedRun = away = 0;
        level = confidence = 0;
    }

    float level = 0;   // input RMS of the last window, for the UI meter
    float confidence = 0; // periodicity of the last window, 0 .. 1: low for noise and breath, high for a clear pitch
    int current = -1;  // sounding note or -1

    void process (const float* in, int n, float gateDb, int octave)
    {
        for (int i = 0; i < n; ++i)
        {
            acc += in[i];
            if (++accN < decim) continue;
            ring[size_t (pos)] = acc / float (decim);
            pos = (pos + 1) % WIN;
            acc = 0;
            accN = 0;
            if (++sinceHop >= HOP) { sinceHop = 0; analyse (gateDb, octave); }
        }
    }

private:
    static constexpr int WIN = 1024, HOP = 128;

    void analyse (float gateDb, int octave)
    {
        std::array<float, WIN> w;
        for (int i = 0; i < WIN; ++i) w[size_t (i)] = ring[size_t ((pos + i) % WIN)];
        double e = 0;
        for (float v : w) e += double (v) * v;
        level = float (std::sqrt (e / WIN));
        const float gate = std::pow (10.0f, gateDb / 20);
        float strength = 0;
        const float hz = level > gate ? windowPitch (w.data(), WIN, rate, strength, scratch.data()) : 0.0f;
        confidence = strength;
        const bool voiced = hz > 0 && strength > 0.7f;

        if (current < 0)
        {
            voicedRun = voiced ? voicedRun + 1 : 0;
            if (voicedRun >= 2)
            {
                semiNow = hzToSemi (hz) + 12 * octave;
                current = std::clamp (int (std::lround (semiNow)), 0, 127);
                if (onNote) onNote (current, std::clamp (std::sqrt (level / std::max (gate * 30, 1e-6f)), 0.2f, 1.0f));
                quiet = away = 0;
            }
            return;
        }
        // hysteresis: stop only after ~15 ms clearly below the gate
        quiet = level < gate * 0.5f || ! voiced ? quiet + 1 : 0;
        if (quiet >= 3)
        {
            if (offNote) offNote (current);
            current = -1;
            voicedRun = 0;
            return;
        }
        if (! voiced) return;
        semiNow = hzToSemi (hz) + 12 * octave;
        away = std::abs (semiNow - current) > 0.7f ? away + 1 : 0;
        if (away >= 3) // a new pitch held for ~15 ms: move the note
        {
            const int next = std::clamp (int (std::lround (semiNow)), 0, 127);
            if (onNote) onNote (next, -1); // velocity -1: legato move
            current = next;
            away = 0;
        }
        if (onBend) onBend (semiNow - current);
    }

    std::vector<float> ring;
    std::array<float, WIN / 2 + 2> scratch {};
    int decim = 2, pos = 0, sinceHop = 0, accN = 0, quiet = 0, voicedRun = 0, away = 0;
    float rate = 24000, acc = 0, semiNow = 0;
};

} // namespace seedling
