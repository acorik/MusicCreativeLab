#pragma once
#include "ui/Widgets.h"

namespace mcl
{

class ParamCombo : public juce::ComboBox, private juce::Timer
{
public:
    ParamCombo (Param& p, const juce::StringArray& items) : param (p)
    {
        addItemList (items, 1);
        setSelectedItemIndex ((int) std::round (p.get()), juce::dontSendNotification);
        onChange = [this] { param.set ((float) getSelectedItemIndex()); };
        setWantsKeyboardFocus (false);
        startTimerHz (8);
    }
private:
    void timerCallback() override
    {
        auto i = (int) std::round (param.get());
        if (i != getSelectedItemIndex() && ! isPopupActive()) setSelectedItemIndex (i, juce::dontSendNotification);
    }
    Param& param;
};

// The "Live" page: one row per track with its looper, plus the scene launcher.
class LiveView : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit LiveView (AudioEngine& e);
    ~LiveView() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (Track&)> onShowPianoRoll;
    juce::Component* getRowComponent (int index);
    juce::Component* getScenesComponent();

private:
    struct Row;
    struct SceneColumn;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();

    AudioEngine& engine;
    juce::Viewport viewport;
    juce::Component rowsHolder;
    juce::OwnedArray<Row> rows;
    std::unique_ptr<SceneColumn> scenes;
    juce::TextButton addTrackButton { "+ Pista" };
    std::vector<int> lastUids;
};

} // namespace mcl
