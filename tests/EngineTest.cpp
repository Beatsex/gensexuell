// Checks the C++ engine against the web UI's genetics, the playing modes, pitch detection and the sound -> seed search.
#include "Analysis.h"
#include "Transcribe.h"
#include "Words.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace seedling;

static int failures = 0;
static void check (bool ok, const char* what)
{
    if (! ok) { std::printf ("FAIL: %s\n", what); ++failures; }
}
static bool near (float a, float b, float tol = 1e-5f) { return std::abs (a - b) < tol; }

// lets a search try `patches` children (at most 5 minutes), so a slow build (sanitizers) is tested on the same amount of work
static void runSearch (Search& search, long patches)
{
    std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
    std::array<bool, Search::STRANDS> done {};
    long tested = 0;
    search.resume();
    for (int i = 0; i < 3000 && tested < patches && ! std::all_of (done.begin(), done.end(), [] (bool d) { return d; }); ++i)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        search.snapshot (sol, done, tested);
    }
    search.pause();
}

static Controls defaults()
{
    Controls c;
    c.base = seedParams (0x5EED1E55u, 20);
    c.len.fill (0.6f);
    return c;
}

// render through the engine with a note held for `hold` blocks, return mono output
static std::vector<float> play (Engine& e, const Genome& g, const Controls& c, const Settings& s, int blocks)
{
    std::vector<float> out, L (256), R (256);
    for (int b = 0; b < blocks; ++b)
    {
        std::fill (L.begin(), L.end(), 0.0f);
        std::fill (R.begin(), R.end(), 0.0f);
        e.render (L.data(), R.data(), 256, c, s);
        for (int i = 0; i < 256; ++i) out.push_back (0.5f * (L[size_t (i)] + R[size_t (i)]));
    }
    return out;
}

int main()
{
    // ---- genetics match index.html (reference values taken from the page)
    const Genome g = defaultGenome (0x5EED1E55u);
    const Params base = seedParams (0x5EED1E55u, 20);
    check (near (base[waveA], 0.9050339f) && near (base[ratioB], 0.5f) && near (base[lfoAmt], 0.1950235f), "seedParams (first genes) matches JS");
    check (near (base[drive], 0.3845241f) && near (base[spread], 0.4986768f) && near (base[envLoop], 1.0f), "seedParams (new genes) matches JS");
    check (near (g.dir[0][waveA], -0.8256038f) && near (g.dir[0][spread], -0.4142749f), "dirsFor matches JS");
    check (near (g.off[11], -0.1063488f), "branch offsets match JS");
    check (near (freshLens (0x5EED1E55u)[1], 0.2702775f), "freshLens matches JS");

    // ---- wave morph: sine 0, saw 0.57, square 0.81, pulse 1 (width 12.5 %)
    {
        float sn, cs;
        Tables::harmonic (0, 1, sn, cs);                check (near (sn, 1) && near (cs, 0), "morph 0 is a sine");
        Tables::harmonic (0, 2, sn, cs);                check (near (sn, 0), "sine has no 2nd harmonic");
        Tables::harmonic (Tables::SAW, 2, sn, cs);      check (near (sn, -1 / PI) && near (cs, 0), "morph 0.57 is a saw");
        Tables::harmonic (Tables::SQUARE, 2, sn, cs);   check (near (sn, 0) && near (cs, 0), "square has no even harmonics");
        Tables::harmonic (Tables::SQUARE, 3, sn, cs);   check (near (sn, 4 / (3 * PI)), "morph 0.81 is a square");
        Tables::harmonic (Tables::SQUARE + 1e-4f, 3, sn, cs);
        check (near (sn, 4 / (3 * PI), 1e-3f) && near (cs, 0, 1e-2f), "pulse starts where the square ends");
        const auto high = [] (float morph) // fraction of the cycle above zero (= pulse width)
        {
            const float* t = Tables::get().table (morph, 20, 48000);
            int n = 0;
            for (int s = 0; s < Tables::SIZE; ++s) n += t[s] > 0;
            return n / float (Tables::SIZE);
        };
        check (near (high (Tables::SQUARE), 0.5f, 0.02f), "square is 50 % high");
        check (near (high (1), Tables::PULSE_MIN, 0.02f), "morph 1 is a 12.5 % pulse");
        const auto rms = [] (float morph)
        {
            const float* t = Tables::get().table (morph, 20, 48000);
            float s = 0;
            for (int i = 0; i < Tables::SIZE; ++i) s += t[i] * t[i];
            return std::sqrt (s / Tables::SIZE);
        };
        check (near (rms (1), rms (Tables::SQUARE), 0.01f) && near (rms (0.9f), rms (Tables::SQUARE), 0.01f), "the pulse keeps the square's level");
    }

    Controls c = defaults();
    c.len.fill (0);
    check (branchParams (g, c, 3) == c.base, "length 0 equals seed");
    c.len.fill (1);
    for (int i = 0; i < NB; ++i)
        for (float v : branchParams (g, c, i)) check (v >= 0 && v <= 1, "branch params in range");
    c.atonality = 0;
    {
        auto p = branchParams (g, c, 5);
        check (p[space] == c.base[space] && p[bass] == c.base[bass], "master genes come from the seed");
        Genome locked = g;
        locked.lock[cutoff] = locked.lock[ratioB] = true;
        const auto q = branchParams (locked, c, 5);
        check (q[cutoff] == c.base[cutoff] && q[ratioB] == c.base[ratioB] && q[reso] == p[reso], "a locked gene plays the seed's value on every branch");
    }

    // ---- bulb modes
    c = defaults();
    std::array<int, NB> brs {};
    check (branchesFor (62, 100, g, c, brs) == 1 && brs[0] == 2, "standard: D plays branch 3");
    c.rotation = 2;
    branchesFor (60, 100, g, c, brs);
    check (brs[0] == 2, "rotation shifts keys to branches");
    c.rotation = 0;
    c.bulbMode = bulbVelocity;
    branchesFor (60, 127, g, c, brs);
    check (brs[0] == 6, "velocity: hardest hit plays the bottom branch");
    c.bulbMode = bulbRanges;
    branchesFor (24, 100, g, c, brs);
    int lowRange = brs[0];
    branchesFor (30, 100, g, c, brs);
    check (lowRange == 0 && brs[0] == 1, "ranges: six semitones per branch");
    c.bulbMode = bulbLayered;
    Genome layered = g;
    layered.layer.fill (false);
    layered.layer[1] = layered.layer[4] = layered.layer[9] = true;
    check (branchesFor (60, 100, layered, c, brs) == 3, "layered: one note plays every active layer");

    Settings s;
    {
        Engine e;
        e.prepare (48000);
        e.noteOn (1, 60, 0.9f, -1, layered, c, s);
        check (e.activeVoices() == 3, "layered note starts one voice per layer");
    }

    // ---- voice modes and glide
    c = defaults();
    c.voiceMode = voiceMono;
    {
        Engine e;
        e.prepare (48000);
        e.noteOn (1, 60, 0.9f, -1, g, c, s);
        e.noteOn (1, 64, 0.9f, -1, g, c, s);
        check (e.activeVoices() == 1, "mono plays one voice");
    }
    c.voiceMode = voiceLegato;
    {
        Engine e;
        e.prepare (48000);
        e.noteOn (1, 60, 0.9f, -1, g, c, s);
        play (e, g, c, s, 20);
        const uint64_t age = e.voices[0].age;
        e.noteOn (1, 67, 0.9f, -1, g, c, s);
        check (e.voices[0].age == age && e.voices[0].note == 67, "legato moves pitch without retriggering");
    }
    {
        // glide: a pure sine patch slides from C4 to C5 and ends at C5
        Params sine = neutralGenes();
        sine[waveA] = 0; sine[mix] = 0; sine[noise] = 0; sine[fm] = 0; sine[sub] = 0.5f; sine[cutoff] = 1; sine[fltEnv] = 0;
        sine[attack] = 0; sine[sustain] = 1; sine[lfoAmt] = 0; sine[pitchMod] = 0.5f; sine[fltType] = 0.5f; sine[drive] = 0.5f;
        Controls cs = defaults();
        cs.base = sine;
        cs.len.fill (0);
        cs.atonality = 0;
        cs.voiceMode = voiceMono;
        cs.glide = 0.2f;
        Engine e;
        e.prepare (48000);
        e.noteOn (1, 60, 0.9f, -1, g, cs, s);
        play (e, g, cs, s, 40);
        e.noteOn (1, 72, 0.9f, -1, g, cs, s);
        const auto early = play (e, g, cs, s, 4);
        play (e, g, cs, s, 200);
        const auto late = play (e, g, cs, s, 40);
        const float fEarly = detectPitch (early.data(), int (early.size()), 48000);
        const float fLate = detectPitch (late.data(), int (late.size()), 48000);
        std::printf ("glide: %.1f Hz -> %.1f Hz\n", fEarly, fLate);
        check (fEarly < 450 && near (fLate, 523.25f, 4), "glide moves from C4 up to C5");
    }
    {
        // pitch split: A by octave + semitones, B relative to A; a sine at C4 (261.63 Hz)
        auto pitchOf = [&] (float octA_, float semiA_, float semiB_, float mixB) {
            Params p = neutralGenes();
            p[waveA] = 0; p[waveB] = 0; p[mix] = mixB; p[noise] = 0; p[fm] = 0; p[sub] = 0.5f; p[cutoff] = 1; p[fltEnv] = 0;
            p[attack] = 0; p[sustain] = 1; p[lfoAmt] = 0; p[fltType] = 0.5f; p[drive] = 0.5f;
            p[octA] = octA_; p[semiA] = semiA_; p[semiB] = semiB_;
            Controls cs = defaults();
            cs.base = p;
            cs.len.fill (0);
            cs.atonality = 0;
            Engine e;
            e.prepare (48000);
            e.noteOn (1, 60, 0.9f, -1, g, cs, s);
            play (e, g, cs, s, 40);
            const auto x = play (e, g, cs, s, 40);
            return detectPitch (x.data(), int (x.size()), 48000);
        };
        const float up = pitchOf (0.75f, 0.5f + 7 / 24.0f, 0.5f, 0), bUp = pitchOf (0.5f, 0.5f, 1, 1);
        std::printf ("pitch split: A +1 oct +7 st %.1f Hz, B +12 st %.1f Hz\n", up, bUp);
        check (near (up, 783.99f, 6) && near (bUp, 523.25f, 4), "octave, semitone and B offsets move the pitch");
    }

    // ---- voice colours outside every seed's range: ring mod, wavefold, stepped LFO; the ranges seeds use stay as they were
    {
        Params v = neutralGenes();
        v[sustain] = 1; v[lfoAmt] = 0.8f;
        const auto render = [] (const Params& x) {
            Controls cv;
            cv.base = x;
            cv.len.fill (0);
            cv.atonality = 0;
            static const Genome g0;
            return renderNote (g0, cv, 0, 48, 0.5f, 24000);
        };
        const auto differs = [] (const std::vector<float>& a, const std::vector<float>& b) {
            double d = 0;
            for (size_t i = 0; i < a.size(); ++i) d += std::abs (a[i] - b[i]);
            return d / double (a.size());
        };
        const auto with = [&] (Gene k, float value) { Params x = v; x[size_t (k)] = value; return render (x); };
        const auto plain = render (v);
        check (differs (with (sub, 0.3f), with (sub, 0.5f)) == 0, "sub 0.3 .. 0.5 is neutral");
        check (differs (with (sub, 0.05f), plain) > 1e-3, "low sub ring-modulates");
        check (differs (with (drive, 0.5f), with (drive, 0.2f)) == 0, "drive below 0.5 is clean");
        check (differs (with (drive, 1), with (drive, 0.8f)) > 1e-3, "high drive folds");
        check (differs (with (lfoShape, 1), with (lfoShape, 0.85f)) > 1e-3, "high lfoShape steps");
        for (const auto& a : { with (sub, 0), with (drive, 1), with (lfoShape, 1) })
        {
            float peak = 0;
            for (float x : a) peak = std::max (peak, std::abs (x));
            check (std::isfinite (peak) && peak > 0.002f && peak < 1, "new colours are finite, audible and below clipping");
        }
    }

    // ---- dynamic feature dimension (G3): the config drives the vector, the defaults stay exact
    {
        const FeatConfig def;
        check (nfeat (def) == NFEAT && featLen (def) == FEAT_LEN, "default config matches the nominal feature size and 2 s window");
        check (int (melBankFor (def).size()) == MEL && int (melBankFor (def)[0].size()) == FFT_N / 2 + 1, "default mel bank shape");

        // a non-default config yields a correctly sized, finite, in-range vector
        FeatConfig small;
        small.melBands = 24; small.frames = 8; small.fftN = 512; small.windowSize = 512;
        static const Genome g0;
        Controls ct;
        ct.base = seedParams (4242, 10);
        ct.atonality = 0;
        const auto audio = renderNote (g0, ct, 0, 60, float (featLen (def)) / FEAT_SR, FEAT_SR, HOLD_SECONDS);
        const auto fSmall = features (audio.data(), int (audio.size()), 60, small);
        check (int (fSmall.size()) == nfeat (small) && int (fSmall.size()) == 24 * 8 + 1, "custom dimension is melBands * frames + 1");
        bool inRange = true;
        for (float v : fSmall) inRange &= std::isfinite (v) && v >= 0.0f && v <= 1.0f;
        check (inRange, "custom features are finite and in range");
        check (distance (fSmall, features (audio.data(), int (audio.size()), 60, small)) == 0, "custom features are deterministic");
        check (distance (fSmall, features (audio.data(), int (audio.size()), 60, def)) == 1.0f, "mismatched feature dimensions are not compared");

        // the shipped net takes the default input and refuses a wrong-sized vector
        Net net;
        std::ifstream in ("Resources/net.bin", std::ios::binary);
        std::vector<char> blob ((std::istreambuf_iterator<char> (in)), {});
        if (! blob.empty() && net.load (blob.data(), blob.size()))
        {
            check (net.inDim() == NFEAT, "loaded net expects the default feature dimension");
            const auto p = net.predict (fSmall), neutral = neutralGenes();
            bool same = true;
            for (int k = 0; k < NP; ++k) same &= near (p[size_t (k)], neutral[size_t (k)]);
            check (same, "wrong-sized input falls back to neutral genes");
        }

        // a net of the wrong input width loads but only guesses neutral
        auto tinyNet = [] (int32_t inputs, int32_t outputs)
        {
            std::vector<char> b;
            auto put = [&b] (const void* p, size_t n) { auto* c = static_cast<const char*> (p); b.insert (b.end(), c, c + n); };
            const int32_t one = 1;
            const std::vector<float> zeros (size_t (inputs * outputs + outputs));
            put ("SDNN", 4);
            put (&one, 4);
            put (&inputs, 4);
            put (&outputs, 4);
            put (zeros.data(), zeros.size() * 4);
            return b;
        };
        const auto wide = tinyNet (8, int32_t (netGenes().size()));
        Net tiny;
        check (tiny.load (wide.data(), wide.size()) && tiny.ready() && tiny.inDim() == 8, "a net of another input width loads and reports its width");
        const auto guess = tiny.predict (features (audio.data(), int (audio.size()), 60, def)), neutralGuess = neutralGenes();
        bool neutral = true;
        for (int k = 0; k < NP; ++k) neutral &= near (guess[size_t (k)], neutralGuess[size_t (k)]);
        check (neutral, "a net of the wrong input width only guesses neutral");

        // the signature ties a net to its features: a mismatch or a broken file is refused with a reason
        check (featSignature (def) == featSignature (FeatConfig {}) && featSignature (def) != featSignature (small), "the signature follows the feature config");
        const auto refused = [&tiny] (std::vector<char> blob)
        {
            return ! tiny.load (blob.data(), blob.size()) && ! tiny.ready() && tiny.inDim() == 0 && ! tiny.loadError().empty();
        };
        const auto sign = [] (std::vector<char> blob, uint32_t signature)
        {
            blob.insert (blob.end(), { 'S', 'I', 'G', 'N' });
            auto* c = reinterpret_cast<const char*> (&signature);
            blob.insert (blob.end(), c, c + 4);
            return blob;
        };
        const auto good = sign (wide, featSignature (def));
        check (tiny.load (good.data(), good.size()) && tiny.ready() && tiny.loadError().empty(), "a net with the right signature loads");
        check (refused (sign (wide, featSignature (small))), "a net with the wrong signature is refused with a reason");
        check (refused (tinyNet (8, int32_t (netGenes().size()) + 1)), "a wrong gene count is refused with a reason");
        check (refused (std::vector<char> (wide.begin(), wide.end() - 4)), "a truncated net is refused with a reason");
        check (refused (std::vector<char> (good.begin(), good.end() - 1)), "a truncated signature is refused with a reason");
    }

    // ---- pitch detection and pitch bend
    {
        std::vector<float> sine (24000);
        for (size_t i = 0; i < sine.size(); ++i) sine[i] = std::sin (2 * PI * 220 * float (i) / 24000);
        check (near (detectPitch (sine.data(), int (sine.size()), 24000), 220, 1), "pitch detection finds 220 Hz");
    }

    // ---- every branch renders finite, audible, and ends after release; wheel targets change the sound
    for (int i = 0; i < NB; ++i)
    {
        Controls cc = defaults();
        Engine e;
        e.prepare (48000);
        e.noteOn (1, 60 + i, 0.9f, -1, g, cc, s);
        const auto a = play (e, g, cc, s, 90);
        e.noteOff (1, 60 + i, cc);
        play (e, g, cc, s, 1200);
        float peak = 0;
        bool finite = true;
        for (float v : a) { peak = std::max (peak, std::abs (v)); finite &= std::isfinite (v); }
        check (finite, "output is finite");
        check (peak > 0.002f && peak < 1.0f, "branch is audible and below clipping");
        check (e.activeVoices() == 0, "voice ends after release");
    }
    {
        Controls cc = defaults();
        cc.wheelTarget = filterDown;
        cc.wheel = 0;
        const float closed = rms (renderNote (g, cc, 0, 60, 0.5f, 24000));
        cc.wheel = 1;
        const float open = rms (renderNote (g, cc, 0, 60, 0.5f, 24000));
        check (open > closed, "Filter - wheel opens the filter as it goes up");
        cc.wheelTarget = growthUp;
        cc.len.fill (0.1f);
        cc.wheel = 1;
        check (effectiveLen (cc, 0) == 1.0f, "Growth + wheel grows every branch");
    }

    {
        // mod S&H: a pitch sweep from the mod envelope holds its first step instead of falling
        auto pitches = [&] (float sh) {
            Params p = neutralGenes();
            p[waveA] = 0; p[mix] = 0; p[noise] = 0; p[fm] = 0; p[cutoff] = 1; p[fltEnv] = 0; p[attack] = 0; p[sustain] = 1;
            p[lfoAmt] = 0; p[pitchMod] = 1; p[modSH] = sh;
            Controls cs = defaults();
            cs.base = p;
            cs.len.fill (0);
            Engine e;
            e.prepare (48000);
            e.noteOn (1, 60, 0.9f, -1, g, cs, s);
            play (e, g, cs, s, 10);
            const auto a = play (e, g, cs, s, 30);
            play (e, g, cs, s, 20);
            const auto b = play (e, g, cs, s, 30);
            return std::pair { detectPitch (a.data(), int (a.size()), 48000), detectPitch (b.data(), int (b.size()), 48000) };
        };
        const auto [smoothA, smoothB] = pitches (0.5f);
        const auto [heldA, heldB] = pitches (0.61f); // about one step per second
        std::printf ("mod S&H: smooth %.0f -> %.0f Hz, stepped %.0f -> %.0f Hz\n", smoothA, smoothB, heldA, heldB);
        check (smoothB < smoothA * 0.97f && near (heldA, heldB, 2) && heldA > 600, "S&H holds the mod envelope in steps");
    }
    {
        // filter type: band-pass, serial low-pass, parallel low-pass and notch sound different from each other
        Params p = neutralGenes();
        p[waveA] = 0.57f; p[noise] = 0.3f; p[reso] = 0.6f; p[cutoff] = 0.5f; p[fltEnv] = 0; p[attack] = 0; p[sustain] = 1;
        std::array<Features, 4> fs;
        for (int i = 0; i < 4; ++i) { p[fltType] = i / 3.0f; fs[size_t (i)] = patchFeatures (p, 60); }
        float closest = 1e9f;
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j) closest = std::min (closest, distance (fs[size_t (i)], fs[size_t (j)]));
        std::printf ("filter types: closest pair %.3f apart\n", closest);
        check (closest > 0.02f, "the four filter types sound different");
    }

    {
        // anti-aliasing: a driven saw at A6 (1760 Hz) should keep its energy on the harmonics
        Params p = neutralGenes();
        p[waveA] = 0.57f; p[mix] = 0; p[noise] = 0; p[fm] = 0; p[cutoff] = 1; p[fltEnv] = 0; p[attack] = 0; p[sustain] = 1;
        p[lfoAmt] = 0; p[drive] = 0.8f; // 1x: 23.5 dB, 2x: 31.8 dB
        Controls cs = defaults();
        cs.base = p;
        cs.len.fill (0);
        Voice v;
        uint32_t rng = 1;
        v.start (93, 0, 0, 0.9f, g, cs, s, 48000, rng, -1, 0);
        const int N = 4800; // 10 Hz bins: every harmonic of 1760 Hz sits on a bin
        std::vector<float> L (N * 2), R (N * 2);
        v.render (L.data(), R.data(), N * 2, rng, Live {});
        const float* x = L.data() + N;
        double all = 0, harm = 0;
        for (int i = 0; i < N; ++i) all += double (x[i]) * x[i];
        for (int k = 0; k * 1760 < 24000; ++k)
        {
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i) { const double a = 2 * PI * k * 176.0 * i / N; re += x[i] * std::cos (a); im -= x[i] * std::sin (a); }
            harm += (k == 0 ? 1.0 : 2.0) * (re * re + im * im) / N;
        }
        const double aliasDb = 10 * std::log10 (std::max (all - harm, 1e-12) / all);
        std::printf ("aliasing: %.1f dB below the harmonics (driven saw at 1760 Hz)\n", -aliasDb);
        check (aliasDb < -28, "a driven saw on a high note keeps its aliases low (oversampled voice)");

        // the same undriven saw at A3 bent up four octaves to A7 (3520 Hz): the table must follow the bend
        p[drive] = 0;
        cs.base = p;
        Voice vb;
        rng = 1;
        vb.start (57, 0, 0, 0.9f, g, cs, s, 48000, rng, -1, 0);
        Live bent;
        bent.bendSemis = 48;
        std::fill (L.begin(), L.end(), 0.0f);
        vb.render (L.data(), R.data(), N * 2, rng, bent);
        double allB = 0, harmB = 0;
        for (int i = 0; i < N; ++i) allB += double (x[i]) * x[i];
        for (int k = 0; k * 3520 < 24000; ++k)
        {
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i) { const double a = 2 * PI * k * 352.0 * i / N; re += x[i] * std::cos (a); im -= x[i] * std::sin (a); }
            harmB += (k == 0 ? 1.0 : 2.0) * (re * re + im * im) / N;
        }
        const double bentDb = 10 * std::log10 (std::max (allB - harmB, 1e-12) / allB);
        std::printf ("aliasing: %.1f dB below the harmonics (saw bent up 4 octaves to 3520 Hz)\n", -bentDb);
        check (bentDb < -40, "a saw bent far up picks a table for its real pitch");
    }

    {
        // MPE: a note channel bends only its own note, the master channel bends all of them
        Params p = neutralGenes();
        p[waveA] = 0; p[mix] = 0; p[noise] = 0; p[fm] = 0; p[sub] = 0.5f; p[cutoff] = 1; p[fltEnv] = 0;
        p[attack] = 0; p[sustain] = 1; p[lfoAmt] = 0; p[fltType] = 0.5f; p[drive] = 0.5f;
        Controls cs = defaults();
        cs.base = p;
        cs.len.fill (0);
        cs.atonality = 0;
        Settings ms = s;
        ms.mpe = true;
        ms.mpeRange = 12;
        ms.pbRange = 2;
        auto pitchWith = [&] (int noteCh, int bendCh, float amount) {
            Engine e;
            e.prepare (48000);
            e.pitchBend (bendCh, amount);
            e.noteOn (noteCh, 60, 0.9f, -1, g, cs, ms);
            play (e, g, cs, ms, 40);
            const auto x = play (e, g, cs, ms, 40);
            return detectPitch (x.data(), int (x.size()), 48000);
        };
        const float own = pitchWith (2, 2, 1), other = pitchWith (2, 3, 1), master = pitchWith (2, 1, 1);
        std::printf ("mpe: own channel %.1f Hz, other channel %.1f Hz, master %.1f Hz\n", own, other, master);
        check (near (own, 523.25f, 4) && near (other, 261.63f, 3) && near (master, 293.66f, 3), "MPE bends per note, the master bends all");
    }

    {
        // low CPU mode: no oversampling, same pitch and level (a saw at C4 with some noise)
        Params p = neutralGenes();
        p[waveA] = 0.57f; p[mix] = 0; p[noise] = 0.2f; p[fm] = 0; p[cutoff] = 1; p[fltEnv] = 0; p[attack] = 0; p[sustain] = 1; p[lfoAmt] = 0;
        auto run = [&] (bool low) {
            Controls cs = defaults();
            cs.base = p;
            cs.len.fill (0);
            cs.lowCpu = low;
            Engine e;
            e.prepare (48000);
            e.noteOn (1, 60, 0.9f, -1, g, cs, s);
            play (e, g, cs, s, 20);
            const auto x = play (e, g, cs, s, 40);
            double sum = 0;
            for (float v : x) sum += double (v) * v;
            return std::pair { detectPitch (x.data(), int (x.size()), 48000), float (10 * std::log10 (sum / double (x.size()))) };
        };
        const auto [fHi, dbHi] = run (false);
        const auto [fLo, dbLo] = run (true);
        std::printf ("low cpu: %.1f Hz %.1f dB, oversampled %.1f Hz %.1f dB\n", fLo, dbLo, fHi, dbHi);
        check (near (fLo, 261.63f, 3) && std::abs (dbLo - dbHi) < 1, "low CPU mode keeps pitch and level");
    }

    // ---- voice stealing: with one voice the next note takes it over without a click
    {
        Engine e;
        e.prepare (48000);
        Settings s;
        s.minPoly = 1;
        Controls c = defaults(); // sines without noise or FM, so a step in the envelope stands out from the wave itself
        for (int k : { waveA, waveB, fm, noise, noiseB }) c.base[size_t (k)] = 0;
        e.noteOn (1, 60, 0.8f, -1, g, c, s);
        const auto before = play (e, g, c, s, 40);
        e.noteOn (1, 67, 0.8f, -1, g, c, s);
        const auto after = play (e, g, c, s, 4);
        float steady = 0, jump = std::abs (after[0] - before.back());
        for (size_t i = before.size() / 2; i < before.size(); ++i) steady = std::max (steady, std::abs (before[i] - before[i - 1]));
        for (size_t i = 1; i < 64; ++i) jump = std::max (jump, std::abs (after[i] - after[i - 1]));
        std::printf ("voice steal: largest step %.4f, held note %.4f\n", jump, steady);
        check (e.activeVoices() == 1 && jump < 1.5f * steady, "a stolen voice does not click");
    }

    // ---- sound -> seed
    {
        const auto f1 = patchFeatures (base, 60), f2 = patchFeatures (base, 60);
        check (distance (f1, f2) == 0, "features are deterministic");
        Params other = seedParams (777, 20);
        check (distance (f1, patchFeatures (other, 60)) > 0.02f, "different patches have different features");

        // the net was trained on this sound: speed-ups must not move the features of a fresh note
        {
            std::ifstream ref ("tests/features_ref.txt");
            std::string line;
            std::getline (ref, line); // comment
            Params busy = base;
            busy[drive] = 0.9f; busy[sub] = 0.7f; busy[holdB] = 0.8f; busy[lfoShape] = 0.7f; busy[lfoAmt] = 0.6f;
            // recorded with MSVC: other compilers and maths libraries round sin, pow and exp a little differently,
            // so the match is to float rounding (a changed sound moves features by 1e-2 and more)
            int same = 0;
            float worst = 0;
            for (const auto& p : { base, busy })
            {
                Features want;
                std::string tok;
                std::getline (ref, line);
                for (std::istringstream in (line); in >> tok;) want.push_back (float (std::strtod (tok.c_str(), nullptr)));
                const auto got = patchFeatures (p, 60);
                if (want.size() != got.size()) continue;
                float d = 0;
                for (size_t i = 0; i < got.size(); ++i) d = std::max (d, std::abs (got[i] - want[i]));
                worst = std::max (worst, d);
                same += d < 1e-3f;
            }
            std::printf ("features against the reference: largest difference %g\n", double (worst));
            check (same == 2, "features of fixed patches match the recorded reference");
        }

        // the attack envelope the search compares next to the melogram
        {
            Params pluck = base, slow = base;
            pluck[attack] = 0; pluck[sustain] = 0; pluck[decay] = 0.2f;
            slow[attack] = 0.75f;
            const auto a = patchAudio (pluck, 60), b = patchAudio (slow, 60);
            const Envelope ea = envelope (a.data(), int (a.size())), eb = envelope (b.data(), int (b.size()));
            std::vector<float> late (a.size() + 4800, 0.0f); // the same pluck after 200 ms of silence
            std::copy (a.begin(), a.end(), late.begin() + 4800);
            const float apart = envDistance (ea, eb), shifted = envDistance (ea, envelope (late.data(), int (late.size())));
            std::printf ("attack: pluck vs slow attack %.3f apart, pluck vs the same pluck after silence %.4f\n", apart, shifted);
            check (envDistance (ea, ea) == 0 && apart > 0.1f && shifted < 0.01f, "the attack envelope tells a pluck from a slow attack and ignores leading silence");
        }

        // locked genes keep the current patch's value, and the genes one note cannot tell keep their start (here neutral)
        {
            Search ls;
            ls.seedBase = 99;
            std::bitset<NP> lk;
            lk[cutoff] = lk[waveA] = true;
            Params values = neutralGenes();
            values[cutoff] = 0.123f;
            values[waveA] = 0.81f;
            ls.setLocks (lk, values);
            const auto target = patchAudio (base, 60);
            ls.setReference (target, 60, nullptr);
            runSearch (ls, 120);
            std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
            std::array<bool, Search::STRANDS> done;
            long tested = 0;
            ls.snapshot (sol, done, tested);
            bool kept = tested > 0, fixed = true;
            for (const auto& strand : sol)
                for (const auto& so : strand)
                {
                    kept = kept && so.genes[cutoff] == 0.123f && so.genes[waveA] == 0.81f;
                    fixed = fixed && so.genes[envKey] == 0.5f && so.genes[fltKey] == 0.5f && so.genes[modVel] == 0.5f;
                }
            check (kept, "the search leaves locked genes at the patch's value");
            check (fixed, "the search keeps key tracking and velocity amount where it started");
        }

        // the search must get close to a target rendered by the engine itself
        Net net;
        std::ifstream in ("Resources/net.bin", std::ios::binary);
        std::vector<char> blob ((std::istreambuf_iterator<char> (in)), {});
        const bool haveNet = ! blob.empty() && net.load (blob.data(), blob.size());
        static const Genome g0;
        if (haveNet) // the net's first guess must beat a neutral start on unseen patches
        {
            float netSum = 0, neutralSum = 0;
            for (uint32_t t = 0; t < 30; ++t)
            {
                Controls cn;
                cn.base = seedParams (90000 + t, 20);
                cn.atonality = 0;
                const auto a = renderNote (g0, cn, 0, 60, FEAT_LEN / FEAT_SR, FEAT_SR, HOLD_SECONDS);
                const auto f = features (a.data(), int (a.size()), 60);
                netSum += similarity (distance (f, patchFeatures (net.predict (f), 60)));
                neutralSum += similarity (distance (f, patchFeatures (neutralGenes(), 60)));
            }
            std::printf ("first guess: net %.1f %%, neutral %.1f %% (average of 30 unseen patches)\n", netSum / 30 * 100, neutralSum / 30 * 100);
            check (netSum > neutralSum, "the net's guess beats a neutral start");
        }
        Params target = seedParams (4242, 10);
        Controls ct;
        ct.base = target;
        ct.atonality = 0;
        const auto ref = renderNote (g0, ct, 0, 60, FEAT_LEN / FEAT_SR, FEAT_SR, HOLD_SECONDS);
        Search search;
        search.setReference (ref, 60, haveNet ? &net : nullptr);
        std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
        std::array<bool, Search::STRANDS> done {};
        long tested = 0;
        search.snapshot (sol, done, tested);
        runSearch (search, 1100);
        search.snapshot (sol, done, tested);
        float best = 0;
        for (auto& st : sol) for (auto& x : st) best = std::max (best, x.score);
        const float randomScore = similarity (distance (features (ref.data(), int (ref.size()), 60), patchFeatures (seedParams (99, 20), 60)));
        std::printf ("search: net %s, %ld patches tried, best match %.1f %% (a random patch: %.1f %%)\n", haveNet ? "on" : "off", tested, best * 100, randomScore * 100);
        check (best > randomScore + 0.1f && best > 0.75f, "search gets close to the target");
    }

    // ---- a custom feature config through the whole chain (G7): signed net, reference, guess, children
    {
        FeatConfig cfg;
        cfg.melBands = 24;
        cfg.frames = 8;
        std::vector<char> blob;
        auto put = [&blob] (const void* p, size_t n) { auto* c = static_cast<const char*> (p); blob.insert (blob.end(), c, c + n); };
        const int32_t one = 1, inputs = nfeat (cfg), outputs = int32_t (netGenes().size());
        const uint32_t signature = featSignature (cfg);
        const std::vector<float> zeros (size_t (inputs * outputs + outputs));
        put ("SDNN", 4);
        put (&one, 4);
        put (&inputs, 4);
        put (&outputs, 4);
        put (zeros.data(), zeros.size() * 4);
        put ("SIGN", 4);
        put (&signature, 4);
        Net net;
        check (! net.load (blob.data(), blob.size()), "a net signed for a custom config is refused by the default one");
        check (net.load (blob.data(), blob.size(), cfg) && net.inDim() == inputs, "a net signed for a custom config loads with it");

        static const Genome g0;
        Controls ct;
        ct.base = seedParams (4242, 10);
        ct.atonality = 0;
        const auto ref = renderNote (g0, ct, 0, 60, FEAT_LEN / FEAT_SR, FEAT_SR, HOLD_SECONDS);
        Search search;
        search.cfg = cfg;
        search.setReference (ref, 60, &net);
        runSearch (search, 380);
        std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
        std::array<bool, Search::STRANDS> done {};
        long tested = 0;
        search.snapshot (sol, done, tested);
        float best = 0;
        bool finite = true;
        for (auto& st : sol) for (auto& x : st) { finite &= std::isfinite (x.score); best = std::max (best, x.score); }
        const float randomScore = similarity (distance (features (ref.data(), int (ref.size()), 60, cfg), patchFeatures (seedParams (99, 20), 60, 0, cfg)));
        std::printf ("custom config search: %d features, %ld patches tried, best match %.1f %% (a random patch: %.1f %%)\n", inputs, tested, best * 100, randomScore * 100);
        check (tested > 0 && finite, "a custom config search runs and every score is finite");
        check (best > randomScore, "a custom config search beats a random patch");
    }

    // ---- text -> seed
    {
        const Wish dark = parseWish ("dunkler Bass"), bright = parseWish ("heller pluck");
        check (dark.weight[dBright] > 0 && dark.target[dBright] < bright.target[dBright], "dunkel/hell set brightness targets");
        check (dark.note == 36 && bright.note == 60, "categories set the register");
        check (parseWish ("nicht hell").target[dBright] < parseWish ("hell").target[dBright], "negation flips a target");
        const Wish g1 = parseWish ("glassy"), g2 = parseWish ("glasig");
        check (g1.target == g2.target && g1.weight == g2.weight, "English and German synonyms agree");
        check (parseWish ("waermes pad").known.size() == 2, "umlaut-free spelling and typos are understood");
        check (parseWish ("wärmeres, dunkleres Pad").known.size() == 3, "umlauts and German inflections are understood");
        check (parseWish ("langsam").weight[dAttack] > 0 && parseWish ("langsam").weight[dLength] == 0, "the longest stem wins");
        check (parseWish ("xyzzy plonk").unknown.size() == 2 && ! parseWish ("xyzzy").any(), "unknown words are reported");
        check (parseWish ("wärmeres Pad mit viel Raum").unknown.empty(), "the manual's example has no unknown words");
        check (parseWish ("viel Raum").genes.front().lo > parseWish ("Raum").genes.front().lo, "viel raises a reverb prior");
        check (parseWish ("wenig Raum").genes.front().hi <= parseWish ("Raum").genes.front().lo, "wenig lowers a reverb prior");
        check (parseWish ("ohne Hall").any() && parseWish ("ohne Hall").genes.front().hi <= 0.4f, "negated reverb sets a low space prior");
        check (parseWish ("Pad ohne Hall").target[dLength] == parseWish ("Pad").target[dLength], "reverb words leave the envelope alone");
        check (parseWish ("nicht hohl").genes.empty(), "only amount genes are inverted");
        const float notTooBright = parseWish ("not too bright").target[dBright];
        check (notTooBright > 0.35f && notTooBright < 0.7f, "not too bright stays moderate");
        check (parseWish ("too bright").weight[dBright] > parseWish ("bright").weight[dBright], "too bright weighs more");
        check (parseWish ("hoher alter CS-80").known.size() == 3, "3-letter stems inflect and hyphenated names match");
        check (parseWish ("pads subs hats").known.size() == 3, "3-letter stems take a plural s");
        check (parseWish ("808-kick").known.size() == 2 && parseWish ("analog-pad").known.size() == 2, "hyphenated compounds split");
        check (parseWish ("hi-hat").known.size() == 1, "hyphenated words stay whole");
        check (parseWish ("nicht xyzzy, warm").target[dBright] == parseWish ("warm").target[dBright], "a comma ends a negation");
        check (parseWish ("sehr xyzzy, warm").weight[dBright] == parseWish ("warm").weight[dBright], "a comma ends a scale");
        check (parseWish ("supersaw").weight[dBright] > 0 && parseWish ("hypersaw").weight[dBright] > 0, "a longer word wins over a modifier inside it");
        check (parseWish ("sanft").weight[dBright] > 0 && parseWish ("Chorus").note == 60 && parseWish ("Chorus").weight[dSustain] == 0, "sanft and chorus keep their own meaning");

        const Wish w = parseWish ("dunkler, weicher Bass");
        const auto takes = textTakes (w, 1234);
        float randomFit = 0;
        for (uint32_t r = 0; r < 20; ++r) randomFit += clamp01 (1 - wishDistance (w, describePatch (seedParams (5000 + r, 20), w.note)) / 0.35f);
        randomFit /= 20;
        std::printf ("text -> seed: %zu takes, best fit %.0f %% (random seeds %.0f %%)\n", takes.size(), takes.empty() ? 0.0f : takes[0].fit * 100, randomFit * 100);
        check (takes.size() == 4, "four takes");
        check (! takes.empty() && takes[0].fit > randomFit + 0.1f, "takes fit the description better than random seeds");
        const auto d = describePatch (takes[0].genes, w.note);
        check (d[dBright] < 0.6f && d[dBass] > 0.2f, "a dark bass take is dark and has low end");
    }

    // ---- performance -> seed
    {
        constexpr float sr = FEAT_SR;
        std::vector<float> melody;
        const float notes[] = { 261.63f, 329.63f, 392.0f };
        for (float hz : notes)
        {
            for (int i = 0; i < int (sr * 0.3f); ++i) melody.push_back (0.5f * std::sin (2 * PI * hz * float (i) / sr));
            melody.insert (melody.end(), size_t (sr * 0.1f), 0.0f);
        }
        const auto ev = transcribe (melody.data(), int (melody.size()), sr);
        std::printf ("transcribe: %zu notes", ev.size());
        for (auto& e : ev) std::printf (" [%d @ %.2fs]", e.midi, e.start);
        std::printf ("\n");
        check (ev.size() == 3, "three notes found");
        if (ev.size() == 3)
        {
            check (ev[0].midi == 60 && ev[1].midi == 64 && ev[2].midi == 67, "pitches C4 E4 G4");
            check (std::abs (ev[1].start - 0.4) < 0.02 && std::abs (ev[2].start - 0.8) < 0.02, "onsets within 20 ms");
        }
        const auto out = renderPerformance (ev, melody.size() / double (sr), g, defaults(), 48000);
        check (std::abs (int (out.size() / 2) - int (melody.size() / sr * 48000)) <= 480, "render keeps the input length");

        // live: the tracker follows a sine that jumps from A3 to E4, then stops
        LiveTracker lt;
        lt.prepare (48000);
        std::vector<std::pair<int, double>> ons;
        int offs = 0;
        double now = 0;
        lt.onNote = [&] (int n, float) { ons.push_back ({ n, now }); };
        lt.offNote = [&] (int) { ++offs; };
        std::vector<float> blk (256);
        auto feed = [&] (float hz, double seconds)
        {
            for (int b = 0; b < int (seconds * 48000 / 256); ++b)
            {
                for (int i = 0; i < 256; ++i) blk[size_t (i)] = hz > 0 ? 0.3f * std::sin (float (2 * PI * hz * (now + i / 48000.0))) : 0.0f;
                lt.process (blk.data(), 256, -45, 0);
                now += 256 / 48000.0;
            }
        };
        feed (220, 0.4);
        feed (329.63f, 0.4);
        feed (0, 0.2);
        check (ons.size() == 2 && ons[0].first == 57 && ons[1].first == 64, "live tracker follows A3 then E4");
        if (ons.size() == 2) check (ons[0].second < 0.06 && ons[1].second - 0.4 < 0.06, "live notes start within 60 ms");
        check (offs == 1, "live note stops on silence");

        // live input switched off in the middle of a note, then on again in silence: nothing may sound
        feed (220, 0.4);
        const size_t before = ons.size();
        lt.reset();
        feed (0, 0.2);
        check (ons.size() == before, "after a reset the tracker does not replay the old input");

        // a lone click correlates with nothing: no pitch, and no read before the correlation array
        std::vector<float> click (1024, 0.0f), scratch (1024 / 2 + 2);
        click[0] = 1;
        float strength = 1;
        check (windowPitch (click.data(), 1024, 24000, strength, scratch.data()) == 0 && strength == 0, "a lone click has no pitch");

        // the same key played again without a gap (an accent): the first note's tail must not cut the second one
        std::vector<float> twice;
        for (float amp : { 0.15f, 0.6f })
            for (int i = 0; i < int (sr * 0.4f); ++i) twice.push_back (amp * std::sin (2 * PI * 261.63f * float (i) / sr));
        twice.insert (twice.end(), size_t (sr * 0.2f), 0.0f);
        const auto ev2 = transcribe (twice.data(), int (twice.size()), sr);
        check (ev2.size() == 2 && ev2[0].midi == 60 && ev2[1].midi == 60 && ev2[0].end <= ev2[1].start, "repeated notes do not overlap");
        if (ev2.size() == 2)
        {
            const auto both = renderPerformance (ev2, twice.size() / double (sr), g, defaults(), 48000);
            const auto alone = renderPerformance ({ ev2[1] }, twice.size() / double (sr), g, defaults(), 48000);
            const auto level = [] (const std::vector<float>& x) { double e = 0; for (size_t i = 2 * 33600; i < 2 * 38400; ++i) e += double (x[i]) * x[i]; return e; };
            std::printf ("repeated note: %.1f dB against the note alone (0.70 .. 0.80 s)\n", 10 * std::log10 (level (both) / level (alone)));
            check (level (both) > 0.5 * level (alone), "the second of two repeated notes sounds its full length");
        }
    }

    std::printf (failures ? "%d FAILED\n" : "all checks passed\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
