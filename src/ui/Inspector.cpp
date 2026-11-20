#include "ui/Inspector.h"
#include "ui/MixerView.h"
#include "dsp/DrumMachine.h"

namespace mcl
{

struct Inspector::DrumPads : public juce::Component, private juce::Timer
{
    DrumPads (AudioEngine& e, DrumMachine& d, Inspector& o) : engine (e), drums (d), owner (o) { startTimerHz (30); }

    juce::Rectangle<int> padBounds (int i) const
    {
        auto w = getWidth() / 4, h = getHeight() / 4;
        return { (i % 4) * w, (3 - i / 4) * h, w, h };
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < DrumMachine::numPads; ++i)
        {
            auto r = padBounds (i).toFloat().reduced (3.0f);
            auto flash = flashes[(size_t) i];
            auto base = i == owner.selectedPad ? Colours::panelHi.brighter (0.15f) : Colours::panelHi;
            g.setColour (base.interpolatedWith (Colours::accent, flash));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (i == owner.selectedPad ? Colours::accent : Colours::outline);
            g.drawRoundedRectangle (r, 6.0f, 1.5f);
            g.setColour (Colours::text);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawFittedText (drums.getPadName (i), r.toNearestInt().reduced (4), juce::Justification::centred, 2);
            g.setColour (Colours::textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::String (i + 1), r.reduced (5.0f), juce::Justification::topLeft);
        }
    }

    void timerCallback() override
    {
        for (int i = 0; i < DrumMachine::numPads; ++i)
            flashes[(size_t) i] = juce::jmax (drums.padFlash[(size_t) i].exchange (0.0f), flashes[(size_t) i] * 0.82f);
        repaint();
    }

    int padAt (juce::Point<int> p) const
    {
        for (int i = 0; i < DrumMachine::numPads; ++i) if (padBounds (i).contains (p)) return i;
        return -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto i = padAt (e.getPosition());
        if (i < 0) return;
        if (owner.selectedPad != i)
        {
            owner.selectedPad = i;
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Inspector> (&owner)] { if (sp != nullptr) sp->rebuild(); });
        }
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m, sounds;
            for (int s = 0; s < (int) DrumMachine::Sound::NumSounds; ++s)
                sounds.addItem (10 + s, DrumMachine::soundName ((DrumMachine::Sound) s), true, drums.getPadSound (i) == (DrumMachine::Sound) s);
            m.addSubMenu ("Sonido sintetizado", sounds);
            m.addItem (1, "Cargar sample (.wav / .aiff / .flac)...");
            m.addItem (2, "Quitar sample", drums.getSampleFile (i).existsAsFile());
            auto* d = &drums;
            m.showMenuAsync ({}, [this, d, i] (int r)
            {
                if (r >= 10) { d->setPadSound (i, (DrumMachine::Sound) (r - 10)); d->clearSample (i); }
                if (r == 2) d->clearSample (i);
                if (r == 1)
                {
                    chooser = std::make_unique<juce::FileChooser> ("Elige un sample para el pad " + juce::String (i + 1),
                                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
                    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                          [d, i] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) d->loadSample (i, fc.getResult()); });
                }
                repaint();
            });
            return;
        }
        engine.keyboardState.noteOn (1, DrumMachine::firstNote + i, 0.9f);
        held = i;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (held >= 0) engine.keyboardState.noteOff (1, DrumMachine::firstNote + held, 0.0f);
        held = -1;
    }

    AudioEngine& engine;
    DrumMachine& drums;
    Inspector& owner;
    std::array<float, DrumMachine::numPads> flashes {};
    int held = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
Inspector::Inspector (AudioEngine& e) : engine (e)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    instLabel.setColour (juce::Label::textColourId, Colours::textDim);
    fxLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    fxLabel.setText ("EFECTOS DE LA PISTA", juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &instLabel, &presetBox, &changeInst, &editorBtn, &fxLabel, &fxEditorBtn, &addFxBtn })
    {
        c->setWantsKeyboardFocus (false);
        content.addAndMakeVisible (c);
    }

    presetBox.onChange = [this]
    {
        if (auto* t = engine.getTrackByUid (trackUid))
            if (t->instrument && presetBox.getSelectedItemIndex() >= 0) { t->instrument->loadPreset (presetBox.getSelectedItemIndex()); engine.sendChangeMessage(); }
    };
    changeInst.onClick = [this] { showInstrumentMenu(); };
    editorBtn.onClick = [this] { if (auto* t = engine.getTrackByUid (trackUid)) if (t->instrument) t->instrument->showEditor(); };
    fxEditorBtn.onClick = [this]
    {
        auto* t = fxTrackUid >= 0 ? engine.getTrackByUid (fxTrackUid) : nullptr;
        auto& list = t != nullptr ? t->effects : engine.getMasterEffects();
        if (juce::isPositiveAndBelow (fxIndex, (int) list.size())) list[(size_t) fxIndex]->showEditor();
    };
    addFxBtn.onClick = [this]
    {
        auto* t = fxTrackUid >= 0 ? engine.getTrackByUid (fxTrackUid) : nullptr;
        auto uid = fxTrackUid;
        showAddEffectMenu (engine, t, &addFxBtn, [this, uid] (int idx) { selectEffect (uid, idx); });
    };

    engine.addChangeListener (this);
    rebuild();
    startTimerHz (4);
}

Inspector::~Inspector() { engine.removeChangeListener (this); }

void Inspector::selectEffect (int uid, int idx)
{
    fxTrackUid = uid;
    fxIndex = idx;
    lastInst = reinterpret_cast<const void*> (1); // force rebuild
    rebuild();
}

void Inspector::changeListenerCallback (juce::ChangeBroadcaster*) { rebuild(); }

void Inspector::timerCallback()
{
    // detect instrument/fx changes made elsewhere
    auto* t = engine.getTrackByUid (engine.selectedTrack.load());
    if (t == nullptr) return;
    std::vector<const void*> fx;
    for (auto& f : t->effects) fx.push_back (f.get());
    if (t->uid != trackUid || t->instrument.get() != lastInst || fx != lastFx) rebuild();
}

void Inspector::showInstrumentMenu()
{
    auto* t = engine.getTrackByUid (trackUid);
    if (t == nullptr || t->kind != TrackKind::Instrument) return;
    juce::PopupMenu m, vst;
    m.addItem (1, "Lab Synth (synth integrado)");
    m.addItem (2, "Lab Drums (bateria de 16 pads)");
    auto inst = engine.plugins.getInstruments();
    for (int i = 0; i < inst.size(); ++i) vst.addItem (100 + i, inst[i].name + "  (" + inst[i].manufacturerName + ")");
    if (inst.isEmpty()) vst.addItem (-1, "Aun no hay VST escaneados", false);
    m.addSubMenu ("Mis instrumentos VST", vst);
    auto uid = trackUid;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&changeInst), [this, uid, inst] (int r)
    {
        auto* tt = engine.getTrackByUid (uid);
        if (tt == nullptr || r <= 0) return;
        std::unique_ptr<Instrument> ni;
        if (r == 1) ni = engine.createBuiltInInstrument ("synth", *tt);
        else if (r == 2) ni = engine.createBuiltInInstrument ("drums", *tt);
        else
        {
            juce::String err;
            ni = engine.createPluginInstrument (inst[r - 100], *tt, err);
            if (! ni) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", "No se pudo cargar: " + err); return; }
        }
        engine.setInstrument (*tt, std::move (ni));
        if (tt->instrument && tt->instrument->hasEditor()) tt->instrument->showEditor();
    });
}

void Inspector::rebuild()
{
    auto* t = engine.getTrackByUid (engine.selectedTrack.load());
    auto newUid = t != nullptr ? t->uid : -1;
    if (newUid != trackUid) { selectedPad = 0; if (fxTrackUid != -1) { fxTrackUid = newUid; fxIndex = 0; } }
    trackUid = newUid;

    instGrid.reset(); fxGrid.reset(); pads.reset(); fxOn.reset();
    fxTabs.clear();
    lastFx.clear();
    lastInst = nullptr;

    if (t == nullptr)
    {
        title.setText ("Sin pista", juce::dontSendNotification);
        layoutContent();
        return;
    }

    title.setText (t->name, juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, t->colour);
    lastInst = t->instrument.get();
    for (auto& f : t->effects) lastFx.push_back (f.get());

    const bool isInst = t->kind == TrackKind::Instrument && t->instrument != nullptr;
    presetBox.setVisible (isInst);
    changeInst.setVisible (t->kind == TrackKind::Instrument);
    editorBtn.setVisible (isInst && t->instrument->hasEditor());
    if (isInst)
    {
        instLabel.setText (t->instrument->getDisplayName() + "   |   clic derecho en cualquier knob = MIDI learn", juce::dontSendNotification);
        presetBox.clear (juce::dontSendNotification);
        auto names = t->instrument->getPresetNames();
        presetBox.addItemList (names, 1);
        presetBox.setText (t->instrument->getCurrentPresetName(), juce::dontSendNotification);

        instGrid = std::make_unique<ParamGrid> (engine);
        auto params = t->instrument->getParams();
        if (auto* dm = dynamic_cast<DrumMachine*> (t->instrument.get()))
        {
            pads = std::make_unique<DrumPads> (engine, *dm, *this);
            content.addAndMakeVisible (*pads);
            ParamList sub { params[0], params[1] };
            for (int k = 0; k < 4; ++k) sub.push_back (params[(size_t) (2 + selectedPad * 4 + k)]);
            params = sub;
        }
        instGrid->setParams (params, t->colour, t->paramPrefix());
        content.addAndMakeVisible (*instGrid);
    }
    else
        instLabel.setText (t->kind == TrackKind::Audio ? "Pista de audio: entrada del micro / interfaz (Monocaster)" : "Sin instrumento",
                           juce::dontSendNotification);

    // mixer params of the track (input, xfade, etc)
    mixGrid = std::make_unique<ParamGrid> (engine);
    ParamList mix { &t->volume, &t->pan, &t->xfade };
    if (t->kind == TrackKind::Audio) { mix.push_back (&t->inputChannel); mix.push_back (&t->monitor); }
    mixGrid->setParams (mix, t->colour, t->paramPrefix());
    content.addAndMakeVisible (*mixGrid);

    // effects
    auto* fxTrack = fxTrackUid >= 0 ? engine.getTrackByUid (fxTrackUid) : nullptr;
    if (fxTrackUid >= 0 && fxTrack == nullptr) { fxTrackUid = trackUid; fxTrack = t; }
    auto& list = fxTrack != nullptr ? fxTrack->effects : engine.getMasterEffects();
    fxLabel.setText (fxTrack != nullptr ? "EFECTOS DE " + fxTrack->name.toUpperCase() : juce::String ("EFECTOS DEL MASTER"), juce::dontSendNotification);
    if (! juce::isPositiveAndBelow (fxIndex, (int) list.size())) fxIndex = list.empty() ? -1 : 0;

    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto* b = fxTabs.add (new juce::TextButton (list[(size_t) i]->getDisplayName()));
        b->setWantsKeyboardFocus (false);
        b->setToggleState (i == fxIndex, juce::dontSendNotification);
        b->setColour (juce::TextButton::buttonOnColourId, Colours::accent2);
        b->onClick = [this, i]
        {
            fxIndex = i;
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Inspector> (this)] { if (sp != nullptr) sp->rebuild(); });
        };
        content.addAndMakeVisible (b);
    }

    fxEditorBtn.setVisible (false);
    if (fxIndex >= 0)
    {
        auto& fx = *list[(size_t) fxIndex];
        auto params = fx.getParams();
        fxOn = std::make_unique<ParamToggle> (engine, *params[0], "ON", Colours::play);
        content.addAndMakeVisible (*fxOn);
        fxGrid = std::make_unique<ParamGrid> (engine);
        params.erase (params.begin());
        fxGrid->setParams (params, Colours::accent2, fxTrack != nullptr ? fxTrack->paramPrefix() : juce::String());
        content.addAndMakeVisible (*fxGrid);
        fxEditorBtn.setVisible (fx.hasEditor());
    }
    layoutContent();
}

void Inspector::layoutContent()
{
    auto w = juce::jmax (200, viewport.getWidth() - viewport.getScrollBarThickness());
    int y = 6;
    auto row = [&] (int h) { auto r = juce::Rectangle<int> (8, y, w - 16, h); y += h + 4; return r; };
    title.setBounds (row (26));
    instLabel.setBounds (row (18));
    if (presetBox.isVisible() || changeInst.isVisible())
    {
        auto r = row (28);
        if (presetBox.isVisible()) presetBox.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
        changeInst.setBounds (r.reduced (2, 0));
    }
    if (editorBtn.isVisible()) editorBtn.setBounds (row (28));
    if (pads) pads->setBounds (row (juce::jmin (260, w - 16)));
    if (instGrid) { auto h = instGrid->getIdealHeight (w - 16); instGrid->setBounds (row (h)); }
    if (mixGrid) { auto h = mixGrid->getIdealHeight (w - 16); mixGrid->setBounds (row (h)); }

    y += 8;
    fxLabel.setBounds (row (22));
    {
        int x = 8, rowY = y;
        for (auto* b : fxTabs)
        {
            auto bw = juce::jmin (w - 16, 20 + b->getButtonText().length() * 7);
            if (x + bw > w - 8) { x = 8; rowY += 28; }
            b->setBounds (x, rowY, bw, 26);
            x += bw + 4;
        }
        if (x + 90 > w - 8) { x = 8; rowY += 28; }
        addFxBtn.setBounds (x, rowY, 90, 26);
        y = rowY + 32;
    }
    if (fxOn)
    {
        auto r = row (28);
        fxOn->setBounds (r.removeFromLeft (60));
        if (fxEditorBtn.isVisible()) fxEditorBtn.setBounds (r.removeFromLeft (160).translated (8, 0));
    }
    if (fxGrid) { auto h = fxGrid->getIdealHeight (w - 16); fxGrid->setBounds (row (h)); }
    content.setSize (w, y + 20);
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void Inspector::resized()
{
    viewport.setBounds (getLocalBounds());
    layoutContent();
}

} // namespace mcl
