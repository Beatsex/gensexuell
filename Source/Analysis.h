// Sound -> seed: spectral features, the neural net that guesses genes from them, and the search that refines the guesses.
// Plain C++ like Engine.h, so the dataset generator, the tests and the plugin share exactly the same code.
#pragma once
#include "Engine.h"
#include <atomic>
#include <complex>
#include <cstring>
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
        for (int k = 0; k < NP; ++k)
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

using Features = std::array<float, NFEAT>;

// 16 frames (denser near the attack) x 32 mel bands of log power, normalised to the loudest cell, plus the note
inline Features features (const float* x, int n, int midi)
{
    static const auto melBank = [] {
        std::vector<std::array<float, FFT_N / 2 + 1>> bank (MEL);
        const auto mel = [] (float hz) { return 2595 * std::log10 (1 + hz / 700); };
        const auto hz = [] (float m) { return 700 * (std::pow (10.0f, m / 2595) - 1); };
        std::array<float, MEL + 2> edges;
        for (int i = 0; i < MEL + 2; ++i) edges[size_t (i)] = hz (mel (40) + (mel (11000) - mel (40)) * i / (MEL + 1));
        for (int b = 0; b < MEL; ++b)
            for (int k = 0; k <= FFT_N / 2; ++k)
            {
                const float f = k * FEAT_SR / FFT_N, lo = edges[size_t (b)], mid = edges[size_t (b + 1)], hi = edges[size_t (b + 2)];
                bank[size_t (b)][size_t (k)] = std::max (0.0f, f < mid ? (f - lo) / (mid - lo) : (hi - f) / (hi - mid));
            }
        return bank;
    }();

    Features out {};
    std::vector<std::complex<float>> buf (FFT_N);
    float top = -100;
    for (int j = 0; j < FRAMES; ++j)
    {
        const float u = float (j) / (FRAMES - 1);
        const int start = int (u * u * (FEAT_LEN - FFT_N));
        for (int i = 0; i < FFT_N; ++i)
        {
            const int idx = start + i;
            const float win = 0.5f - 0.5f * std::cos (2 * PI * i / (FFT_N - 1));
            buf[size_t (i)] = { idx < n ? x[idx] * win : 0.0f, 0.0f };
        }
        fft (buf);
        for (int b = 0; b < MEL; ++b)
        {
            float e = 0;
            for (int k = 0; k <= FFT_N / 2; ++k) e += melBank[size_t (b)][size_t (k)] * std::norm (buf[size_t (k)]);
            const float v = std::log10 (e + 1e-10f);
            out[size_t (j * MEL + b)] = v;
            top = std::max (top, v);
        }
    }
    for (int i = 0; i < MEL * FRAMES; ++i) out[size_t (i)] = clamp01 ((out[size_t (i)] - top) / 8 + 1);
    out[NFEAT - 1] = clamp01 ((midi - 36) / 48.0f);
    return out;
}

inline float distance (const Features& a, const Features& b)
{
    float d = 0;
    for (int i = 0; i < MEL * FRAMES; ++i) d += std::abs (a[size_t (i)] - b[size_t (i)]);
    return d / (MEL * FRAMES);
}

// similarity shown on the strands: 1 = identical features
inline float similarity (float d) { return clamp01 (1 - d / 0.25f); }

// how a patch sounds at the analysis rate: dry, one note, held for HOLD_SECONDS
inline Features patchFeatures (const Params& genes, int midi, float tuning = 0)
{
    static const Genome g;
    Controls c;
    c.base = genes;
    c.len.fill (0);
    c.atonality = 0;
    c.tuning = tuning;
    const auto audio = renderNote (g, c, 0, midi, FEAT_LEN / FEAT_SR, FEAT_SR, HOLD_SECONDS);
    return features (audio.data(), FEAT_LEN, midi);
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
        return ! layers.empty() && layers.front().in == NFEAT && layers.back().out == int (netGenes().size());
    }

    bool ready() const { return ! layers.empty(); }

    Params predict (const Features& f) const
    {
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

    // reference: mono audio at FEAT_SR (any length, first FEAT_LEN samples used)
    void setReference (const std::vector<float>& audio, int midi, const Net* net)
    {
        pause();
        refNote = midi;
        ref = features (audio.data(), int (audio.size()), midi);
        guess = net != nullptr && net->ready() ? net->predict (ref) : neutralGenes();
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
        if (st.parentDist < 0) st.parentDist = distance (ref, patchFeatures (st.parent, refNote));

        while (! stopFlag && ! st.done)
        {
            Params best = st.parent;
            float bestDist = 1e9f;
            for (int c = 0; c < CHILDREN && ! stopFlag; ++c)
            {
                Params child = st.parent;
                for (int k : netGenes()) child[size_t (k)] = clamp01 (child[size_t (k)] + n (rng) * st.sigma);
                const float d = distance (ref, patchFeatures (child, refNote));
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

    Features ref {};
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
