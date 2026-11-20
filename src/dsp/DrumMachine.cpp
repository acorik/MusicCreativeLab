#include "dsp/DrumMachine.h"

namespace mcl
{

using S = DrumMachine::Sound;

namespace
{
    struct KitPad { S sound; float tune, decay, level; };
    struct Kit { const char* name; KitPad pads[DrumMachine::numPads]; };

    const Kit kits[] = {
        { "UK Garage 2-Step", {
            { S::Kick, 1.0f, 1.0f, 1.0f },  { S::Rim, 1.0f, 1.0f, 0.7f },    { S::Snare, 1.0f, 0.9f, 0.85f }, { S::Clap, 1.0f, 1.0f, 0.9f },
            { S::ClosedHat, 1.0f, 1.0f, 0.6f }, { S::Shaker, 1.0f, 1.0f, 0.5f }, { S::OpenHat, 1.0f, 1.0f, 0.55f }, { S::Perc, 1.0f, 1.0f, 0.6f },
            { S::TomLo, 1.0f, 1.0f, 0.8f }, { S::TomHi, 1.0f, 1.0f, 0.8f }, { S::Snap, 1.0f, 1.0f, 0.8f },  { S::Crash, 1.0f, 1.0f, 0.5f },
            { S::Sub808, 1.0f, 1.0f, 0.9f },{ S::VoxChop, 1.0f, 1.0f, 0.7f },{ S::VoxChop, 1.5f, 1.0f, 0.7f },{ S::VoxChop, 0.75f, 1.0f, 0.7f } } },
        { "Soft Fabric (calma)", {
            { S::Kick, 0.85f, 0.7f, 0.8f }, { S::Rim, 0.8f, 0.8f, 0.5f },   { S::Snare, 0.8f, 1.3f, 0.6f },  { S::Snap, 0.9f, 1.4f, 0.7f },
            { S::Shaker, 1.0f, 1.2f, 0.4f },{ S::ClosedHat, 0.8f, 1.5f, 0.35f },{ S::OpenHat, 0.8f, 1.5f, 0.35f },{ S::Perc, 0.7f, 1.5f, 0.45f },
            { S::TomLo, 0.8f, 1.5f, 0.6f }, { S::TomHi, 0.8f, 1.5f, 0.6f }, { S::Clap, 0.8f, 1.6f, 0.6f },  { S::Crash, 0.7f, 1.8f, 0.35f },
            { S::Sub808, 0.9f, 1.5f, 0.8f },{ S::VoxChop, 0.67f, 1.6f, 0.6f },{ S::VoxChop, 0.89f, 1.6f, 0.6f },{ S::VoxChop, 1.12f, 1.6f, 0.6f } } },
        { "Heavy Drop", {
            { S::Kick, 0.9f, 1.4f, 1.2f },  { S::Sub808, 0.8f, 1.6f, 1.0f }, { S::Snare, 0.9f, 1.2f, 1.0f }, { S::Clap, 0.9f, 1.3f, 1.0f },
            { S::ClosedHat, 1.1f, 0.8f, 0.7f },{ S::Shaker, 1.2f, 0.8f, 0.6f },{ S::OpenHat, 1.1f, 1.2f, 0.6f },{ S::Perc, 1.2f, 0.8f, 0.7f },
            { S::TomLo, 0.7f, 1.4f, 0.9f }, { S::TomHi, 0.9f, 1.3f, 0.9f }, { S::Snap, 1.0f, 1.2f, 0.9f },  { S::Crash, 0.9f, 1.4f, 0.6f },
            { S::Kick, 0.7f, 2.0f, 1.0f },  { S::VoxChop, 1.0f, 0.8f, 0.8f },{ S::VoxChop, 1.33f, 0.8f, 0.8f },{ S::VoxChop, 0.5f, 0.8f, 0.8f } } },
    };
}

juce::String DrumMachine::soundName (Sound s)
{
    static const char* names[] = { "Kick", "808", "Snare", "Clap", "Hat", "Open Hat", "Rim", "Tom Lo", "Tom Hi",
                                   "Shaker", "Perc", "Crash", "Snap", "Vox Chop" };
    return names[juce::jlimit (0, (int) Sound::NumSounds - 1, (int) s)];
}

juce::String DrumMachine::getPadName (int pad) const
{
    auto& p = pads[(size_t) pad];
    if (p.sample != nullptr) return p.sampleFile.getFileNameWithoutExtension();
    return soundName (p.sound);
}

DrumMachine::DrumMachine (const juce::String& prefix)
{
    for (int i = 0; i < numPads; ++i)
    {
        auto pn = "Pad" + juce::String (i + 1) + " ";
        auto mk = [&] (const juce::String& n, float mn, float mx, float def, float skew = 1.0f)
        {
            owned.push_back (std::make_unique<Param> (prefix + sanitise (pn + n).toLowerCase(), pn + n, mn, mx, def, juce::String(), skew));
            return owned.back().get();
        };
        pads[(size_t) i].level = mk ("Level", 0.0f, 1.5f, 0.8f);
        pads[(size_t) i].tune  = mk ("Tune", 0.25f, 4.0f, 1.0f, 0.5f);
        pads[(size_t) i].decay = mk ("Decay", 0.1f, 4.0f, 1.0f, 0.5f);
        pads[(size_t) i].pan   = mk ("Pan", -1.0f, 1.0f, 0.0f);
    }
    owned.push_back (std::make_unique<Param> (prefix + "punch", "Punch", 0.0f, 1.0f, 0.3f));
    masterPunch = owned.back().get();
    owned.push_back (std::make_unique<Param> (prefix + "level", "Kit Level", 0.0f, 1.5f, 1.0f));
    masterLevel = owned.back().get();
    loadPreset (0);
}

ParamList DrumMachine::getParams()
{
    ParamList l;
    l.push_back (masterPunch);
    l.push_back (masterLevel);
    for (auto& o : owned) if (o.get() != masterPunch && o.get() != masterLevel) l.push_back (o.get());
    return l;
}

juce::StringArray DrumMachine::getPresetNames() const
{
    juce::StringArray s;
    for (auto& k : kits) s.add (k.name);
    return s;
}

void DrumMachine::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) std::size (kits))) return;
    auto& k = kits[index];
    for (int i = 0; i < numPads; ++i)
    {
        auto& p = pads[(size_t) i];
        p.sound = k.pads[i].sound;
        p.tune->set (k.pads[i].tune);
        p.decay->set (k.pads[i].decay);
        p.level->set (k.pads[i].level);
    }
    currentPreset = k.name;
}

juce::ValueTree DrumMachine::getState() const
{
    auto v = Instrument::getState();
    for (int i = 0; i < numPads; ++i)
    {
        v.setProperty ("sound" + juce::String (i), (int) pads[(size_t) i].sound, nullptr);
        if (pads[(size_t) i].sampleFile.existsAsFile())
            v.setProperty ("sample" + juce::String (i), pads[(size_t) i].sampleFile.getFullPathName(), nullptr);
    }
    return v;
}

void DrumMachine::setState (const juce::ValueTree& v)
{
    Instrument::setState (v);
    for (int i = 0; i < numPads; ++i)
    {
        pads[(size_t) i].sound = (Sound) (int) v.getProperty ("sound" + juce::String (i), (int) pads[(size_t) i].sound);
        auto path = v.getProperty ("sample" + juce::String (i)).toString();
        if (path.isNotEmpty()) loadSample (i, juce::File (path));
    }
}

bool DrumMachine::loadSample (int pad, const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return false;

    auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 20.0));
    auto buf = std::make_shared<juce::AudioBuffer<float>> (2, len);
    reader->read (buf.get(), 0, len, 0, true, true);
    if (reader->numChannels == 1) buf->copyFrom (1, 0, *buf, 0, 0, len);

    const juce::SpinLock::ScopedLockType sl (sampleLock);
    pads[(size_t) pad].sample = buf;
    pads[(size_t) pad].sampleRateOfSample = reader->sampleRate;
    pads[(size_t) pad].sampleFile = file;
    return true;
}

void DrumMachine::clearSample (int pad)
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    pads[(size_t) pad].sample.reset();
    pads[(size_t) pad].sampleFile = juce::File();
}

void DrumMachine::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices) v.active = false;
}

void DrumMachine::trigger (int pad, float velocity)
{
    auto& p = pads[(size_t) pad];

    // Closed hat chokes open hat (like a real hi-hat)
    if (p.sound == S::ClosedHat)
        for (auto& v : voices) if (v.active && v.sound == S::OpenHat) v.active = false;

    // Retrigger same pad: reuse its voice, else find a free one
    Voice* target = nullptr;
    for (auto& v : voices) if (v.active && v.pad == pad) { target = &v; break; }
    if (target == nullptr) for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr) target = &voices[0];

    auto& v = *target;
    v.f1.reset(); v.f2.reset();
    v.sample.reset();
    v.samplePos = 0.0; v.sampleStep = 1.0;
    v.phase = v.phase2 = 0.0f;
    v.active = true;
    v.pad = pad;
    v.sound = p.sound;
    v.vel = velocity;
    v.tune = p.tune->get();
    v.decay = p.decay->get();
    v.level = p.level->get();
    v.pan = p.pan->get();
    v.t = 0.0;

    {
        const juce::SpinLock::ScopedTryLockType sl (sampleLock);
        if (sl.isLocked() && p.sample != nullptr)
        {
            v.sample = p.sample;
            v.sampleStep = p.sampleRateOfSample / sampleRate * v.tune;
        }
    }

    auto sr = sampleRate;
    auto setBP = [sr] (juce::IIRFilter& f, double fc, double q) { f.setCoefficients (juce::IIRCoefficients::makeBandPass (sr, juce::jmin (fc, sr * 0.45), q)); };
    auto setHP = [sr] (juce::IIRFilter& f, double fc) { f.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, juce::jmin (fc, sr * 0.45))); };

    switch (v.sound)
    {
        case S::Snare:     setHP (v.f1, 1500.0 * v.tune); break;
        case S::Clap:      setBP (v.f1, 1100.0 * v.tune, 1.2); break;
        case S::ClosedHat:
        case S::OpenHat:   setHP (v.f1, 7000.0 * v.tune); break;
        case S::Shaker:    setBP (v.f1, 6000.0 * v.tune, 1.5); break;
        case S::Crash:     setHP (v.f1, 4500.0 * v.tune); break;
        case S::Snap:      setBP (v.f1, 2600.0 * v.tune, 2.0); break;
        case S::Perc:      setBP (v.f1, 700.0 * v.tune, 3.0); break;
        case S::VoxChop:   setBP (v.f1, 750.0, 5.0); setBP (v.f2, 1150.0, 6.0); break;
        default: break;
    }

    padFlash[(size_t) pad].store (velocity);
}

float DrumMachine::renderVoice (Voice& v) noexcept
{
    const auto t = (float) v.t;
    const auto dt = (float) (1.0 / sampleRate);
    const auto twoPi = juce::MathConstants<float>::twoPi;
    auto noise = [this] { return rng.nextFloat() * 2.0f - 1.0f; };
    auto expEnv = [t] (float tau) { return std::exp (-t / juce::jmax (0.001f, tau)); };
    float s = 0.0f, env = 1.0f;

    if (v.sample != nullptr)
    {
        auto& b = *v.sample;
        auto idx = (int) v.samplePos;
        if (idx >= b.getNumSamples() - 1) { v.active = false; return 0.0f; }
        auto frac = (float) (v.samplePos - idx);
        auto a = b.getSample (0, idx), c = b.getSample (0, idx + 1);
        s = a + (c - a) * frac;
        v.samplePos += v.sampleStep;
        env = t < 0.05f * v.decay * 20.0f ? 1.0f : expEnv (0.3f * v.decay);
        v.t += dt;
        if (env < 0.0005f) v.active = false;
        return s * env;
    }

    switch (v.sound)
    {
        case S::Kick:
        {
            auto f = 48.0f * v.tune + 140.0f * std::exp (-t * 28.0f);
            v.phase += f * dt; if (v.phase > 1.0f) v.phase -= 1.0f;
            env = expEnv (0.32f * v.decay);
            s = std::tanh (1.6f * std::sin (twoPi * v.phase)) + noise() * std::exp (-t * 400.0f) * 0.25f;
            break;
        }
        case S::Sub808:
        {
            auto f = 46.0f * v.tune + 30.0f * std::exp (-t * 18.0f);
            v.phase += f * dt; if (v.phase > 1.0f) v.phase -= 1.0f;
            env = expEnv (1.1f * v.decay);
            s = std::tanh (1.8f * std::sin (twoPi * v.phase));
            break;
        }
        case S::Snare:
        {
            v.phase += 185.0f * v.tune * dt; if (v.phase > 1.0f) v.phase -= 1.0f;
            s = std::sin (twoPi * v.phase) * std::exp (-t * 25.0f) * 0.6f
              + v.f1.processSingleSampleRaw (noise()) * expEnv (0.16f * v.decay);
            env = juce::jmax (std::exp (-t * 25.0f), expEnv (0.16f * v.decay));
            if (env < 0.001f) v.active = false;
            return s;
        }
        case S::Clap:
        {
            float burst = 0.0f;
            for (float o : { 0.0f, 0.011f, 0.022f, 0.031f })
                if (t >= o) burst = juce::jmax (burst, std::exp (-(t - o) * (o < 0.03f ? 180.0f : 1.0f / (0.18f * v.decay))));
            s = v.f1.processSingleSampleRaw (noise()) * 2.2f;
            env = burst;
            if (t > 0.05f && env < 0.001f) v.active = false;
            return s * env;
        }
        case S::ClosedHat:
        case S::OpenHat:
        {
            float metal = 0.0f;
            const float ratios[] = { 1.0f, 1.342f, 1.2312f, 1.6532f, 1.9523f, 2.1523f };
            v.phase += 400.0f * v.tune * dt;
            for (auto r : ratios) metal += (std::fmod (v.phase * r, 1.0f) < 0.5f ? 1.0f : -1.0f);
            s = v.f1.processSingleSampleRaw (metal * 0.15f + noise() * 0.6f);
            env = expEnv ((v.sound == S::OpenHat ? 0.35f : 0.045f) * v.decay);
            break;
        }
        case S::Rim:
        {
            v.phase += 820.0f * v.tune * dt; v.phase2 += 1650.0f * v.tune * dt;
            s = std::sin (twoPi * v.phase) * 0.6f + std::sin (twoPi * v.phase2) * 0.4f + noise() * std::exp (-t * 600.0f) * 0.5f;
            env = std::exp (-t / (0.018f * v.decay));
            break;
        }
        case S::TomLo:
        case S::TomHi:
        {
            auto base = (v.sound == S::TomLo ? 95.0f : 160.0f) * v.tune;
            v.phase += (base + base * 0.6f * std::exp (-t * 20.0f)) * dt; if (v.phase > 1.0f) v.phase -= 1.0f;
            s = std::sin (twoPi * v.phase);
            env = expEnv (0.3f * v.decay);
            break;
        }
        case S::Shaker:
        {
            s = v.f1.processSingleSampleRaw (noise()) * 1.5f;
            env = juce::jmin (1.0f, t / 0.006f) * expEnv (0.07f * v.decay);
            break;
        }
        case S::Perc:
        {
            v.phase += 540.0f * v.tune * dt; v.phase2 += 800.0f * v.tune * dt;
            auto sq = (std::fmod (v.phase, 1.0f) < 0.5f ? 1.0f : -1.0f) + (std::fmod (v.phase2, 1.0f) < 0.5f ? 1.0f : -1.0f);
            s = v.f1.processSingleSampleRaw (sq * 0.5f) * 2.0f;
            env = expEnv (0.12f * v.decay);
            break;
        }
        case S::Crash:
        {
            s = v.f1.processSingleSampleRaw (noise());
            env = expEnv (1.1f * v.decay);
            break;
        }
        case S::Snap:
        {
            s = v.f1.processSingleSampleRaw (noise()) * 2.5f + noise() * std::exp (-t * 900.0f) * 0.4f;
            env = expEnv (0.05f * v.decay);
            break;
        }
        case S::VoxChop:
        {
            auto f = 220.0f * v.tune;
            v.phase += f * dt; if (v.phase > 1.0f) v.phase -= 1.0f;
            auto saw = 2.0f * v.phase - 1.0f;
            s = (v.f1.processSingleSampleRaw (saw) + v.f2.processSingleSampleRaw (saw) * 0.7f) * 3.0f;
            env = juce::jmin (1.0f, t / 0.008f) * expEnv (0.22f * v.decay);
            break;
        }
        case S::NumSounds: break;
    }

    v.t += dt;
    if (t > 0.01f && env < 0.0005f) v.active = false;
    return s * env;
}

void DrumMachine::render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo&)
{
    auto* L = out.getWritePointer (0);
    auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const int n = out.getNumSamples();
    const float punch = masterPunch->get();
    const float master = masterLevel->get() * 0.7f;

    auto it = midi.begin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i)
        {
            auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                auto pad = m.getNoteNumber() - firstNote;
                // notes outside 36..51 wrap onto the pads so any keyboard works
                pad = ((pad % numPads) + numPads) % numPads;
                trigger (pad, m.getFloatVelocity());
            }
            ++it;
        }

        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            auto s = renderVoice (v) * v.level * (0.3f + 0.7f * v.vel);
            l += s * std::sqrt (0.5f * (1.0f - v.pan));
            r += s * std::sqrt (0.5f * (1.0f + v.pan));
        }
        if (punch > 0.001f)
        {
            auto d = 1.0f + punch * 4.0f;
            l = std::tanh (l * d) / std::sqrt (d) * 1.2f;
            r = std::tanh (r * d) / std::sqrt (d) * 1.2f;
        }
        L[i] += l * master;
        if (R) R[i] += r * master;
    }
}

} // namespace mcl
