#pragma once
#include "core/Param.h"
#include "core/Transport.h"

namespace mcl
{

// Base class for everything that turns MIDI into sound: built-in synths,
// the drum machine and hosted VST3 instruments.
class Instrument
{
public:
    virtual ~Instrument() = default;

    virtual juce::String getTypeId() const = 0;       // "synth", "drums", "plugin"...
    virtual juce::String getDisplayName() const = 0;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo& beat) = 0;
    virtual void allNotesOff() {}

    virtual ParamList getParams() { return {}; }

    virtual juce::StringArray getPresetNames() const { return {}; }
    virtual void loadPreset (int) {}
    virtual juce::String getCurrentPresetName() const { return currentPreset; }

    // Hosted plugins have their own GUI
    virtual bool hasEditor() const { return false; }
    virtual juce::AudioProcessorEditor* createEditor() { return nullptr; }
    virtual void showEditor() {}

    // Generic state = parameter values. Plugins override to store their chunk.
    virtual juce::ValueTree getState() const
    {
        juce::ValueTree v ("INSTRUMENT");
        v.setProperty ("type", getTypeId(), nullptr);
        v.setProperty ("preset", currentPreset, nullptr);
        for (auto* p : const_cast<Instrument*> (this)->getParams())
            v.setProperty (juce::Identifier (sanitise (p->name)), p->get(), nullptr);
        return v;
    }

    virtual void setState (const juce::ValueTree& v)
    {
        currentPreset = v.getProperty ("preset").toString();
        for (auto* p : getParams())
        {
            auto key = juce::Identifier (sanitise (p->name));
            if (v.hasProperty (key)) p->set ((float) v.getProperty (key));
        }
    }

    static juce::String sanitise (const juce::String& s)
    {
        return s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    }

protected:
    juce::String currentPreset;
};

} // namespace mcl
