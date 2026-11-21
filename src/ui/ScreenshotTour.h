#pragma once
// Developer tool: drives the UI through every screen and saves screenshots
// (plus the screen positions of important controls) for the illustrated guide.
// Run on Linux under Xvfb:  MCL_TOUR=/path/to/dir "Music Creative Lab"
// Captures use ImageMagick's `import -window root` so popup menus are included.

#include "project/Project.h"
#include "dsp/DrumMachine.h"

namespace mcl
{

struct MainComponent::Tour : private juce::Thread
{
    Tour (MainComponent& o, juce::File dir) : juce::Thread ("tour audio pump"), owner (o), outDir (std::move (dir))
    {
        outDir.createDirectory();
        buildSteps();
        // No sound card on the build machine: run the engine from a thread in real time
        owner.engine.deviceManager.closeAudioDevice();
        owner.engine.audioDeviceAboutToStart (&fake);
        startThread();
        juce::Timer::callAfterDelay (2500, [this] { runStep (0); });
    }

    ~Tour() override { stopThread (2000); }

private:
    struct FakeDevice : juce::AudioIODevice
    {
        FakeDevice() : AudioIODevice ("tour", "tour") {}
        juce::StringArray getOutputChannelNames() override { return { "L", "R" }; }
        juce::StringArray getInputChannelNames() override { return { "In" }; }
        juce::Array<double> getAvailableSampleRates() override { return { 48000.0 }; }
        juce::Array<int> getAvailableBufferSizes() override { return { 256 }; }
        int getDefaultBufferSize() override { return 256; }
        juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start (juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return 256; }
        double getCurrentSampleRate() override { return 48000.0; }
        int getCurrentBitDepth() override { return 24; }
        juce::BigInteger getActiveOutputChannels() const override { return 3; }
        juce::BigInteger getActiveInputChannels() const override { return 1; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
    };

    void run() override
    {
        juce::AudioBuffer<float> out (2, 256), in (1, 256);
        juce::Random r;
        auto next = juce::Time::getMillisecondCounterHiRes();
        while (! threadShouldExit())
        {
            // a little fake "voice" on the input so the mic meter moves
            for (int i = 0; i < 256; ++i) in.setSample (0, i, (r.nextFloat() - 0.5f) * 0.2f * (float) std::sin (phase += 0.0005));
            owner.engine.audioDeviceIOCallbackWithContext (in.getArrayOfReadPointers(), 1, out.getArrayOfWritePointers(), 2, 256, {});
            next += 256.0 / 48.0;
            auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0) juce::Thread::sleep ((int) wait);
        }
    }

    struct Step
    {
        std::function<void()> action;
        int waitMs;
        juce::String capture;               // empty = no capture
        std::function<void (juce::DynamicObject&)> regions;
    };

    // ---------- helpers ----------
    Track* trackNamed (const juce::String& n)
    {
        for (int i = 0; i < owner.engine.getNumTracks(); ++i)
            if (owner.engine.getTrack (i)->name == n) return owner.engine.getTrack (i);
        return nullptr;
    }
    int rowIndexOf (const juce::String& n)
    {
        for (int i = 0; i < owner.engine.getNumTracks(); ++i) if (owner.engine.getTrack (i)->name == n) return i;
        return 0;
    }
    void select (const juce::String& n) { if (auto* t = trackNamed (n)) owner.engine.selectTrack (t->uid); }

    static juce::Component* findByText (juce::Component* root, const juce::String& text)
    {
        if (root == nullptr) return nullptr;
        if (auto* b = dynamic_cast<juce::Button*> (root)) if (b->getButtonText() == text && b->isShowing()) return b;
        for (auto* c : root->getChildren()) if (auto* f = findByText (c, text)) return f;
        return nullptr;
    }
    static ParamKnob* findKnob (juce::Component* root, const juce::String& paramName)
    {
        if (root == nullptr) return nullptr;
        if (auto* k = dynamic_cast<ParamKnob*> (root)) if (k->getParam().name == paramName && k->isShowing()) return k;
        for (auto* c : root->getChildren()) if (auto* f = findKnob (c, paramName)) return f;
        return nullptr;
    }
    static juce::Component* findCombo (juce::Component* root, const juce::String& text)
    {
        if (root == nullptr) return nullptr;
        if (auto* cb = dynamic_cast<juce::ComboBox*> (root)) if (cb->getText() == text && cb->isShowing()) return cb;
        for (auto* c : root->getChildren()) if (auto* f = findCombo (c, text)) return f;
        return nullptr;
    }

    static juce::var rectVar (juce::Rectangle<int> r)
    {
        auto s = juce::Desktop::getInstance().getGlobalScaleFactor();
        juce::Array<juce::var> a { juce::roundToInt (r.getX() * s), juce::roundToInt (r.getY() * s),
                                   juce::roundToInt (r.getWidth() * s), juce::roundToInt (r.getHeight() * s) };
        return a;
    }
    static void put (juce::DynamicObject& o, const juce::String& name, juce::Component* c)
    {
        if (c != nullptr && c->isShowing()) o.setProperty (name, rectVar (c->getScreenBounds()));
    }

    void capture (const Step& s)
    {
        auto file = outDir.getChildFile (s.capture + ".png");
        juce::ChildProcess p;
        if (p.start (juce::StringArray { "import", "-window", "root", file.getFullPathName() })) p.waitForProcessToFinish (20000);

        auto* obj = new juce::DynamicObject();
        put (*obj, "window", &owner);
        if (s.regions) s.regions (*obj);
        regions.getDynamicObject()->setProperty (s.capture, juce::var (obj));
        outDir.getChildFile ("regions.json").replaceWithText (juce::JSON::toString (regions));
    }

    void runStep (size_t i)
    {
        if (i >= steps.size())
        {
            outDir.getChildFile ("done.txt").replaceWithText ("ok");
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return;
        }
        if (steps[i].action) steps[i].action();
        juce::Timer::callAfterDelay (steps[i].waitMs, [this, i]
        {
            if (steps[i].capture.isNotEmpty()) capture (steps[i]);
            runStep (i + 1);
        });
    }

    void add (std::function<void()> a, int wait, juce::String cap = {}, std::function<void (juce::DynamicObject&)> reg = {})
    {
        steps.push_back ({ std::move (a), wait, std::move (cap), std::move (reg) });
    }

    // ---------- the tour ----------
    void buildSteps()
    {
        auto& e = owner.engine;
        auto& m = owner;

        add ([&] { Project::loadDemo (e, 0); m.bpmSlider.setValue (132.0, juce::dontSendNotification); m.tabs.setCurrentTabIndex (4); }, 1500,
             "01_tutoriales", [&] (juce::DynamicObject& o) { put (o, "tabs", &m.tabs); });

        add ([&] { m.tabs.setCurrentTabIndex (0); select ("Keys"); e.play(); }, 5000, "02_pantalla_principal", [&] (juce::DynamicObject& o)
        {
            put (o, "topbar", &m.fileBtn); put (o, "browser", &m.browser); put (o, "tabs", &m.tabs);
            put (o, "inspector", &m.inspector); put (o, "keyboard", &m.keyboard); put (o, "scenes", m.live->getScenesComponent());
            put (o, "play", &m.playBtn); put (o, "energy", m.energyKnob.get()); put (o, "learn", &m.learnBtn); put (o, "audio", &m.audioBtn);
        });

        add ({}, 300, "03_barra_superior", [&] (juce::DynamicObject& o)
        {
            put (o, "file", &m.fileBtn); put (o, "play", &m.playBtn); put (o, "stop", &m.stopBtn); put (o, "rec", &m.recBtn);
            put (o, "metro", &m.metroBtn); put (o, "tap", &m.tapBtn); put (o, "bpm", &m.bpmSlider); put (o, "pos", &m.posLabel);
            put (o, "energy", m.energyKnob.get()); put (o, "swing", m.swingKnob.get()); put (o, "riserbars", m.riserKnob.get());
            put (o, "xfade", m.xfadeKnob.get()); put (o, "recset", &m.recSetBtn); put (o, "panic", &m.panicBtn);
            put (o, "learn", &m.learnBtn); put (o, "audio", &m.audioBtn); put (o, "cpu", &m.cpuLabel);
        });

        add ({}, 300, "04_fila_pista", [&] (juce::DynamicObject& o)
        {
            auto* row = m.live->getRowComponent (rowIndexOf ("Keys"));
            put (o, "row", row);
            put (o, "rec", findByText (row, "REC")); put (o, "undo", findByText (row, "Undo")); put (o, "x2", findByText (row, "x2"));
            put (o, "clear", findByText (row, "Borrar")); put (o, "mute", findByText (row, "M")); put (o, "solo", findByText (row, "S"));
            put (o, "mode", findCombo (row, "Largo fijo")); put (o, "quant", findCombo (row, "1/16")); put (o, "bars", findCombo (row, "4 comp"));
            if (row != nullptr && row->getNumChildComponents() > 1) put (o, "play", row->getChildComponent (1));
        });

        add ([&] { e.launchScene (2); }, 700, "05_escenas", [&] (juce::DynamicObject& o) { put (o, "scenes", m.live->getScenesComponent()); });
        add ({}, 4500, {});
        add ([&] { e.startRiserToNextScene(); }, 3500, "06_riser", [&] (juce::DynamicObject& o)
        {
            put (o, "scenes", m.live->getScenesComponent()); put (o, "energy", m.energyKnob.get());
        });
        add ({}, 6000, {});

        add ([&] { select ("Keys"); }, 1200, "07_inspector_synth", [&] (juce::DynamicObject& o)
        {
            put (o, "inspector", &m.inspector);
            put (o, "cutoff", findKnob (&m.inspector, "Cutoff")); put (o, "morph", findKnob (&m.inspector, "Key Morph"));
            put (o, "attack", findKnob (&m.inspector, "Attack")); put (o, "change", findByText (&m.inspector, "Cambiar instrumento"));
        });

        add ([&] { e.launchScene (2); select ("Drums Garage"); }, 4000, "08_inspector_drums", [&] (juce::DynamicObject& o)
        {
            put (o, "inspector", &m.inspector); put (o, "punch", findKnob (&m.inspector, "Punch"));
        });

        add ([&] { select ("Pad"); }, 800, {});
        add ([&] { if (auto* t = trackNamed ("Pad")) m.inspector.selectEffect (t->uid, 1); }, 800, {});
        add ([&] { m.inspector.scrollToBottom(); }, 600, "09_efectos", [&] (juce::DynamicObject& o)
        {
            put (o, "inspector", &m.inspector); put (o, "addfx", findByText (&m.inspector, "+ Efecto"));
            put (o, "depth", findKnob (&m.inspector, "Depth"));
        });

        add ([&] { m.inspector.scrollToTop(); m.tabs.setCurrentTabIndex (1); }, 2500, "10_mixer", [&] (juce::DynamicObject& o)
        {
            put (o, "tabs", &m.tabs);
            put (o, "addfx", findByText (m.mixer, "+ FX"));
        });

        add ([&] { select ("Keys"); m.tabs.setCurrentTabIndex (2); }, 2500, "11_piano_roll", [&] (juce::DynamicObject& o)
        {
            put (o, "tabs", &m.tabs); put (o, "quant", findByText (m.pianoRoll, "Cuantizar"));
            put (o, "dup", findByText (m.pianoRoll, "Duplicar x2"));
        });

        add ([&] { m.tabs.setCurrentTabIndex (0); }, 800, "12_navegador", [&] (juce::DynamicObject& o)
        {
            put (o, "browser", &m.browser); put (o, "scan", findByText (&m.browser, "Escanear mis VST"));
            put (o, "free", findByText (&m.browser, "VST gratis recomendados"));
        });

        add ([&] { if (auto* b = dynamic_cast<juce::Button*> (findByText (m.live, "+ Pista"))) b->triggerClick(); }, 1200, "13_menu_pista",
             [&] (juce::DynamicObject& o) { put (o, "addtrack", findByText (m.live, "+ Pista")); });
        add ([&] { juce::PopupMenu::dismissAllActiveMenus(); select ("Keys"); }, 1200, {});

        add ([&] { if (auto* k = findKnob (&m.inspector, "Cutoff")) ParamKnob::showLearnMenu (e, k->getParam().id, "sel.inst.cutoff", k); }, 1200,
             "14_menu_learn", [&] (juce::DynamicObject& o) { put (o, "inspector", &m.inspector); put (o, "cutoff", findKnob (&m.inspector, "Cutoff")); });
        add ([&] { juce::PopupMenu::dismissAllActiveMenus(); }, 600, {});

        add ([&] { e.mapper.learnModeUI = true; e.mapper.startLearn ("sel.inst.cutoff"); }, 1200, "15_learn_activo", [&] (juce::DynamicObject& o)
        {
            put (o, "inspector", &m.inspector); put (o, "learn", &m.learnBtn); put (o, "cutoff", findKnob (&m.inspector, "Cutoff"));
        });

        add ([&] { m.tabs.setCurrentTabIndex (3); }, 1500, "16_controladores", [&] (juce::DynamicObject& o)
        {
            put (o, "tabs", &m.tabs); put (o, "learnmode", findByText (m.mapping, "MODO LEARN (clic en un control y luego mueve tu controlador)"));
        });
        add ([&] { e.mapper.cancelLearn(); e.mapper.learnModeUI = false; m.tabs.setCurrentTabIndex (0); }, 600, {});

        add ([&] { select ("Micro"); if (auto* t = trackNamed ("Micro")) { t->monitor.set (1.0f); m.inspector.selectEffect (t->uid, 2); } }, 1500,
             "17_micro", [&] (juce::DynamicObject& o)
        {
            put (o, "inspector", &m.inspector); put (o, "input", findKnob (&m.inspector, "Input")); put (o, "monitor", findKnob (&m.inspector, "Monitor"));
            auto* row = m.live->getRowComponent (rowIndexOf ("Micro"));
            put (o, "row", row);
        });

        // --- recording a drum loop in free mode ---
        add ([&] { e.stop(); e.stop(); Project::newEmptyProject (e); m.bpmSlider.setValue (130.0, juce::dontSendNotification); }, 1200, "18_proyecto_nuevo",
             [&] (juce::DynamicObject& o) { put (o, "tabs", &m.tabs); });
        add ([&] { if (auto* t = trackNamed ("Bateria")) { t->recMode.set (1.0f); e.selectTrack (t->uid); } }, 800, {});
        add ([&] { e.recordOnSelected(); }, 1500, "19_esperando_nota", [&] (juce::DynamicObject& o)
        {
            auto* row = m.live->getRowComponent (0);
            put (o, "row", row); put (o, "mode", findCombo (row, "Libre (silencios)"));
        });
        // play a little 2-step pattern on the pads: kick, hat, clap, hat...
        const int pattern[] = { 36, 40, 39, 40, 36, 40, 39, 40, 36, 40, 39, 41, 36, 40, 39, 40 };
        for (int k = 0; k < 16; ++k)
        {
            auto n = pattern[k];
            add ([&e, n] { e.keyboardState.noteOn (1, n, 0.9f); }, 80, {});
            add ([&e, n] { e.keyboardState.noteOff (1, n, 0.0f); }, 150, k == 9 ? juce::String ("20_grabando") : juce::String(),
                 k == 9 ? std::function<void (juce::DynamicObject&)> ([&] (juce::DynamicObject& o)
                 { put (o, "row", m.live->getRowComponent (0)); put (o, "inspector", &m.inspector); }) : std::function<void (juce::DynamicObject&)>());
        }
        add ({}, 5500, "21_loop_cerrado", [&] (juce::DynamicObject& o) { put (o, "row", m.live->getRowComponent (0)); });
        add ([&] { m.tabs.setCurrentTabIndex (2); }, 1500, "22_piano_roll_drums", [&] (juce::DynamicObject& o) { put (o, "tabs", &m.tabs); });
        add ([&] { m.tabs.setCurrentTabIndex (0); if (auto* a = e.findAction ("transport.recset")) a->trigger(); }, 1100, "23_rec_set",
             [&] (juce::DynamicObject& o) { put (o, "recset", &m.recSetBtn); put (o, "cpu", &m.cpuLabel); });
        add ([&] { if (auto* a = e.findAction ("transport.recset")) a->trigger(); e.getLastRecording().deleteFile(); e.stop(); }, 500, {});
    }

    MainComponent& owner;
    juce::File outDir;
    FakeDevice fake;
    std::vector<Step> steps;
    juce::var regions { new juce::DynamicObject() };
    double phase = 0.0;
};

} // namespace mcl
