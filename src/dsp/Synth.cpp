#include "dsp/Synth.h"

namespace mcl
{

namespace
{
    struct AnySound : juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt){ t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float basicWave (int w, float ph, float dt) noexcept
    {
        switch (w)
        {
            case 0:  return 2.0f * ph - 1.0f - polyBlep (ph, dt);                                   // saw
            case 1:  { auto ph2 = ph + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
                       return (ph < 0.5f ? 1.0f : -1.0f) + polyBlep (ph, dt) - polyBlep (ph2, dt); } // square
            case 2:  return 1.0f - 4.0f * std::abs (ph - 0.5f);                                     // triangle
            default: return std::sin (juce::MathConstants<float>::twoPi * ph);                      // sine
        }
    }

    // Continuous wave position 0..3 = saw -> square -> triangle -> sine
    inline float morphWave (float pos, float ph, float dt) noexcept
    {
        pos = juce::jlimit (0.0f, 3.0f, pos);
        auto a = (int) pos;
        auto frac = pos - (float) a;
        auto s = basicWave (a, ph, dt);
        if (frac > 0.001f && a < 3) s += (basicWave (a + 1, ph, dt) - s) * frac;
        return s;
    }

    inline float hash01 (int a, int b) noexcept
    {
        auto h = (uint32_t) (a * 374761393 + b * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
    }
}

//==============================================================================
class LabSynthVoice : public juce::SynthesiserVoice
{
public:
    LabSynthVoice (LabSynth& o) : owner (o), p (o.p) {}

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }

    void prepare (double sr, int block)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) block, 2 };
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        amp.setSampleRate (sr);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        note = midiNote;
        vel = velocity;
        auto target = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        bool glide = p.glide->get() > 0.001f && owner.lastFreq > 0.0f;
        freq = glide ? owner.lastFreq : target;
        targetFreq = target;
        owner.lastFreq = target;

        // KEY MORPH: each note gets its own colour
        auto m = p.morph->get();
        auto c = ++owner.morphCounter;
        morphCutoff = std::pow (2.0f, (hash01 (c, midiNote) - 0.5f) * 2.0f * m);
        morphWaveOff = (hash01 (c + 7, midiNote) - 0.5f) * 1.2f * m;
        morphDetune = 1.0f + (hash01 (c + 13, midiNote) - 0.5f) * 1.5f * m;
        morphPan = (hash01 (c + 29, midiNote) - 0.5f) * m;

        for (int i = 0; i < maxUnison; ++i)
        {
            phase1[i] = hash01 (c + i, 3);
            phase2[i] = hash01 (c + i, 5);
        }
        lfoPhase = hash01 (c, 11);
        fEnv = 1.0f;

        juce::ADSR::Parameters ap { p.attack->get(), p.decay->get(), p.sustain->get(), p.release->get() };
        amp.setParameters (ap);
        amp.noteOn();
        filter.reset();
        counter = 0;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) amp.noteOff();
        else { amp.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int v) override  { bend = ((float) v - 8192.0f) / 8192.0f * 2.0f; }
    void controllerMoved (int cc, int v) override { if (cc == 1) modWheel = (float) v / 127.0f; }

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;

        const int uni = juce::jlimit (1, maxUnison, (int) std::round (p.unison->get()));
        const float detuneCents = p.detune->get() * 45.0f * morphDetune;
        const float w1 = p.wave1->get() + morphWaveOff, w2 = p.wave2->get() + morphWaveOff;
        const float mix = p.oscMix->get();
        const float semi2 = std::round (p.osc2Semi->get());
        const float noiseAmt = p.noise->get();
        const float drive = p.drive->get();
        const float velAmp = 1.0f - p.velSens->get() + p.velSens->get() * vel;
        const float gain = 0.22f * p.level->get() * velAmp / std::sqrt ((float) uni);
        const float fDecayCoef = std::exp (-1.0f / (float) (sampleRate * juce::jmax (0.01f, p.fDecay->get())));
        const float glideCoef = std::exp (-1.0f / (float) (sampleRate * juce::jmax (0.001f, p.glide->get())));
        const float lfoInc = p.lfoRate->get() / (float) sampleRate;
        const float pitchBendRatio = std::pow (2.0f, bend / 12.0f);

        float detuneRatio[maxUnison], panL[maxUnison], panR[maxUnison];
        for (int u = 0; u < uni; ++u)
        {
            auto spread = uni == 1 ? 0.0f : ((float) u / (float) (uni - 1)) * 2.0f - 1.0f;
            detuneRatio[u] = std::pow (2.0f, spread * detuneCents / 1200.0f);
            auto pan = juce::jlimit (-1.0f, 1.0f, spread * 0.8f + morphPan);
            panL[u] = std::sqrt (0.5f * (1.0f - pan));
            panR[u] = std::sqrt (0.5f * (1.0f + pan));
        }

        auto* L = out.getWritePointer (0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        for (int i = start; i < start + num; ++i)
        {
            freq = targetFreq + glideCoef * (freq - targetFreq);
            lfoPhase += lfoInc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            auto lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
            auto f = freq * pitchBendRatio * std::pow (2.0f, lfo * (p.lfoPitch->get() + modWheel * 0.5f) / 12.0f);

            if ((counter++ & 31) == 0)
            {
                auto keyFactor = std::pow (targetFreq / 261.6f, p.keyTrack->get());
                auto envOct = p.filterEnv->get() * 5.0f * fEnv + vel * 0.6f * p.velSens->get();
                auto lfoOct = lfo * p.lfoCutoff->get() * 2.0f;
                auto fc = p.cutoff->get() * keyFactor * morphCutoff * std::pow (2.0f, envOct + lfoOct);
                filter.setCutoffFrequency (juce::jlimit (30.0f, (float) sampleRate * 0.45f, fc));
                filter.setResonance (0.5f + p.resonance->get() * 9.0f);
            }
            fEnv *= fDecayCoef;

            float sl = 0.0f, sr = 0.0f;
            auto f2 = f * std::pow (2.0f, semi2 / 12.0f);
            for (int u = 0; u < uni; ++u)
            {
                auto dt1 = f * detuneRatio[u] / (float) sampleRate;
                auto dt2 = f2 * detuneRatio[u] / (float) sampleRate;
                auto s = morphWave (w1, phase1[u], dt1) * (1.0f - mix) + morphWave (w2, phase2[u], dt2) * mix;
                phase1[u] += dt1; if (phase1[u] >= 1.0f) phase1[u] -= 1.0f;
                phase2[u] += dt2; if (phase2[u] >= 1.0f) phase2[u] -= 1.0f;
                sl += s * panL[u];
                sr += s * panR[u];
            }
            if (noiseAmt > 0.0f)
            {
                auto n = (rng.nextFloat() * 2.0f - 1.0f) * noiseAmt;
                sl += n; sr += n;
            }

            sl = filter.processSample (0, sl);
            sr = filter.processSample (1, sr);

            if (drive > 0.001f)
            {
                auto d = 1.0f + drive * 8.0f;
                sl = std::tanh (sl * d) / std::sqrt (d);
                sr = std::tanh (sr * d) / std::sqrt (d);
            }

            auto e = amp.getNextSample() * gain;
            L[i] += sl * e;
            if (R) R[i] += sr * e;

            if (! amp.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    static constexpr int maxUnison = 7;
    LabSynth& owner;
    LabSynth::Params& p;
    double sampleRate = 48000.0;
    int note = 60, counter = 0;
    float vel = 1.0f, freq = 440.0f, targetFreq = 440.0f, bend = 0.0f, modWheel = 0.0f;
    float phase1[maxUnison] {}, phase2[maxUnison] {};
    float lfoPhase = 0.0f, fEnv = 1.0f;
    float morphCutoff = 1.0f, morphWaveOff = 0.0f, morphDetune = 1.0f, morphPan = 0.0f;
    juce::ADSR amp;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Random rng;
};

//==============================================================================
Param* LabSynth::add (const juce::String& prefix, const juce::String& name, float mn, float mx, float def,
                      const juce::String& suffix, float skew)
{
    owned.push_back (std::make_unique<Param> (prefix + Instrument::sanitise (name).toLowerCase(), name, mn, mx, def, suffix, skew));
    return owned.back().get();
}

LabSynth::LabSynth (const juce::String& prefix)
{
    p.wave1     = add (prefix, "Wave A", 0.0f, 3.0f, 0.0f);
    p.wave2     = add (prefix, "Wave B", 0.0f, 3.0f, 1.0f);
    p.osc2Semi  = add (prefix, "B Semi", -24.0f, 24.0f, 12.0f, " st");
    p.oscMix    = add (prefix, "A/B Mix", 0.0f, 1.0f, 0.3f);
    p.unison    = add (prefix, "Unison", 1.0f, 7.0f, 3.0f);
    p.detune    = add (prefix, "Detune", 0.0f, 1.0f, 0.25f);
    p.noise     = add (prefix, "Air", 0.0f, 0.5f, 0.0f);
    p.cutoff    = add (prefix, "Cutoff", 40.0f, 18000.0f, 2500.0f, " Hz", 0.3f);
    p.resonance = add (prefix, "Reso", 0.0f, 1.0f, 0.15f);
    p.filterEnv = add (prefix, "Filter Env", 0.0f, 1.0f, 0.2f);
    p.keyTrack  = add (prefix, "Key Track", 0.0f, 1.0f, 0.3f);
    p.attack    = add (prefix, "Attack", 0.001f, 4.0f, 0.01f, " s", 0.4f);
    p.decay     = add (prefix, "Decay", 0.01f, 4.0f, 0.5f, " s", 0.4f);
    p.sustain   = add (prefix, "Sustain", 0.0f, 1.0f, 0.7f);
    p.release   = add (prefix, "Release", 0.01f, 6.0f, 0.5f, " s", 0.4f);
    p.fDecay    = add (prefix, "F.Env Time", 0.01f, 4.0f, 0.4f, " s", 0.4f);
    p.lfoRate   = add (prefix, "LFO Rate", 0.05f, 12.0f, 0.3f, " Hz", 0.4f);
    p.lfoCutoff = add (prefix, "LFO>Filter", 0.0f, 1.0f, 0.1f);
    p.lfoPitch  = add (prefix, "LFO>Pitch", 0.0f, 1.0f, 0.0f);
    p.morph     = add (prefix, "Key Morph", 0.0f, 1.0f, 0.4f);
    p.velSens   = add (prefix, "Velocity", 0.0f, 1.0f, 0.5f);
    p.drive     = add (prefix, "Drive", 0.0f, 1.0f, 0.0f);
    p.glide     = add (prefix, "Glide", 0.0f, 1.0f, 0.0f, " s", 0.5f);
    p.level     = add (prefix, "Level", 0.0f, 1.5f, 0.8f);

    synth.addSound (new AnySound());
    for (int i = 0; i < 16; ++i) synth.addVoice (new LabSynthVoice (*this));
    loadPreset (0);
}

LabSynth::~LabSynth() = default;

ParamList LabSynth::getParams()
{
    ParamList l;
    for (auto& o : owned) l.push_back (o.get());
    return l;
}

void LabSynth::prepare (double sampleRate, int maxBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<LabSynthVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate, maxBlock);
}

void LabSynth::render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo&)
{
    synth.renderNextBlock (out, midi, 0, out.getNumSamples());
}

//==============================================================================
namespace
{
    struct SynthPreset
    {
        const char* name;
        //  wA   wB  semi  mix  uni  det  air   cut    res  fenv  key   att   dec   sus   rel   fdec  lfoR lfoF lfoP morph vel drive glide lvl
        float v[24];
    };

    const SynthPreset synthPresets[] = {
        { "Fred Pad (calma)",   { 0.0f, 0.0f, 12, 0.30f, 5, 0.35f, 0.05f, 1800, 0.15f, 0.15f, 0.3f, 0.80f, 1.5f, 0.80f, 2.5f, 1.20f, 0.20f, 0.25f, 0.02f, 0.50f, 0.5f, 0.10f, 0.00f, 0.8f } },
        { "Glass Keys",         { 2.6f, 3.0f, 12, 0.40f, 2, 0.10f, 0.00f, 4000, 0.10f, 0.30f, 0.3f, 0.005f,0.8f, 0.30f, 0.9f, 0.40f, 0.30f, 0.05f, 0.00f, 0.35f, 0.6f, 0.00f, 0.00f, 0.9f } },
        { "Garage Bass",        { 1.0f, 3.0f,-12, 0.50f, 1, 0.00f, 0.00f,  500, 0.35f, 0.55f, 0.0f, 0.002f,0.25f,0.50f, 0.12f,0.18f, 0.30f, 0.00f, 0.00f, 0.10f, 0.4f, 0.35f, 0.05f, 0.9f } },
        { "Emo Lead",           { 0.0f, 1.0f, 0,  0.25f, 3, 0.20f, 0.00f, 2500, 0.25f, 0.30f, 0.3f, 0.02f, 0.4f, 0.70f, 0.5f, 0.50f, 5.00f, 0.00f, 0.15f, 0.25f, 0.5f, 0.20f, 0.08f, 0.8f } },
        { "Pluck Chops",        { 0.0f, 1.0f, 12, 0.30f, 2, 0.15f, 0.00f,  900, 0.20f, 0.70f, 0.4f, 0.001f,0.25f,0.00f, 0.3f, 0.15f, 0.30f, 0.00f, 0.00f, 0.60f, 0.6f, 0.10f, 0.00f, 0.9f } },
        { "Sub Boom",           { 3.0f, 3.0f, 0,  0.00f, 1, 0.00f, 0.00f,  300, 0.00f, 0.10f, 0.0f, 0.002f,1.5f, 0.00f, 0.4f, 0.20f, 0.30f, 0.00f, 0.00f, 0.00f, 0.3f, 0.50f, 0.10f, 1.0f } },
        { "Supersaw Drop",      { 0.0f, 0.0f, 12, 0.35f, 7, 0.55f, 0.02f, 6000, 0.10f, 0.20f, 0.3f, 0.01f, 0.5f, 0.90f, 0.4f, 0.30f, 0.30f, 0.00f, 0.00f, 0.20f, 0.4f, 0.15f, 0.00f, 0.7f } },
        { "Choir Air",          { 2.0f, 3.0f, 12, 0.40f, 4, 0.30f, 0.20f, 1200, 0.20f, 0.10f, 0.2f, 1.20f, 2.0f, 0.90f, 3.0f, 1.00f, 0.15f, 0.40f, 0.03f, 0.70f, 0.4f, 0.00f, 0.00f, 0.8f } },
        { "Warm Organ Stab",    { 1.5f, 3.0f, 12, 0.50f, 2, 0.08f, 0.00f, 1600, 0.30f, 0.40f, 0.2f, 0.003f,0.3f, 0.40f, 0.25f,0.20f, 0.30f, 0.00f, 0.00f, 0.30f, 0.6f, 0.25f, 0.00f, 0.9f } },
        { "Dreamy Arp",         { 2.3f, 0.0f, 7,  0.30f, 3, 0.20f, 0.03f, 2200, 0.30f, 0.50f, 0.3f, 0.002f,0.35f,0.20f, 0.6f, 0.30f, 0.25f, 0.20f, 0.00f, 0.80f, 0.5f, 0.00f, 0.00f, 0.8f } },
    };
}

juce::StringArray LabSynth::getPresetNames() const
{
    juce::StringArray s;
    for (auto& pr : synthPresets) s.add (pr.name);
    return s;
}

void LabSynth::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) std::size (synthPresets))) return;
    auto& pr = synthPresets[index];
    for (size_t i = 0; i < owned.size() && i < 24; ++i) owned[i]->set (pr.v[i]);
    currentPreset = pr.name;
}

} // namespace mcl
