#pragma once
#include "dsp/Instrument.h"
#include <array>

namespace mcl
{

// 16-pad drum machine laid out like the Akai MPD218 (bank A: notes 36..51).
// Every pad is synthesised (no samples needed), but you can load your own
// .wav on any pad.
class DrumMachine : public Instrument
{
public:
    static constexpr int numPads = 16;
    static constexpr int firstNote = 36;

    enum class Sound { Kick, Sub808, Snare, Clap, ClosedHat, OpenHat, Rim, TomLo, TomHi,
                       Shaker, Perc, Crash, Snap, VoxChop, NumSounds };

    explicit DrumMachine (const juce::String& paramPrefix);

    juce::String getTypeId() const override       { return "drums"; }
    juce::String getDisplayName() const override  { return "Lab Drums"; }
    void prepare (double sampleRate, int maxBlock) override;
    void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo& beat) override;
    void allNotesOff() override { for (auto& v : voices) v.active = false; }

    ParamList getParams() override;
    juce::StringArray getPresetNames() const override;
    void loadPreset (int index) override;

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& v) override;

    static juce::String soundName (Sound s);
    juce::String getPadName (int pad) const;
    Sound getPadSound (int pad) const { return pads[(size_t) pad].sound; }
    void setPadSound (int pad, Sound s) { pads[(size_t) pad].sound = s; }

    bool loadSample (int pad, const juce::File& file);
    void clearSample (int pad);
    juce::File getSampleFile (int pad) const { return pads[(size_t) pad].sampleFile; }

    // UI flash
    std::array<std::atomic<float>, numPads> padFlash {};

private:
    struct Pad
    {
        Sound sound = Sound::Kick;
        Param* level = nullptr;
        Param* tune = nullptr;
        Param* decay = nullptr;
        Param* pan = nullptr;
        std::shared_ptr<juce::AudioBuffer<float>> sample;
        double sampleRateOfSample = 44100.0;
        juce::File sampleFile;
    };

    struct Voice
    {
        bool active = false;
        int pad = 0;
        Sound sound = Sound::Kick;
        double t = 0.0;
        float phase = 0.0f, phase2 = 0.0f, vel = 1.0f, tune = 1.0f, decay = 1.0f, level = 1.0f, pan = 0.0f;
        juce::IIRFilter f1, f2;
        std::shared_ptr<juce::AudioBuffer<float>> sample;
        double samplePos = 0.0, sampleStep = 1.0;
    };

    void trigger (int pad, float velocity);
    float renderVoice (Voice& v) noexcept;

    std::vector<std::unique_ptr<Param>> owned;
    std::array<Pad, numPads> pads;
    std::array<Voice, numPads * 2> voices;
    Param* masterPunch = nullptr;
    Param* masterLevel = nullptr;
    double sampleRate = 48000.0;
    juce::Random rng;
    juce::SpinLock sampleLock;
};

} // namespace mcl
