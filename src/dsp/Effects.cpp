#include "dsp/Effects.h"

namespace mcl
{

const juce::StringArray& getSyncDivisionNames()
{
    static juce::StringArray names { "1/32", "1/16", "1/16T", "1/8", "1/8T", "3/16", "1/4", "3/8", "1/2", "1 bar", "2 bars" };
    return names;
}

double syncDivisionToBeats (int index)
{
    static const double beats[] = { 0.125, 0.25, 1.0 / 6.0, 0.5, 1.0 / 3.0, 0.75, 1.0, 1.5, 2.0, 4.0, 8.0 };
    return beats[juce::jlimit (0, 10, index)];
}

namespace
{
    int divIndex (const Param* p) { return juce::jlimit (0, 10, (int) std::round (p->get())); }

//==============================================================================
class FilterFx : public Effect
{
public:
    FilterFx (const juce::String& pre) : Effect (pre)
    {
        mode = add ("Mode", 0.0f, 2.0f, 0.0f)->withChoices ({ "Low-pass", "High-pass", "Band-pass" });
        cutoff = add ("Cutoff", 30.0f, 20000.0f, 20000.0f, " Hz", 0.3f);
        reso = add ("Reso", 0.0f, 1.0f, 0.2f);
        mix = add ("Mix", 0.0f, 1.0f, 1.0f);
    }
    juce::String getTypeId() const override { return "filter"; }
    juce::String getDisplayName() const override { return "Filter"; }
    void prepare (double sr, int block) override
    {
        sampleRate = sr;
        f.prepare ({ sr, (juce::uint32) block, 2 });
        smooth.reset (sr, 30.0f, cutoff->get());
    }
    void reset() override { f.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        auto m = (int) std::round (mode->get());
        f.setType (m == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                 : m == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                          : juce::dsp::StateVariableTPTFilterType::bandpass);
        f.setResonance (0.5f + reso->get() * 8.0f);
        auto wet = mix->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto c = smooth.next (cutoff->get());
            if ((i & 15) == 0) f.setCutoffFrequency (juce::jlimit (20.0f, (float) sampleRate * 0.45f, c));
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto x = b.getSample (ch, i);
                b.setSample (ch, i, x + (f.processSample (ch, x) - x) * wet);
            }
        }
    }
private:
    Param *mode, *cutoff, *reso, *mix;
    juce::dsp::StateVariableTPTFilter<float> f;
    Smoothed smooth;
    double sampleRate = 48000.0;
};

//==============================================================================
class EqFx : public Effect
{
public:
    EqFx (const juce::String& pre) : Effect (pre)
    {
        low = add ("Low", -18.0f, 18.0f, 0.0f, " dB");
        mid = add ("Mid", -18.0f, 18.0f, 0.0f, " dB");
        midFreq = add ("Mid Freq", 200.0f, 8000.0f, 1000.0f, " Hz", 0.4f);
        high = add ("High", -18.0f, 18.0f, 0.0f, " dB");
        lowCut = add ("Low Cut", 20.0f, 500.0f, 20.0f, " Hz", 0.5f);
    }
    juce::String getTypeId() const override { return "eq"; }
    juce::String getDisplayName() const override { return "EQ 3-band"; }
    void prepare (double sr, int) override { sampleRate = sr; last[0] = 1e9f; update(); }
    void reset() override { for (auto& ch : filters) for (auto& f : ch) f.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        update();
        for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
            for (auto& f : filters[ch]) f.processSamples (b.getWritePointer (ch), b.getNumSamples());
    }
private:
    void update()
    {
        float now[5] = { low->get(), mid->get(), midFreq->get(), high->get(), lowCut->get() };
        if (std::equal (std::begin (now), std::end (now), std::begin (last))) return;
        std::copy (std::begin (now), std::end (now), std::begin (last));
        auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
        for (auto& ch : filters)
        {
            ch[0].setCoefficients (juce::IIRCoefficients::makeLowShelf (sampleRate, 200.0, 0.7, g (now[0])));
            ch[1].setCoefficients (juce::IIRCoefficients::makePeakFilter (sampleRate, now[2], 0.8, g (now[1])));
            ch[2].setCoefficients (juce::IIRCoefficients::makeHighShelf (sampleRate, 6000.0, 0.7, g (now[3])));
            ch[3].setCoefficients (juce::IIRCoefficients::makeHighPass (sampleRate, now[4], 0.7));
        }
    }
    Param *low, *mid, *midFreq, *high, *lowCut;
    juce::IIRFilter filters[2][4];
    float last[5] {};
    double sampleRate = 48000.0;
};

//==============================================================================
class CompressorFx : public Effect
{
public:
    CompressorFx (const juce::String& pre) : Effect (pre)
    {
        threshold = add ("Threshold", -60.0f, 0.0f, -18.0f, " dB");
        ratio = add ("Ratio", 1.0f, 20.0f, 4.0f, ":1", 0.5f);
        attack = add ("Attack", 0.1f, 100.0f, 5.0f, " ms", 0.5f);
        release = add ("Release", 10.0f, 1000.0f, 120.0f, " ms", 0.5f);
        makeup = add ("Makeup", 0.0f, 24.0f, 4.0f, " dB");
    }
    juce::String getTypeId() const override { return "compressor"; }
    juce::String getDisplayName() const override { return "Compressor"; }
    void prepare (double sr, int block) override { comp.prepare ({ sr, (juce::uint32) block, 2 }); }
    void reset() override { comp.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        comp.setThreshold (threshold->get()); comp.setRatio (ratio->get());
        comp.setAttack (attack->get()); comp.setRelease (release->get());
        juce::dsp::AudioBlock<float> blk (b);
        comp.process (juce::dsp::ProcessContextReplacing<float> (blk));
        b.applyGain (juce::Decibels::decibelsToGain (makeup->get()));
    }
private:
    Param *threshold, *ratio, *attack, *release, *makeup;
    juce::dsp::Compressor<float> comp;
};

//==============================================================================
class GateFx : public Effect
{
public:
    GateFx (const juce::String& pre) : Effect (pre)
    {
        threshold = add ("Threshold", -80.0f, 0.0f, -45.0f, " dB");
        ratio = add ("Ratio", 1.0f, 20.0f, 10.0f, ":1");
        release = add ("Release", 10.0f, 500.0f, 80.0f, " ms");
    }
    juce::String getTypeId() const override { return "gate"; }
    juce::String getDisplayName() const override { return "Noise Gate"; }
    void prepare (double sr, int block) override { gate.prepare ({ sr, (juce::uint32) block, 2 }); }
    void reset() override { gate.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        gate.setThreshold (threshold->get()); gate.setRatio (ratio->get());
        gate.setAttack (1.0f); gate.setRelease (release->get());
        juce::dsp::AudioBlock<float> blk (b);
        gate.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }
private:
    Param *threshold, *ratio, *release;
    juce::dsp::NoiseGate<float> gate;
};

//==============================================================================
class ReverbFx : public Effect
{
public:
    ReverbFx (const juce::String& pre) : Effect (pre)
    {
        size = add ("Size", 0.0f, 1.0f, 0.75f);
        damping = add ("Damping", 0.0f, 1.0f, 0.4f);
        width = add ("Width", 0.0f, 1.0f, 1.0f);
        mix = add ("Mix", 0.0f, 1.0f, 0.3f);
        freeze = add ("Freeze", 0.0f, 1.0f, 0.0f)->withChoices ({ "Off", "On" });
    }
    juce::String getTypeId() const override { return "reverb"; }
    juce::String getDisplayName() const override { return "Reverb"; }
    void prepare (double sr, int) override { reverb.setSampleRate (sr); }
    void reset() override { reverb.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        juce::Reverb::Parameters p;
        p.roomSize = size->get(); p.damping = damping->get(); p.width = width->get();
        p.wetLevel = mix->get(); p.dryLevel = 1.0f - mix->get() * 0.5f;
        p.freezeMode = freeze->get() > 0.5f ? 1.0f : 0.0f;
        reverb.setParameters (p);
        if (b.getNumChannels() >= 2) reverb.processStereo (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
        else reverb.processMono (b.getWritePointer (0), b.getNumSamples());
    }
private:
    Param *size, *damping, *width, *mix, *freeze;
    juce::Reverb reverb;
};

//==============================================================================
class DelayFx : public Effect
{
public:
    DelayFx (const juce::String& pre) : Effect (pre)
    {
        division = add ("Time", 0.0f, 10.0f, 5.0f)->withChoices (getSyncDivisionNames());  // 3/16 = dotted eighth
        feedback = add ("Feedback", 0.0f, 0.95f, 0.4f);
        tone = add ("Tone", 500.0f, 16000.0f, 5000.0f, " Hz", 0.4f);
        pingPong = add ("Ping Pong", 0.0f, 1.0f, 1.0f)->withChoices ({ "Off", "On" });
        mix = add ("Mix", 0.0f, 1.0f, 0.25f);
    }
    juce::String getTypeId() const override { return "delay"; }
    juce::String getDisplayName() const override { return "Delay (sync)"; }
    void prepare (double sr, int) override
    {
        sampleRate = sr;
        buf.setSize (2, (int) (sr * 6.0)); buf.clear(); pos = 0;
        timeSmooth.reset (sr, 80.0f, (float) (sr * 0.375));
    }
    void reset() override { buf.clear(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo& beat) override
    {
        auto target = (float) juce::jlimit (16.0, buf.getNumSamples() - 4.0, syncDivisionToBeats (divIndex (division)) * beat.samplesPerBeat());
        auto fb = feedback->get(), wet = mix->get();
        bool pp = pingPong->get() > 0.5f;
        auto coef = std::exp (-juce::MathConstants<float>::twoPi * tone->get() / (float) sampleRate);
        const int len = buf.getNumSamples();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto d = timeSmooth.next (target);
            auto readPos = (float) pos - d; if (readPos < 0) readPos += (float) len;
            auto i0 = (int) readPos; auto frac = readPos - (float) i0; auto i1 = (i0 + 1) % len;
            float out[2];
            for (int ch = 0; ch < 2; ++ch) out[ch] = buf.getSample (ch, i0) + (buf.getSample (ch, i1) - buf.getSample (ch, i0)) * frac;
            auto inL = b.getSample (0, i), inR = b.getNumChannels() > 1 ? b.getSample (1, i) : inL;
            for (int ch = 0; ch < 2; ++ch) lp[ch] = out[ch] + coef * (lp[ch] - out[ch]);
            if (pp)
            {
                buf.setSample (0, pos, (inL + inR) * 0.5f + lp[1] * fb);
                buf.setSample (1, pos, lp[0] * fb);
            }
            else
            {
                buf.setSample (0, pos, inL + lp[0] * fb);
                buf.setSample (1, pos, inR + lp[1] * fb);
            }
            b.setSample (0, i, inL + out[0] * wet);
            if (b.getNumChannels() > 1) b.setSample (1, i, inR + out[1] * wet);
            pos = (pos + 1) % len;
        }
    }
private:
    Param *division, *feedback, *tone, *pingPong, *mix;
    juce::AudioBuffer<float> buf;
    int pos = 0;
    float lp[2] {};
    Smoothed timeSmooth;
    double sampleRate = 48000.0;
};

//==============================================================================
class ChorusFx : public Effect
{
public:
    ChorusFx (const juce::String& pre) : Effect (pre)
    {
        rate = add ("Rate", 0.05f, 5.0f, 0.6f, " Hz", 0.5f);
        depth = add ("Depth", 0.0f, 1.0f, 0.4f);
        delay = add ("Delay", 2.0f, 30.0f, 8.0f, " ms");
        feedback = add ("Feedback", -0.9f, 0.9f, 0.0f);
        mix = add ("Mix", 0.0f, 1.0f, 0.4f);
    }
    juce::String getTypeId() const override { return "chorus"; }
    juce::String getDisplayName() const override { return "Chorus / Doubler"; }
    void prepare (double sr, int block) override { chorus.prepare ({ sr, (juce::uint32) block, 2 }); }
    void reset() override { chorus.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        chorus.setRate (rate->get()); chorus.setDepth (depth->get()); chorus.setCentreDelay (delay->get());
        chorus.setFeedback (feedback->get()); chorus.setMix (mix->get());
        juce::dsp::AudioBlock<float> blk (b);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }
private:
    Param *rate, *depth, *delay, *feedback, *mix;
    juce::dsp::Chorus<float> chorus;
};

//==============================================================================
class PhaserFx : public Effect
{
public:
    PhaserFx (const juce::String& pre) : Effect (pre)
    {
        rate = add ("Rate", 0.05f, 5.0f, 0.3f, " Hz", 0.5f);
        depth = add ("Depth", 0.0f, 1.0f, 0.6f);
        centre = add ("Centre", 200.0f, 5000.0f, 1000.0f, " Hz", 0.4f);
        feedback = add ("Feedback", -0.9f, 0.9f, 0.5f);
        mix = add ("Mix", 0.0f, 1.0f, 0.5f);
    }
    juce::String getTypeId() const override { return "phaser"; }
    juce::String getDisplayName() const override { return "Phaser"; }
    void prepare (double sr, int block) override { phaser.prepare ({ sr, (juce::uint32) block, 2 }); }
    void reset() override { phaser.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        phaser.setRate (rate->get()); phaser.setDepth (depth->get()); phaser.setCentreFrequency (centre->get());
        phaser.setFeedback (feedback->get()); phaser.setMix (mix->get());
        juce::dsp::AudioBlock<float> blk (b);
        phaser.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }
private:
    Param *rate, *depth, *centre, *feedback, *mix;
    juce::dsp::Phaser<float> phaser;
};

//==============================================================================
class DistortionFx : public Effect
{
public:
    DistortionFx (const juce::String& pre) : Effect (pre)
    {
        drive = add ("Drive", 0.0f, 1.0f, 0.4f);
        tone = add ("Tone", 500.0f, 18000.0f, 8000.0f, " Hz", 0.4f);
        mix = add ("Mix", 0.0f, 1.0f, 1.0f);
        output = add ("Output", -24.0f, 6.0f, -3.0f, " dB");
    }
    juce::String getTypeId() const override { return "distortion"; }
    juce::String getDisplayName() const override { return "Saturator / Distortion"; }
    void prepare (double sr, int) override { sampleRate = sr; }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        auto d = 1.0f + drive->get() * drive->get() * 30.0f;
        auto coef = std::exp (-juce::MathConstants<float>::twoPi * tone->get() / (float) sampleRate);
        auto wet = mix->get(), out = juce::Decibels::decibelsToGain (output->get());
        for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
        {
            auto* x = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                auto s = std::tanh (x[i] * d) / std::tanh (d);
                lp[ch] = s + coef * (lp[ch] - s);
                x[i] = (x[i] + (lp[ch] - x[i]) * wet) * out;
            }
        }
    }
private:
    Param *drive, *tone, *mix, *output;
    float lp[2] {};
    double sampleRate = 48000.0;
};

//==============================================================================
class BitcrusherFx : public Effect
{
public:
    BitcrusherFx (const juce::String& pre) : Effect (pre)
    {
        bits = add ("Bits", 2.0f, 16.0f, 8.0f);
        downsample = add ("Downsample", 1.0f, 32.0f, 4.0f);
        mix = add ("Mix", 0.0f, 1.0f, 0.6f);
    }
    juce::String getTypeId() const override { return "bitcrusher"; }
    juce::String getDisplayName() const override { return "Bitcrusher / Lo-Fi"; }
    void prepare (double, int) override {}
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        auto levels = std::pow (2.0f, bits->get() - 1.0f);
        auto ds = juce::jmax (1, (int) downsample->get());
        auto wet = mix->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            if (++counter >= ds)
            {
                counter = 0;
                for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
                    held[ch] = std::round (b.getSample (ch, i) * levels) / levels;
            }
            for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
            {
                auto x = b.getSample (ch, i);
                b.setSample (ch, i, x + (held[ch] - x) * wet);
            }
        }
    }
private:
    Param *bits, *downsample, *mix;
    int counter = 0;
    float held[2] {};
};

//==============================================================================
// Sidechain-style "pump" locked to the beat: the classic house/garage breathing.
class PumpFx : public Effect
{
public:
    PumpFx (const juce::String& pre) : Effect (pre)
    {
        depth = add ("Depth", 0.0f, 1.0f, 0.6f);
        division = add ("Rate", 0.0f, 10.0f, 6.0f)->withChoices (getSyncDivisionNames()); // 1/4
        shape = add ("Release", 0.05f, 1.0f, 0.45f);
    }
    juce::String getTypeId() const override { return "pump"; }
    juce::String getDisplayName() const override { return "Pump (sidechain)"; }
    void prepare (double sr, int) override { sm.reset (sr, 3.0f, 1.0f); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo& beat) override
    {
        auto div = syncDivisionToBeats (divIndex (division));
        auto dep = depth->get(), rel = shape->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto ph = (float) (std::fmod (beat.beatAt (i), div) / div);
            auto rec = juce::jmin (1.0f, ph / rel);
            auto target = beat.playing ? 1.0f - dep * (1.0f - rec * rec * (3.0f - 2.0f * rec)) : 1.0f;
            auto g = sm.next (target);
            for (int ch = 0; ch < b.getNumChannels(); ++ch) b.setSample (ch, i, b.getSample (ch, i) * g);
        }
    }
private:
    Param *depth, *division, *shape;
    Smoothed sm;
};

//==============================================================================
// Beat repeat / stutter: when "Hold" is on, repeats a slice of the audio
class StutterFx : public Effect
{
public:
    StutterFx (const juce::String& pre) : Effect (pre)
    {
        hold = add ("Hold", 0.0f, 1.0f, 0.0f)->withChoices ({ "Off", "On" });
        division = add ("Slice", 0.0f, 10.0f, 1.0f)->withChoices (getSyncDivisionNames()); // 1/16
        mix = add ("Mix", 0.0f, 1.0f, 1.0f);
    }
    juce::String getTypeId() const override { return "stutter"; }
    juce::String getDisplayName() const override { return "Stutter / Beat Repeat"; }
    void prepare (double sr, int) override { buf.setSize (2, (int) (sr * 4.0)); buf.clear(); pos = 0; }
    void process (juce::AudioBuffer<float>& b, const BeatInfo& beat) override
    {
        const int len = buf.getNumSamples();
        bool on = hold->get() > 0.5f;
        auto slice = (int) juce::jlimit (64.0, len / 2.0, syncDivisionToBeats (divIndex (division)) * beat.samplesPerBeat());
        auto wet = mix->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < 2; ++ch) buf.setSample (ch, pos, b.getSample (juce::jmin (ch, b.getNumChannels() - 1), i));
            if (on && ! active) { active = true; start = pos; k = 0; }
            if (! on) active = false;
            if (active)
            {
                auto rp = (start + (k % slice)) % len;
                // short fade at slice edges to avoid clicks
                auto edge = juce::jmin (k % slice, slice - (k % slice));
                auto fade = juce::jmin (1.0f, (float) edge / 64.0f);
                for (int ch = 0; ch < b.getNumChannels() && ch < 2; ++ch)
                {
                    auto x = b.getSample (ch, i);
                    b.setSample (ch, i, x + (buf.getSample (ch, rp) * fade - x) * wet);
                }
                ++k;
            }
            pos = (pos + 1) % len;
        }
    }
private:
    Param *hold, *division, *mix;
    juce::AudioBuffer<float> buf;
    int pos = 0, start = 0, k = 0;
    bool active = false;
};

//==============================================================================
class AutoPanFx : public Effect
{
public:
    AutoPanFx (const juce::String& pre) : Effect (pre)
    {
        division = add ("Rate", 0.0f, 10.0f, 6.0f)->withChoices (getSyncDivisionNames());
        depth = add ("Depth", 0.0f, 1.0f, 0.7f);
        tremolo = add ("Tremolo", 0.0f, 1.0f, 0.0f);
    }
    juce::String getTypeId() const override { return "autopan"; }
    juce::String getDisplayName() const override { return "Auto-Pan / Tremolo"; }
    void prepare (double, int) override {}
    void process (juce::AudioBuffer<float>& b, const BeatInfo& beat) override
    {
        if (b.getNumChannels() < 2) return;
        auto div = syncDivisionToBeats (divIndex (division));
        auto d = depth->get(), trem = tremolo->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto ph = (float) (std::fmod (beat.beatAt (i), div) / div);
            auto lfo = std::sin (juce::MathConstants<float>::twoPi * ph);
            auto pan = lfo * d * (1.0f - trem);
            auto amp = 1.0f - trem * d * (0.5f + 0.5f * lfo);
            b.setSample (0, i, b.getSample (0, i) * amp * std::sqrt (1.0f - pan) );
            b.setSample (1, i, b.getSample (1, i) * amp * std::sqrt (1.0f + pan) );
        }
    }
private:
    Param *division, *depth, *tremolo;
};

//==============================================================================
class WidenerFx : public Effect
{
public:
    WidenerFx (const juce::String& pre) : Effect (pre) { width = add ("Width", 0.0f, 2.0f, 1.4f); }
    juce::String getTypeId() const override { return "widener"; }
    juce::String getDisplayName() const override { return "Stereo Widener"; }
    void prepare (double, int) override {}
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        if (b.getNumChannels() < 2) return;
        auto w = width->get();
        auto* l = b.getWritePointer (0); auto* r = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            auto m = (l[i] + r[i]) * 0.5f, s = (l[i] - r[i]) * 0.5f * w;
            l[i] = m + s; r[i] = m - s;
        }
    }
private:
    Param* width;
};

//==============================================================================
class RingModFx : public Effect
{
public:
    RingModFx (const juce::String& pre) : Effect (pre)
    {
        freq = add ("Freq", 20.0f, 2000.0f, 90.0f, " Hz", 0.4f);
        mix = add ("Mix", 0.0f, 1.0f, 0.5f);
    }
    juce::String getTypeId() const override { return "ringmod"; }
    juce::String getDisplayName() const override { return "Robot (Ring Mod)"; }
    void prepare (double sr, int) override { sampleRate = sr; }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        auto inc = freq->get() / (float) sampleRate, wet = mix->get();
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            auto m = std::sin (juce::MathConstants<float>::twoPi * phase);
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto x = b.getSample (ch, i);
                b.setSample (ch, i, x + (x * m - x) * wet);
            }
        }
    }
private:
    Param *freq, *mix;
    float phase = 0.0f;
    double sampleRate = 48000.0;
};

//==============================================================================
class LimiterFx : public Effect
{
public:
    LimiterFx (const juce::String& pre) : Effect (pre)
    {
        threshold = add ("Ceiling", -12.0f, 0.0f, -0.5f, " dB");
        release = add ("Release", 10.0f, 500.0f, 80.0f, " ms");
    }
    juce::String getTypeId() const override { return "limiter"; }
    juce::String getDisplayName() const override { return "Limiter"; }
    void prepare (double sr, int block) override { lim.prepare ({ sr, (juce::uint32) block, 2 }); }
    void reset() override { lim.reset(); }
    void process (juce::AudioBuffer<float>& b, const BeatInfo&) override
    {
        lim.setThreshold (threshold->get()); lim.setRelease (release->get());
        juce::dsp::AudioBlock<float> blk (b);
        lim.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }
private:
    Param *threshold, *release;
    juce::dsp::Limiter<float> lim;
};

} // namespace

//==============================================================================
const std::vector<EffectType>& getBuiltInEffectTypes()
{
    static std::vector<EffectType> types {
        { "filter",     "Filter",                 "Filtro / EQ" },
        { "eq",         "EQ 3-band",              "Filtro / EQ" },
        { "compressor", "Compressor",             "Dinamica" },
        { "gate",       "Noise Gate",             "Dinamica" },
        { "limiter",    "Limiter",                "Dinamica" },
        { "reverb",     "Reverb",                 "Espacio" },
        { "delay",      "Delay (sync)",           "Espacio" },
        { "chorus",     "Chorus / Doubler",       "Modulacion" },
        { "phaser",     "Phaser",                 "Modulacion" },
        { "autopan",    "Auto-Pan / Tremolo",     "Modulacion" },
        { "widener",    "Stereo Widener",         "Modulacion" },
        { "pump",       "Pump (sidechain)",       "Ritmo" },
        { "stutter",    "Stutter / Beat Repeat",  "Ritmo" },
        { "distortion", "Saturator / Distortion", "Color" },
        { "bitcrusher", "Bitcrusher / Lo-Fi",     "Color" },
        { "ringmod",    "Robot (Ring Mod)",       "Color" },
    };
    return types;
}

std::unique_ptr<Effect> createBuiltInEffect (const juce::String& id, const juce::String& prefix)
{
    if (id == "filter")     return std::make_unique<FilterFx> (prefix);
    if (id == "eq")         return std::make_unique<EqFx> (prefix);
    if (id == "compressor") return std::make_unique<CompressorFx> (prefix);
    if (id == "gate")       return std::make_unique<GateFx> (prefix);
    if (id == "limiter")    return std::make_unique<LimiterFx> (prefix);
    if (id == "reverb")     return std::make_unique<ReverbFx> (prefix);
    if (id == "delay")      return std::make_unique<DelayFx> (prefix);
    if (id == "chorus")     return std::make_unique<ChorusFx> (prefix);
    if (id == "phaser")     return std::make_unique<PhaserFx> (prefix);
    if (id == "autopan")    return std::make_unique<AutoPanFx> (prefix);
    if (id == "widener")    return std::make_unique<WidenerFx> (prefix);
    if (id == "pump")       return std::make_unique<PumpFx> (prefix);
    if (id == "stutter")    return std::make_unique<StutterFx> (prefix);
    if (id == "distortion") return std::make_unique<DistortionFx> (prefix);
    if (id == "bitcrusher") return std::make_unique<BitcrusherFx> (prefix);
    if (id == "ringmod")    return std::make_unique<RingModFx> (prefix);
    return nullptr;
}

} // namespace mcl
