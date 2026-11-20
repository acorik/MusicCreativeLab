#include "engine/PluginHost.h"

namespace mcl
{

//==============================================================================
PluginManager::PluginManager() : juce::Thread ("VST scanner")
{
   #if JUCE_PLUGINHOST_VST3
    formats.addFormat (new juce::VST3PluginFormat());
   #endif

    auto listFile = dataFolder().getChildFile ("plugins.xml");
    if (auto xml = juce::parseXML (listFile))
    {
        known.recreateFromXml (*xml);
        if (auto* f = xml->getChildByName ("EXTRA_FOLDERS"))
            extraFolders.addTokens (f->getAllSubText(), ";", "");
    }
}

PluginManager::~PluginManager()
{
    stopThread (10000);
}

juce::File PluginManager::dataFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MusicCreativeLab");
    f.createDirectory();
    return f;
}

void PluginManager::save()
{
    if (auto xml = known.createXml())
    {
        auto* extra = xml->createNewChildElement ("EXTRA_FOLDERS");
        extra->addTextElement (extraFolders.joinIntoString (";"));
        xml->writeTo (dataFolder().getChildFile ("plugins.xml"));
    }
}

juce::FileSearchPath PluginManager::getSearchPath() const
{
    juce::FileSearchPath path;
    for (auto* f : formats.getFormats())
    {
        auto def = f->getDefaultLocationsToSearch();
        for (int i = 0; i < def.getNumPaths(); ++i) path.add (def[i]);
    }
   #if JUCE_WINDOWS
    path.addIfNotAlreadyThere (juce::File ("C:\\Program Files\\Common Files\\VST3"));
    path.addIfNotAlreadyThere (juce::File ("C:\\Program Files\\Image-Line\\FL Studio 21\\Plugins\\VST"));
   #endif
    for (auto& e : extraFolders) path.addIfNotAlreadyThere (juce::File (e));
    return path;
}

void PluginManager::addSearchFolder (const juce::File& folder)
{
    extraFolders.addIfNotAlreadyThere (folder.getFullPathName());
    save();
}

void PluginManager::startScan()
{
    if (! isThreadRunning()) startThread();
}

void PluginManager::run()
{
    auto deadMansPedal = dataFolder().getChildFile ("scan-crash-guard.txt");
    for (auto* format : formats.getFormats())
    {
        juce::PluginDirectoryScanner scanner (known, *format, getSearchPath(), true, deadMansPedal, false);
        juce::String name;
        while (! threadShouldExit())
        {
            {
                const juce::ScopedLock sl (statusLock);
                status = "Escaneando " + scanner.getNextPluginFileThatWillBeScanned()
                         + " (" + juce::String ((int) (scanner.getProgress() * 100.0f)) + "%)";
            }
            sendChangeMessage();
            if (! scanner.scanNextFile (true, name)) break;
        }
    }
    {
        const juce::ScopedLock sl (statusLock);
        status = "Listo: " + juce::String (known.getNumTypes()) + " plugins encontrados";
    }
    save();
    sendChangeMessage();
}

std::unique_ptr<juce::AudioPluginInstance> PluginManager::create (const juce::PluginDescription& d, double sr, int block, juce::String& error)
{
    return formats.createPluginInstance (d, sr, block, error);
}

std::unique_ptr<juce::AudioPluginInstance> PluginManager::createFromXml (const juce::XmlElement& descXml, double sr, int block, juce::String& error)
{
    juce::PluginDescription d;
    if (! d.loadFromXml (descXml)) { error = "Descripcion de plugin invalida"; return nullptr; }
    return create (d, sr, block, error);
}

juce::Array<juce::PluginDescription> PluginManager::getInstruments() const
{
    juce::Array<juce::PluginDescription> r;
    for (auto& d : known.getTypes()) if (d.isInstrument) r.add (d);
    return r;
}

juce::Array<juce::PluginDescription> PluginManager::getEffects() const
{
    juce::Array<juce::PluginDescription> r;
    for (auto& d : known.getTypes()) if (! d.isInstrument) r.add (d);
    return r;
}

//==============================================================================
void PluginParamBridge::attach (juce::AudioPluginInstance& p, const juce::String& prefix, int maxParams)
{
    links.clear();
    auto& params = p.getParameters();
    for (int i = 0; i < juce::jmin (maxParams, params.size()); ++i)
    {
        auto* target = params[i];
        auto name = target->getName (24);
        if (name.isEmpty()) name = "Param " + juce::String (i + 1);
        auto v = target->getValue();
        auto proxy = std::make_unique<Param> (prefix + "p" + juce::String (i), name, 0.0f, 1.0f, v);
        links.push_back ({ std::move (proxy), target, v });
    }
}

void PluginParamBridge::pushToPlugin()
{
    for (auto& l : links)
    {
        auto v = l.proxy->get();
        if (v != l.last)
        {
            l.target->setValueNotifyingHost (v);
            l.last = v;
        }
        else
        {
            // keep the proxy in sync if the plugin GUI moved it
            auto pv = l.target->getValue();
            if (std::abs (pv - l.last) > 1.0e-4f) { l.proxy->set (pv); l.last = pv; }
        }
    }
}

ParamList PluginParamBridge::getParams()
{
    ParamList r;
    for (auto& l : links) r.push_back (l.proxy.get());
    return r;
}

//==============================================================================
namespace
{
    struct PluginWindow : juce::DocumentWindow
    {
        PluginWindow (juce::AudioPluginInstance& p)
            : juce::DocumentWindow (p.getName(), juce::Colour (0xff1e1d28), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            if (auto* ed = p.createEditorIfNeeded())
            {
                setContentOwned (ed, true);
                setResizable (ed->isResizable(), false);
            }
            setAlwaysOnTop (true);
            centreWithSize (juce::jmax (300, getWidth()), juce::jmax (200, getHeight()));
        }
        void closeButtonPressed() override { setVisible (false); }
    };
}

std::unique_ptr<juce::DocumentWindow> openPluginWindow (juce::AudioPluginInstance& p, std::unique_ptr<juce::DocumentWindow> existing)
{
    if (existing == nullptr) existing = std::make_unique<PluginWindow> (p);
    existing->setVisible (true);
    existing->toFront (true);
    return existing;
}

static juce::ValueTree pluginStateTree (juce::AudioPluginInstance& plugin, const juce::String& tag)
{
    juce::ValueTree v (tag);
    v.setProperty ("type", "plugin", nullptr);
    if (auto xml = plugin.getPluginDescription().createXml())
        v.setProperty ("desc", xml->toString (juce::XmlElement::TextFormat().singleLine()), nullptr);
    juce::MemoryBlock mb;
    plugin.getStateInformation (mb);
    v.setProperty ("state", mb.toBase64Encoding(), nullptr);
    return v;
}

static void restorePluginState (juce::AudioPluginInstance& plugin, const juce::ValueTree& v)
{
    juce::MemoryBlock mb;
    if (mb.fromBase64Encoding (v.getProperty ("state").toString()) && mb.getSize() > 0)
        plugin.setStateInformation (mb.getData(), (int) mb.getSize());
}

static void fillPlayHead (juce::AudioPlayHead::PositionInfo& info, const BeatInfo& beat)
{
    info.setBpm (beat.bpm);
    info.setIsPlaying (beat.playing);
    info.setPpqPosition (beat.blockStartBeat);
    info.setPpqPositionOfLastBarStart (std::floor (beat.blockStartBeat / beatsPerBar) * beatsPerBar);
    info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
    info.setTimeInSamples ((juce::int64) (beat.blockStartBeat * beat.samplesPerBeat()));
    info.setTimeInSeconds (beat.blockStartBeat * 60.0 / beat.bpm);
}

PluginInstrument::PluginInstrument (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& prefix)
    : plugin (std::move (p))
{
    bridge.attach (*plugin, prefix);
    plugin->setPlayHead (&head);
    currentPreset = plugin->getNumPrograms() > 0 ? plugin->getProgramName (plugin->getCurrentProgram()) : juce::String();
}

PluginInstrument::~PluginInstrument()
{
    window.reset();
    plugin->releaseResources();
}

void PluginInstrument::showEditor() { window = openPluginWindow (*plugin, std::move (window)); }

void PluginInstrument::prepare (double sr, int block)
{
    plugin->enableAllBuses();
    plugin->setRateAndBufferSizeDetails (sr, block);
    plugin->prepareToPlay (sr, block);
    scratch.setSize (juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels()), block);
}

void PluginInstrument::render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const BeatInfo& beat)
{
    fillPlayHead (head.info, beat);
    bridge.pushToPlugin();
    if (pendingAllOff.exchange (false))
        for (int ch = 1; ch <= 16; ++ch) midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);

    const int n = out.getNumSamples();
    if (scratch.getNumSamples() < n) return; // block bigger than prepared - skip safely
    scratch.clear();
    juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), scratch.getNumChannels(), n);
    plugin->processBlock (view, midi);
    for (int ch = 0; ch < juce::jmin (2, out.getNumChannels()); ++ch)
        out.addFrom (ch, 0, view, juce::jmin (ch, view.getNumChannels() - 1), 0, n);
}

juce::StringArray PluginInstrument::getPresetNames() const
{
    juce::StringArray s;
    for (int i = 0; i < plugin->getNumPrograms(); ++i) s.add (plugin->getProgramName (i));
    return s;
}

void PluginInstrument::loadPreset (int index)
{
    if (juce::isPositiveAndBelow (index, plugin->getNumPrograms()))
    {
        plugin->setCurrentProgram (index);
        currentPreset = plugin->getProgramName (index);
    }
}

juce::ValueTree PluginInstrument::getState() const { return pluginStateTree (*plugin, "INSTRUMENT"); }
void PluginInstrument::setState (const juce::ValueTree& v) { restorePluginState (*plugin, v); }

//==============================================================================
PluginEffect::PluginEffect (std::unique_ptr<juce::AudioPluginInstance> p, const juce::String& prefix)
    : Effect (prefix), plugin (std::move (p))
{
    bridge.attach (*plugin, prefix);
}

PluginEffect::~PluginEffect()
{
    window.reset();
    plugin->releaseResources();
}

void PluginEffect::showEditor() { window = openPluginWindow (*plugin, std::move (window)); }

ParamList PluginEffect::getParams()
{
    auto l = Effect::getParams();
    for (auto* p : bridge.getParams()) l.push_back (p);
    return l;
}

void PluginEffect::prepare (double sr, int block)
{
    plugin->enableAllBuses();
    plugin->setRateAndBufferSizeDetails (sr, block);
    plugin->prepareToPlay (sr, block);
    scratch.setSize (juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels()), block);
}

void PluginEffect::process (juce::AudioBuffer<float>& b, const BeatInfo&)
{
    bridge.pushToPlugin();
    const int n = b.getNumSamples();
    if (scratch.getNumSamples() < n) return;
    scratch.clear();
    for (int ch = 0; ch < juce::jmin (scratch.getNumChannels(), b.getNumChannels()); ++ch)
        scratch.copyFrom (ch, 0, b, ch, 0, n);
    juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), scratch.getNumChannels(), n);
    emptyMidi.clear();
    plugin->processBlock (view, emptyMidi);
    for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
        b.copyFrom (ch, 0, view, juce::jmin (ch, view.getNumChannels() - 1), 0, n);
}

juce::ValueTree PluginEffect::getState() const
{
    auto v = pluginStateTree (*plugin, "FX");
    v.setProperty ("On", enabled->get(), nullptr);
    return v;
}

void PluginEffect::setState (const juce::ValueTree& v)
{
    restorePluginState (*plugin, v);
    if (v.hasProperty ("On")) enabled->set ((float) v.getProperty ("On"));
}

} // namespace mcl
