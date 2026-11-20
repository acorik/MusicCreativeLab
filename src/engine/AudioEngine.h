#pragma once
#include "engine/Track.h"
#include "engine/PluginHost.h"
#include "control/MidiMapper.h"
#include <map>

namespace mcl
{

struct Scene
{
    juce::String name;
    float energy = 0.5f;
    struct TrackState { bool active = true; float volume = 0.8f; };
    std::map<int, TrackState> tracks; // by track uid
};

// The heart of the app: runs every track, the master bus, scenes,
// transitions (riser -> drop), the metronome and the set recorder.
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback,
                    public juce::ChangeBroadcaster
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialiseDevices (const juce::XmlElement* savedState);
    juce::AudioDeviceManager deviceManager;
    PluginManager plugins;
    MidiMapper mapper;
    Transport transport;
    juce::MidiKeyboardState keyboardState;

    // --- tracks (message thread) ---
    Track* addTrack (TrackKind kind, const juce::String& name, int forcedUid = -1, bool withDefaultFx = true);
    void removeTrack (int uid);
    void clearAllTracks();
    int getNumTracks() const;
    Track* getTrack (int index) const;
    Track* getTrackByUid (int uid) const;
    int indexOfTrack (int uid) const;
    void moveTrack (int uid, int delta);

    void setInstrument (Track& t, std::unique_ptr<Instrument> inst);
    Effect* addEffect (Track& t, std::unique_ptr<Effect> fx);
    Effect* addEffect (Track& t, const juce::String& builtInId);
    void removeEffect (Track& t, int index);
    void moveEffect (Track& t, int index, int delta);
    Effect* addMasterEffect (std::unique_ptr<Effect> fx);
    void removeMasterEffect (int index);
    std::vector<std::unique_ptr<Effect>>& getMasterEffects() { return masterFx; }
    juce::String newMasterFxPrefix() { return "master.fx" + juce::String (++masterFxCounter) + "."; }

    std::unique_ptr<Instrument> createBuiltInInstrument (const juce::String& type, Track& t);
    std::unique_ptr<Instrument> createPluginInstrument (const juce::PluginDescription& d, Track& t, juce::String& error);
    std::unique_ptr<Effect> createPluginEffect (const juce::PluginDescription& d, const juce::String& prefix, juce::String& error);

    std::atomic<int> selectedTrack { -1 };   // uid
    Track* getSelectedTrack() const { return getTrackByUid (selectedTrack.load()); }
    void selectTrack (int uid) { selectedTrack = uid; sendChangeMessage(); }
    void selectRelative (int delta);

    std::atomic<bool> padsToDrums { true };   // MPD218 / channel 10 notes go to the drum track

    // --- transport ---
    void play();
    void stop();
    void togglePlay() { transport.playing ? stop() : play(); }
    void recordOnSelected();

    // --- master ---
    Param masterVolume, energy, crossfader, swing, riserBars;
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };

    // --- scenes ---
    std::vector<Scene> scenes;
    std::atomic<int> currentScene { -1 }, pendingScene { -1 };
    void captureScene (int index);
    void launchScene (int index);               // quantised to the next bar
    void startRiserToNextScene();               // build-up over N bars, then DROP
    bool isRiserActive() const { return riserActive.load(); }
    float getRiserProgress() const { return riserProgress.load(); }

    // --- set recorder ---
    bool startRecordingSet (const juce::File& file);
    void stopRecordingSet();
    bool isRecordingSet() const { return recorderActive.load(); }
    juce::File getLastRecording() const { return lastRecording; }

    // --- registry for MIDI / OSC / Stream Deck ---
    Param* findParam (const juce::String& id);
    Action* findAction (const juce::String& id);
    juce::String describeTarget (const juce::String& id);
    std::vector<Action>& getActions() { return actions; }
    std::vector<Param*> getAllParams();

    void panic();

    double getSampleRate() const { return sampleRate; }
    int getBlockSize() const { return blockSize; }
    double getCpuUsage() { return deviceManager.getCpuUsage(); }

    juce::CriticalSection& getLock() { return lock; }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    // MidiInputCallback
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m) override;

    void refreshMidiInputs();
    juce::StringArray getOpenMidiInputs() const { const juce::ScopedLock sl (midiLock); return openMidi; }

private:
    void buildActions();
    void processChunk (const float* const* in, int numIn, int numSamples);
    void applyScene (int index);
    Track* findDrumTrack() const;

    mutable juce::CriticalSection lock, midiLock;
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<std::unique_ptr<Effect>> masterFx;
    std::vector<Action> actions;
    int nextUid = 1, masterFxCounter = 0;

    double sampleRate = 48000.0;
    int blockSize = 512;
    static constexpr int maxChunk = 512;

    juce::AudioBuffer<float> mix;
    juce::MidiBuffer liveMidi, drumMidi, selMidi;
    juce::MidiMessageCollector collector, drumCollector;
    juce::StringArray openMidi;

    // master processing
    juce::dsp::StateVariableTPTFilter<float> energyFilter, riserFilter, riserHpf;
    std::map<juce::String, Action> dynamicActions;
    Smoothed energySmooth;
    std::atomic<bool> riserActive { false };
    std::atomic<float> riserProgress { 0.0f };
    double riserStart = 0.0, riserLength = 16.0;
    int riserTargetScene = -1;
    juce::Random rng;
    double metroPhase = 0.0; float metroEnv = 0.0f; float metroFreq = 1000.0f;

    // recorder
    juce::TimeSliceThread writerThread { "set recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::CriticalSection writerLock;
    std::atomic<bool> recorderActive { false };
    juce::File lastRecording;
};

} // namespace mcl
