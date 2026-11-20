#pragma once
#include <JuceHeader.h>

namespace mcl
{

namespace Colours
{
    const juce::Colour bg        { 0xff15141c };
    const juce::Colour panel     { 0xff1e1d28 };
    const juce::Colour panelHi   { 0xff272636 };
    const juce::Colour outline   { 0xff35334a };
    const juce::Colour text      { 0xffe9e6f5 };
    const juce::Colour textDim   { 0xff9a96b0 };
    const juce::Colour accent    { 0xffff7a45 };  // orange (FL vibe)
    const juce::Colour accent2   { 0xffb57bff };  // purple
    const juce::Colour rec       { 0xffff4060 };
    const juce::Colour play      { 0xff4fe08a };
    const juce::Colour learn     { 0xff36c8ff };
}

class Theme : public juce::LookAndFeel_V4
{
public:
    Theme()
    {
        using namespace juce;
        setColourScheme ({ Colours::bg, Colours::panel, Colours::panelHi, Colours::outline, Colours::text,
                           Colours::accent, Colours::text, Colours::panelHi, Colours::text });
        setColour (ResizableWindow::backgroundColourId, Colours::bg);
        setColour (TextButton::buttonColourId, Colours::panelHi);
        setColour (TextButton::buttonOnColourId, Colours::accent);
        setColour (TextButton::textColourOffId, Colours::text);
        setColour (TextButton::textColourOnId, Colours::bg);
        setColour (Slider::thumbColourId, Colours::accent);
        setColour (Slider::rotarySliderFillColourId, Colours::accent);
        setColour (Slider::rotarySliderOutlineColourId, Colours::outline);
        setColour (Slider::trackColourId, Colours::accent);
        setColour (Slider::backgroundColourId, Colours::outline);
        setColour (Slider::textBoxOutlineColourId, Colours::panel.withAlpha (0.0f));
        setColour (Slider::textBoxTextColourId, Colours::textDim);
        setColour (ComboBox::backgroundColourId, Colours::panelHi);
        setColour (ComboBox::outlineColourId, Colours::outline);
        setColour (PopupMenu::backgroundColourId, Colours::panel);
        setColour (PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.8f));
        setColour (ListBox::backgroundColourId, Colours::panel);
        setColour (TabbedButtonBar::tabOutlineColourId, Colours::outline);
        setColour (TabbedButtonBar::frontOutlineColourId, Colours::accent);
        setColour (TextEditor::backgroundColourId, Colours::panelHi);
        setColour (TextEditor::outlineColourId, Colours::outline);
        setColour (TreeView::backgroundColourId, Colours::panel);
        setColour (ScrollBar::thumbColourId, Colours::outline);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        auto r = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto c = bounds.getCentre();
        auto angle = start + pos * (end - start);
        auto lw = jmax (2.5f, r * 0.18f);
        auto fill = s.findColour (Slider::rotarySliderFillColourId);

        Path bg; bg.addCentredArc (c.x, c.y, r - lw, r - lw, 0.0f, start, end, true);
        g.setColour (Colours::outline);
        g.strokePath (bg, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));

        Path v; v.addCentredArc (c.x, c.y, r - lw, r - lw, 0.0f, start, angle, true);
        g.setColour (fill);
        g.strokePath (v, PathStrokeType (lw, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour (Colours::panelHi);
        g.fillEllipse (Rectangle<float> (r * 1.1f, r * 1.1f).withCentre (c));
        Point<float> tip (c.x + (r - lw * 2.2f) * std::sin (angle), c.y - (r - lw * 2.2f) * std::cos (angle));
        g.setColour (Colours::text);
        g.drawLine ({ c, tip }, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bgc;
        if (over) c = c.brighter (0.12f);
        if (down) c = c.darker (0.2f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::outline.brighter (0.1f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) h * 0.55f), juce::Font::bold));
    }
};

} // namespace mcl
