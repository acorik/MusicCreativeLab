#pragma once
#include "ui/Widgets.h"
#include "ui/LiveView.h"
#include "ui/MixerView.h"
#include "ui/PianoRoll.h"
#include "ui/Inspector.h"
#include "ui/Browser.h"
#include "ui/MappingView.h"
#include "ui/TutorialView.h"
#include "control/OscControl.h"

namespace mcl
{

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent (juce::ApplicationProperties& props);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void saveAutosave();
    void openFile (const juce::File& f);

private:
    void timerCallback() override;
    void showFileMenu();
    void showAudioSettings();
    void loadDemo (int index, bool confirm);
    void saveProject (bool saveAs);
    void openProject();
    void setProjectFile (const juce::File& f);
    bool triggerAction (const juce::String& id);

    juce::ApplicationProperties& properties;
    AudioEngine engine;
    OscControl osc { engine };

    // top bar
    juce::TextButton fileBtn { "Archivo" }, audioBtn { "Audio" };
    ActionButton playBtn, stopBtn, recBtn, metroBtn, tapBtn, recSetBtn, panicBtn;
    juce::Slider bpmSlider;
    juce::Label bpmLabel, posLabel, cpuLabel, projectLabel;
    std::unique_ptr<ParamKnob> energyKnob, swingKnob, riserKnob, xfadeKnob;
    juce::TextButton learnBtn { "MIDI LEARN" };
    std::unique_ptr<LevelMeter> masterMeter;

    Browser browser;
    Inspector inspector;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    LiveView* live = nullptr;
    MixerView* mixer = nullptr;
    PianoRoll* pianoRoll = nullptr;
    MappingView* mapping = nullptr;
    TutorialView* tutorials = nullptr;

    juce::MidiKeyboardComponent keyboard { engine.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File projectFile;
    int tick = 0, keyOctave = 4;
};

} // namespace mcl
