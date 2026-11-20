#include "ui/PianoRoll.h"

namespace mcl
{

static size_t hashClip (const MidiClip& c)
{
    size_t h = std::hash<double>() (c.lengthBeats);
    for (auto& n : c.notes)
        h = h * 31 + (size_t) n.pitch * 7 + std::hash<double>() (n.start) + std::hash<double>() (n.length) * 3 + (size_t) (n.velocity * 100.0f);
    return h;
}

//==============================================================================
struct PianoRoll::Grid : public juce::Component
{
    explicit Grid (PianoRoll& o) : owner (o) {}

    static constexpr int lowNote = 21, highNote = 108, rowH = 13, keysW = 54;
    double gridBeats() const
    {
        static const double g[] = { 1.0, 0.5, 0.25, 0.125 };
        return g[juce::jlimit (0, 3, owner.gridBox.getSelectedItemIndex())];
    }
    double beatsToX (double b) const { return keysW + b / juce::jmax (0.25, owner.clip.lengthBeats) * (getWidth() - keysW); }
    double xToBeats (double x) const { return (x - keysW) / (getWidth() - keysW) * juce::jmax (0.25, owner.clip.lengthBeats); }
    int yToPitch (int y) const { return highNote - y / rowH; }
    int pitchToY (int p) const { return (highNote - p) * rowH; }
    static bool isBlack (int p) { auto n = p % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

    int hitNote (juce::Point<int> pos) const
    {
        auto p = yToPitch (pos.y);
        auto b = xToBeats (pos.x);
        for (int i = (int) owner.clip.notes.size() - 1; i >= 0; --i)
        {
            auto& n = owner.clip.notes[(size_t) i];
            if (n.pitch == p && b >= n.start && b <= n.start + n.length) return i;
        }
        return -1;
    }

    void paint (juce::Graphics& g) override
    {
        auto& clip = owner.clip;
        const auto w = getWidth();
        for (int p = lowNote; p <= highNote; ++p)
        {
            auto y = pitchToY (p);
            g.setColour (isBlack (p) ? juce::Colour (0xff18171f) : juce::Colour (0xff1f1e29));
            g.fillRect (keysW, y, w - keysW, rowH);
            g.setColour (isBlack (p) ? juce::Colour (0xff222130) : juce::Colour (0xffd8d5e6));
            g.fillRect (0, y, keysW - (isBlack (p) ? 18 : 2), rowH - 1);
            if (p % 12 == 0)
            {
                g.setColour (Colours::bg);
                g.setFont (juce::FontOptions (10.0f));
                g.drawText ("C" + juce::String (p / 12 - 1), 2, y, keysW - 6, rowH, juce::Justification::centredRight);
                g.setColour (Colours::outline);
                g.drawHorizontalLine (y + rowH - 1, (float) keysW, (float) w);
            }
        }
        auto len = juce::jmax (0.25, clip.lengthBeats);
        for (double b = 0.0; b <= len + 1e-6; b += gridBeats())
        {
            auto x = (float) beatsToX (b);
            auto isBar = std::fmod (b, 4.0) < 1e-6, isBeat = std::fmod (b, 1.0) < 1e-6;
            g.setColour (isBar ? Colours::textDim.withAlpha (0.6f) : isBeat ? Colours::outline : Colours::outline.withAlpha (0.35f));
            g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
        }

        auto* t = owner.getTrack();
        auto col = t != nullptr ? t->colour : Colours::accent;
        for (size_t i = 0; i < clip.notes.size(); ++i)
        {
            auto& n = clip.notes[i];
            if (n.pitch < lowNote || n.pitch > highNote) continue;
            juce::Rectangle<float> r ((float) beatsToX (n.start), (float) pitchToY (n.pitch) + 1.0f,
                                      juce::jmax (4.0f, (float) (beatsToX (n.start + n.length) - beatsToX (n.start))), (float) rowH - 2.0f);
            g.setColour (col.withMultipliedBrightness (0.55f + 0.6f * n.velocity));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour ((int) i == dragIndex ? Colours::text : col.brighter (0.4f));
            g.drawRoundedRectangle (r, 2.5f, 1.0f);
        }

        if (t != nullptr)
        {
            auto st = t->getClipState();
            if (st == ClipState::Playing || st == ClipState::Overdub || st == ClipState::Recording)
            {
                auto x = (float) beatsToX (t->getClipPosition());
                g.setColour (Colours::text.withAlpha (0.8f));
                g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < keysW)
        {
            auditionPitch = yToPitch (e.y);
            owner.engine.keyboardState.noteOn (1, auditionPitch, 0.8f);
            return;
        }
        auto idx = hitNote (e.getPosition());
        if (e.mods.isPopupMenu() || (idx >= 0 && e.getNumberOfClicks() > 1))
        {
            if (idx >= 0) { owner.clip.notes.erase (owner.clip.notes.begin() + idx); owner.pushClip(); repaint(); }
            return;
        }
        auto snap = [this] (double b) { return std::floor (b / gridBeats() + 1e-6) * gridBeats(); };
        if (idx < 0)
        {
            Note n;
            n.pitch = yToPitch (e.y);
            n.start = juce::jlimit (0.0, owner.clip.lengthBeats - gridBeats(), snap (xToBeats (e.x)));
            n.length = gridBeats();
            n.velocity = 0.8f;
            owner.clip.notes.push_back (n);
            idx = (int) owner.clip.notes.size() - 1;
            resizing = true;
            owner.engine.keyboardState.noteOn (1, n.pitch, 0.8f);
            auditionPitch = n.pitch;
        }
        else
        {
            auto& n = owner.clip.notes[(size_t) idx];
            auto rightEdge = beatsToX (n.start + n.length);
            resizing = e.x > rightEdge - 6 || e.mods.isShiftDown();
            if (e.mods.isAltDown()) { velocityDrag = true; resizing = false; }
            auditionPitch = n.pitch;
            owner.engine.keyboardState.noteOn (1, n.pitch, n.velocity);
        }
        dragIndex = idx;
        dragStart = owner.clip.notes[(size_t) idx];
        dragOrigin = e.getPosition();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (dragIndex, (int) owner.clip.notes.size())) return;
        auto& n = owner.clip.notes[(size_t) dragIndex];
        auto g = gridBeats();
        if (velocityDrag)
            n.velocity = juce::jlimit (0.05f, 1.0f, dragStart.velocity - (float) (e.y - dragOrigin.y) / 150.0f);
        else if (resizing)
        {
            auto end = std::round (xToBeats (e.x) / g) * g;
            n.length = juce::jlimit (g * 0.5, owner.clip.lengthBeats - n.start, end - n.start);
            if (end - n.start < g) n.length = g;
        }
        else
        {
            auto db = std::round ((xToBeats (e.x) - xToBeats (dragOrigin.x)) / g) * g;
            n.start = juce::jlimit (0.0, owner.clip.lengthBeats - g * 0.5, dragStart.start + db);
            auto newPitch = juce::jlimit (lowNote, highNote, dragStart.pitch + (dragOrigin.y - e.y) / rowH);
            if (newPitch != n.pitch)
            {
                owner.engine.keyboardState.noteOff (1, auditionPitch, 0.0f);
                n.pitch = newPitch;
                auditionPitch = newPitch;
                owner.engine.keyboardState.noteOn (1, newPitch, n.velocity);
            }
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (auditionPitch >= 0) owner.engine.keyboardState.noteOff (1, auditionPitch, 0.0f);
        auditionPitch = -1;
        if (dragIndex >= 0) owner.pushClip();
        dragIndex = -1;
        resizing = velocityDrag = false;
        repaint();
    }

    PianoRoll& owner;
    int dragIndex = -1, auditionPitch = -1;
    bool resizing = false, velocityDrag = false;
    Note dragStart;
    juce::Point<int> dragOrigin;
};

//==============================================================================
PianoRoll::PianoRoll (AudioEngine& e) : engine (e)
{
    grid = std::make_unique<Grid> (*this);
    viewport.setViewedComponent (grid.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    lengthBox.addItemList ({ "1 compas", "2 compases", "4 compases", "8 compases", "16 compases" }, 1);
    lengthBox.onChange = [this]
    {
        static const double bars[] = { 1, 2, 4, 8, 16 };
        clip.lengthBeats = bars[juce::jlimit (0, 4, lengthBox.getSelectedItemIndex())] * 4.0;
        clip.notes.erase (std::remove_if (clip.notes.begin(), clip.notes.end(), [this] (const Note& n) { return n.start >= clip.lengthBeats; }), clip.notes.end());
        pushClip();
    };
    gridBox.addItemList ({ "Rejilla 1/4", "Rejilla 1/8", "Rejilla 1/16", "Rejilla 1/32" }, 1);
    gridBox.setSelectedItemIndex (2, juce::dontSendNotification);
    gridBox.onChange = [this] { grid->repaint(); };

    quantizeBtn.onClick = [this]
    {
        QuantizeSettings q; q.grid = grid->gridBeats(); q.swing = engine.swing.get();
        clip.quantize (q); pushClip();
    };
    upBtn.onClick    = [this] { clip.transpose (12); pushClip(); };
    downBtn.onClick  = [this] { clip.transpose (-12); pushClip(); };
    semiUp.onClick   = [this] { clip.transpose (1); pushClip(); };
    semiDown.onClick = [this] { clip.transpose (-1); pushClip(); };
    clearBtn.onClick = [this] { clip.notes.clear(); pushClip(); };
    dupBtn.onClick   = [this] { if (clip.lengthBeats <= 32.0) { clip.duplicate(); pushClip(); } };
    velBtn.onClick   = [this]
    {
        juce::Random r;
        for (auto& n : clip.notes) { n.velocity = juce::jlimit (0.1f, 1.0f, n.velocity + (r.nextFloat() - 0.5f) * 0.2f); n.start = juce::jmax (0.0, n.start + (r.nextDouble() - 0.5) * 0.02); }
        pushClip();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &lengthBox, &gridBox, &quantizeBtn, &downBtn, &semiDown, &semiUp, &upBtn, &dupBtn, &velBtn, &clearBtn, &info })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
    info.setColour (juce::Label::textColourId, Colours::textDim);
    info.setFont (juce::FontOptions (12.0f));
    engine.addChangeListener (this);
    pullClip (true);
    startTimerHz (25);
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PianoRoll> (this)]
    {
        if (sp != nullptr) sp->viewport.setViewPosition (0, (Grid::highNote - 84) * Grid::rowH);
    });
}

PianoRoll::~PianoRoll() { engine.removeChangeListener (this); }

Track* PianoRoll::getTrack() const
{
    auto* t = engine.getTrackByUid (trackUid);
    return t != nullptr && t->kind == TrackKind::Instrument ? t : nullptr;
}

void PianoRoll::changeListenerCallback (juce::ChangeBroadcaster*) { pullClip (false); }

void PianoRoll::pushClip()
{
    if (auto* t = getTrack())
    {
        clip.sort();
        t->setClipFromUI (clip, true);
        lastHash = hashClip (clip);
    }
    grid->repaint();
}

void PianoRoll::pullClip (bool force)
{
    auto sel = engine.selectedTrack.load();
    if (sel != trackUid) { trackUid = sel; force = true; }
    auto* t = getTrack();
    if (t == nullptr)
    {
        clip = MidiClip();
        info.setText ("Selecciona una pista de instrumento para ver su piano roll", juce::dontSendNotification);
        grid->repaint();
        return;
    }
    if (grid->dragIndex >= 0) return;
    auto c = t->getClipCopy();
    auto h = hashClip (c);
    if (force || h != lastHash)
    {
        clip = c;
        lastHash = h;
        static const double bars[] = { 1, 2, 4, 8, 16 };
        for (int i = 0; i < 5; ++i) if (std::abs (bars[i] * 4.0 - clip.lengthBeats) < 0.01) lengthBox.setSelectedItemIndex (i, juce::dontSendNotification);
        grid->repaint();
    }
    info.setText (t->name + "  |  clic: nota  |  arrastra: mover  |  borde/shift: largo  |  alt+arrastra: velocidad  |  doble clic / clic dcho: borrar",
                  juce::dontSendNotification);
}

void PianoRoll::timerCallback()
{
    if (engine.selectedTrack.load() != trackUid) pullClip (true);
    else if (auto* t = getTrack())
    {
        auto st = t->getClipState();
        if (st == ClipState::Recording || st == ClipState::Overdub || (juce::Time::getMillisecondCounter() / 40) % 10 == 0) pullClip (false);
    }
    grid->repaint();
}

void PianoRoll::paint (juce::Graphics& g) { g.fillAll (Colours::bg); }

void PianoRoll::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto bar = r.removeFromTop (34);
    lengthBox.setBounds (bar.removeFromLeft (130).reduced (2));
    gridBox.setBounds (bar.removeFromLeft (120).reduced (2));
    for (auto* b : { &quantizeBtn, &downBtn, &semiDown, &semiUp, &upBtn, &dupBtn, &velBtn, &clearBtn })
        b->setBounds (bar.removeFromLeft (b->getButtonText().length() > 4 ? 96 : 54).reduced (2));
    info.setBounds (r.removeFromBottom (20));
    viewport.setBounds (r);
    grid->setSize (r.getWidth() - viewport.getScrollBarThickness(), (Grid::highNote - Grid::lowNote + 1) * Grid::rowH);
}

} // namespace mcl
