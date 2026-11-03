// Renders random patches and writes (features, genes) pairs for training the sound -> seed net.
// usage: DatasetGen <out.bin> <count> [threads]
#include "Analysis.h"
#include <cstdio>
#include <fstream>

using namespace seedling;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: DatasetGen <out.bin> <count> [threads]\n"); return 1; }
    const int count = std::atoi (argv[2]);
    const int nThreads = argc > 3 ? std::atoi (argv[3]) : int (std::max (1u, std::thread::hardware_concurrency()));
    const auto& genes = netGenes();
    const int NG = int (genes.size());

    std::vector<float> feats (size_t (count) * NFEAT), targets (size_t (count) * size_t (NG));
    std::atomic<int> next { 0 }, skipped { 0 };
    Tables::get(); // build the wavetables once before the workers start

    auto work = [&] (uint32_t seed) {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> u (0, 1);
        for (int i; (i = next++) < count;)
        {
            for (;;)
            {
                // half the patches look like real seeds, half cover the whole gene space
                const bool uniform = u (rng) < 0.5f;
                Params p = uniform ? neutralGenes() : seedParams (uint32_t (rng()), u (rng) * 100);
                for (int k : genes)
                    if (uniform || u (rng) < 0.15f) p[size_t (k)] = u (rng);
                const int midi = 36 + int (u (rng) * 49);
                static const Genome g;
                Controls c;
                c.base = p;
                c.atonality = 0;
                const auto audio = renderNote (g, c, 0, midi, FEAT_LEN / FEAT_SR, FEAT_SR, HOLD_SECONDS);
                if (rms (audio) < 1e-4f) { ++skipped; continue; } // silent patches teach nothing
                const auto f = features (audio.data(), FEAT_LEN, midi);
                std::copy (f.begin(), f.end(), feats.begin() + ptrdiff_t (i) * NFEAT);
                // values that sound the same get one target, so the net isn't punished for what it can't hear
                Params t = p;
                t[modAttack] = std::max (t[modAttack], 0.5f);
                t[lfoShape] = std::max (t[lfoShape], 0.5f);
                t[lfoDelay] = std::max (t[lfoDelay], 0.5f);
                t[volFade] = std::max (t[volFade], 0.75f);
                t[drive] = std::max (t[drive], 0.5f);
                if (t[sub] >= 0.3f && t[sub] <= 0.5f) t[sub] = 0.4f;
                for (int j = 0; j < NG; ++j) targets[size_t (i) * size_t (NG) + size_t (j)] = t[size_t (genes[size_t (j)])];
                break;
            }
            if (i % 5000 == 0) std::printf ("%d / %d\n", i, count), std::fflush (stdout);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < nThreads; ++t) pool.emplace_back (work, 1234u + uint32_t (t) * 7919u);
    for (auto& t : pool) t.join();

    std::ofstream out (argv[1], std::ios::binary);
    const int32_t header[3] = { count, NFEAT, NG };
    out.write (reinterpret_cast<const char*> (header), sizeof (header));
    out.write (reinterpret_cast<const char*> (feats.data()), std::streamsize (feats.size() * 4));
    out.write (reinterpret_cast<const char*> (targets.data()), std::streamsize (targets.size() * 4));
    std::printf ("wrote %d samples (%d silent patches skipped)\n", count, skipped.load());
    return 0;
}
