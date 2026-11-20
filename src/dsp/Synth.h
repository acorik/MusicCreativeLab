#pragma once
#include "dsp/Instrument.h"

namespace mcl
{

// "Lab Synth": a polyphonic virtual-analog synth with unison, filter, LFO
// and the KEY MORPH feature: every note you play shifts the timbre a bit
// (filter, waveform blend, detune, stereo), so repeated chords "breathe".
class LabSynth : public Instrument
{
public:
    explicit LabSynth (const juce::String& paramPrefix);
    ~LabSynth() override;

    juce::String getTypeId() const override       { return "synth"; }
    juce::String getDisplayName() const override  { return "Lab Synth"; }
    void prepare (double sampleRate, int maxBlock) override;
    void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo& beat) override;
    void allNotesOff() override { synth.allNotesOff (0, true); }

    ParamList getParams() override;
    juce::StringArray getPresetNames() const override;
    void loadPreset (int index) override;

    struct Params
    {
        Param *wave1, *wave2, *osc2Semi, *oscMix, *unison, *detune, *noise,
              *cutoff, *resonance, *filterEnv, *keyTrack,
              *attack, *decay, *sustain, *release, *fDecay,
              *lfoRate, *lfoCutoff, *lfoPitch, *morph, *velSens, *drive, *glide, *level;
    };

private:
    std::vector<std::unique_ptr<Param>> owned;
    Params p {};
    juce::Synthesiser synth;
    int morphCounter = 0;
    float lastFreq = 0.0f;

    Param* add (const juce::String& prefix, const juce::String& name, float mn, float mx, float def,
                const juce::String& suffix = {}, float skew = 1.0f);
    friend class LabSynthVoice;
};

} // namespace mcl
