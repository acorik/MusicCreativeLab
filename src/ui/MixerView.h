#pragma once
#include "ui/Widgets.h"

namespace mcl
{

void showAddEffectMenu (AudioEngine& engine, Track* track, juce::Component* target, std::function<void (int)> onAdded = {});

// FL-style mixer: one channel strip per track + master.
class MixerView : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit MixerView (AudioEngine& e);
    ~MixerView() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    // track uid (-1 = master), fx index
    std::function<void (int, int)> onSelectEffect;

private:
    struct Strip;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild (bool force);

    AudioEngine& engine;
    juce::Viewport viewport;
    juce::Component holder;
    juce::OwnedArray<Strip> strips;
    std::unique_ptr<Strip> master;
    std::vector<juce::String> signature;
};

} // namespace mcl
