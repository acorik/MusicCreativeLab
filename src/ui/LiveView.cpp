#include "ui/LiveView.h"
#include "project/Project.h"

namespace mcl
{

static void showAddTrackMenu (AudioEngine& engine, juce::Component* target)
{
    juce::PopupMenu m, synths, drums, mic, vst;
    auto& presets = Project::getTrackPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        auto& p = presets[(size_t) i];
        auto& menu = p.instrument == "drums" ? drums : p.instrument == "audio" ? mic : synths;
        menu.addItem (100 + i, p.name);
    }
    auto inst = engine.plugins.getInstruments();
    for (int i = 0; i < inst.size(); ++i) vst.addItem (1000 + i, inst[i].name + "  (" + inst[i].manufacturerName + ")");
    if (inst.isEmpty()) vst.addItem (-1, "Aun no hay VST escaneados (Navegador > Escanear VST)", false);

    m.addSubMenu ("Synths y bajos", synths);
    m.addSubMenu ("Baterias (MPD218)", drums);
    m.addSubMenu ("Micro / voz", mic);
    if (! MCL_LITE) m.addSubMenu ("Mis instrumentos VST", vst);
    m.addSeparator();
    m.addItem (2, "Pista Lab Synth vacia");
    m.addItem (3, "Pista de audio vacia");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&engine, presets, inst] (int r)
    {
        if (r >= 1000)
        {
            auto* t = engine.addTrack (TrackKind::Instrument, inst[r - 1000].name);
            juce::String err;
            if (auto i = engine.createPluginInstrument (inst[r - 1000], *t, err)) engine.setInstrument (*t, std::move (i));
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", "No se pudo cargar: " + err);
                engine.setInstrument (*t, engine.createBuiltInInstrument ("synth", *t));
            }
            engine.selectTrack (t->uid);
        }
        else if (r >= 100) engine.selectTrack (Project::addTrackFromPreset (engine, presets[(size_t) r - 100])->uid);
        else if (r == 2) { auto* t = engine.addTrack (TrackKind::Instrument, "Synth"); engine.setInstrument (*t, engine.createBuiltInInstrument ("synth", *t)); engine.selectTrack (t->uid); }
        else if (r == 3) engine.selectTrack (engine.addTrack (TrackKind::Audio, "Audio")->uid);
    });
}

//==============================================================================
struct LiveView::Row : public juce::Component, private juce::Timer
{
    Row (AudioEngine& e, Track& t, LiveView& o)
        : engine (e), track (t), owner (o),
          rec  (e, "t" + juce::String (t.uid) + ".record", "REC"),
          play (e, "t" + juce::String (t.uid) + ".toggle", juce::String::fromUTF8 ("\xe2\x96\xb6/\xe2\x96\xa0")),
          undo (e, "t" + juce::String (t.uid) + ".undo", "Undo"),
          dbl  (e, "t" + juce::String (t.uid) + ".double", "x2"),
          clear(e, "t" + juce::String (t.uid) + ".clear", "Borrar"),
          bars (t.loopBars, { "1 comp", "2 comp", "4 comp", "8 comp", "16 comp" }),
          quant (t.quantize, { "Sin cuant.", "1/4", "1/8", "1/16", "1/32" }),
          mode (t.recMode, { "Largo fijo", "Libre (silencios)" }),
          mute (e, t.mute, "M", juce::Colour (0xffffb347)),
          solo (e, t.solo, "S", juce::Colour (0xff36c8ff)),
          mon  (e, t.monitor, "Monitor", Colours::play)
    {
        rec.setColour (juce::TextButton::buttonColourId, Colours::rec.withAlpha (0.55f));
        for (auto* c : std::initializer_list<juce::Component*> { &rec, &play, &undo, &dbl, &clear, &bars, &quant, &mute, &solo })
            addAndMakeVisible (c);
        if (t.kind == TrackKind::Instrument) addAndMakeVisible (mode);
        else addAndMakeVisible (mon);
        mode.setTooltip ("Libre: la grabacion empieza con tu primer golpe y el loop se cierra solo tras 2 compases de silencio");
        bars.setTooltip ("Largo del loop en compases (modo fijo)");
        quant.setTooltip ("Cuantizacion al grabar (con swing del master)");
        startTimerHz (MCL_LITE ? 20 : 30);
    }

    void timerCallback() override
    {
        auto st = track.getClipState();
        if (st == ClipState::Playing || st == ClipState::Overdub || st == ClipState::Recording)
        {
            if (++noteRefresh % 15 == 0 && track.kind == TrackKind::Instrument) preview = track.getClipCopy();
        }
        else if (st != lastState) preview = track.getClipCopy();
        lastState = st;
        activity = juce::jmax (track.activity.exchange (0.0f), activity * 0.85f);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        engine.selectTrack (track.uid);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Renombrar");
            m.addItem (2, "Abrir piano roll", track.kind == TrackKind::Instrument);
            m.addItem (3, "Subir");
            m.addItem (4, "Bajar");
            m.addSeparator();
            m.addItem (5, "Crossfader: ninguno", true, (int) track.xfade.get() == 0);
            m.addItem (6, "Crossfader: lado A", true, (int) track.xfade.get() == 1);
            m.addItem (7, "Crossfader: lado B", true, (int) track.xfade.get() == 2);
            m.addSeparator();
            m.addItem (8, "Eliminar pista");
            auto uid = track.uid;
            auto* eng = &engine;
            auto* own = &owner;
            m.showMenuAsync ({}, [eng, own, uid] (int r)
            {
                auto* t = eng->getTrackByUid (uid);
                if (t == nullptr) return;
                if (r == 1)
                {
                    auto* w = new juce::AlertWindow ("Renombrar pista", {}, juce::MessageBoxIconType::NoIcon);
                    w->addTextEditor ("name", t->name);
                    w->addButton ("OK", 1); w->addButton ("Cancelar", 0);
                    w->enterModalState (true, juce::ModalCallbackFunction::create ([eng, uid, w] (int res)
                    {
                        if (res == 1) if (auto* tt = eng->getTrackByUid (uid)) { tt->name = w->getTextEditorContents ("name"); eng->sendChangeMessage(); }
                    }), true);
                }
                if (r == 2 && own->onShowPianoRoll) own->onShowPianoRoll (*t);
                if (r == 3) eng->moveTrack (uid, -1);
                if (r == 4) eng->moveTrack (uid, 1);
                if (r >= 5 && r <= 7) t->xfade.set ((float) (r - 5));
                if (r == 8) eng->removeTrack (uid);
            });
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (track.kind == TrackKind::Instrument && owner.onShowPianoRoll) owner.onShowPianoRoll (track);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f, 2.0f);
        const bool selected = engine.selectedTrack.load() == track.uid;
        g.setColour (selected ? Colours::panelHi.brighter (0.06f) : Colours::panel);
        g.fillRoundedRectangle (b, 6.0f);
        if (selected) { g.setColour (track.colour.withAlpha (0.8f)); g.drawRoundedRectangle (b, 6.0f, 1.5f); }

        auto strip = b.removeFromLeft (6.0f);
        g.setColour (track.colour.withAlpha (0.4f + 0.6f * activity));
        g.fillRoundedRectangle (strip, 3.0f);

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (track.name, nameArea, juce::Justification::centredLeft, true);
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (11.5f));
        auto sub = track.kind == TrackKind::Audio ? juce::String ("Audio / micro") :
                   track.instrument ? track.instrument->getDisplayName() + " - " + track.instrument->getCurrentPresetName() : juce::String ("sin instrumento");
        g.drawText (sub, nameArea.translated (0, 18), juce::Justification::centredLeft, true);

        // clip lane
        auto lane = clipArea.toFloat();
        g.setColour (Colours::bg);
        g.fillRoundedRectangle (lane, 4.0f);
        auto st = track.getClipState();
        auto len = juce::jmax (0.25, track.getClipLength());
        // free-mode recording: the clip is open-ended, so show it growing in powers of two bars
        const bool freeRec = st == ClipState::Recording && preview.lengthBeats > 4.0 * 32.0;
        if (freeRec)
        {
            auto need = juce::jmax (4.0, track.getClipPosition() + 0.5);
            for (auto& n : preview.notes) need = juce::jmax (need, n.start + n.length);
            double l = 4.0;
            while (l < need) l *= 2.0;
            len = l;
        }
        auto stateCol = st == ClipState::Recording ? Colours::rec : st == ClipState::Overdub ? juce::Colour (0xffff9f5a)
                      : st == ClipState::Playing ? track.colour : st == ClipState::Armed || st == ClipState::WaitingForNote ? Colours::rec.withAlpha (0.5f)
                      : Colours::outline;

        // mini notes
        if (track.kind == TrackKind::Instrument && ! preview.notes.empty())
        {
            int lo = 127, hi = 0;
            for (auto& n : preview.notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
            auto range = (float) juce::jmax (12, hi - lo + 1);
            auto plen = freeRec ? len : juce::jmax (0.25, preview.lengthBeats);
            g.setColour (track.colour.withAlpha (st == ClipState::Stopped ? 0.3f : 0.75f));
            for (auto& n : preview.notes)
            {
                auto x = lane.getX() + (float) (n.start / plen) * lane.getWidth();
                auto w = juce::jmax (2.0f, (float) (n.length / plen) * lane.getWidth());
                auto y = lane.getBottom() - 4.0f - ((float) (n.pitch - lo + 1) / range) * (lane.getHeight() - 8.0f);
                g.fillRect (x, y, w, 2.5f);
            }
        }
        else if (track.kind == TrackKind::Audio && track.hasContent())
        {
            g.setColour (track.colour.withAlpha (0.35f));
            g.fillRoundedRectangle (lane.reduced (2.0f, lane.getHeight() * 0.3f), 3.0f);
        }

        // bar lines
        g.setColour (Colours::outline.withAlpha (0.6f));
        for (double bb = 4.0; bb < len; bb += 4.0)
            g.drawVerticalLine ((int) (lane.getX() + (float) (bb / len) * lane.getWidth()), lane.getY(), lane.getBottom());

        if (st == ClipState::Playing || st == ClipState::Overdub || st == ClipState::Recording)
        {
            auto x = lane.getX() + (float) (track.getClipPosition() / len) * lane.getWidth();
            g.setColour (stateCol);
            g.fillRect (x - 1.0f, lane.getY(), 2.5f, lane.getHeight());
        }
        g.setColour (stateCol);
        g.drawRoundedRectangle (lane, 4.0f, st == ClipState::Empty ? 1.0f : 2.0f);

        auto label = clipStateName (st);
        if (st == ClipState::Playing || st == ClipState::Overdub || st == ClipState::Recording)
            label << (freeRec ? "  (libre) " : "  ") << juce::String (len / 4.0, 0) << " comp";
        if (st == ClipState::Empty) label = "Vacio: pulsa REC";
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (st == ClipState::Empty ? Colours::textDim : Colours::text);
        g.drawText (label, clipArea.reduced (8, 2), juce::Justification::topLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 6);
        auto nameCol = r.removeFromLeft (150);
        nameArea = nameCol.withHeight (22).withY (r.getY() + 4);
        r.removeFromLeft (6);
        auto top = r.removeFromTop (28);
        r.removeFromTop (4);
        clipArea = r;

        auto place = [] (juce::Rectangle<int>& area, juce::Component& c, int w) { c.setBounds (area.removeFromLeft (w).reduced (2, 1)); };
        place (top, rec, 60); place (top, play, 50); place (top, undo, 52); place (top, dbl, 36); place (top, clear, 60);
        place (top, mute, 30); place (top, solo, 30);
        top.removeFromLeft (6);
        if (track.kind == TrackKind::Instrument) place (top, mode, 132); else place (top, mon, 90);
        place (top, quant, 92);
        place (top, bars, 86);
    }

    AudioEngine& engine;
    Track& track;
    LiveView& owner;
    ActionButton rec, play, undo, dbl, clear;
    ParamCombo bars, quant, mode;
    ParamToggle mute, solo, mon;
    juce::Rectangle<int> nameArea, clipArea;
    MidiClip preview;
    ClipState lastState = ClipState::Empty;
    int noteRefresh = 0;
    float activity = 0.0f;
};

//==============================================================================
struct LiveView::SceneColumn : public juce::Component, private juce::Timer
{
    explicit SceneColumn (AudioEngine& e) : engine (e), riser (e, "master.riser", "RISER  >  DROP")
    {
        for (int i = 0; i < 8; ++i)
        {
            auto* b = launch.add (new ActionButton (e, "scene." + juce::String (i + 1), "Escena " + juce::String (i + 1)));
            addAndMakeVisible (b);
            auto* s = save.add (new juce::TextButton ("Guardar"));
            s->setTooltip ("Guarda en esta escena que pistas suenan, sus volumenes y la energia");
            s->setWantsKeyboardFocus (false);
            s->onClick = [this, i] { engine.captureScene (i); };
            addAndMakeVisible (s);
        }
        riser.setColour (juce::TextButton::buttonColourId, Colours::accent2.withAlpha (0.6f));
        riser.setTooltip ("Sube la tension durante N compases (filtro + ruido) y lanza la siguiente escena justo en el DROP");
        addAndMakeVisible (riser);
        startTimerHz (15);
    }

    void timerCallback() override
    {
        for (int i = 0; i < launch.size(); ++i)
        {
            auto has = i < (int) engine.scenes.size();
            auto name = has ? engine.scenes[(size_t) i].name : "Escena " + juce::String (i + 1) + " (vacia)";
            if (launch[i]->getButtonText() != name) launch[i]->setButtonText (name);
            auto cur = engine.currentScene.load() == i, pend = engine.pendingScene.load() == i;
            launch[i]->setColour (juce::TextButton::buttonColourId,
                                  cur ? Colours::play.withAlpha (0.55f) : pend ? Colours::accent.withAlpha ((juce::Time::getMillisecondCounter() / 250) % 2 ? 0.6f : 0.2f)
                                      : has ? Colours::panelHi : Colours::panel);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("ESCENAS  (se lanzan al siguiente compas)", getLocalBounds().removeFromTop (28).reduced (10, 0), juce::Justification::centredLeft);
        if (engine.isRiserActive())
        {
            auto r = riser.getBounds().toFloat().reduced (2.0f);
            g.setColour (Colours::accent2);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * engine.getRiserProgress()), 4.0f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        r.removeFromTop (22);
        for (int i = 0; i < launch.size(); ++i)
        {
            auto row = r.removeFromTop (38);
            save[i]->setBounds (row.removeFromRight (70).reduced (2));
            launch[i]->setBounds (row.reduced (2));
        }
        r.removeFromTop (10);
        riser.setBounds (r.removeFromTop (54).reduced (2));
    }

    AudioEngine& engine;
    juce::OwnedArray<ActionButton> launch;
    juce::OwnedArray<juce::TextButton> save;
    ActionButton riser;
};

//==============================================================================
LiveView::LiveView (AudioEngine& e) : engine (e)
{
    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    scenes = std::make_unique<SceneColumn> (e);
    addAndMakeVisible (*scenes);
    addTrackButton.setWantsKeyboardFocus (false);
    addTrackButton.onClick = [this] { showAddTrackMenu (engine, &addTrackButton); };
    rowsHolder.addAndMakeVisible (addTrackButton);
    engine.addChangeListener (this);
    rebuild();
    startTimerHz (2);
}

LiveView::~LiveView() { engine.removeChangeListener (this); }

void LiveView::changeListenerCallback (juce::ChangeBroadcaster*) { rebuild(); }

void LiveView::timerCallback()
{
    std::vector<int> uids;
    for (int i = 0; i < engine.getNumTracks(); ++i) uids.push_back (engine.getTrack (i)->uid);
    if (uids != lastUids) rebuild();
}

void LiveView::rebuild()
{
    std::vector<int> uids;
    for (int i = 0; i < engine.getNumTracks(); ++i) uids.push_back (engine.getTrack (i)->uid);
    if (uids != lastUids)
    {
        rows.clear();
        for (int i = 0; i < engine.getNumTracks(); ++i)
        {
            auto* r = rows.add (new Row (engine, *engine.getTrack (i), *this));
            rowsHolder.addAndMakeVisible (r);
        }
        lastUids = uids;
        resized();
    }
    for (auto* r : rows) r->repaint();
}

void LiveView::paint (juce::Graphics& g) { g.fillAll (Colours::bg); }

juce::Component* LiveView::getRowComponent (int index) { return juce::isPositiveAndBelow (index, rows.size()) ? rows[index] : nullptr; }
juce::Component* LiveView::getScenesComponent() { return scenes.get(); }

void LiveView::resized()
{
    auto r = getLocalBounds().reduced (6);
    scenes->setBounds (r.removeFromRight (220));
    viewport.setBounds (r);
    const int rowH = 92;
    auto w = r.getWidth() - (rows.size() * rowH + 50 > r.getHeight() ? viewport.getScrollBarThickness() : 0);
    rowsHolder.setSize (w, rows.size() * rowH + 50);
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * rowH, w, rowH);
    addTrackButton.setBounds (8, rows.size() * rowH + 8, 140, 32);
}

} // namespace mcl
