#pragma once
#include "ui/Widgets.h"

namespace mcl
{

class PianoRoll : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit PianoRoll (AudioEngine& e);
    ~PianoRoll() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Grid;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    Track* getTrack() const;
    void pushClip();
    void pullClip (bool force);

    AudioEngine& engine;
    std::unique_ptr<Grid> grid;
    juce::Viewport viewport;
    juce::ComboBox lengthBox, gridBox;
    juce::TextButton quantizeBtn { "Cuantizar" }, upBtn { "+1 oct" }, downBtn { "-1 oct" }, semiUp { "+1" }, semiDown { "-1" },
                     clearBtn { "Borrar todo" }, dupBtn { "Duplicar x2" }, velBtn { "Humanizar" };
    juce::Label info;
    MidiClip clip;
    int trackUid = -1;
    size_t lastHash = 0;
    friend struct Grid;
};

} // namespace mcl
