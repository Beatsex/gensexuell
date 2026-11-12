// Speed check for the voice engine and the sound -> seed search's inner loop; not part of CI.
// Prints the realtime factor of 32 held voices (normal and low CPU mode) and the time of 50 patchFeatures calls.
#include "Analysis.h"
#include "Transcribe.h"
#include <chrono>
#include <cstdio>

using namespace seedling;

static double seconds (const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
}

static double voices (bool lowCpu)
{
    constexpr float sr = 48000, length = 10;
    const Genome g = defaultGenome (0x5EED1E55u);
    Controls c;
    c.base = seedParams (0x5EED1E55u, 20);
    c.len.fill (0.6f);
    c.lowCpu = lowCpu;
    Settings s;
    s.minPoly = s.maxPoly = MAXV;
    Engine e;
    e.prepare (sr);
    for (int i = 0; i < MAXV; ++i) e.noteOn (1, 36 + i, 0.8f, -1, g, c, s);
    std::vector<float> L (256), R (256);
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < int (sr * length); pos += 256) e.render (L.data(), R.data(), 256, c, s);
    return length / seconds (t0);
}

int main()
{
    std::printf ("32 voices, normal:  %.2fx realtime\n", voices (false));
    std::printf ("32 voices, low CPU: %.2fx realtime\n", voices (true));
    const auto t0 = std::chrono::steady_clock::now();
    float sum = 0;
    for (int i = 0; i < 50; ++i) sum += patchFeatures (seedParams (uint32_t (i + 1), 20), 60)[0];
    std::printf ("50 patchFeatures: %.3f s (%g)\n", seconds (t0), double (sum));

    // pitch: performance -> seed (windowPitch every 10 ms) and detectPitch (sample -> seed, text -> seed)
    std::vector<float> tone (size_t (FEAT_SR * 5));
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.5f * std::sin (2 * PI * (220 + 40 * float (i / 24000)) * float (i) / FEAT_SR);
    const auto t1 = std::chrono::steady_clock::now();
    size_t notes = 0;
    for (int i = 0; i < 5; ++i) notes += transcribe (tone.data(), int (tone.size()), FEAT_SR).size();
    for (int i = 0; i < 200; ++i) sum += detectPitch (tone.data() + i * 100, 12000, FEAT_SR);
    std::printf ("pitch (5 transcribe, 200 detectPitch): %.3f s (%zu, %g)\n", seconds (t1), notes, double (sum));
}
