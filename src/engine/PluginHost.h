#pragma once
#include "dsp/Instrument.h"
#include "dsp/Effects.h"

namespace mcl
{

// Scans and creates VST3 plugins installed on the user's PC.
class PluginManager : public juce::ChangeBroadcaster, private juce::Thread
{
public:
    PluginManager();
    ~PluginManager() override;

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;

    juce::FileSearchPath getSearchPath() const;
    void addSearchFolder (const juce::File& folder);

    void startScan();
    bool isScanning() const { return isThreadRunning(); }
    juce::String getScanStatus() const { const juce::ScopedLock sl (statusLock); return status; }

    std::unique_ptr<juce::AudioPluginInstance> create (const juce::PluginDescription& d, double sr, int block, juce::String& error);
    std::unique_ptr<juce::AudioPluginInstance> createFromXml (const juce::XmlElement& descXml, double sr, int block, juce::String& error);

    juce::Array<juce::PluginDescription> getInstruments() const;
    juce::Array<juce::PluginDescription> getEffects() const;

private:
    void run() override;
    void save();
    juce::File dataFolder() const;

    juce::CriticalSection statusLock;
    juce::String status;
    juce::StringArray extraFolders;
};

//==============================================================================
// Exposes the first N parameters of a plugin as mcl::Param so that MIDI learn,
// Stream Deck and scenes can control them.
class PluginParamBridge
{
public:
    void attach (juce::AudioPluginInstance& p, const juce::String& prefix, int maxParams = 48);
    void pushToPlugin();
    ParamList getParams();
private:
    struct Link { std::unique_ptr<Param> proxy; juce::AudioProcessorParameter* target; float last; };
    std::vector<Link> links;
};

// Floating window for a plugin's own GUI
std::unique_ptr<juce::DocumentWindow> openPluginWindow (juce::AudioPluginInstance& p, std::unique_ptr<juce::DocumentWindow> existing);

class PluginInstrument : public Instrument
{
public:
    PluginInstrument (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& prefix);
    ~PluginInstrument() override;

    juce::String getTypeId() const override       { return "plugin"; }
    juce::String getDisplayName() const override  { return plugin->getName(); }
    void prepare (double sampleRate, int maxBlock) override;
    void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo& beat) override;
    void allNotesOff() override { pendingAllOff = true; }
    ParamList getParams() override { return bridge.getParams(); }

    juce::StringArray getPresetNames() const override;
    void loadPreset (int index) override;

    bool hasEditor() const override { return plugin->hasEditor(); }
    juce::AudioProcessorEditor* createEditor() override { return plugin->createEditorIfNeeded(); }

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& v) override;

    juce::AudioPluginInstance& getPlugin() { return *plugin; }
    void showEditor() override;

private:
    std::unique_ptr<juce::DocumentWindow> window;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> scratch;
    PluginParamBridge bridge;
    std::atomic<bool> pendingAllOff { false };
    juce::AudioPlayHead::PositionInfo posInfo;
    struct Head : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override { return info; }
        PositionInfo info;
    } head;
};

class PluginEffect : public Effect
{
public:
    PluginEffect (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& prefix);
    ~PluginEffect() override;

    juce::String getTypeId() const override       { return "plugin"; }
    juce::String getDisplayName() const override  { return plugin->getName(); }
    void prepare (double sampleRate, int maxBlock) override;
    void process (juce::AudioBuffer<float>& buffer, const BeatInfo& beat) override;
    ParamList getParams() override;

    bool hasEditor() const override { return plugin->hasEditor(); }
    juce::AudioProcessorEditor* createEditor() override { return plugin->createEditorIfNeeded(); }

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& v) override;
    void showEditor() override;

private:
    std::unique_ptr<juce::DocumentWindow> window;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer emptyMidi;
    PluginParamBridge bridge;
};

} // namespace mcl
