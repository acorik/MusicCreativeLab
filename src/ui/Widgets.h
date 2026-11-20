#pragma once
#include "engine/AudioEngine.h"
#include "ui/Theme.h"

namespace mcl
{

// Rotary/linear control bound to a Param, with right-click MIDI learn.
class ParamKnob : public juce::Component, private juce::Timer
{
public:
    ParamKnob (AudioEngine& e, Param& p, bool linear = false, juce::String selId = {});
    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    Param& getParam() { return param; }
    void setColour (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); slider.setColour (juce::Slider::trackColourId, c); }

    static void showLearnMenu (AudioEngine& engine, const juce::String& id, const juce::String& selId, juce::Component* target);

private:
    void timerCallback() override;
    AudioEngine& engine;
    Param& param;
    juce::String selectedId; // "sel.xxx" variant for track params
    juce::Slider slider;
    juce::Label label;
    bool linearStyle;
    juce::String mappedText;
};

// Button bound to an engine Action, with right-click MIDI learn.
class ActionButton : public juce::TextButton
{
public:
    ActionButton (AudioEngine& e, juce::String actionId, juce::String text);
    void mouseDown (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics& g, bool over, bool down) override;
private:
    AudioEngine& engine;
    juce::String id;
};

// Toggle button bound to a 0/1 Param (mute, solo, fx on...)
class ParamToggle : public juce::TextButton, private juce::Timer
{
public:
    ParamToggle (AudioEngine& e, Param& p, juce::String text, juce::Colour onColour, juce::String selId = {});
    void mouseDown (const juce::MouseEvent& e) override;
private:
    void timerCallback() override;
    AudioEngine& engine;
    Param& param;
    juce::String selectedId;
};

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter (std::atomic<float>& l, std::atomic<float>& r) : left (l), right (r) { startTimerHz (30); }
    void paint (juce::Graphics& g) override;
private:
    void timerCallback() override;
    std::atomic<float>& left;
    std::atomic<float>& right;
    float shownL = 0.0f, shownR = 0.0f;
};

// Generic grid of knobs for a list of params
class ParamGrid : public juce::Component
{
public:
    ParamGrid (AudioEngine& e) : engine (e) {}
    void setParams (const ParamList& params, juce::Colour colour, const juce::String& trackPrefix = {});
    void resized() override;
    int getIdealHeight (int width) const;
private:
    AudioEngine& engine;
    juce::OwnedArray<ParamKnob> knobs;
};

} // namespace mcl
