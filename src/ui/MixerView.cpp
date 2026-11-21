#include "ui/MixerView.h"

namespace mcl
{

void showAddEffectMenu (AudioEngine& engine, Track* track, juce::Component* target, std::function<void (int)> onAdded)
{
    juce::PopupMenu m;
    std::map<juce::String, juce::PopupMenu> cats;
    auto& types = getBuiltInEffectTypes();
    for (int i = 0; i < (int) types.size(); ++i) cats[types[(size_t) i].category].addItem (1 + i, types[(size_t) i].name);
    for (auto& [name, sub] : cats) m.addSubMenu (name, sub);

    juce::PopupMenu vst;
    auto fx = engine.plugins.getEffects();
    for (int i = 0; i < fx.size(); ++i) vst.addItem (1000 + i, fx[i].name + "  (" + fx[i].manufacturerName + ")");
    if (fx.isEmpty()) vst.addItem (-1, "Aun no hay VST escaneados", false);
    if (! MCL_LITE)
    {
        m.addSeparator();
        m.addSubMenu ("Mis efectos VST", vst);
    }

    auto uid = track != nullptr ? track->uid : -1;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&engine, uid, fx, onAdded] (int r)
    {
        if (r <= 0) return;
        auto* t = uid >= 0 ? engine.getTrackByUid (uid) : nullptr;
        if (uid >= 0 && t == nullptr) return;
        auto prefix = t != nullptr ? t->newFxPrefix() : engine.newMasterFxPrefix();
        std::unique_ptr<Effect> e;
        if (r >= 1000)
        {
            juce::String err;
            e = engine.createPluginEffect (fx[r - 1000], prefix, err);
            if (! e) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", "No se pudo cargar: " + err); return; }
        }
        else e = createBuiltInEffect (getBuiltInEffectTypes()[(size_t) r - 1].id, prefix);

        if (t != nullptr) engine.addEffect (*t, std::move (e));
        else engine.addMasterEffect (std::move (e));
        if (onAdded) onAdded (t != nullptr ? (int) t->effects.size() - 1 : (int) engine.getMasterEffects().size() - 1);
    });
}

//==============================================================================
struct MixerView::Strip : public juce::Component
{
    Strip (AudioEngine& e, Track* t, MixerView& o) : engine (e), track (t), owner (o)
    {
        if (track != nullptr)
        {
            fader = std::make_unique<ParamKnob> (e, track->volume, true, "sel.volume");
            panKnob = std::make_unique<ParamKnob> (e, track->pan, false, "sel.pan");
            mute = std::make_unique<ParamToggle> (e, track->mute, "M", juce::Colour (0xffffb347), "sel.mute");
            solo = std::make_unique<ParamToggle> (e, track->solo, "S", juce::Colour (0xff36c8ff), "sel.solo");
            meter = std::make_unique<LevelMeter> (track->peakL, track->peakR);
            fader->setColour (track->colour);
            panKnob->setColour (track->colour);
        }
        else
        {
            fader = std::make_unique<ParamKnob> (e, e.masterVolume, true);
            panKnob = std::make_unique<ParamKnob> (e, e.crossfader, false);
            meter = std::make_unique<LevelMeter> (e.masterPeakL, e.masterPeakR);
        }
        addAndMakeVisible (*fader);
        addAndMakeVisible (*panKnob);
        addAndMakeVisible (*meter);
        if (mute) { addAndMakeVisible (*mute); addAndMakeVisible (*solo); }

        addFx.setButtonText ("+ FX");
        addFx.setWantsKeyboardFocus (false);
        addFx.onClick = [this]
        {
            auto uid = track != nullptr ? track->uid : -1;
            auto* own = &owner;
            showAddEffectMenu (engine, track, &addFx, [own, uid] (int idx) { if (own->onSelectEffect) own->onSelectEffect (uid, idx); });
        };
        addAndMakeVisible (addFx);
        rebuildFx();
    }

    std::vector<std::unique_ptr<Effect>>& fxList() { return track != nullptr ? track->effects : engine.getMasterEffects(); }

    void rebuildFx()
    {
        fxButtons.clear();
        auto& list = fxList();
        for (int i = 0; i < (int) list.size(); ++i)
        {
            auto* b = fxButtons.add (new juce::TextButton (list[(size_t) i]->getDisplayName()));
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, list[(size_t) i]->isOn() ? Colours::panelHi : Colours::bg);
            b->onClick = [this, i]
            {
                if (juce::ModifierKeys::currentModifiers.isPopupMenu()) return;
                if (owner.onSelectEffect) owner.onSelectEffect (track != nullptr ? track->uid : -1, i);
            };
            b->addMouseListener (this, false);
            b->getProperties().set ("fxIndex", i);
            addAndMakeVisible (b);
        }
        resized();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (e.eventComponent))
        {
            if (! e.mods.isPopupMenu() || ! b->getProperties().contains ("fxIndex")) return;
            auto idx = (int) b->getProperties()["fxIndex"];
            juce::PopupMenu m;
            m.addItem (1, "Activar / desactivar");
            m.addItem (2, "Subir");
            m.addItem (3, "Bajar");
            m.addItem (4, "Eliminar");
            auto uid = track != nullptr ? track->uid : -1;
            auto* eng = &engine;
            m.showMenuAsync ({}, [eng, uid, idx] (int r)
            {
                auto* t = uid >= 0 ? eng->getTrackByUid (uid) : nullptr;
                auto& list = t != nullptr ? t->effects : eng->getMasterEffects();
                if (! juce::isPositiveAndBelow (idx, (int) list.size())) return;
                if (r == 1) { auto* p = list[(size_t) idx]->getParams()[0]; p->set (p->get() > 0.5f ? 0.0f : 1.0f); eng->sendChangeMessage(); }
                if (r == 2 && t) eng->moveEffect (*t, idx, -1);
                if (r == 3 && t) eng->moveEffect (*t, idx, 1);
                if (r == 4) { if (t) eng->removeEffect (*t, idx); else eng->removeMasterEffect (idx); }
            });
        }
        else if (track != nullptr) engine.selectTrack (track->uid);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const bool sel = track != nullptr && engine.selectedTrack.load() == track->uid;
        g.setColour (sel ? Colours::panelHi : Colours::panel);
        g.fillRoundedRectangle (b, 6.0f);
        auto col = track != nullptr ? track->colour : Colours::accent;
        g.setColour (col);
        g.fillRoundedRectangle (b.removeFromTop (6.0f), 3.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawFittedText (track != nullptr ? track->name : juce::String ("MASTER"), getLocalBounds().reduced (6, 8).removeFromTop (18), juce::Justification::centred, 1);
        if (track != nullptr)
        {
            auto side = (int) track->xfade.get();
            if (side > 0)
            {
                g.setColour (Colours::accent2);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (side == 1 ? "XF: A" : "XF: B", getLocalBounds().reduced (6, 26).removeFromTop (14), juce::Justification::centred);
            }
        }
        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("FX", fxArea.withHeight (14).translated (0, -15), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        r.removeFromTop (40);
        fxArea = r.removeFromTop (8 * 22 + 30).withTrimmedTop (16);
        auto fa = fxArea;
        for (auto* b : fxButtons) b->setBounds (fa.removeFromTop (22).reduced (0, 1));
        addFx.setBounds (fa.removeFromTop (24).reduced (0, 1));
        r.removeFromTop (6);
        panKnob->setBounds (r.removeFromTop (78).withSizeKeepingCentre (70, 78));
        if (mute)
        {
            auto ms = r.removeFromBottom (28);
            mute->setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (2));
            solo->setBounds (ms.reduced (2));
        }
        auto meterArea = r.removeFromRight (14);
        meter->setBounds (meterArea.reduced (0, 18));
        fader->setBounds (r);
    }

    AudioEngine& engine;
    Track* track;
    MixerView& owner;
    std::unique_ptr<ParamKnob> fader, panKnob;
    std::unique_ptr<ParamToggle> mute, solo;
    std::unique_ptr<LevelMeter> meter;
    juce::OwnedArray<juce::TextButton> fxButtons;
    juce::TextButton addFx;
    juce::Rectangle<int> fxArea;
};

//==============================================================================
MixerView::MixerView (AudioEngine& e) : engine (e)
{
    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    engine.addChangeListener (this);
    rebuild (true);
    startTimerHz (4);
}

MixerView::~MixerView() { engine.removeChangeListener (this); }

void MixerView::changeListenerCallback (juce::ChangeBroadcaster*) { rebuild (false); }
void MixerView::timerCallback() { rebuild (false); }

void MixerView::rebuild (bool force)
{
    std::vector<juce::String> sig;
    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        auto* t = engine.getTrack (i);
        juce::String s (t->uid);
        s << t->name;
        for (auto& fx : t->effects) s << "|" << fx->getDisplayName() << (fx->isOn() ? "1" : "0");
        sig.push_back (s);
    }
    juce::String ms ("master");
    for (auto& fx : engine.getMasterEffects()) ms << "|" << fx->getDisplayName() << (fx->isOn() ? "1" : "0");
    sig.push_back (ms);

    if (! force && sig == signature) { for (auto* s : strips) s->repaint(); return; }
    signature = sig;

    strips.clear();
    master.reset();
    for (int i = 0; i < engine.getNumTracks(); ++i)
        holder.addAndMakeVisible (strips.add (new Strip (engine, engine.getTrack (i), *this)));
    master = std::make_unique<Strip> (engine, nullptr, *this);
    addAndMakeVisible (*master);
    resized();
}

void MixerView::paint (juce::Graphics& g) { g.fillAll (Colours::bg); }

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (6);
    const int w = 128;
    if (master) master->setBounds (r.removeFromRight (w + 20));
    viewport.setBounds (r);
    holder.setSize (juce::jmax (r.getWidth(), strips.size() * w), r.getHeight() - viewport.getScrollBarThickness());
    for (int i = 0; i < strips.size(); ++i) strips[i]->setBounds (i * w, 0, w, holder.getHeight());
}

} // namespace mcl
