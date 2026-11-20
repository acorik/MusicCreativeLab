#pragma once
#include "ui/Widgets.h"

namespace mcl
{

struct FreePlugin { const char* name; const char* type; const char* url; const char* note; };
const std::vector<FreePlugin>& getRecommendedFreePlugins();

// Left panel: demos, track presets, built-in effects and the user's scanned VST3 plugins.
class Browser : public juce::Component, private juce::ListBoxModel, private juce::ChangeListener, private juce::Timer
{
public:
    explicit Browser (AudioEngine& e);
    ~Browser() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (int)> onLoadDemo;

private:
    struct Item { int kind; int index; juce::String text; bool header; };
    enum Kind { Header, Demo, Preset, Fx, VstInst, VstFx };

    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void activate (int row);

    AudioEngine& engine;
    juce::TextEditor search;
    juce::ListBox list { "browser", this };
    juce::TextButton scanBtn { "Escanear mis VST" }, folderBtn { "+ Carpeta" }, freeBtn { "VST gratis recomendados" };
    juce::Label status;
    std::vector<Item> items;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace mcl
