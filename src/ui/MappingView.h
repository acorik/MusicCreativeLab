#pragma once
#include "ui/Widgets.h"
#include "control/OscControl.h"

namespace mcl
{

juce::String getShortcutHelpText();

// "Controladores" page: MIDI learn, mapping list, MIDI monitor, Stream Deck / OSC.
class MappingView : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    MappingView (AudioEngine& e, OscControl& osc);
    ~MappingView() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct MapModel;
    struct ActionModel;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    AudioEngine& engine;
    OscControl& osc;
    std::unique_ptr<MapModel> mapModel;
    std::unique_ptr<ActionModel> actionModel;
    juce::ListBox mapList, actionList;
    juce::TextButton learnMode { "MODO LEARN (clic en un control y luego mueve tu controlador)" },
                     cancelLearn { "Cancelar learn" }, defaults { "Restaurar mapeo por defecto" }, removeBtn { "Quitar mapeo seleccionado" },
                     padsToDrums { "Pads MPD218 / canal 10 -> bateria" };
    juce::Label learnStatus, monitor, devices, oscLabel;
    juce::TextEditor help;
};

} // namespace mcl
