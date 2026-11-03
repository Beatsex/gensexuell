// Sound -> seed: spectral features, the neural net that guesses genes from them, and the search that refines the guesses.
// Plain C++ like Engine.h, so the dataset generator, the tests and the plugin share exactly the same code.
#pragma once
#include "Engine.h"
#include <atomic>
#include <complex>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <thread>

namespace seedling {

constexpr float FEAT_SR = 24000;
constexpr int FEAT_LEN = 48000, FFT_N = 1024, MEL = 32, FRAMES = 16, NFEAT = MEL * FRAMES + 1;
constexpr float HOLD_SECONDS = 1.5f;

// genes the net predicts: everything that shapes a single dry voice (not the master effects or panning)
inline const std::vector<int>& netGenes()
{
    static const std::vector<int> g = [] {
        std::vector<int> v;
        for (int k = 0; k < NP; ++k) // every non-master gene (the expansion genes are wired now)
            if (! isMasterGene (k) && k != spread) v.push_back (k);
        return v;
    }();
    return g;
}

inline Params neutralGenes()
{
    Params p;
    p.fill (0.5f);
    p[space] = 0.3f;
    return p;
}

// ---------------------------------------------------------------- features
inline void fft (std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const std::complex<float> w (std::cos (2 * PI / float (len)), -std::sin (2 * PI / float (len)));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> wn (1, 0);
            for (size_t j = 0; j < len / 2; ++j, wn *= w)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * wn;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
            }
        }
    }
}

using Features = std::vector<float>;

// Extraction parameters for the melogram. The defaults reproduce the original 16 x 32 / 24 kHz
// features exactly, so an existing net.bin stays valid. melBands/frames/fftN/sampleRate and the hz
// range now drive the whole extraction: the vector (and the net's input width) is nfeat(cfg), no
// longer a compile-time array. Changing them needs a fresh dataset and a retrained net (step G3).
struct FeatConfig
{
    int   sampleRate  = int (FEAT_SR);
    int   fftN        = FFT_N;
    int   melBands    = MEL;
    int   frames      = FRAMES;
    int   windowSize  = FFT_N;
    float minHz       = 40.0f;
    float maxHz       = 11000.0f;
    float noiseFloor  = -1.0e30f;
    float clipAt      =  1.0e30f;
    int   normalize   = 0;      // 0 = peak, 1 = rms
    int   sqrtSqrt    = 0;      // extra square roots on the band energy (0, 1 or 2)
    bool  interleaved = false;  // storage-order hint for consumers
};

inline int nfeat (const FeatConfig& c) { return c.melBands * c.frames + 1; }

// analysis window length in samples for a config: 2 s, so the default equals FEAT_LEN exactly
inline int featLen (const FeatConfig& c) { return int (std::lround (FEAT_LEN * (double (c.sampleRate) / FEAT_SR))); }

// mel filterbank for a config, built once per distinct config and cached per thread.
// The default key (24 kHz, fftN 1024, 32 bands, 40..11000 Hz) reproduces the original bank.
inline const std::vector<std::vector<float>>& melBankFor (const FeatConfig& c)
{
    struct Key
    {
        int sr, fftN, mel;
        float lo, hi;
        bool operator< (const Key& o) const
        {
            if (sr != o.sr) return sr < o.sr;
            if (fftN != o.fftN) return fftN < o.fftN;
            if (mel != o.mel) return mel < o.mel;
            if (lo != o.lo) return lo < o.lo;
            return hi < o.hi;
        }
    };
    static thread_local std::map<Key, std::vector<std::vector<float>>> cache;
    const Key key { c.sampleRate, c.fftN, c.melBands, c.minHz, c.maxHz };
    if (const auto it = cache.find (key); it != cache.end()) return it->second;

    const int bins = c.fftN / 2 + 1;
    std::vector<std::vector<float>> bank (size_t (c.melBands), std::vector<float> (size_t (bins), 0.0f));
    const auto mel = [] (float hz) { return 2595 * std::log10 (1 + hz / 700); };
    const auto hz = [] (float m) { return 700 * (std::pow (10.0f, m / 2595) - 1); };
    std::vector<float> edges (size_t (c.melBands) + 2);
    for (int i = 0; i < c.melBands + 2; ++i)
        edges[size_t (i)] = hz (mel (c.minHz) + (mel (c.maxHz) - mel (c.minHz)) * i / (c.melBands + 1));
    for (int b = 0; b < c.melBands; ++b)
        for (int k = 0; k < bins; ++k)
        {
            const float f = k * float (c.sampleRate) / c.fftN, lo = edges[size_t (b)], mid = edges[size_t (b + 1)], hi = edges[size_t (b + 2)];
            bank[size_t (b)][size_t (k)] = std::max (0.0f, f < mid ? (f - lo) / (mid - lo) : (hi - f) / (hi - mid));
        }
    return cache.emplace (key, std::move (bank)).first->second;
}

// `frames` windows (denser near the attack) x `melBands` mel bands of log power, normalised to the
// loudest cell, plus the note. The vector length is nfeat(cfg); the defaults are bit-identical to
// the original fixed 16 x 32 extraction, so an existing net.bin stays valid.
inline Features features (const float* x, int n, int midi, const FeatConfig& cfg = {})
{
    const int mel = cfg.melBands, frames = cfg.frames, fftN = cfg.fftN;
    const auto& melBank = melBankFor (cfg);

    Features out (static_cast<size_t> (nfeat (cfg)), 0.0f);
    std::vector<std::complex<float>> buf (static_cast<size_t> (fftN));
    const int span = std::max (fftN, featLen (cfg));
    float top = -100, sumSq = 0;
    for (int j = 0; j < frames; ++j)
    {
        const float u = frames > 1 ? float (j) / (frames - 1) : 0.0f;
        const int start = int (u * u * (span - fftN));
        for (int i = 0; i < fftN; ++i)
        {
            const int idx = start + i;
            const float win = fftN > 1 ? 0.5f - 0.5f * std::cos (2 * PI * i / (fftN - 1)) : 1.0f;
            buf[size_t (i)] = { idx < n ? x[idx] * win : 0.0f, 0.0f };
        }
        fft (buf);
        for (int b = 0; b < mel; ++b)
        {
            float e = 0;
            const auto& band = melBank[size_t (b)];
            for (int k = 0; k < fftN / 2 + 1; ++k) e += band[size_t (k)] * std::norm (buf[size_t (k)]);
            for (int s = 0; s < cfg.sqrtSqrt && s < 2; ++s) e = std::sqrt (std::max (e, 0.0f));
            float v = std::log10 (e + 1e-10f);
            v = std::clamp (v, cfg.noiseFloor, cfg.clipAt);
            out[size_t (j * mel + b)] = v;
            top = std::max (top, v);
            sumSq += v * v;
        }
    }
    // peak normalisation (default) or rms; both centre on the reference and scale by the same span
    const int cells = mel * frames;
    const float ref = cfg.normalize == 1 ? std::sqrt (sumSq / float (cells)) : top;
    for (int i = 0; i < cells; ++i) out[size_t (i)] = clamp01 ((out[size_t (i)] - ref) / 8 + 1);
    out.back() = clamp01 ((midi - 36) / 48.0f);
    return out;
}

// cell-wise distance; different dimensions are never comparable, so they read as "as far as possible"
inline float distance (const Features& a, const Features& b)
{
    if (a.size() != b.size() || a.size() < 2) return 1.0f;
    const int cells = int (a.size()) - 1; // every cell but the note
    float d = 0;
    for (int i = 0; i < cells; ++i) d += std::abs (a[size_t (i)] - b[size_t (i)]);
    return d / cells;
}

// similarity shown on the strands: 1 = identical features
inline float similarity (float d) { return clamp01 (1 - d / 0.25f); }

// how a patch sounds at the analysis rate: dry, one note, held for HOLD_SECONDS
inline Features patchFeatures (const Params& genes, int midi, float tuning = 0, const FeatConfig& cfg = {})
{
    static const Genome g;
    Controls c;
    c.base = genes;
    c.len.fill (0);
    c.atonality = 0;
    c.tuning = tuning;
    const int span = featLen (cfg);
    const auto audio = renderNote (g, c, 0, midi, float (span) / float (cfg.sampleRate), float (cfg.sampleRate), HOLD_SECONDS);
    return features (audio.data(), int (audio.size()), midi, cfg);
}

// ---------------------------------------------------------------- neural net
// MLP stored as "SDNN", layer count, then per layer: in, out, weights [out][in], bias [out]. ReLU between, sigmoid out.
class Net
{
public:
    bool load (const void* data, size_t size)
    {
        layers.clear();
        auto* p = static_cast<const char*> (data);
        const char* end = p + size;
        auto read = [&] (void* dst, size_t n) { if (p + n > end) return false; std::memcpy (dst, p, n); p += n; return true; };
        char magic[4];
        int32_t count = 0;
        if (! read (magic, 4) || std::memcmp (magic, "SDNN", 4) != 0 || ! read (&count, 4)) return false;
        for (int l = 0; l < count; ++l)
        {
            Layer L;
            int32_t in = 0, out = 0;
            if (! read (&in, 4) || ! read (&out, 4)) return false;
            L.in = in;
            L.out = out;
            L.w.resize (size_t (in) * size_t (out));
            L.b.resize (size_t (out));
            if (! read (L.w.data(), L.w.size() * 4) || ! read (L.b.data(), L.b.size() * 4)) return false;
            layers.push_back (std::move (L));
        }
        // the input width lives in the file (any nfeat(cfg)); only the gene count is fixed
        return ! layers.empty() && layers.front().in > 0 && layers.back().out == int (netGenes().size());
    }

    bool ready() const { return ! layers.empty(); }

    int inDim() const { return layers.empty() ? 0 : layers.front().in; }

    Params predict (const Features& f) const
    {
        if (int (f.size()) != inDim()) return neutralGenes(); // wrong feature config: fall back to neutral
        std::vector<float> x (f.begin(), f.end()), y;
        for (size_t l = 0; l < layers.size(); ++l)
        {
            const auto& L = layers[l];
            y.assign (size_t (L.out), 0.0f);
            for (int o = 0; o < L.out; ++o)
            {
                float s = L.b[size_t (o)];
                const float* w = &L.w[size_t (o) * size_t (L.in)];
                for (int i = 0; i < L.in; ++i) s += w[i] * x[size_t (i)];
                y[size_t (o)] = l + 1 < layers.size() ? std::max (0.0f, s) : 1 / (1 + std::exp (-s));
            }
            x.swap (y);
        }
        Params p = neutralGenes();
        const auto& g = netGenes();
        for (size_t i = 0; i < g.size(); ++i) p[size_t (g[i])] = x[i];
        return p;
    }

private:
    struct Layer
    {
        int in = 0, out = 0;
        std::vector<float> w, b;
    };
    std::vector<Layer> layers;
};

// ---------------------------------------------------------------- search
// Four strands, each an evolution strategy that starts from the net's guess (or the current patch) and climbs towards
// the reference. Every improvement becomes a solution on its strand; a strand stops when it stops improving.
class Search
{
public:
    static constexpr int STRANDS = 4, CHILDREN = 6, MAX_GENERATIONS = 300;

    struct Solution
    {
        Params genes;
        float score = 0;
    };

    ~Search() { pause(); }

    // the features the reference, the guess and every child are measured with; must match the net's input width
    FeatConfig cfg;

    // reference: mono audio at FEAT_SR (any length, the config's window length used)
    void setReference (const std::vector<float>& audio, int midi, const Net* net)
    {
        pause();
        refNote = midi;
        ref = features (audio.data(), int (audio.size()), midi, cfg);
        guess = net != nullptr && net->ready() && net->inDim() == int (ref.size()) ? net->predict (ref) : neutralGenes();
        std::lock_guard<std::mutex> lock (m);
        for (int s = 0; s < STRANDS; ++s) resetStrand (s, nullptr);
    }

    void restart (int s, const Params* from)
    {
        const bool was = running();
        pause();
        {
            std::lock_guard<std::mutex> lock (m);
            resetStrand (s, from);
        }
        if (was) resume();
    }

    void resume()
    {
        if (running()) return;
        stopFlag = false;
        for (int s = 0; s < STRANDS; ++s) threads[size_t (s)] = std::thread ([this, s] { run (s); });
    }

    void pause()
    {
        stopFlag = true;
        for (auto& t : threads)
            if (t.joinable()) t.join();
    }

    bool running() const { for (auto& t : threads) if (t.joinable()) return ! stopFlag; return false; }

    // copies of each strand's solutions, done flags and the number of patches tried
    void snapshot (std::array<std::vector<Solution>, STRANDS>& sol, std::array<bool, STRANDS>& done, long& tested) const
    {
        std::lock_guard<std::mutex> lock (m);
        for (int s = 0; s < STRANDS; ++s)
        {
            sol[size_t (s)] = strands[size_t (s)].solutions;
            done[size_t (s)] = strands[size_t (s)].done;
        }
        tested = triedCount.load();
    }

    // expansion (G2): per-strand live status for the UI
    struct Status { int generation = 0; float sigma = 0, bestScore = 0; bool done = false; };
    void status (std::array<Status, STRANDS>& out) const
    {
        std::lock_guard<std::mutex> lock (m);
        for (int s = 0; s < STRANDS; ++s)
        {
            const auto& st = strands[size_t (s)];
            out[size_t (s)] = { st.generation, st.sigma, st.solutions.empty() ? 0.0f : st.solutions.back().score, st.done };
        }
    }

    int note() const { return refNote; }

private:
    struct Strand
    {
        Params parent {};
        float parentDist = 1, sigma = 0.12f;
        int generation = 0;
        bool done = false;
        std::vector<Solution> solutions;
        uint32_t seed = 1;
    };

    void resetStrand (int s, const Params* from)
    {
        auto& st = strands[size_t (s)];
        st = {};
        st.seed = uint32_t (std::random_device {}()) | 1u;
        std::mt19937 rng (st.seed);
        std::normal_distribution<float> n (0, 1);
        static constexpr float startNoise[STRANDS] = { 0.0f, 0.08f, 0.15f, 0.25f };
        st.parent = from != nullptr ? *from : guess;
        if (from == nullptr)
            for (int k : netGenes()) st.parent[size_t (k)] = clamp01 (st.parent[size_t (k)] + n (rng) * startNoise[s]);
        st.parentDist = -1; // evaluated by the worker
        st.sigma = from != nullptr ? 0.06f : 0.12f;
    }

    void run (int s)
    {
        std::mt19937 rng;
        std::normal_distribution<float> n (0, 1);
        Strand st;
        {
            std::lock_guard<std::mutex> lock (m);
            st = strands[size_t (s)];
        }
        rng.seed (st.seed + uint32_t (st.generation));
        if (st.parentDist < 0) st.parentDist = distance (ref, patchFeatures (st.parent, refNote, 0, cfg));

        while (! stopFlag && ! st.done)
        {
            Params best = st.parent;
            float bestDist = 1e9f;
            for (int c = 0; c < CHILDREN && ! stopFlag; ++c)
            {
                Params child = st.parent;
                for (int k : netGenes()) child[size_t (k)] = clamp01 (child[size_t (k)] + n (rng) * st.sigma);
                const float d = distance (ref, patchFeatures (child, refNote, 0, cfg));
                ++triedCount;
                if (d < bestDist) { bestDist = d; best = child; }
            }
            if (stopFlag) break;

            const bool improved = bestDist < st.parentDist;
            if (improved)
            {
                const bool worthShowing = st.solutions.empty() || similarity (bestDist) > st.solutions.back().score + 0.004f;
                st.parent = best;
                st.parentDist = bestDist;
                st.sigma = std::min (0.3f, st.sigma * 1.15f);
                if (worthShowing) st.solutions.push_back ({ best, similarity (bestDist) });
            }
            else
                st.sigma *= 0.88f;
            if (st.solutions.empty()) st.solutions.push_back ({ st.parent, similarity (st.parentDist) });
            st.done = st.sigma < 0.004f || ++st.generation >= MAX_GENERATIONS || similarity (st.parentDist) > 0.985f;

            std::lock_guard<std::mutex> lock (m);
            strands[size_t (s)] = st;
        }
    }

    Features ref;
    Params guess {};
    int refNote = 60;
    std::array<Strand, STRANDS> strands;
    std::array<std::thread, STRANDS> threads;
    std::atomic<bool> stopFlag { true };
    std::atomic<long> triedCount { 0 };
    mutable std::mutex m;
};

// nearest MIDI note of a detected frequency
inline int nearestNote (float hz) { return hz > 0 ? int (std::lround (69 + 12 * std::log2 (hz / 440))) : 60; }

} // namespace seedling
