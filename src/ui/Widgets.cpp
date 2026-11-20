#include "ui/Widgets.h"

namespace mcl
{

void ParamKnob::showLearnMenu (AudioEngine& engine, const juce::String& id, const juce::String& selId, juce::Component* target)
{
    juce::PopupMenu m;
    auto current = engine.mapper.mappingLabelFor (id);
    auto currentSel = selId.isNotEmpty() ? engine.mapper.mappingLabelFor (selId) : juce::String();
    m.addSectionHeader (engine.describeTarget (id));
    m.addItem (1, "MIDI Learn (mueve un knob / pulsa un pad)");
    if (selId.isNotEmpty()) m.addItem (2, "MIDI Learn para la pista SELECCIONADA (sigue a la seleccion)");
    if (current.isNotEmpty() || currentSel.isNotEmpty())
        m.addItem (3, "Quitar mapeo MIDI (" + (current.isNotEmpty() ? current : currentSel) + ")");
    m.addSeparator();
    m.addItem (5, "Copiar direccion OSC (Stream Deck)");
    if (auto* p = engine.findParam (id)) { juce::ignoreUnused (p); m.addItem (4, "Valor por defecto"); }

    auto opts = target != nullptr ? juce::PopupMenu::Options().withTargetComponent (target) : juce::PopupMenu::Options();
    m.showMenuAsync (opts, [&engine, id, selId] (int r)
    {
        if (r == 1) engine.mapper.startLearn (id);
        if (r == 2) engine.mapper.startLearn (selId);
        if (r == 3) { engine.mapper.removeMappingsFor (id); if (selId.isNotEmpty()) engine.mapper.removeMappingsFor (selId); }
        if (r == 4) if (auto* p = engine.findParam (id)) p->set (p->defaultValue);
        if (r == 5)
        {
            auto isAction = engine.findAction (id) != nullptr;
            juce::SystemClipboard::copyTextToClipboard (isAction ? "/mcl/action/" + id : "/mcl/param/" + id);
        }
    });
}

//==============================================================================
ParamKnob::ParamKnob (AudioEngine& e, Param& p, bool linear, juce::String selId)
    : engine (e), param (p), selectedId (std::move (selId)), linearStyle (linear)
{
    slider.setSliderStyle (linear ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    slider.setNormalisableRange (p.range());
    slider.setDoubleClickReturnValue (true, p.defaultValue);
    slider.setValue (p.get(), juce::dontSendNotification);
    slider.setTextValueSuffix (p.suffix);
    slider.setNumDecimalPlacesToDisplay (p.maxValue - p.minValue > 50.0f ? 0 : 2);
    if (! p.choices.isEmpty())
    {
        slider.setRange (p.minValue, p.maxValue, 1.0);
        slider.textFromValueFunction = [this] (double v) { return param.choices[juce::jlimit (0, param.choices.size() - 1, (int) std::round (v - param.minValue))]; };
        slider.updateText();
    }
    slider.onValueChange = [this] { param.set ((float) slider.getValue()); };
    slider.addMouseListener (this, false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    label.setText (p.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setColour (juce::Label::textColourId, Colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    startTimerHz (20);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    slider.setBounds (r);
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto learnT = engine.mapper.getLearnTarget();
    if (learnT.isNotEmpty() && (learnT == param.id || learnT == selectedId))
    {
        g.setColour (Colours::learn.withAlpha (0.25f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (Colours::learn);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
    }
    else if (engine.mapper.learnModeUI.load())
    {
        g.setColour (Colours::learn.withAlpha (0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);
    }
    if (mappedText.isNotEmpty())
    {
        g.setColour (Colours::learn);
        g.fillEllipse ((float) getWidth() - 8.0f, 2.0f, 6.0f, 6.0f);
    }
}

void ParamKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showLearnMenu (engine, param.id, selectedId, this); return; }
    if (engine.mapper.learnModeUI.load()) engine.mapper.startLearn (selectedId.isNotEmpty() ? selectedId : param.id);
}

void ParamKnob::timerCallback()
{
    if (! slider.isMouseButtonDown())
    {
        auto v = (double) param.get();
        if (std::abs (slider.getValue() - v) > 1e-6) slider.setValue (v, juce::dontSendNotification);
    }
    auto mt = engine.mapper.mappingLabelFor (param.id);
    if (mt.isEmpty() && selectedId.isNotEmpty()) mt = engine.mapper.mappingLabelFor (selectedId);
    if (mt != mappedText) { mappedText = mt; slider.setTooltip (mt.isEmpty() ? juce::String() : "MIDI: " + mt); }
    repaint();
}

//==============================================================================
ActionButton::ActionButton (AudioEngine& e, juce::String actionId, juce::String text)
    : juce::TextButton (std::move (text)), engine (e), id (std::move (actionId))
{
    onClick = [this]
    {
        if (engine.mapper.learnModeUI.load()) { engine.mapper.startLearn (id); return; }
        if (auto* a = engine.findAction (id)) if (a->trigger) a->trigger();
    };
    setWantsKeyboardFocus (false);
    if (auto* a = engine.findAction (id)) setTooltip (a->name + "  (clic derecho: MIDI learn)");
}

void ActionButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { ParamKnob::showLearnMenu (engine, id, {}, this); return; }
    juce::TextButton::mouseDown (e);
}

void ActionButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    juce::TextButton::paintButton (g, over, down);
    if (engine.mapper.getLearnTarget() == id)
    {
        g.setColour (Colours::learn);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.5f);
    }
    else if (engine.mapper.mappingLabelFor (id).isNotEmpty())
    {
        g.setColour (Colours::learn);
        g.fillEllipse ((float) getWidth() - 7.0f, 3.0f, 4.0f, 4.0f);
    }
}

//==============================================================================
ParamToggle::ParamToggle (AudioEngine& e, Param& p, juce::String text, juce::Colour onColour, juce::String selId)
    : juce::TextButton (std::move (text)), engine (e), param (p), selectedId (std::move (selId))
{
    setClickingTogglesState (false);
    setColour (juce::TextButton::buttonOnColourId, onColour);
    setWantsKeyboardFocus (false);
    onClick = [this]
    {
        if (engine.mapper.learnModeUI.load()) { engine.mapper.startLearn (selectedId.isNotEmpty() ? selectedId : param.id); return; }
        param.set (param.get() > 0.5f ? 0.0f : 1.0f);
    };
    startTimerHz (15);
}

void ParamToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { ParamKnob::showLearnMenu (engine, param.id, selectedId, this); return; }
    juce::TextButton::mouseDown (e);
}

void ParamToggle::timerCallback()
{
    auto on = param.get() > 0.5f;
    if (on != getToggleState()) setToggleState (on, juce::dontSendNotification);
}

//==============================================================================
void LevelMeter::timerCallback()
{
    auto l = left.exchange (0.0f), r = right.exchange (0.0f);
    shownL = juce::jmax (l, shownL * 0.85f);
    shownR = juce::jmax (r, shownR * 0.85f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (b, 2.0f);
    auto draw = [&] (juce::Rectangle<float> r, float v)
    {
        auto db = juce::Decibels::gainToDecibels (v, -60.0f);
        auto norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * norm);
        juce::ColourGradient grad (juce::Colour (0xff4fe08a), r.getBottomLeft(), juce::Colour (0xffff4060), r.getTopLeft(), false);
        grad.addColour (0.75, juce::Colour (0xffffd34d));
        g.setGradientFill (grad);
        g.fillRect (fill);
    };
    auto half = b.reduced (1.0f);
    auto lr = half.removeFromLeft (half.getWidth() / 2.0f).reduced (0.5f, 0.0f);
    draw (lr, shownL);
    draw (half.reduced (0.5f, 0.0f), shownR);
}

//==============================================================================
void ParamGrid::setParams (const ParamList& params, juce::Colour colour, const juce::String& trackPrefix)
{
    knobs.clear();
    for (auto* p : params)
    {
        juce::String sel;
        if (trackPrefix.isNotEmpty() && p->id.startsWith (trackPrefix)) sel = "sel." + p->id.substring (trackPrefix.length());
        auto* k = knobs.add (new ParamKnob (engine, *p, false, sel));
        k->setColour (colour);
        addAndMakeVisible (k);
    }
    resized();
}

int ParamGrid::getIdealHeight (int width) const
{
    const int kw = 70, kh = 84;
    auto cols = juce::jmax (1, width / kw);
    auto rows = (knobs.size() + cols - 1) / cols;
    return rows * kh;
}

void ParamGrid::resized()
{
    const int kw = 70, kh = 84;
    auto cols = juce::jmax (1, getWidth() / kw);
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds ((i % cols) * kw, (i / cols) * kh, kw - 4, kh - 4);
}

} // namespace mcl
