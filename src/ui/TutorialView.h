#pragma once
#include "ui/Theme.h"

namespace mcl
{

struct Tutorial
{
    juce::String title;
    juce::StringArray steps;
    int demo = -1; // demo to load with the "Probar" button
};

const std::vector<Tutorial>& getTutorials();

class TutorialView : public juce::Component, private juce::ListBoxModel
{
public:
    TutorialView();
    void resized() override;
    void paint (juce::Graphics& g) override;
    std::function<void (int)> onLoadDemo;
    void showTutorial (int index);

private:
    int getNumRows() override { return (int) getTutorials().size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override { showTutorial (row); }
    void showStep();

    juce::ListBox list { "tutorials", this };
    juce::Label title, stepLabel;
    juce::TextEditor body;
    juce::TextButton prev { "< Anterior" }, next { "Siguiente >" }, tryDemo { "Cargar la demo de este tutorial" };
    int current = 0, step = 0;
};

} // namespace mcl
