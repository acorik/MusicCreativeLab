#pragma once
#include "ui/Widgets.h"

namespace mcl
{

// Right-hand panel: instrument + selected effect of the selected track.
class Inspector : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit Inspector (AudioEngine& e);
    ~Inspector() override;
    void resized() override;
    void paint (juce::Graphics& g) override;
    void selectEffect (int trackUid, int fxIndex);

private:
    struct DrumPads;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    void layoutContent();
    void showInstrumentMenu();

    AudioEngine& engine;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label title, instLabel, fxLabel;
    juce::ComboBox presetBox;
    juce::TextButton changeInst { "Cambiar instrumento" }, editorBtn { "Abrir GUI del VST" }, fxEditorBtn { "Abrir GUI del VST" }, addFxBtn { "+ Efecto" };
    std::unique_ptr<ParamGrid> instGrid, fxGrid, mixGrid;
    std::unique_ptr<DrumPads> pads;
    std::unique_ptr<ParamToggle> fxOn;
    juce::OwnedArray<juce::TextButton> fxTabs;
    int selectedPad = 0;

    int trackUid = -1, fxTrackUid = -2, fxIndex = -1;
    const void* lastInst = nullptr;
    std::vector<const void*> lastFx;
};

} // namespace mcl
