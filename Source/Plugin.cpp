#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>
#include <PresetData.h>
#include "Analysis.h"
#include "Transcribe.h"
#include "Words.h"
#include "grain/EngineSet.h"
#include "grain/Lowpass.h"
#include "grain/Reverb.h"
#include "retro/Distortion.h"
#include "retro/TapeMachine.h"
#include "retro/FDN.h"

using namespace juce;
using namespace seedling;

namespace
{
// ---------------------------------------------------------------- parameters
// One table for every host parameter and where it lives in the UI's state object `S`:
// top-level S[key], S.len[index] or S.base[key] (genes).
enum class Where { top, len, gene };

struct ParamDef
{
    String id, name;
    Where where;
    String key;
    int index = 0;
    float lo = 0, hi = 1, step = 0.001f, def = 0, skew = 1;
    StringArray choices;
};

const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs = [] {
        std::vector<ParamDef> d;
        auto top = [&] (const char* id, const char* name, float lo, float hi, float step, float def, float skew = 1)
        { d.push_back ({ id, name, Where::top, id, 0, lo, hi, step, def, skew, {} }); };
        auto choice = [&] (const char* id, const char* name, StringArray options, float def = 0)
        { d.push_back ({ id, name, Where::top, id, 0, 0, float (options.size() - 1), 1, def, 1, options }); };

        top ("tuning", "Tuning", -12, 12, 0.1f, 0);
        top ("effect", "Effect", 0, 100, 1, 35);
        top ("volume", "Volume", -24, 6, 0.5f, -8);
        top ("release", "Release", 0, 100, 1, 40);
        top ("atonality", "Atonality", 0, 100, 1, 20);
        top ("velSens", "Velocity", 0, 100, 1, 100);
        top ("glide", "Glide", 0, 10, 0.01f, 0, 0.3f);
        top ("wheel", "Mod Wheel", 0, 1, 0.001f, 0);
        top ("wheelScale", "Wheel Scaling", 0, 100, 1, 100);
        top ("rotation", "Rotation", 0, 11, 1, 0);
        choice ("bulbMode", "Bulb Mode", { "Standard", "Velocity", "Ranges", "Layered" });
        choice ("voiceMode", "Voice Mode", { "Poly", "Mono", "Legato" });
        choice ("wheelTarget", "Wheel Target", { "Growth +", "Growth -", "Filter +", "Filter -", "Env Time +", "Env Time -", "FM", "LFO", "Effect", "Volume" });
        choice ("tempoSync", "Tempo Sync", { "Off", "1/2", "1/4 D", "1/2 T", "1/4", "1/8 D", "1/4 T", "1/8" });
        choice ("fxType", "FX", { "Off", "Mosaic", "Cloud", "Shard", "Tap" });
        choice ("fxVariant", "FX Variant", { "A", "B", "C", "D" });
        top ("fxSwarm", "FX Swarm", 0, 1, 0.001f, 0.5f);
        top ("fxTail", "FX Tail", 0, 1, 0.001f, 0.5f);
        top ("fxSpan", "FX Span", 0, 1, 0.001f, 0.5f);
        top ("fxScatter", "FX Scatter", 0, 1, 0.001f, 0);
        top ("fxMix", "FX Mix", 0, 1, 0.001f, 0.35f);
        // retro effects (drivesexuell, tapesexuell, verbsexuell): saved per patch, not genes
        choice ("fxOrder", "FX Order", { "Drive > Tape > Verb", "Drive > Verb > Tape", "Tape > Drive > Verb", "Tape > Verb > Drive", "Verb > Drive > Tape", "Verb > Tape > Drive" });
        choice ("fxPlace", "FX Place", { "After grain FX", "Before grain FX" });
        choice ("drvOn", "Drive On", { "Off", "On" });
        choice ("drvStyle", "Drive Style", { "A", "E", "N", "T", "P" });
        top ("drvAmt", "Drive Amount", 0, 1, 0.001f, 0.35f);
        top ("drvTone", "Drive Tone", 0, 1, 0.001f, 0.5f);
        top ("drvMix", "Drive Mix", 0, 1, 0.001f, 1);
        choice ("tapeOn", "Tape On", { "Off", "On" });
        choice ("tapeSpeed", "Tape Speed", { "7.5 ips", "15 ips", "30 ips" }, 1);
        top ("tapeDrive", "Tape Drive", 0, 1, 0.001f, 0.35f);
        top ("tapeWow", "Tape Wow", 0, 1, 0.001f, 0.25f);
        top ("tapeHiss", "Tape Hiss", 0, 1, 0.001f, 0.1f);
        top ("tapeMix", "Tape Mix", 0, 1, 0.001f, 1);
        choice ("verbOn", "Verb On", { "Off", "On" });
        choice ("verbMode", "Verb Mode", { "Concert Hall", "Bright Hall", "Dirty Hall", "Chaotic Hall", "Vintage Hall", "Cathedral", "Palace",
                                           "Plate", "Smooth Plate", "Dirty Plate", "Chamber", "Vintage Chamber", "Chaotic Chamber", "Room",
                                           "Smooth Room", "Ambience", "Sanctuary", "Nonlin", "Random Space", "Chorus Space", "Chaotic Neutral" });
        top ("verbDecay", "Verb Decay", 0, 1, 0.001f, 0.5f);
        top ("verbSize", "Verb Size", 0, 1, 0.001f, 0.55f);
        top ("verbPre", "Verb Pre-Delay", 0, 200, 0.1f, 10);
        top ("verbDamp", "Verb Damping", 0, 1, 0.001f, 0.45f);
        top ("verbMix", "Verb Mix", 0, 1, 0.001f, 0.3f);
        choice ("lowCpu", "Low CPU Mode", { "Off", "On" });
        choice ("liveOn", "Live Input", { "Off", "On" });
        top ("liveGate", "Live Gate", -70, -10, 1, -45);
        top ("liveGlide", "Live Glide", 0, 0.5f, 0.001f, 0.03f, 0.5f);
        choice ("liveFollow", "Live Follow Level", { "Off", "On" });
        top ("liveOctave", "Live Octave", -2, 2, 1, 0);

        const auto lens = freshLens (0x5EED1E55u);
        for (int i = 0; i < NB; ++i)
            d.push_back ({ "len" + String (i), "Branch " + String (i + 1), Where::len, "len", i, 0, 1, 0.001f, i == 2 ? 0.75f : lens[size_t (i)], 1, {} });
        const auto genes = seedParams (0x5EED1E55u, 20);
        for (int k = 0; k < NP; ++k)
            d.push_back ({ String ("g_") + KEYS[k], String ("Gene ") + KEYS[k], Where::gene, KEYS[k], k, 0, 1, 0.001f, genes[size_t (k)], 1, {} });
        return d;
    }();
    return defs;
}

AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& p : paramDefs())
    {
        if (! p.choices.isEmpty())
            l.add (std::make_unique<AudioParameterChoice> (ParameterID { p.id, 1 }, p.name, p.choices, int (p.def)));
        else
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { p.id, 1 }, p.name, NormalisableRange<float> (p.lo, p.hi, p.step, p.skew), p.def));
    }
    return l;
}

var readPath (const var& s, const ParamDef& p)
{
    switch (p.where)
    {
        case Where::top: return s[Identifier (p.key)];
        case Where::len: return s["len"][p.index];
        case Where::gene: return s["base"][Identifier (p.key)];
    }
    return {};
}

float velocityCurve (float v, int curve)
{
    switch (curve)
    {
        case 0: return std::pow (v, 1.6f); // low: play harder
        case 2: return std::pow (v, 0.6f); // high
        case 3: return 0.85f;              // fixed
        default: return v;
    }
}

File userDir() { return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Seedling"); }

struct RetroSettings
{
    bool driveOn = false, tapeOn = false, verbOn = false, before = false; // before: ahead of grain FX, Haze and Dusk
    int driveStyle = 0, tapeSpeed = 1, verbMode = 0, order = 0;           // order: one of the six orders of drive, tape, verb
    float drive = 0.35f, driveTone = 0.5f, driveMix = 1, tapeDrive = 0.35f, tapeWow = 0.25f, tapeHiss = 0.1f, tapeMix = 1;
    float verbDecay = 0.5f, verbSize = 0.55f, verbPreMs = 10, verbDamp = 0.45f, verbMix = 0.3f;
};
struct FxSettings { int family = -1, variant = 0; float swarm = 0.5f, tail = 0.5f, span = 0.5f, scatter = 0, mix = 0.35f; RetroSettings retro {}; };

// chorus -> shelves -> grain FX -> Haze -> Dusk -> drive / tape / verb in the chosen order (or before the grain FX) -> volume + soft clip;
// one per output path (live, offline render)
struct MasterChain
{
    void prepare (double sampleRate, int block)
    {
        sr = sampleRate;
        gain.reset (sampleRate, 0.02);
        grain.prepare (sampleRate);
        haze.prepare (sampleRate);
        dusk.prepare (sampleRate);
        dsp::ProcessSpec spec { sampleRate, juce::uint32 (std::max (block, 512)), 2 };
        chorusFx.prepare (spec);
        chorusFx.setRate (0.35f);
        chorusFx.setCentreDelay (9);
        chorusFx.setFeedback (0.1f);
        lowShelf.prepare (spec);
        highShelf.prepare (spec);
        for (auto& f : driveDc) { f.reset(); f.setCutoff (15.0f, sampleRate); }
        for (auto& f : driveTone) f.reset();
        tape.prepare (sampleRate);
        verb.prepare (sampleRate, 0.4f);
        for (auto& p : verbPre) p.prepare (sampleRate, 0.25);
        verbMode = -1;
    }

    // the retro effects, as their plugins run them (fixed: drive low/high cut open, tape bias 0.4 and bump 3 dB, verb width 1)
    void processRetro (float* L, float* R, int n, const RetroSettings& r)
    {
        static constexpr int orders[6][3] { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
        for (int e : orders[std::clamp (r.order, 0, 5)])
        {
            if (e == 0) processDrive (L, R, n, r);
            else if (e == 1) processTape (L, R, n, r);
            else processVerb (L, R, n, r);
        }
    }

    void processDrive (float* L, float* R, int n, const RetroSettings& r)
    {
        if (r.driveOn)
        {
            dist.setStyle (r.driveStyle);
            dist.setDrive (r.drive);
            dist.setPunish (false);
            for (auto& t : driveTone) t.setHighShelf (sr, 800.0f, 0.7f, (r.driveTone - 0.5f) * 24.0f);
            for (int i = 0; i < n; ++i)
            {
                const float wl = driveTone[0].process (driveDc[0].process (dist.process (L[i])));
                const float wr = driveTone[1].process (driveDc[1].process (dist.process (R[i])));
                L[i] += (wl - L[i]) * r.driveMix;
                R[i] += (wr - R[i]) * r.driveMix;
            }
        }
    }

    void processTape (float* L, float* R, int n, const RetroSettings& r)
    {
        if (r.tapeOn)
        {
            tape.setDrive (r.tapeDrive);
            tape.setSpeed (r.tapeSpeed);
            tape.setBias (0.4f);
            tape.setBump (3.0f);
            tape.setWowFlutter (r.tapeWow);
            tape.setNoise (r.tapeHiss);
            for (int i = 0; i < n; ++i)
            {
                float wl = 0, wr = 0;
                tape.process (L[i], R[i], wl, wr);
                L[i] += (wl - L[i]) * r.tapeMix;
                R[i] += (wr - R[i]) * r.tapeMix;
            }
        }
    }

    void processVerb (float* L, float* R, int n, const RetroSettings& r)
    {
        if (r.verbOn)
        {
            if (r.verbMode != verbMode) verb.setMode (verbMode = r.verbMode);
            verb.setDecay (r.verbDecay);
            verb.setSize (r.verbSize);
            verb.setDamping (r.verbDamp);
            verb.setModDepth (0.2f);
            verb.setModRate (0.5f);
            verb.setEarlyDiffusion (0.6f);
            verb.setLateDiffusion (0.5f);
            verb.setAttack (0);
            verb.setHighShelf (-6);
            verb.setBassMult (0.5f);
            const float pre = r.verbPreMs * 0.001f * float (sr);
            for (int i = 0; i < n; ++i)
            {
                const float pl = verbPre[0].readLinear (pre), pr = verbPre[1].readLinear (pre);
                verbPre[0].write (L[i]);
                verbPre[1].write (R[i]);
                float wl = 0, wr = 0;
                verb.process (pl, pr, wl, wr);
                L[i] += (wl - L[i]) * r.verbMix;
                R[i] += (wr - R[i]) * r.verbMix;
            }
        }
    }

    void process (AudioBuffer<float>& buffer, const Controls& c, const FxSettings& f)
    {
        const int n = buffer.getNumSamples();
        const float w = wheelAmount (c);
        const float effect = std::clamp (c.effect + (c.wheelTarget == effectAmount ? w * 100 : 0.0f), 0.0f, 100.0f) / 100;
        const auto& g = c.base;

        if (g[chorus] > 0.5f)
        {
            chorusFx.setMix ((g[chorus] - 0.5f) * 1.6f);
            chorusFx.setDepth (0.25f + (g[chorus] - 0.5f));
            chorusFx.setRate (0.35f * std::exp2 ((g[fxChorus] - 0.5f) * 6)); // FX CHORUS: 0.04 .. 2.8 Hz, 0.35 at 0.5
            dsp::AudioBlock<float> block (buffer);
            chorusFx.process (dsp::ProcessContextReplacing<float> (block));
        }
        if (std::abs (g[bass] - 0.5f) > 0.01f || std::abs (g[treble] - 0.5f) > 0.01f)
        {
            *lowShelf.state = *dsp::IIR::Coefficients<float>::makeLowShelf (sr, 200, 0.7f, Decibels::decibelsToGain ((g[bass] - 0.5f) * 24));
            *highShelf.state = *dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4000, 0.7f, Decibels::decibelsToGain ((g[treble] - 0.5f) * 24));
            dsp::AudioBlock<float> block (buffer);
            lowShelf.process (dsp::ProcessContextReplacing<float> (block));
            highShelf.process (dsp::ProcessContextReplacing<float> (block));
        }

        if (f.retro.before) processRetro (buffer.getWritePointer (0), buffer.getWritePointer (1), n, f.retro);

        grain::dsp::EngineSet::Params fx;
        fx.family = f.family; // -1: every engine disabled (Mix 0, passes the dry signal)
        fx.variant = f.variant;
        fx.swarm = f.swarm;
        fx.tail = std::clamp (f.tail + (g[fxLength] - 0.5f), 0.0f, 1.0f);   // expansion gene
        fx.span = std::clamp (f.span + (g[fxSize] - 0.5f), 0.0f, 1.0f);     // expansion gene
        fx.scatter = f.scatter;
        fx.mix = std::clamp (f.mix + (g[fxMix] - 0.5f), 0.0f, 1.0f);        // expansion gene
        fx.bpm = c.bpm;
        const int hazeMode = std::clamp (int (g[space] * 4), 0, 3); // Room Plate Hall Swell
        const float rvbScale = std::exp2 ((g[rvbSize] - 0.5f) * 2.0f);      // expansion gene: reverb amount (1 at 0.5)
        const float duskScale = std::exp2 ((g[fxDamp] - 0.5f) * 2.0f);      // expansion gene: extra damping (1 at 0.5)
        const float duskHz = g[damp] <= 0.5f ? grain::dsp::Lowpass::bypassHz : 19900.0f * std::exp2 (-(g[damp] - 0.5f) * 14) * duskScale;
        for (int pos = 0; pos < n; pos += 512)
        {
            const int m = std::min (512, n - pos);
            float* chans[2] { buffer.getWritePointer (0) + pos, buffer.getWritePointer (1) + pos };
            grain.process (chans, 2, m, fx);
            haze.process (chans, 2, m, effect * rvbScale, hazeMode);
            dusk.process (chans, 2, m, duskHz);
        }
        if (! f.retro.before) processRetro (buffer.getWritePointer (0), buffer.getWritePointer (1), n, f.retro);

        const float vol = c.volume + (c.wheelTarget == volumeAmount ? w * 12 : 0.0f);
        gain.setTargetValue (Decibels::decibelsToGain (vol) * 4);
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float k = gain.getNextValue();
            L[i] = std::tanh (L[i] * k); // soft clipper
            R[i] = std::tanh (R[i] * k);
        }
    }

    double sr = 48000;
    SmoothedValue<float> gain;
    grain::dsp::EngineSet grain;
    grain::dsp::Reverb haze;
    grain::dsp::Lowpass dusk;
    dsp::Chorus<float> chorusFx;
    dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>> lowShelf, highShelf;
    retro::Distortion dist;
    retro::OnePoleHP driveDc[2];
    retro::Biquad driveTone[2];
    retro::TapeMachine tape;
    retro::FDN verb;
    retro::DelayLine verbPre[2];
    int verbMode = -1;
};

// base64 of little-endian int16 -> floats
std::vector<float> decodePcm16 (const String& b64)
{
    MemoryOutputStream mo;
    Base64::convertFromBase64 (mo, b64);
    const auto* p = static_cast<const int16_t*> (mo.getData());
    std::vector<float> out (mo.getDataSize() / 2);
    for (size_t i = 0; i < out.size(); ++i) out[i] = p[i] / 32768.0f;
    return out;
}

var fileResult (const File& f)
{
    if (! f.existsAsFile()) return {};
    DynamicObject::Ptr o = new DynamicObject();
    o->setProperty ("path", f.getFullPathName());
    o->setProperty ("name", f.getFileNameWithoutExtension());
    o->setProperty ("json", f.loadFileAsString());
    return var (o.get());
}
} // namespace

//==============================================================================
class SeedlingProcessor : public AudioProcessor, private AsyncUpdater
{
public:
    SeedlingProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)
                                           .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
          apvts (*this, nullptr, "params", makeLayout())
    {
        genome = audioGenome = defaultGenome (0x5EED1E55u);
        for (auto& p : paramDefs()) raw.push_back (apvts.getRawParameterValue (p.id));
        cc.fill (-1);
       #if SEEDLING_HAS_NET
        net.load (BinaryData::net_bin, size_t (BinaryData::net_binSize), search.cfg);
       #endif
        if (const var m = JSON::parse (userDir().getChildFile ("midi.json")); m.isObject()) // "make default" from MIDI settings
        {
            defaultMidi = m;
            applySettings (m);
        }
    }

    ~SeedlingProcessor() override { search.pause(); }

    // ------------------------------------------------------------ audio
    void prepareToPlay (double sampleRate, int block) override
    {
        sr = sampleRate;
        engine.prepare (float (sampleRate));
        master.prepare (sampleRate, block);
        tracker.prepare (sampleRate);
        sideIn.assign (size_t (std::max (block, 4096)), 0.0f);
        follow = 0;
        auditionPos = -1;
    }

    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet();
        return l.getMainOutputChannelSet() == AudioChannelSet::stereo()
            && (in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
    }

    float value (const String& id) const { return apvts.getRawParameterValue (id)->load(); }
    float valueAt (size_t i) const { return raw[i]->load(); }

    Controls controls() const
    {
        Controls c;
        const auto& defs = paramDefs();
        for (size_t i = 0; i < defs.size(); ++i)
        {
            const auto& p = defs[i];
            const float v = valueAt (i);
            if (p.where == Where::len) c.len[size_t (p.index)] = v;
            else if (p.where == Where::gene) c.base[size_t (p.index)] = v;
        }
        c.tuning = value ("tuning");
        c.effect = value ("effect");
        c.volume = value ("volume");
        c.release = value ("release");
        c.atonality = value ("atonality");
        c.velSens = value ("velSens");
        c.glide = value ("glide");
        c.wheel = std::max ({ value ("wheel"), midiWheel.load(), pressureWheel.load() });
        c.wheelScale = value ("wheelScale");
        c.rotation = int (value ("rotation"));
        c.bulbMode = int (value ("bulbMode"));
        c.voiceMode = int (value ("voiceMode"));
        c.wheelTarget = int (value ("wheelTarget"));
        c.tempoSync = int (value ("tempoSync"));
        c.lowCpu = value ("lowCpu") > 0.5f;
        if (value ("liveOn") > 0.5f) // a monophonic input plays one legato voice
        {
            c.voiceMode = voiceLegato;
            c.glide = value ("liveGlide");
        }
        return c;
    }

    FxSettings fxSettings() const
    {
        RetroSettings r;
        r.order = int (value ("fxOrder"));
        r.before = value ("fxPlace") > 0.5f;
        r.driveOn = value ("drvOn") > 0.5f;
        r.driveStyle = int (value ("drvStyle"));
        r.drive = value ("drvAmt");
        r.driveTone = value ("drvTone");
        r.driveMix = value ("drvMix");
        r.tapeOn = value ("tapeOn") > 0.5f;
        r.tapeSpeed = int (value ("tapeSpeed"));
        r.tapeDrive = value ("tapeDrive");
        r.tapeWow = value ("tapeWow");
        r.tapeHiss = value ("tapeHiss");
        r.tapeMix = value ("tapeMix");
        r.verbOn = value ("verbOn") > 0.5f;
        r.verbMode = int (value ("verbMode"));
        r.verbDecay = value ("verbDecay");
        r.verbSize = value ("verbSize");
        r.verbPreMs = value ("verbPre");
        r.verbDamp = value ("verbDamp");
        r.verbMix = value ("verbMix");
        return { int (value ("fxType")) - 1, int (value ("fxVariant")), value ("fxSwarm"), value ("fxTail"), value ("fxSpan"), value ("fxScatter"), value ("fxMix"), r };
    }

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi) override
    {
        ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples();
        const bool live = value ("liveOn") > 0.5f;
        float inLevel = 0;
        if (live && getTotalNumInputChannels() > 0) // the sidechain shares the buffer: read it before clearing
        {
            if (int (sideIn.size()) < n) sideIn.resize (size_t (n));
            const int chans = std::min (2, getTotalNumInputChannels());
            for (int i = 0; i < n; ++i)
            {
                float v = 0;
                for (int ch = 0; ch < chans; ++ch) v += buffer.getReadPointer (ch)[i];
                sideIn[size_t (i)] = v / float (chans);
            }
        }
        buffer.clear();

        {
            const SpinLock::ScopedTryLockType lock (stateLock);
            if (lock.isLocked())
            {
                audioGenome = genome;
                audioSettings = settings;
                audioCc = cc;
            }
        }
        auto c = controls();
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition(); pos.hasValue() && pos->getBpm().hasValue())
                c.bpm = float (*pos->getBpm());
        const auto& s = audioSettings;

        // notes played on the UI name their branch; the pitch is the key that branch sits on
        uiEvents.popAll ([&] (const HostNote& h)
        {
            if (h.mask != 0) // on-screen keyboard: a real pitch, played like a MIDI note
            {
                if (h.on) keyOn (UI_CHANNEL, h.note, h.vel, c, s);
                else keyOff (UI_CHANNEL, h.note, c);
                return;
            }
            const int key = 60 + (h.note - c.rotation + NB) % NB;
            if (h.on) engine.noteOn (UI_CHANNEL, key, h.vel, h.note, audioGenome, c, s);
            else engine.noteOff (UI_CHANNEL, key, c);
        });

        if (live && getTotalNumInputChannels() > 0)
        {
            tracker.onNote = [&] (int note, float vel)
            {
                const int prev = tracker.current;
                engine.noteOn (LIVE_CHANNEL, note, vel < 0 ? 0.8f : vel, -1, audioGenome, c, s);
                if (vel < 0 && prev >= 0) engine.noteOff (LIVE_CHANNEL, prev, c); // legato move: drop the old key
                liveNote = note;
            };
            tracker.offNote = [&] (int note) { engine.noteOff (LIVE_CHANNEL, note, c); liveNote = -1; };
            tracker.onBend = [&] (float semis) { engine.pitchBend (LIVE_CHANNEL, std::clamp (semis / std::max (1.0f, s.pbRange), -1.0f, 1.0f)); };
            tracker.process (sideIn.data(), n, value ("liveGate"), int (value ("liveOctave")));
            inLevel = tracker.level;
            liveLevel = inLevel;
        }
        else if (! live && liveNote.load() >= 0)
        {
            engine.allOff();
            liveNote = -1;
        }

        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        int at = 0;
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            const int to = std::clamp (m.samplePosition, at, n);
            engine.render (L + at, R + at, to - at, c, s);
            at = to;
            handleMidi (msg, c, s);
        }
        engine.render (L + at, R + at, n - at, c, s);
        if (live && value ("liveFollow") > 0.5f) // the input's level shapes the synth
        {
            const float target = std::min (2.0f, inLevel / 0.125f);
            for (int i = 0; i < n; ++i)
            {
                follow += (target - follow) * 0.002f;
                L[i] *= follow;
                R[i] *= follow;
            }
        }
        master.process (buffer, c, fxSettings());
        outPeak = std::max (outPeak.load(), buffer.getMagnitude (0, buffer.getNumSamples()));
        voiceCount = engine.activeVoices();
        playAudition (L, R, n);
        wheelOut = c.wheel;
    }

    void keyOn (int ch, int note, float vel, const Controls& c, const Settings& s)
    {
        engine.noteOn (ch, note, vel, -1, audioGenome, c, s);
        std::array<int, NB> brs {};
        const int nb = branchesFor (note, int (vel * 127), audioGenome, c, brs);
        int mask = 0;
        for (int i = 0; i < nb; ++i) mask |= 1 << brs[size_t (i)];
        hostNotes.push ({ note, true, mask, vel });
    }

    void keyOff (int ch, int note, const Controls& c)
    {
        engine.noteOff (ch, note, c);
        hostNotes.push ({ note, false, 0, 0 });
    }

    void handleMidi (const MidiMessage& msg, Controls& c, const Settings& s)
    {
        const int ch = msg.getChannel();
        if (! s.mpe && s.inChannel > 0 && ch != s.inChannel) return;
        if (s.mpe && ch >= 2) // MPE note channel: pressure and CC74 belong to that one note
        {
            if (msg.isChannelPressure()) { engine.pressure (ch, msg.getChannelPressureValue() / 127.0f); return; }
            if (msg.isController())
            {
                if (msg.getControllerNumber() == 74) engine.slide (ch, std::clamp ((msg.getControllerValue() - 64) / 63.0f, -1.0f, 1.0f));
                return;
            }
        }

        if (msg.isNoteOn())
        {
            keyOn (ch, msg.getNoteNumber(), velocityCurve (msg.getFloatVelocity(), s.velCurve), c, s);
        }
        else if (msg.isNoteOff())
            keyOff (ch, msg.getNoteNumber(), c);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allOff();
        else if (msg.isPitchWheel())
            engine.pitchBend (ch, (msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isController())
        {
            const int num = msg.getControllerNumber();
            const float v = msg.getControllerValue() / 127.0f;
            if (learning.load() >= 0) // MIDI learn takes the next controller
            {
                learnedCc = num;
                learnedParam = learning.exchange (-1);
                return;
            }
            if (num == 1) midiWheel = v, c.wheel = std::max (c.wheel, v);
            for (size_t i = 0; i < audioCc.size(); ++i)
                if (audioCc[i] == num)
                    if (auto* p = apvts.getParameter (paramDefs()[i].id)) p->setValueNotifyingHost (v);
        }
        else if ((msg.isChannelPressure() || msg.isAftertouch()) && s.pressure < 3)
        {
            const float v = (msg.isChannelPressure() ? msg.getChannelPressureValue() : msg.getAfterTouchValue()) / 127.0f;
            static constexpr float curve[] = { 0.5f, 1.0f, 2.0f };
            pressureWheel = std::pow (v, curve[s.pressure]);
        }
        else if (msg.isProgramChange() && s.programChange)
        {
            pendingProgram = msg.getProgramChangeNumber();
            triggerAsyncUpdate();
        }
    }

    // audition: plays a buffer once through the output (sample reference, performance original or result)
    void playAudition (float* L, float* R, int n)
    {
        const SpinLock::ScopedTryLockType lock (auditionLock);
        if (! lock.isLocked() || auditionPos < 0) return;
        const double step = auditionRate / sr;
        const size_t frames = auditionData.size() / size_t (auditionChannels);
        for (int i = 0; i < n && auditionPos < double (frames - 1); ++i, auditionPos += step)
        {
            const size_t f = size_t (auditionPos) * size_t (auditionChannels);
            L[i] += auditionData[f] * 0.8f;
            R[i] += auditionData[f + size_t (auditionChannels - 1)] * 0.8f;
        }
        if (auditionPos >= double (frames - 1)) auditionPos = -1;
    }

    void audition (std::vector<float> data, int channels, double rate)
    {
        const SpinLock::ScopedLockType lock (auditionLock);
        auditionData = std::move (data);
        auditionChannels = channels;
        auditionRate = rate;
        auditionPos = auditionData.empty() ? -1 : 0;
    }
    void stopAudition() { const SpinLock::ScopedLockType lock (auditionLock); auditionPos = -1; }

    Genome currentGenome()
    {
        const SpinLock::ScopedLockType lock (stateLock);
        return genome;
    }

    // a performance through the current seed and the full master chain, stereo interleaved at 48 kHz
    std::vector<float> renderPerformanceWithMaster (const std::vector<NoteEvent>& notes, double seconds)
    {
        constexpr double rate = 48000;
        Controls c = controls();
        c.wheel = 0;
        auto dry = renderPerformance (notes, seconds + 3, currentGenome(), c, float (rate)); // 3 s for the tail
        MasterChain mc;
        mc.prepare (rate, 480);
        const int total = int (dry.size() / 2);
        AudioBuffer<float> buf (2, 480);
        for (int pos = 0; pos < total; pos += 480)
        {
            const int m = std::min (480, total - pos);
            buf.setSize (2, m, false, false, true);
            for (int i = 0; i < m; ++i)
            {
                buf.setSample (0, i, dry[size_t (pos + i) * 2]);
                buf.setSample (1, i, dry[size_t (pos + i) * 2 + 1]);
            }
            mc.process (buf, c, fxSettings());
            for (int i = 0; i < m; ++i)
            {
                dry[size_t (pos + i) * 2] = buf.getSample (0, i);
                dry[size_t (pos + i) * 2 + 1] = buf.getSample (1, i);
            }
        }
        return dry;
    }

    // ------------------------------------------------------------ state from the UI
    void setValue (const String& id, float v)
    {
        auto* p = apvts.getParameter (id);
        if (p == nullptr) return;
        const float norm = p->convertTo0to1 (v);
        if (std::abs (norm - p->getValue()) < 1e-5f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }

    void applySettings (const var& m)
    {
        Settings s;
        auto num = [&] (const char* k, float def) { return m[k].isVoid() ? def : float (m[k]); };
        s.inChannel = int (num ("inChannel", 0));
        s.velCurve = int (num ("velCurve", 1));
        s.pressure = int (num ("pressure", 3));
        s.minPoly = std::clamp (int (num ("minPoly", 16)), 1, MAXV);
        s.maxPoly = std::clamp (int (num ("maxPoly", 32)), s.minPoly, MAXV);
        s.pbRange = num ("pbRange", 2);
        s.masterTune = std::clamp (num ("masterTune", 440), 340.0f, 540.0f);
        s.pbHeldOnly = bool (m["pbHeldOnly"]);
        s.programChange = m["programChange"].isVoid() || bool (m["programChange"]);
        s.mpe = bool (m["mpe"]);
        s.mpeRange = std::clamp (num ("mpeRange", 48), 0.0f, 96.0f);
        const SpinLock::ScopedLockType lock (stateLock);
        settings = s;
    }

    void setUiState (const var& s)
    {
        uiState = JSON::toString (s, true);
        Genome g;
        if (auto* br = s["br"].getArray(); br != nullptr && br->size() == NB)
            for (int i = 0; i < NB; ++i)
            {
                const var& b = br->getReference (i);
                g.off[size_t (i)] = float (b["off"]);
                if (auto* dir = b["dir"].getDynamicObject())
                    for (int k = 0; k < NP; ++k) g.dir[size_t (i)][size_t (k)] = float (dir->getProperty (KEYS[k]));
            }
        if (auto* gains = s["gain"].getArray(); gains != nullptr && gains->size() == NB)
            for (int i = 0; i < NB; ++i) g.gain[size_t (i)] = std::clamp (float (gains->getReference (i)), 0.0f, 2.0f);
        if (auto* layers = s["layers"].getArray(); layers != nullptr && layers->size() == NB)
            for (int i = 0; i < NB; ++i) g.layer[size_t (i)] = bool (layers->getReference (i));

        std::array<int, 512> map;
        map.fill (-1);
        const auto& defs = paramDefs();
        if (auto* o = s["cc"].getDynamicObject())
            for (size_t i = 0; i < defs.size() && i < map.size(); ++i)
                if (o->hasProperty (defs[i].id)) map[i] = int (o->getProperty (defs[i].id));
        {
            const SpinLock::ScopedLockType lock (stateLock);
            genome = g;
            cc = map;
        }
        if (s["midi"].isObject()) applySettings (s["midi"]);
        for (auto& p : defs)
            if (const var v = readPath (s, p); ! v.isVoid()) setValue (p.id, float (v));
    }

    // parameters as {top, len, base} for the UI
    var paramsVar() const
    {
        DynamicObject::Ptr top = new DynamicObject(), base = new DynamicObject();
        Array<var> len;
        for (auto& p : paramDefs())
        {
            const float v = value (p.id);
            if (p.where == Where::top) top->setProperty (Identifier (p.key), v);
            else if (p.where == Where::len) len.add (v);
            else base->setProperty (Identifier (p.key), v);
        }
        DynamicObject::Ptr o = new DynamicObject();
        o->setProperty ("top", var (top.get()));
        o->setProperty ("len", len);
        o->setProperty ("base", var (base.get()));
        return var (o.get());
    }

    var uiStateVar() const
    {
        var s = JSON::parse (uiState);
        if (! s.isObject() && defaultMidi.isObject()) // first open: hand the UI the saved default MIDI settings
        {
            DynamicObject::Ptr o = new DynamicObject();
            o->setProperty ("midi", defaultMidi);
            return var (o.get());
        }
        return s;
    }

    // program change: copy the stored patch over the current state (runs on the message thread)
    void handleAsyncUpdate() override
    {
        var s = JSON::parse (uiState);
        const int prog = pendingProgram.exchange (-1);
        if (! s.isObject() || prog < 0) return;
        const var patch = s["programs"][prog];
        if (auto* p = patch.getDynamicObject())
        {
            for (auto& [k, v] : p->getProperties()) s.getDynamicObject()->setProperty (k, v);
            setUiState (s);
            uiDirty = true;
        }
    }

    // ------------------------------------------------------------ analysis for the UI
    // one second of a branch (or of the seed) at C4, held for 0.8 s, without the master effects
    std::vector<float> renderBranch (int branch, bool seedOnly)
    {
        Controls c = controls();
        c.wheel = 0;
        if (seedOnly) c.len.fill (0);
        Genome g;
        {
            const SpinLock::ScopedLockType lock (stateLock);
            g = genome;
        }
        return renderNote (g, c, branch, 60, 1.0f, FEAT_SR, 0.8f);
    }

    std::vector<float> soundRef;               // sound -> seed reference at FEAT_SR
    std::vector<float> perfAudio;              // performance -> seed: input at FEAT_SR
    std::vector<NoteEvent> perfNotes;
    std::vector<float> perfResult;             // rendered, stereo interleaved at 48 kHz

    void getStateInformation (MemoryBlock& dest) override
    {
        auto state = apvts.copyState();
        state.setProperty ("ui", uiState, nullptr);
        if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
    }

    void setStateInformation (const void* data, int size) override
    {
        auto xml = getXmlFromBinary (data, size);
        if (xml == nullptr) return;
        auto state = ValueTree::fromXml (*xml);
        apvts.replaceState (state);
        if (const var s = JSON::parse (state["ui"].toString()); s.isObject()) setUiState (s);
        uiDirty = true;
    }

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const String getName() const override { return "gensexuell"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    // ------------------------------------------------------------ shared with the editor
    AudioProcessorValueTreeState apvts;
    Net net;
    Search search;
    std::atomic<float> wheelOut { 0 }, liveLevel { 0 }, outPeak { 0 };
    std::atomic<int> voiceCount { 0 };
    std::atomic<int> liveNote { -1 };
    std::atomic<bool> uiDirty { false };
    std::atomic<int> learning { -1 };
    std::atomic<int> learnedParam { -1 }, learnedCc { -1 }; // set by the audio thread, read by the editor

    struct HostNote { int note; bool on; int mask; float vel; };
    struct NoteFifo // audio thread -> UI timer
    {
        AbstractFifo fifo { 256 };
        std::array<HostNote, 256> items;
        void push (HostNote h)
        {
            const auto w = fifo.write (1);
            if (w.blockSize1 > 0) items[size_t (w.startIndex1)] = h;
        }
        template <typename Fn> void popAll (Fn&& fn)
        {
            const auto r = fifo.read (fifo.getNumReady());
            r.forEach ([&] (int i) { fn (items[size_t (i)]); });
        }
    } hostNotes, uiEvents; // uiEvents: editor -> audio thread

private:
    std::vector<std::atomic<float>*> raw;
    Engine engine;
    Genome genome, audioGenome;
    Settings settings, audioSettings;
    std::array<int, 512> cc {}, audioCc {};
    SpinLock stateLock, auditionLock;
    String uiState;
    var defaultMidi;
    double sr = 48000;
    std::atomic<float> midiWheel { 0 }, pressureWheel { 0 };
    std::atomic<int> pendingProgram { -1 };
    std::vector<float> auditionData, sideIn;
    int auditionChannels = 1;
    double auditionRate = FEAT_SR, auditionPos = -1;
    MasterChain master;
    LiveTracker tracker;
    float follow = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedlingProcessor)
};

//==============================================================================
class SeedlingEditor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int baseW = 1280, baseH = 800;

    explicit SeedlingEditor (SeedlingProcessor& p)
        : AudioProcessorEditor (p), proc (p),
          web (WebBrowserComponent::Options {}
                   .withBackend (WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2 {}
                                                .withUserDataFolder (File::getSpecialLocation (File::tempDirectory).getChildFile ("SeedlingWebView2"))
                                                .withBackgroundColour (Colour (0xffF2F1ED)))
                   .withNativeIntegrationEnabled()
                   .withKeepPageLoadedWhenBrowserIsHidden()
                   .withResourceProvider ([] (const String& url) { return resource (url); })
                   .withEventListener ("ui", [this] (var v) { onUi (v); })
                   .withNativeFunction ("measure", [this] (auto&, auto done) { done (measure()); })
                   .withNativeFunction ("savePatch", [this] (auto& a, auto done) { savePatch (a[0].toString(), a[1].toString(), std::move (done)); })
                   .withNativeFunction ("openPatch", [this] (auto&, auto done) { openFile ("*.seedling", std::move (done)); })
                   .withNativeFunction ("siblingPatch", [] (auto& a, auto done) { done (siblingPatch (a[0].toString(), int (a[1]))); })
                   .withNativeFunction ("presetList", [] (auto&, auto done) { done (presetList()); })
                   .withNativeFunction ("presetLoad", [] (auto& a, auto done) { done (presetLoad (a[0].toString())); })
                   .withNativeFunction ("saveMidi", [this] (auto& a, auto done) { saveMidi (a[0].toString(), bool (a[1]), std::move (done)); })
                   .withNativeFunction ("openMidi", [this] (auto&, auto done) { openFile ("*.seedmidi", std::move (done)); })
                   .withNativeFunction ("learn", [this] (auto& a, auto done) { proc.learning = paramIndex (a[0].toString()); done ({}); })
                   .withNativeFunction ("zoom", [this] (auto& a, auto done) { setZoom (float (a[0])); done ({}); })
                   .withNativeFunction ("genoSet", [this] (auto& a, auto done) { done (genoSet (a[0])); })
                   .withNativeFunction ("genoRun", [this] (auto& a, auto done) { bool (a[0]) ? proc.search.resume() : proc.search.pause(); done ({}); })
                   .withNativeFunction ("genoRestart", [this] (auto& a, auto done) { genoRestart (int (a[0]), bool (a[1])); done ({}); })
                   .withNativeFunction ("genoPoll", [this] (auto&, auto done) { done (genoPoll()); })
                   .withNativeFunction ("genoGet", [this] (auto& a, auto done) { done (genoGet (int (a[0]), int (a[1]))); })
                   .withNativeFunction ("genoPlayRef", [this] (auto&, auto done) { proc.audition (proc.soundRef, 1, FEAT_SR); done ({}); })
                   .withNativeFunction ("textSeed", [this] (auto& a, auto done) { textSeed (a[0].toString(), int (a[1]), std::move (done)); })
                   .withNativeFunction ("perfAnalyze", [this] (auto& a, auto done) { done (perfAnalyze (a[0].toString(), a[1])); })
                   .withNativeFunction ("perfRender", [this] (auto&, auto done) { perfRender (std::move (done)); })
                   .withNativeFunction ("perfMidi", [this] (auto& a, auto done) { done (perfMidi (a[0].toString())); })
                   .withNativeFunction ("audition", [this] (auto& a, auto done) { auditionWhat (a[0].toString()); done ({}); })
                   .withNativeFunction ("dragOut", [this] (auto& a, auto done) { dragOut (a[0].toString()); done ({}); })
                   .withNativeFunction ("reveal", [] (auto& a, auto done) { File (a[0].toString()).revealToUser(); done ({}); }))
    {
        addAndMakeVisible (web);
        web.goToURL (WebBrowserComponent::getResourceProviderRoot());
        setResizable (true, true);
        setResizeLimits (640, 400, 2560, 1600);
        setSize (baseW, baseH);
        startTimerHz (30);
    }

    void resized() override { web.setBounds (getLocalBounds()); }

private:
    static std::optional<WebBrowserComponent::Resource> resource (const String& url)
    {
        // fonts sit at the same relative path the page uses when opened from the repo, so nothing loads from the internet
        const auto make = [] (const char* data, int size, const char* mime)
        {
            const auto* d = reinterpret_cast<const std::byte*> (data);
            return WebBrowserComponent::Resource { { d, d + size }, mime };
        };
        if (url == "/" || url == "/index.html") return make (BinaryData::index_html, BinaryData::index_htmlSize, "text/html");
        if (url == "/fonts/inter.woff2") return make (BinaryData::inter_woff2, BinaryData::inter_woff2Size, "font/woff2");
        if (url == "/fonts/jetbrainsmono.woff2") return make (BinaryData::jetbrainsmono_woff2, BinaryData::jetbrainsmono_woff2Size, "font/woff2");
        return std::nullopt;
    }

    static int paramIndex (const String& id)
    {
        const auto& defs = paramDefs();
        for (size_t i = 0; i < defs.size(); ++i)
            if (defs[i].id == id) return int (i);
        return -1;
    }

    void onUi (const var& v)
    {
        const auto type = v["type"].toString();
        if (type == "hello")
            send ({ { "type", "state" }, { "s", proc.uiStateVar() }, { "p", proc.paramsVar() }, { "net", proc.net.ready() } });
        else if (type == "state")
        {
            proc.setUiState (v["s"]);
            lastSent = JSON::toString (proc.paramsVar(), true);
        }
        else if (type == "key") // n = pitch, from the on-screen keyboard
            proc.uiEvents.push ({ jlimit (0, 127, int (v["n"])), bool (v["on"]), 1, jlimit (0.0f, 1.0f, float (v["v"])) });
        else if (type == "note") // b = branch
            proc.uiEvents.push ({ jlimit (0, NB - 1, int (v["b"])), bool (v["on"]), 0, jlimit (0.0f, 1.0f, float (v["v"])) });
    }

    void send (std::initializer_list<std::pair<const char*, var>> fields)
    {
        auto* o = new DynamicObject();
        for (auto& [k, val] : fields) o->setProperty (k, val);
        web.emitEventIfBrowserIsVisible ("host", var (o));
    }

    void timerCallback() override
    {
        proc.hostNotes.popAll ([this] (const SeedlingProcessor::HostNote& h)
                               { send ({ { "type", "note" }, { "n", h.note }, { "on", h.on }, { "mask", h.mask } }); });
        if (proc.uiDirty.exchange (false))
            send ({ { "type", "state" }, { "s", proc.uiStateVar() }, { "p", proc.paramsVar() }, { "net", proc.net.ready() } });
        // DAW automation or MIDI moved a parameter: push it into the UI
        const var p = proc.paramsVar();
        if (const auto now = JSON::toString (p, true); now != lastSent)
        {
            lastSent = now;
            send ({ { "type", "params" }, { "p", p } });
        }
        if (const float w = proc.wheelOut.load(); std::abs (w - lastWheel) > 0.002f)
            send ({ { "type", "wheel" }, { "w", lastWheel = w } });
        // output meter: fast attack, slow fall
        meterOut = std::max (proc.outPeak.exchange (0.0f), meterOut * 0.85f);
        if (const int v = proc.voiceCount.load(); v != lastVoices || std::abs (meterOut - lastOut) > 0.001f)
            send ({ { "type", "meter" }, { "out", lastOut = meterOut }, { "voices", lastVoices = v } });
        if (const int i = proc.learnedParam.exchange (-1); i >= 0)
            send ({ { "type", "learned" }, { "id", paramDefs()[size_t (i)].id }, { "cc", proc.learnedCc.load() } });
        if (proc.value ("liveOn") > 0.5f || lastLiveNote >= 0)
        {
            const int note = proc.liveNote.load();
            send ({ { "type", "live" }, { "level", proc.liveLevel.load() }, { "note", note }, { "input", proc.getTotalNumInputChannels() > 0 } });
            lastLiveNote = proc.value ("liveOn") > 0.5f ? note : -1;
        }
    }

    // ------------------------------------------------------------ text -> seed (runs off the message thread)
    void textSeed (const String& text, int take, WebBrowserComponent::NativeFunctionCompletion done)
    {
        const std::string t = text.toStdString();
        Thread::launch ([t, take, done]
        {
            const Wish w = parseWish (t);
            const auto takes = textTakes (w, uint32_t (std::hash<std::string> {} (t)) + uint32_t (take) * 7919u);
            DynamicObject::Ptr o = new DynamicObject();
            Array<var> known, unknown, list;
            for (auto& k : w.known) known.add (String (k));
            for (auto& u : w.unknown) unknown.add (String (u));
            for (auto& tk : takes)
            {
                DynamicObject::Ptr g = new DynamicObject(), item = new DynamicObject();
                for (int k = 0; k < NP; ++k) g->setProperty (KEYS[k], tk.genes[size_t (k)]);
                item->setProperty ("genes", var (g.get()));
                item->setProperty ("fit", tk.fit);
                list.add (var (item.get()));
            }
            o->setProperty ("known", known);
            o->setProperty ("unknown", unknown);
            o->setProperty ("note", w.note);
            o->setProperty ("takes", list);
            done (var (o.get()));
        });
    }

    // ------------------------------------------------------------ performance -> seed
    // pcm: base64 int16 mono at FEAT_SR; opts: {smoothing, minNote, keepBends, octave}
    var perfAnalyze (const String& pcm, const var& opts)
    {
        proc.perfAudio = decodePcm16 (pcm);
        TranscribeOptions o;
        if (! opts["smoothing"].isVoid()) o.smoothing = float (opts["smoothing"]);
        if (! opts["minNote"].isVoid()) o.minNote = float (opts["minNote"]);
        o.keepBends = ! opts["keepBends"].isVoid() ? bool (opts["keepBends"]) : true;
        o.octave = int (opts["octave"]);
        proc.perfNotes = transcribe (proc.perfAudio.data(), int (proc.perfAudio.size()), FEAT_SR, o);
        proc.perfResult.clear();
        Array<var> notes;
        for (auto& n : proc.perfNotes)
        {
            DynamicObject::Ptr e = new DynamicObject();
            e->setProperty ("s", n.start);
            e->setProperty ("e", n.end);
            e->setProperty ("m", n.midi);
            e->setProperty ("v", n.velocity);
            notes.add (var (e.get()));
        }
        DynamicObject::Ptr r = new DynamicObject();
        r->setProperty ("notes", notes);
        r->setProperty ("seconds", proc.perfAudio.size() / double (FEAT_SR));
        return var (r.get());
    }

    void perfRender (WebBrowserComponent::NativeFunctionCompletion done)
    {
        if (proc.perfNotes.empty()) return done ({});
        proc.perfResult = proc.renderPerformanceWithMaster (proc.perfNotes, proc.perfAudio.size() / double (FEAT_SR));
        const File f = rendersDir().getNonexistentChildFile ("gensexuell performance", ".wav");
        WavAudioFormat wav;
        if (auto out = f.createOutputStream())
        {
            const auto frames = int (proc.perfResult.size() / 2);
            AudioBuffer<float> buf (2, frames);
            for (int i = 0; i < frames; ++i)
            {
                buf.setSample (0, i, proc.perfResult[size_t (i) * 2]);
                buf.setSample (1, i, proc.perfResult[size_t (i) * 2 + 1]);
            }
            std::unique_ptr<OutputStream> stream (out.release());
            if (auto writer = wav.createWriterFor (stream, AudioFormatWriterOptions {}.withSampleRate (48000).withNumChannels (2).withBitsPerSample (24)))
                writer->writeFromAudioSampleBuffer (buf, 0, frames);
        }
        DynamicObject::Ptr r = new DynamicObject();
        r->setProperty ("path", f.getFullPathName());
        r->setProperty ("seconds", proc.perfResult.size() / 2 / 48000.0);
        done (var (r.get()));
    }

    // MIDI file at 120 bpm; bends are written for a 12-semitone wheel range (set by RPN at the start)
    var perfMidi (const String& name)
    {
        if (proc.perfNotes.empty()) return {};
        constexpr int ppq = 960;
        const auto tick = [] (double sec) { return sec * 2 * ppq; };
        MidiMessageSequence seq;
        seq.addEvent (MidiMessage::tempoMetaEvent (500000), 0);
        for (const auto m : MidiRPNGenerator::generate (1, 0, 12 << 7, false, true)) seq.addEvent (m.getMessage(), 0);
        for (auto& n : proc.perfNotes)
        {
            seq.addEvent (MidiMessage::noteOn (1, n.midi, n.velocity), tick (n.start));
            for (size_t k = 0; k < n.bend.size(); ++k)
                seq.addEvent (MidiMessage::pitchWheel (1, jlimit (0, 16383, 8192 + int (n.bend[k] / 12 * 8191))), tick (n.start + k * 0.01));
            seq.addEvent (MidiMessage::pitchWheel (1, 8192), tick (n.end));
            seq.addEvent (MidiMessage::noteOff (1, n.midi), tick (n.end));
        }
        seq.updateMatchedPairs();
        MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        file.addTrack (seq);
        const File f = rendersDir().getNonexistentChildFile (File::createLegalFileName (name.isEmpty() ? "gensexuell performance" : name), ".mid");
        if (auto out = f.createOutputStream()) file.writeTo (*out);
        return f.getFullPathName();
    }

    void auditionWhat (const String& what)
    {
        if (what == "original") proc.audition (proc.perfAudio, 1, FEAT_SR);
        else if (what == "result") proc.audition (proc.perfResult, 2, 48000);
        else proc.stopAudition();
    }

    // drag a rendered file straight into the DAW (called while the mouse button is still down)
    void dragOut (const String& path)
    {
        if (File (path).existsAsFile())
            DragAndDropContainer::performExternalDragDropOfFiles ({ path }, false, this);
    }

    static File rendersDir()
    {
        auto d = userDir().getChildFile ("Renders");
        d.createDirectory();
        return d;
    }

    // ------------------------------------------------------------ measurements: tuning, loudness, branch levels
    var measure()
    {
        DynamicObject::Ptr o = new DynamicObject();
        const auto seed = proc.renderBranch (0, true); // all lengths 0: the seed itself
        Array<var> rmsList;
        for (int i = 0; i < NB; ++i) rmsList.add (rms (proc.renderBranch (i, false)));
        o->setProperty ("branchRms", rmsList);
        o->setProperty ("rms", rms (seed));
        o->setProperty ("pitch", detectPitch (seed.data(), int (seed.size()), FEAT_SR));
        return var (o.get());
    }

    // ------------------------------------------------------------ files
    void savePatch (const String& json, const String& name, WebBrowserComponent::NativeFunctionCompletion done)
    {
        userDir().createDirectory();
        chooser = std::make_unique<FileChooser> ("Save patch", userDir().getChildFile (File::createLegalFileName (name.isEmpty() ? "gensexuell" : name) + ".seedling"), "*.seedling");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                              [json, done] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == File()) return done ({});
                                  f = f.withFileExtension ("seedling");
                                  f.replaceWithText (json);
                                  done (fileResult (f));
                              });
    }

    void saveMidi (const String& json, bool asDefault, WebBrowserComponent::NativeFunctionCompletion done)
    {
        userDir().createDirectory();
        if (asDefault)
        {
            userDir().getChildFile ("midi.json").replaceWithText (json);
            return done (true);
        }
        chooser = std::make_unique<FileChooser> ("Save MIDI settings", userDir().getChildFile ("MIDI.seedmidi"), "*.seedmidi");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                              [json, done] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != File()) f.withFileExtension ("seedmidi").replaceWithText (json);
                                  done (f != File());
                              });
    }

    void openFile (const String& pattern, WebBrowserComponent::NativeFunctionCompletion done)
    {
        userDir().createDirectory();
        chooser = std::make_unique<FileChooser> ("Open", userDir(), pattern);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [done] (const FileChooser& fc) { done (fileResult (fc.getResult())); });
    }

    // next/previous (or random with step 0) .seedling file next to `path`, or the folder listing with step 99
    static var siblingPatch (const String& path, int step)
    {
        const File cur = File::isAbsolutePath (path) ? File (path) : File(); // a factory preset has no file
        const File dir = cur.existsAsFile() ? cur.getParentDirectory() : userDir();
        auto files = dir.findChildFiles (File::findFiles, false, "*.seedling");
        files.sort();
        if (files.isEmpty()) return {};
        if (step == 99)
        {
            Array<var> list;
            for (auto& f : files) list.add (f.getFullPathName());
            return list;
        }
        if (step == 100) return fileResult (File (path)); // load by path
        int i = files.indexOf (cur);
        i = step == 0 ? Random::getSystemRandom().nextInt (files.size()) : (i < 0 ? 0 : (i + step + files.size()) % files.size());
        return fileResult (files[i]);
    }

    // the preset browser: the factory presets built into the plugin, then the user's .seedling files under Documents/Seedling.
    // A file in a folder named like a category sorts there; a copy of a factory preset (the installer puts them in Presets) is left out
    static var presetList()
    {
        Array<var> list;
        StringArray seen;
        const auto add = [&] (const String& cat, const String& name, const String& key)
        {
            DynamicObject::Ptr o = new DynamicObject();
            o->setProperty ("cat", cat);
            o->setProperty ("name", name);
            o->setProperty ("key", key);
            list.add (var (o.get()));
            seen.add (cat + "/" + name);
        };
        for (int i = 0; i < PresetData::namedResourceListSize; ++i)
        {
            int size = 0;
            const auto* data = PresetData::getNamedResource (PresetData::namedResourceList[i], size);
            const auto p = JSON::parse (String::fromUTF8 (data, size));
            add (p["cat"].toString(), p["name"].toString(), String ("factory:") + PresetData::namedResourceList[i]);
        }
        auto files = userDir().findChildFiles (File::findFiles, true, "*.seedling");
        files.sort();
        for (auto& f : files)
        {
            const auto dir = f.getParentDirectory();
            const String cat = dir == userDir() || dir.getFileName() == "Presets" ? String ("USER") : dir.getFileName().toUpperCase();
            const auto name = f.getFileNameWithoutExtension().toUpperCase();
            if (! seen.contains (cat + "/" + name)) add (cat, name, f.getFullPathName());
        }
        return list;
    }

    // a preset by its key from presetList: "factory:<resource>" or a file path; the key comes back as the patch path
    static var presetLoad (const String& key)
    {
        if (! key.startsWith ("factory:")) return File::isAbsolutePath (key) ? fileResult (File (key)) : var();
        int size = 0;
        const auto* data = PresetData::getNamedResource (key.fromFirstOccurrenceOf (":", false, false).toRawUTF8(), size);
        if (data == nullptr) return {};
        DynamicObject::Ptr o = new DynamicObject();
        o->setProperty ("path", key);
        o->setProperty ("json", String::fromUTF8 (data, size));
        return var (o.get());
    }

    void setZoom (float z)
    {
        z = std::clamp (z, 0.5f, 2.0f);
        setSize (int (baseW * z), int (baseH * z));
    }

    // ------------------------------------------------------------ sound -> seed
    var genoSet (const var& samples)
    {
        std::vector<float> audio;
        if (auto* a = samples.getArray())
            for (auto& v : *a) audio.push_back (float (v));
        audio.resize (std::min (audio.size(), size_t (FEAT_LEN)));
        const float hz = detectPitch (audio.data(), int (audio.size()), FEAT_SR);
        const int note = hz > 0 ? std::clamp (nearestNote (hz), 24, 96) : 60;
        proc.search.setReference (audio, note, &proc.net);
        proc.soundRef = audio;
        DynamicObject::Ptr o = new DynamicObject();
        o->setProperty ("pitch", hz);
        o->setProperty ("note", note);
        o->setProperty ("netHint", netHint());
        return var (o.get());
    }

    // why the search cannot use the net; empty when it can, or when no net was shipped at all
    String netHint() const
    {
        if (! proc.net.ready()) return String (proc.net.loadError());
        return proc.net.inDim() != nfeat (proc.search.cfg) ? "NET DOES NOT FIT THE FEATURES: STRANDS START NEUTRAL" : "";
    }

    void genoRestart (int strand, bool fromCurrent)
    {
        const auto c = proc.controls();
        proc.search.restart (jlimit (0, Search::STRANDS - 1, strand), fromCurrent ? &c.base : nullptr);
    }

    var genoPoll()
    {
        std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
        std::array<bool, Search::STRANDS> done {};
        long tested = 0;
        proc.search.snapshot (sol, done, tested);
        Array<var> strands;
        std::array<Search::Status, Search::STRANDS> st;
        proc.search.status (st);
        for (int s = 0; s < Search::STRANDS; ++s)
        {
            DynamicObject::Ptr o = new DynamicObject();
            Array<var> scores;
            for (auto& x : sol[size_t (s)]) scores.add (x.score);
            o->setProperty ("scores", scores);
            o->setProperty ("done", done[size_t (s)]);
            o->setProperty ("generation", st[size_t (s)].generation);
            o->setProperty ("sigma", st[size_t (s)].sigma);
            strands.add (var (o.get()));
        }
        DynamicObject::Ptr r = new DynamicObject();
        r->setProperty ("strands", strands);
        r->setProperty ("tested", (int64) tested);
        r->setProperty ("running", proc.search.running());
        r->setProperty ("netDim", proc.net.inDim());
        r->setProperty ("featDim", nfeat (proc.search.cfg));
        r->setProperty ("netHint", netHint());
        return var (r.get());
    }

    var genoGet (int strand, int index)
    {
        std::array<std::vector<Search::Solution>, Search::STRANDS> sol;
        std::array<bool, Search::STRANDS> done {};
        long tested = 0;
        proc.search.snapshot (sol, done, tested);
        const auto& list = sol[size_t (jlimit (0, Search::STRANDS - 1, strand))];
        if (index < 0 || index >= int (list.size())) return {};
        DynamicObject::Ptr o = new DynamicObject();
        for (int k = 0; k < NP; ++k) o->setProperty (KEYS[k], list[size_t (index)].genes[size_t (k)]);
        return var (o.get());
    }

    SeedlingProcessor& proc;
    WebBrowserComponent web;
    std::unique_ptr<FileChooser> chooser;
    String lastSent;
    float lastWheel = -1, meterOut = 0, lastOut = -1;
    int lastVoices = -1;
    int lastLiveNote = -1;
};

AudioProcessorEditor* SeedlingProcessor::createEditor() { return new SeedlingEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeedlingProcessor(); }
