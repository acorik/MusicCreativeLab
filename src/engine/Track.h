#pragma once
#include "core/MidiClip.h"
#include "dsp/Instrument.h"
#include "dsp/Effects.h"

namespace mcl
{

enum class TrackKind { Instrument, Audio };

enum class ClipState
{
    Empty = 0,
    Armed,          // waiting for the next bar to start recording
    WaitingForNote, // free mode: recording starts with your first hit
    Recording,
    Playing,
    Overdub,
    Stopped
};

juce::String clipStateName (ClipState s);

// One mixer channel: instrument (or mic input) -> FX chain -> volume/pan.
// It also owns a looping clip: a MIDI loop (instrument tracks) or an audio loop (mic).
class Track
{
public:
    Track (int uid, TrackKind kind, juce::String name);
    ~Track();

    const int uid;
    const TrackKind kind;
    juce::String name;
    juce::Colour colour;

    juce::String paramPrefix() const { return "t" + juce::String (uid) + "."; }
    juce::String newFxPrefix()       { return paramPrefix() + "fx" + juce::String (++fxCounter) + "."; }
    void bumpFxCounter (int n)       { fxCounter = juce::jmax (fxCounter, n); }

    // --- audio thread ---
    void prepare (double sampleRate, int maxBlock);
    void process (int numSamples, const BeatInfo& beat, const juce::MidiBuffer& liveMidi,
                  const float* const* input, int numInputs);
    juce::AudioBuffer<float>& getOutput() { return output; }
    void panic();

    // --- instrument / fx (swap only via AudioEngine, which holds the lock) ---
    std::unique_ptr<Instrument> instrument;
    std::vector<std::unique_ptr<Effect>> effects;

    // --- mixer params (mappable) ---
    Param volume, pan, mute, solo, monitor, loopBars, quantize, recMode, inputChannel, xfade;
    std::atomic<float> sceneGain { 1.0f }; // set by engine (crossfader)

    // --- commands (any thread) ---
    void requestRecord()  { cmdRecord = true; }
    void requestClear()   { cmdClear = true; }
    void requestUndo()    { cmdUndo = true; }
    void requestToggleClip() { cmdToggle = true; }
    void requestDouble()  { cmdDouble = true; }

    ClipState getClipState() const     { return (ClipState) clipState.load(); }
    double getClipPosition() const     { return clipPos.load(); }   // beats inside the loop (UI)
    double getClipLength() const       { return clipLenUI.load(); }
    bool hasContent() const            { return hasContentFlag.load(); }

    // MIDI clip access for the piano roll (lock while touching notes)
    juce::SpinLock clipLock;
    MidiClip clip;
    double clipOrigin = 0.0;         // absolute beat where the loop starts
    QuantizeSettings quant;          // grid from 'quantize' param + global swing
    std::atomic<float> swing { 0.0f };

    void setClipFromUI (const MidiClip& c, bool startPlaying);
    MidiClip getClipCopy();

    // meters
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<float> activity { 0.0f }; // flashes on notes

    std::vector<Param*> getMixerParams();
    static double loopBarsFromIndex (int idx) { static const double b[] = { 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 4, idx)]; }
    static double gridFromIndex (int idx)     { static const double g[] = { 0.0, 1.0, 0.5, 0.25, 0.125 }; return g[juce::jlimit (0, 4, idx)]; }

private:
    void handleCommands (const BeatInfo& beat);
    void recordMidi (const juce::MidiBuffer& live, const BeatInfo& beat, int numSamples);
    void scheduleClip (juce::MidiBuffer& out, const BeatInfo& beat, int numSamples);
    void finishFreeRecording (double endBeat);
    void processAudioLoop (int numSamples, const BeatInfo& beat, const float* mono);
    double clipLocal (double absBeat) const;
    void setState (ClipState s) { clipState = (int) s; }
    ClipState state() const { return (ClipState) clipState.load(); }

    double sampleRate = 48000.0;
    int fxCounter = 0;
    juce::AudioBuffer<float> output, monoIn;
    juce::MidiBuffer midiScratch;

    std::atomic<bool> cmdRecord { false }, cmdClear { false }, cmdUndo { false }, cmdToggle { false }, cmdDouble { false };
    std::atomic<int> clipState { (int) ClipState::Empty };
    std::atomic<double> clipPos { 0.0 }, clipLenUI { 16.0 };
    std::atomic<bool> hasContentFlag { false };

    double recordStart = 0.0, lastEventBeat = 0.0, lastNoteEndBeat = 0.0;
    bool freeMode = false;
    std::vector<Note> undoSnapshot;
    struct Held { bool on = false; double absStart = 0.0; float vel = 0.0f; };
    Held held[128];
    int heldCount = 0;

    struct PendingOff { int pitch; double endBeat; };
    std::vector<PendingOff> pendingOffs;

    // audio looper (mic tracks)
    juce::AudioBuffer<float> loopAudio;
    int loopLenSamples = 0;
    double loopLenBeats = 0.0;

    Smoothed volSmooth, panSmooth;
};

} // namespace mcl
