#include "ui/MainComponent.h"
#include "project/Project.h"
#include "ui/ScreenshotTour.h"

namespace mcl
{

static juce::File autosaveFile()
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MusicCreativeLab");
    f.createDirectory();
    return f.getChildFile ("autosave.mcl");
}

MainComponent::MainComponent (juce::ApplicationProperties& props)
    : properties (props),
      playBtn (engine, "transport.play", "PLAY"),
      stopBtn (engine, "transport.stop", "STOP"),
      recBtn (engine, "sel.record", "REC"),
      metroBtn (engine, "transport.metronome", "Metro"),
      tapBtn (engine, "transport.tap", "Tap"),
      recSetBtn (engine, "transport.recset", "REC SET"),
      panicBtn (engine, "master.panic", "Panico"),
      browser (engine), inspector (engine)
{
    // audio + midi devices
    auto saved = props.getUserSettings()->getXmlValue ("audioDevice");
    engine.initialiseDevices (saved.get());
    if (auto m = props.getUserSettings()->getXmlValue ("mappings"))
        engine.mapper.fromValueTree (juce::ValueTree::fromXml (*m));
    osc.start (props.getUserSettings()->getIntValue ("oscPort", 9000));

    // top bar
    for (auto* c : std::initializer_list<juce::Component*> { &fileBtn, &audioBtn, &playBtn, &stopBtn, &recBtn, &metroBtn, &tapBtn, &recSetBtn,
                                                             &panicBtn, &bpmSlider, &bpmLabel, &posLabel, &cpuLabel, &learnBtn, &projectLabel })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
    playBtn.setColour (juce::TextButton::buttonColourId, Colours::play.withAlpha (0.35f));
    recBtn.setColour (juce::TextButton::buttonColourId, Colours::rec.withAlpha (0.45f));
    recSetBtn.setColour (juce::TextButton::buttonColourId, Colours::rec.withAlpha (0.25f));
    fileBtn.onClick = [this] { showFileMenu(); };
    audioBtn.onClick = [this] { showAudioSettings(); };

    bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bpmSlider.setRange (60.0, 200.0, 0.5);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 24);
    bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
    bpmSlider.onValueChange = [this] { engine.transport.bpm = bpmSlider.getValue(); };
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, Colours::textDim);
    posLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold));
    posLabel.setColour (juce::Label::textColourId, Colours::accent);
    posLabel.setJustificationType (juce::Justification::centred);
    cpuLabel.setColour (juce::Label::textColourId, Colours::textDim);
    cpuLabel.setFont (juce::FontOptions (11.0f));
    projectLabel.setColour (juce::Label::textColourId, Colours::textDim);
    projectLabel.setFont (juce::FontOptions (11.0f));

    energyKnob = std::make_unique<ParamKnob> (engine, engine.energy);
    swingKnob = std::make_unique<ParamKnob> (engine, engine.swing);
    riserKnob = std::make_unique<ParamKnob> (engine, engine.riserBars);
    xfadeKnob = std::make_unique<ParamKnob> (engine, engine.crossfader);
    energyKnob->setColour (Colours::accent2);
    xfadeKnob->setColour (Colours::accent2);
    for (auto* k : { energyKnob.get(), swingKnob.get(), riserKnob.get(), xfadeKnob.get() }) addAndMakeVisible (k);
    masterMeter = std::make_unique<LevelMeter> (engine.masterPeakL, engine.masterPeakR);
    addAndMakeVisible (*masterMeter);

    learnBtn.setClickingTogglesState (true);
    learnBtn.setColour (juce::TextButton::buttonOnColourId, Colours::learn);
    learnBtn.setTooltip ("Modo MIDI learn: haz clic en un control y mueve tu MPD218 / Numark / Stream Deck");
    learnBtn.onClick = [this] { engine.mapper.learnModeUI = learnBtn.getToggleState(); if (! learnBtn.getToggleState()) engine.mapper.cancelLearn(); };

    // centre
    live = new LiveView (engine);
    mixer = new MixerView (engine);
    pianoRoll = new PianoRoll (engine);
    mapping = new MappingView (engine, osc);
    tutorials = new TutorialView();
    tabs.addTab ("LIVE", Colours::panel, live, true);
    tabs.addTab ("MIXER", Colours::panel, mixer, true);
    tabs.addTab ("PIANO ROLL", Colours::panel, pianoRoll, true);
    tabs.addTab ("CONTROLADORES", Colours::panel, mapping, true);
    tabs.addTab ("TUTORIALES", Colours::panel, tutorials, true);
    tabs.setTabBarDepth (34);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);
    live->onShowPianoRoll = [this] (Track& t) { engine.selectTrack (t.uid); tabs.setCurrentTabIndex (2); };
    mixer->onSelectEffect = [this] (int uid, int idx) { inspector.selectEffect (uid, idx); };
    browser.onLoadDemo = [this] (int i) { loadDemo (i, true); };
    tutorials->onLoadDemo = [this] (int i) { loadDemo (i, true); tabs.setCurrentTabIndex (0); };

    addAndMakeVisible (browser);
    addAndMakeVisible (inspector);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyPressBaseOctave (4);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Colours::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours::accent.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    // restore last session or open the first demo
    auto af = autosaveFile();
    if (af.existsAsFile() && Project::loadFromFile (engine, af).isEmpty()) {}
    else
    {
        Project::loadDemo (engine, 0);
        tabs.setCurrentTabIndex (4);
    }
    if (auto t = juce::SystemStats::getEnvironmentVariable ("MCL_TAB", {}); t.isNotEmpty()) tabs.setCurrentTabIndex (t.getIntValue());
    bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
    setProjectFile (juce::File (props.getUserSettings()->getValue ("lastProject")));

    setWantsKeyboardFocus (true);
    setSize (1680, 980);
    startTimerHz (15);

    if (auto dir = juce::SystemStats::getEnvironmentVariable ("MCL_TOUR", {}); dir.isNotEmpty())
        startScreenshotTour (juce::File (dir));
}

void MainComponent::startScreenshotTour (const juce::File& d) { tour = std::make_unique<Tour> (*this, d); }

MainComponent::~MainComponent()
{
    tour.reset();
    saveAutosave();
    if (auto xml = engine.deviceManager.createStateXml()) properties.getUserSettings()->setValue ("audioDevice", xml.get());
    if (auto xml = engine.mapper.toValueTree().createXml()) properties.getUserSettings()->setValue ("mappings", xml.get());
    properties.saveIfNeeded();
    tabs.clearTabs();
}

void MainComponent::openFile (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    auto err = Project::loadFromFile (engine, f);
    bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
    setProjectFile (f);
    tabs.setCurrentTabIndex (0);
    if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Proyecto", err);
}

void MainComponent::saveAutosave() { Project::saveToFile (engine, autosaveFile()); }

void MainComponent::setProjectFile (const juce::File& f)
{
    projectFile = f.existsAsFile() ? f : juce::File();
    projectLabel.setText (projectFile.existsAsFile() ? projectFile.getFileNameWithoutExtension() : juce::String ("Proyecto sin guardar"), juce::dontSendNotification);
    if (projectFile.existsAsFile()) properties.getUserSettings()->setValue ("lastProject", projectFile.getFullPathName());
}

void MainComponent::loadDemo (int index, bool confirm)
{
    auto doLoad = [this, index]
    {
        Project::loadDemo (engine, index);
        bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
        setProjectFile ({});
    };
    if (! confirm || engine.getNumTracks() == 0) { doLoad(); return; }
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Cargar demo",
                                        "Se reemplaza el proyecto actual por la demo:\n" + Project::getDemoNames()[index] + "\n\n" + Project::getDemoDescriptions()[index],
                                        "Cargar", "Cancelar", nullptr,
                                        juce::ModalCallbackFunction::create ([doLoad] (int r) { if (r == 1) doLoad(); }));
}

void MainComponent::saveProject (bool saveAs)
{
    if (! saveAs && projectFile.existsAsFile()) { Project::saveToFile (engine, projectFile); return; }
    chooser = std::make_unique<juce::FileChooser> ("Guardar proyecto", Project::getDefaultFolder().getChildFile ("Mi set.mcl"), "*.mcl");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("mcl");
        if (Project::saveToFile (engine, f)) setProjectFile (f);
    });
}

void MainComponent::openProject()
{
    chooser = std::make_unique<juce::FileChooser> ("Abrir proyecto", Project::getDefaultFolder(), "*.mcl");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        auto err = Project::loadFromFile (engine, f);
        bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);
        setProjectFile (f);
        if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Proyecto", err);
    });
}

void MainComponent::showFileMenu()
{
    juce::PopupMenu m, demos;
    auto names = Project::getDemoNames();
    for (int i = 0; i < names.size(); ++i) demos.addItem (100 + i, names[i]);
    m.addItem (1, "Nuevo proyecto (pistas listas para grabar)");
    m.addItem (2, "Abrir...");
    m.addItem (3, "Guardar");
    m.addItem (4, "Guardar como...");
    m.addSubMenu ("Demos", demos);
    m.addSeparator();
    m.addItem (5, "Abrir carpeta de sets grabados");
    m.addItem (6, "Acerca de Music Creative Lab");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn), [this] (int r)
    {
        if (r == 1) { Project::newEmptyProject (engine); bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification); setProjectFile ({}); }
        if (r == 2) openProject();
        if (r == 3) saveProject (false);
        if (r == 4) saveProject (true);
        if (r >= 100) loadDemo (r - 100, true);
        if (r == 5)
        {
            auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Music Creative Lab").getChildFile ("Sets");
            f.createDirectory();
            f.startAsProcess();
        }
        if (r == 6)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Music Creative Lab",
                "Version " + juce::String (JUCE_APPLICATION_VERSION_STRING) + "\nEstacion de musica en directo: loops, synths, escenas, MIDI learn y VST3.\n"
                "Hecho con JUCE. Codigo: github.com/acorik/MusicCreativeLab");
    });
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (engine.deviceManager, 1, 8, 2, 2, false, false, true, false);
    sel->setSize (560, 520);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio: elige tu Monocaster (ASIO o WASAPI) y el buffer";
    o.dialogBackgroundColour = Colours::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

bool MainComponent::triggerAction (const juce::String& id)
{
    if (auto* a = engine.findAction (id)) { if (a->trigger) a->trigger(); return true; }
    return false;
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr) return false;
    const auto code = key.getKeyCode();
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (code == juce::KeyPress::spaceKey)  return triggerAction ("transport.play");
    if (code == juce::KeyPress::escapeKey) return triggerAction ("transport.stop");
    if (code == juce::KeyPress::upKey)     { engine.selectRelative (-1); return true; }
    if (code == juce::KeyPress::downKey)   { engine.selectRelative (1); return true; }
    if (code == juce::KeyPress::leftKey)   { keyOctave = juce::jmax (1, keyOctave - 1); keyboard.setKeyPressBaseOctave (keyOctave); keyboard.setLowestVisibleKey (keyOctave * 12 - 12); return true; }
    if (code == juce::KeyPress::rightKey)  { keyOctave = juce::jmin (8, keyOctave + 1); keyboard.setKeyPressBaseOctave (keyOctave); keyboard.setLowestVisibleKey (keyOctave * 12 - 12); return true; }
    if (code == juce::KeyPress::deleteKey) return triggerAction ("sel.clear");
    if (code == juce::KeyPress::F5Key)     return triggerAction ("transport.recset");
    if (key.getModifiers().isCtrlDown() && ch == 's') { saveProject (false); return true; }
    if (key.getModifiers().isCtrlDown() && ch == 'o') { openProject(); return true; }

    // F13..F20 scenes, F21..F24 extra (Stream Deck friendly)
    const int fkeys[] = { juce::KeyPress::F13Key, juce::KeyPress::F14Key, juce::KeyPress::F15Key, juce::KeyPress::F16Key,
                          juce::KeyPress::F17Key, juce::KeyPress::F18Key, juce::KeyPress::F19Key, juce::KeyPress::F20Key };
    for (int i = 0; i < 8; ++i) if (code == fkeys[i]) return triggerAction ("scene." + juce::String (i + 1));
    if (code == juce::KeyPress::F21Key) return triggerAction ("sel.record");
    if (code == juce::KeyPress::F22Key) return triggerAction ("master.riser");
    if (code == juce::KeyPress::F23Key) return triggerAction ("transport.play");
    if (code == juce::KeyPress::F24Key) return triggerAction ("sel.undo");

    if (key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()) return false;
    if (ch >= '1' && ch <= '8') return triggerAction ("scene." + juce::String ((int) (ch - '0')));
    switch (ch)
    {
        case 'r': return triggerAction ("sel.record");
        case 'z': return triggerAction ("sel.undo");
        case 'x': return triggerAction ("sel.double");
        case 'v': return triggerAction ("sel.toggle");
        case 'b': return triggerAction ("master.riser");
        case 'm': return triggerAction ("transport.metronome");
        case 'n': return triggerAction ("transport.tap");
        case 'q': return triggerAction ("master.panic");
        default: break;
    }
    return false;
}

void MainComponent::timerCallback()
{
    auto beat = engine.transport.getBeat();
    auto bar = (int) std::floor (beat / beatsPerBar) + 1;
    auto b = (int) std::floor (std::fmod (beat, (double) beatsPerBar)) + 1;
    posLabel.setText (juce::String (bar) + " . " + juce::String (b), juce::dontSendNotification);
    playBtn.setToggleState (engine.transport.playing.load(), juce::dontSendNotification);
    playBtn.setColour (juce::TextButton::buttonOnColourId, Colours::play);
    metroBtn.setToggleState (engine.transport.metronome.load(), juce::dontSendNotification);
    auto recSet = engine.isRecordingSet();
    recSetBtn.setToggleState (recSet && (tick / 4) % 2 == 0, juce::dontSendNotification);
    recSetBtn.setColour (juce::TextButton::buttonOnColourId, Colours::rec);
    learnBtn.setToggleState (engine.mapper.learnModeUI.load() || engine.mapper.isLearning(), juce::dontSendNotification);
    if (! bpmSlider.isMouseButtonDown() && std::abs (bpmSlider.getValue() - engine.transport.bpm.load()) > 0.01)
        bpmSlider.setValue (engine.transport.bpm.load(), juce::dontSendNotification);

    if (++tick % 8 == 0)
    {
        juce::String dev = engine.deviceManager.getCurrentAudioDevice() != nullptr
                         ? engine.deviceManager.getCurrentAudioDeviceType() + " | " + juce::String (engine.getBlockSize()) + " muestras | "
                           + juce::String (engine.getSampleRate() / 1000.0, 1) + " kHz"
                         : juce::String ("SIN AUDIO: pulsa 'Audio'");
        cpuLabel.setText (dev + "\nCPU " + juce::String ((int) (engine.getCpuUsage() * 100.0)) + "%"
                          + (recSet ? "   |  GRABANDO SET" : ""), juce::dontSendNotification);
    }
    if (tick % 30 == 0) engine.refreshMidiInputs();
    if (tick % 900 == 0) saveAutosave(); // every minute

    // keep the computer keyboard playing notes unless typing in a text box
    if (tick % 5 == 0 && isShowing())
    {
        auto* f = juce::Component::getCurrentlyFocusedComponent();
        if (f == nullptr || (dynamic_cast<juce::TextEditor*> (f) == nullptr && f != &keyboard && dynamic_cast<juce::ComboBox*> (f) == nullptr
                             && f->getTopLevelComponent() == getTopLevelComponent()))
            keyboard.grabKeyboardFocus();
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    auto top = getLocalBounds().removeFromTop (74).toFloat();
    g.setColour (Colours::panel);
    g.fillRect (top);
    g.setColour (Colours::outline);
    g.drawHorizontalLine ((int) top.getBottom(), 0.0f, (float) getWidth());
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("MUSIC CREATIVE LAB", 12, 6, 230, 30, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (74).reduced (8, 6);
    {
        auto left = top.removeFromLeft (230);
        projectLabel.setBounds (left.removeFromBottom (16).withTrimmedLeft (90));
        left.removeFromTop (30);
        fileBtn.setBounds (left.removeFromLeft (84).withHeight (28));
    }
    auto place = [&top] (juce::Component& c, int w) { c.setBounds (top.removeFromLeft (w).reduced (3, 10)); };
    place (playBtn, 74); place (stopBtn, 64); place (recBtn, 64); place (metroBtn, 62); place (tapBtn, 50);
    bpmLabel.setBounds (top.removeFromLeft (34).reduced (0, 18));
    bpmSlider.setBounds (top.removeFromLeft (120).reduced (2, 16));
    posLabel.setBounds (top.removeFromLeft (100));
    for (auto* k : { energyKnob.get(), swingKnob.get(), riserKnob.get(), xfadeKnob.get() }) k->setBounds (top.removeFromLeft (70).reduced (2, 0));
    place (recSetBtn, 90);
    place (panicBtn, 72);
    place (learnBtn, 110);
    masterMeter->setBounds (top.removeFromRight (16).reduced (0, 4));
    top.removeFromRight (6);
    audioBtn.setBounds (top.removeFromRight (80).reduced (3, 10));
    cpuLabel.setBounds (top.reduced (4, 0));

    keyboard.setBounds (r.removeFromBottom (64));
    keyboard.setKeyWidth (juce::jmax (12.0f, (float) keyboard.getWidth() / 50.0f));
    browser.setBounds (r.removeFromLeft (232));
    inspector.setBounds (r.removeFromRight (366));
    tabs.setBounds (r);
}

} // namespace mcl
