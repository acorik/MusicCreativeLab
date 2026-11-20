#pragma once
#include "engine/AudioEngine.h"

namespace mcl
{

namespace Project
{
    juce::ValueTree save (AudioEngine& engine);
    juce::String load (AudioEngine& engine, const juce::ValueTree& tree); // returns error text (empty = ok)

    bool saveToFile (AudioEngine& engine, const juce::File& f);
    juce::String loadFromFile (AudioEngine& engine, const juce::File& f);

    juce::File getDefaultFolder();

    // Built-in demo projects
    juce::StringArray getDemoNames();
    juce::StringArray getDemoDescriptions();
    void loadDemo (AudioEngine& engine, int index);
    void newEmptyProject (AudioEngine& engine);

    // Track presets (instrument preset + FX chain) for the browser
    struct TrackPreset { juce::String name, category, instrument; int preset; juce::StringArray fx; };
    const std::vector<TrackPreset>& getTrackPresets();
    Track* addTrackFromPreset (AudioEngine& engine, const TrackPreset& p);
}

} // namespace mcl
