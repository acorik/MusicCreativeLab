#include "ui/Browser.h"
#include "project/Project.h"

namespace mcl
{

const std::vector<FreePlugin>& getRecommendedFreePlugins()
{
    static std::vector<FreePlugin> p {
        { "Surge XT",                  "Synth",       "https://surge-synthesizer.github.io/",            "Synth enorme y open source. Pads, leads, bajos: de todo." },
        { "Vital (version gratis)",    "Synth",       "https://vital.audio/",                            "Wavetable tipo Serum. Perfecto para synths de UK garage." },
        { "Odin 2",                    "Synth",       "https://thewavewarden.com/odin2",                 "Synth analogico open source, suena muy grande." },
        { "Dexed",                     "Synth FM",    "https://asb2m10.github.io/dexed/",                "Pianos electricos y campanas estilo DX7." },
        { "TAL-NoiseMaker",            "Synth",       "https://tal-software.com/products/tal-noisemaker","Synth analogico sencillo y calido." },
        { "Spitfire LABS",             "Pianos/Orq.", "https://labs.spitfireaudio.com/",                 "Pianos, cuerdas, guitarras, coros... (perfecto para el rollo emotivo)." },
        { "Decent Sampler",            "Sampler",     "https://www.decentsamples.com/product/decent-sampler-plugin/", "Cientos de librerias gratis: guitarras, orquesta, percusion." },
        { "Ample Guitar M Lite II",    "Guitarra",    "https://www.amplesound.net/en/pro-pd.asp?id=7",   "Guitarra acustica realista gratis." },
        { "Valhalla Supermassive",     "Reverb/Delay","https://valhalladsp.com/shop/reverb/valhalla-supermassive/", "Reverbs gigantes, el sonido 'Fred again'." },
        { "Kilohearts Essentials",     "Efectos",     "https://kilohearts.com/products/kilohearts_essentials", "30+ efectos: chorus, distortion, pitch, etc." },
        { "MeldaProduction MFreeFX",   "Efectos",     "https://www.meldaproduction.com/MFreeFXBundle",   "Pack de 30+ efectos de mezcla y creativos." },
        { "TDR Nova",                  "EQ dinamico", "https://www.tokyodawn.net/tdr-nova/",              "EQ profesional gratis, ideal para la voz." },
        { "Graillon 3 (free)",         "Voz",         "https://www.auburnsounds.com/products/Graillon.html", "Autotune / pitch para la voz del SM7B." },
    };
    return p;
}

Browser::Browser (AudioEngine& e) : engine (e)
{
    search.setTextToShowWhenEmpty ("Buscar...", Colours::textDim);
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, Colours::panel);
    addAndMakeVisible (list);
    for (auto* b : { &scanBtn, &folderBtn, &freeBtn }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); b->setVisible (! MCL_LITE); }
    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (status);

    scanBtn.onClick = [this] { engine.plugins.startScan(); };
    folderBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Carpeta donde tienes VST3", juce::File ("C:\\Program Files\\Common Files\\VST3"));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory()) { engine.plugins.addSearchFolder (fc.getResult()); engine.plugins.startScan(); }
        });
    };
    freeBtn.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Gratis y legales: se descargan desde su web oficial");
        auto& p = getRecommendedFreePlugins();
        for (int i = 0; i < (int) p.size(); ++i) m.addItem (i + 1, juce::String (p[(size_t) i].name) + "   [" + p[(size_t) i].type + "]");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&freeBtn), [] (int r)
        {
            if (r > 0) juce::URL (getRecommendedFreePlugins()[(size_t) r - 1].url).launchInDefaultBrowser();
        });
    };

    engine.plugins.addChangeListener (this);
    refresh();
    startTimerHz (2);
}

Browser::~Browser() { engine.plugins.removeChangeListener (this); }

void Browser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    status.setText (engine.plugins.getScanStatus(), juce::dontSendNotification);
    if (! engine.plugins.isScanning()) refresh();
}

void Browser::timerCallback()
{
    auto txt = MCL_LITE ? juce::String ("Version Lite (sin VST)  |  doble clic para anadir")
             : engine.plugins.isScanning() ? engine.plugins.getScanStatus()
                                           : juce::String (engine.plugins.known.getNumTypes()) + " VST en tu PC  |  doble clic para anadir";
    if (status.getText() != txt) status.setText (txt, juce::dontSendNotification);
}

void Browser::refresh()
{
    auto q = search.getText().trim();
    auto match = [&q] (const juce::String& s) { return q.isEmpty() || s.containsIgnoreCase (q); };
    items.clear();
    auto header = [this] (const juce::String& t) { items.push_back ({ Header, 0, t, true }); };

    header ("DEMOS - proyectos listos para tocar");
    auto demos = Project::getDemoNames();
    for (int i = 0; i < demos.size(); ++i) if (match (demos[i])) items.push_back ({ Demo, i, demos[i], false });

    header ("PISTAS (instrumento + efectos)");
    auto& presets = Project::getTrackPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (match (presets[(size_t) i].name)) items.push_back ({ Preset, i, presets[(size_t) i].name + "   | " + presets[(size_t) i].category, false });

    header ("EFECTOS (a la pista seleccionada)");
    auto& fx = getBuiltInEffectTypes();
    for (int i = 0; i < (int) fx.size(); ++i) if (match (fx[(size_t) i].name)) items.push_back ({ Fx, i, fx[(size_t) i].name, false });

    if (MCL_LITE) return; // Lite edition: no VST hosting

    header ("MIS VST - instrumentos");
    auto vi = engine.plugins.getInstruments();
    for (int i = 0; i < vi.size(); ++i) if (match (vi[i].name)) items.push_back ({ VstInst, i, vi[i].name + "   | " + vi[i].manufacturerName, false });
    if (vi.isEmpty()) items.push_back ({ Header, 0, "   (pulsa 'Escanear mis VST')", false });

    header ("MIS VST - efectos");
    auto vf = engine.plugins.getEffects();
    for (int i = 0; i < vf.size(); ++i) if (match (vf[i].name)) items.push_back ({ VstFx, i, vf[i].name + "   | " + vf[i].manufacturerName, false });

    list.updateContent();
    list.repaint();
}

void Browser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) items.size())) return;
    auto& it = items[(size_t) row];
    if (it.header)
    {
        g.setColour (Colours::accent);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (it.text, 6, 0, w - 8, h, juce::Justification::bottomLeft);
        return;
    }
    if (selected) { g.setColour (Colours::panelHi); g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f); }
    g.setColour (it.kind == Header ? Colours::textDim : Colours::text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (it.text, 14, 0, w - 16, h, juce::Justification::centredLeft, true);
}

juce::String Browser::getTooltipForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) items.size())) return {};
    auto& it = items[(size_t) row];
    if (it.kind == Demo) return Project::getDemoDescriptions()[it.index];
    if (it.kind == Fx) return "Doble clic: anadir a la pista seleccionada";
    return "Doble clic para anadir";
}

void Browser::listBoxItemClicked (int, const juce::MouseEvent&) {}
void Browser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&) { activate (row); }

void Browser::activate (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) items.size())) return;
    auto it = items[(size_t) row];
    auto* sel = engine.getSelectedTrack();
    switch (it.kind)
    {
        case Demo: if (onLoadDemo) onLoadDemo (it.index); break;
        case Preset: engine.selectTrack (Project::addTrackFromPreset (engine, Project::getTrackPresets()[(size_t) it.index])->uid); break;
        case Fx: if (sel) engine.addEffect (*sel, getBuiltInEffectTypes()[(size_t) it.index].id); break;
        case VstInst:
        {
            auto d = engine.plugins.getInstruments()[it.index];
            auto* t = engine.addTrack (TrackKind::Instrument, d.name);
            juce::String err;
            if (auto inst = engine.createPluginInstrument (d, *t, err))
            {
                engine.setInstrument (*t, std::move (inst));
                t->instrument->showEditor();
            }
            else
            {
                engine.setInstrument (*t, engine.createBuiltInInstrument ("synth", *t));
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", "No se pudo cargar " + d.name + ": " + err);
            }
            engine.selectTrack (t->uid);
            break;
        }
        case VstFx:
        {
            if (sel == nullptr) break;
            juce::String err;
            auto d = engine.plugins.getEffects()[it.index];
            if (auto fx = engine.createPluginEffect (d, sel->newFxPrefix(), err))
            {
                auto* raw = engine.addEffect (*sel, std::move (fx));
                raw->showEditor();
            }
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST", "No se pudo cargar " + d.name + ": " + err);
            break;
        }
        default: break;
    }
}

void Browser::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void Browser::resized()
{
    auto r = getLocalBounds().reduced (6);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);
    auto bottom = r.removeFromBottom (MCL_LITE ? 28 : 92);
    if (MCL_LITE) { status.setBounds (bottom); list.setBounds (r); return; }
    auto b1 = bottom.removeFromTop (30);
    folderBtn.setBounds (b1.removeFromRight (86).reduced (2));
    scanBtn.setBounds (b1.reduced (2));
    freeBtn.setBounds (bottom.removeFromTop (30).reduced (2));
    status.setBounds (bottom);
    list.setBounds (r);
}

} // namespace mcl
