#include "engine/AudioEngine.h"
#include "dsp/Synth.h"
#include "dsp/DrumMachine.h"

namespace mcl
{

AudioEngine::AudioEngine()
    : masterVolume ("master.volume", "Master", 0.0f, 1.25f, 0.85f),
      energy ("master.energy", "Energy", 0.0f, 1.0f, 1.0f),
      crossfader ("master.crossfader", "Crossfader", 0.0f, 1.0f, 0.5f),
      swing ("master.swing", "Swing", 0.0f, 0.5f, 0.12f),
      riserBars ("master.riserbars", "Riser Bars", 1.0f, 8.0f, 4.0f)
{
    mapper.findParam = [this] (const juce::String& id) { return findParam (id); };
    mapper.findAction = [this] (const juce::String& id) { return findAction (id); };
    mapper.describeTarget = [this] (const juce::String& id) { return describeTarget (id); };
    mapper.loadDefaultMappings();
    buildActions();
    addMasterEffect (createBuiltInEffect ("limiter", newMasterFxPrefix()));
    writerThread.startThread();
}

AudioEngine::~AudioEngine()
{
    stopRecordingSet();
    deviceManager.removeAudioCallback (this);
    for (auto& id : openMidi) deviceManager.removeMidiInputDeviceCallback (id, this);
    deviceManager.closeAudioDevice();
    writerThread.stopThread (2000);
}

void AudioEngine::initialiseDevices (const juce::XmlElement* saved)
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.bufferSize = MCL_LITE ? 512 : 256; // Lite: safer on slow CPUs (Chromebooks)
    auto err = deviceManager.initialise (2, 2, saved, true, {}, &setup);
    if (err.isNotEmpty()) deviceManager.initialiseWithDefaultDevices (2, 2);

    // First run: prefer ASIO, then WASAPI exclusive for the lowest latency
    if (saved == nullptr)
    {
        for (auto pref : { "ASIO", "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)" })
        {
            for (auto& type : deviceManager.getAvailableDeviceTypes())
            {
                if (type->getTypeName() == pref)
                {
                    type->scanForDevices();
                    auto names = type->getDeviceNames();
                    if (names.isEmpty()) continue;
                    deviceManager.setCurrentAudioDeviceType (pref, true);
                    goto chosen;
                }
            }
        }
    }
chosen:
    deviceManager.addAudioCallback (this);
    refreshMidiInputs();
}

void AudioEngine::refreshMidiInputs()
{
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        const juce::ScopedLock sl (midiLock);
        if (openMidi.contains (d.identifier)) continue;
        deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.addMidiInputDeviceCallback (d.identifier, this);
        openMidi.add (d.identifier);
    }
}

//==============================================================================
Track* AudioEngine::addTrack (TrackKind kind, const juce::String& name, int forcedUid, bool withDefaultFx)
{
    int uid = forcedUid > 0 ? forcedUid : nextUid;
    nextUid = juce::jmax (nextUid, uid + 1);
    auto t = std::make_unique<Track> (uid, kind, name);
    t->prepare (sampleRate, maxChunk);
    if (kind == TrackKind::Audio && withDefaultFx)
    {
        // Default vocal chain for the SM7B: gate -> EQ -> compressor -> reverb -> delay
        for (auto id : { "gate", "eq", "compressor", "chorus", "delay", "reverb" })
        {
            auto fx = createBuiltInEffect (id, t->newFxPrefix());
            fx->prepare (sampleRate, maxChunk);
            if (juce::String (id) == "chorus" || juce::String (id) == "delay") fx->getParams()[0]->set (0.0f);
            if (juce::String (id) == "eq") { if (auto* p = fx->getParams()[5]) p->set (80.0f); }
            t->effects.push_back (std::move (fx));
        }
    }
    auto* raw = t.get();
    {
        const juce::ScopedLock sl (lock);
        tracks.push_back (std::move (t));
    }
    if (selectedTrack.load() < 0) selectedTrack = raw->uid;
    sendChangeMessage();
    return raw;
}

void AudioEngine::removeTrack (int uid)
{
    std::unique_ptr<Track> doomed;
    {
        const juce::ScopedLock sl (lock);
        for (auto it = tracks.begin(); it != tracks.end(); ++it)
            if ((*it)->uid == uid) { doomed = std::move (*it); tracks.erase (it); break; }
    }
    mapper.removeMappingsFor ("t" + juce::String (uid));
    if (selectedTrack.load() == uid) selectedTrack = tracks.empty() ? -1 : tracks.front()->uid;
    sendSynchronousChangeMessage();
}

void AudioEngine::clearAllTracks()
{
    std::vector<std::unique_ptr<Track>> doomed;
    {
        const juce::ScopedLock sl (lock);
        doomed = std::move (tracks);
        tracks.clear();
        scenes.clear();
        currentScene = -1; pendingScene = -1;
    }
    selectedTrack = -1;
    nextUid = 1;
    sendSynchronousChangeMessage();
}

int AudioEngine::getNumTracks() const { const juce::ScopedLock sl (lock); return (int) tracks.size(); }

Track* AudioEngine::getTrack (int index) const
{
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (index, (int) tracks.size()) ? tracks[(size_t) index].get() : nullptr;
}

Track* AudioEngine::getTrackByUid (int uid) const
{
    const juce::ScopedLock sl (lock);
    for (auto& t : tracks) if (t->uid == uid) return t.get();
    return nullptr;
}

int AudioEngine::indexOfTrack (int uid) const
{
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < tracks.size(); ++i) if (tracks[i]->uid == uid) return (int) i;
    return -1;
}

void AudioEngine::moveTrack (int uid, int delta)
{
    {
        const juce::ScopedLock sl (lock);
        auto i = indexOfTrack (uid);
        auto j = i + delta;
        if (i < 0 || ! juce::isPositiveAndBelow (j, (int) tracks.size())) return;
        std::swap (tracks[(size_t) i], tracks[(size_t) j]);
    }
    sendChangeMessage();
}

void AudioEngine::selectRelative (int delta)
{
    auto n = getNumTracks();
    if (n == 0) return;
    auto i = indexOfTrack (selectedTrack.load());
    i = ((i + delta) % n + n) % n;
    if (auto* t = getTrack (i)) selectTrack (t->uid);
}

void AudioEngine::setInstrument (Track& t, std::unique_ptr<Instrument> inst)
{
    if (inst) inst->prepare (sampleRate, maxChunk);
    {
        const juce::ScopedLock sl (lock);
        std::swap (t.instrument, inst);
    }
    sendSynchronousChangeMessage();
    // old instrument (now in 'inst') is destroyed here, outside the audio lock
}

Effect* AudioEngine::addEffect (Track& t, std::unique_ptr<Effect> fx)
{
    if (! fx) return nullptr;
    fx->prepare (sampleRate, maxChunk);
    auto* raw = fx.get();
    {
        const juce::ScopedLock sl (lock);
        t.effects.push_back (std::move (fx));
    }
    sendChangeMessage();
    return raw;
}

Effect* AudioEngine::addEffect (Track& t, const juce::String& id)
{
    return addEffect (t, createBuiltInEffect (id, t.newFxPrefix()));
}

void AudioEngine::removeEffect (Track& t, int index)
{
    std::unique_ptr<Effect> doomed;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (index, (int) t.effects.size())) return;
        doomed = std::move (t.effects[(size_t) index]);
        t.effects.erase (t.effects.begin() + index);
    }
    sendSynchronousChangeMessage();
}

void AudioEngine::moveEffect (Track& t, int index, int delta)
{
    {
        const juce::ScopedLock sl (lock);
        auto j = index + delta;
        if (! juce::isPositiveAndBelow (index, (int) t.effects.size()) || ! juce::isPositiveAndBelow (j, (int) t.effects.size())) return;
        std::swap (t.effects[(size_t) index], t.effects[(size_t) j]);
    }
    sendChangeMessage();
}

Effect* AudioEngine::addMasterEffect (std::unique_ptr<Effect> fx)
{
    if (! fx) return nullptr;
    fx->prepare (sampleRate, maxChunk);
    auto* raw = fx.get();
    {
        const juce::ScopedLock sl (lock);
        // keep the limiter last
        auto pos = masterFx.end();
        if (! masterFx.empty() && masterFx.back()->getTypeId() == "limiter" && fx->getTypeId() != "limiter") pos = masterFx.end() - 1;
        masterFx.insert (pos, std::move (fx));
    }
    sendChangeMessage();
    return raw;
}

void AudioEngine::removeMasterEffect (int index)
{
    std::unique_ptr<Effect> doomed;
    {
        const juce::ScopedLock sl (lock);
        if (! juce::isPositiveAndBelow (index, (int) masterFx.size())) return;
        doomed = std::move (masterFx[(size_t) index]);
        masterFx.erase (masterFx.begin() + index);
    }
    sendSynchronousChangeMessage();
}

std::unique_ptr<Instrument> AudioEngine::createBuiltInInstrument (const juce::String& type, Track& t)
{
    auto prefix = t.paramPrefix() + "inst.";
    if (type == "drums") return std::make_unique<DrumMachine> (prefix);
    return std::make_unique<LabSynth> (prefix);
}

std::unique_ptr<Instrument> AudioEngine::createPluginInstrument (const juce::PluginDescription& d, Track& t, juce::String& error)
{
    auto p = plugins.create (d, sampleRate, maxChunk, error);
    if (! p) return nullptr;
    return std::make_unique<PluginInstrument> (std::move (p), t.paramPrefix() + "inst.");
}

std::unique_ptr<Effect> AudioEngine::createPluginEffect (const juce::PluginDescription& d, const juce::String& prefix, juce::String& error)
{
    auto p = plugins.create (d, sampleRate, maxChunk, error);
    if (! p) return nullptr;
    return std::make_unique<PluginEffect> (std::move (p), prefix);
}

Track* AudioEngine::findDrumTrack() const
{
    for (auto& t : tracks)
        if (t->instrument && t->instrument->getTypeId() == "drums") return t.get();
    return nullptr;
}

//==============================================================================
void AudioEngine::play()
{
    transport.playing = true;
}

void AudioEngine::stop()
{
    if (! transport.playing)
    {
        transport.positionBeats = 0.0;
        panic();
    }
    transport.playing = false;
}

void AudioEngine::recordOnSelected()
{
    if (auto* t = getSelectedTrack())
    {
        if (! transport.playing) transport.playing = true;
        t->requestRecord();
    }
}

void AudioEngine::panic()
{
    const juce::ScopedLock sl (lock);
    for (auto& t : tracks) t->panic();
    keyboardState.allNotesOff (0);
}

//==============================================================================
void AudioEngine::captureScene (int index)
{
    const juce::ScopedLock sl (lock);
    if (index < 0) return;
    if ((int) scenes.size() <= index) scenes.resize ((size_t) index + 1);
    auto& s = scenes[(size_t) index];
    if (s.name.isEmpty()) s.name = "Escena " + juce::String (index + 1);
    s.energy = energy.get();
    s.tracks.clear();
    for (auto& t : tracks) s.tracks[t->uid] = { t->mute.get() < 0.5f, t->volume.get() };
    currentScene = index;
}

void AudioEngine::launchScene (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) scenes.size())) return;
    if (! transport.playing) { const juce::ScopedLock sl (lock); applyScene (index); }
    else pendingScene = index;
}

void AudioEngine::applyScene (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) scenes.size())) return;
    auto& s = scenes[(size_t) index];
    for (auto& t : tracks)
    {
        auto it = s.tracks.find (t->uid);
        if (it == s.tracks.end()) continue;
        t->mute.set (it->second.active ? 0.0f : 1.0f);
        t->volume.set (it->second.volume);
    }
    energy.set (s.energy);
    currentScene = index;
    pendingScene = -1;
}

void AudioEngine::startRiserToNextScene()
{
    auto beat = transport.getBeat();
    if (! transport.playing) { transport.playing = true; }
    auto bars = juce::jlimit (1, 8, (int) std::round (riserBars.get()));
    auto end = Transport::nextBar (beat) + (bars - 1) * beatsPerBar;
    riserStart = beat;
    riserLength = juce::jmax (1.0, end - beat);
    auto n = (int) scenes.size();
    riserTargetScene = n == 0 ? -1 : (currentScene.load() + 1) % n;
    riserActive = true;
}

//==============================================================================
bool AudioEngine::startRecordingSet (const juce::File& file)
{
    stopRecordingSet();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release();
    const juce::ScopedLock sl (writerLock);
    threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 32768);
    lastRecording = file;
    recorderActive = true;
    return true;
}

void AudioEngine::stopRecordingSet()
{
    recorderActive = false;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> doomed;
    {
        const juce::ScopedLock sl (writerLock);
        doomed = std::move (threaded);
    }
}

//==============================================================================
void AudioEngine::buildActions()
{
    auto add = [this] (const juce::String& id, const juce::String& name, std::function<void()> f)
    { actions.push_back ({ id, name, std::move (f) }); };

    add ("transport.play",      "Play / Pausa",           [this] { togglePlay(); });
    add ("transport.stop",      "Stop",                   [this] { stop(); });
    add ("transport.tap",       "Tap tempo",              [this] { transport.tap(); });
    add ("transport.metronome", "Metronomo on/off",       [this] { transport.metronome = ! transport.metronome; });
    add ("transport.recset",    "Grabar set (WAV)",       [this]
    {
        if (isRecordingSet()) { stopRecordingSet(); return; }
        auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Music Creative Lab")
                     .getChildFile ("Sets").getChildFile ("Set " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav");
        startRecordingSet (f);
    });
    add ("sel.record",  "Grabar / Overdub pista seleccionada", [this] { recordOnSelected(); });
    add ("sel.clear",   "Borrar loop seleccionado",            [this] { if (auto* t = getSelectedTrack()) t->requestClear(); });
    add ("sel.undo",    "Deshacer ultimo overdub",             [this] { if (auto* t = getSelectedTrack()) t->requestUndo(); });
    add ("sel.toggle",  "Parar / reanudar loop",               [this] { if (auto* t = getSelectedTrack()) t->requestToggleClip(); });
    add ("sel.double",  "Duplicar loop (x2)",                  [this] { if (auto* t = getSelectedTrack()) t->requestDouble(); });
    add ("sel.mute",    "Mute pista seleccionada",             [this] { if (auto* t = getSelectedTrack()) t->mute.set (t->mute.get() > 0.5f ? 0.0f : 1.0f); });
    add ("sel.next",    "Siguiente pista",                     [this] { juce::MessageManager::callAsync ([this] { selectRelative (1); }); });
    add ("sel.prev",    "Pista anterior",                      [this] { juce::MessageManager::callAsync ([this] { selectRelative (-1); }); });
    for (int i = 0; i < 8; ++i)
        add ("scene." + juce::String (i + 1), "Lanzar escena " + juce::String (i + 1), [this, i] { launchScene (i); });
    add ("scene.next",   "Siguiente escena",  [this] { if (! scenes.empty()) launchScene ((currentScene.load() + 1) % (int) scenes.size()); });
    add ("scene.prev",   "Escena anterior",   [this] { if (! scenes.empty()) launchScene ((currentScene.load() - 1 + (int) scenes.size()) % (int) scenes.size()); });
    add ("master.riser", "Riser -> DROP",     [this] { startRiserToNextScene(); });
    add ("master.panic", "Panico (silenciar notas)", [this] { panic(); });
    add ("master.energyup",   "Energia +",    [this] { energy.set (energy.get() + 0.1f); });
    add ("master.energydown", "Energia -",    [this] { energy.set (energy.get() - 0.1f); });
}

Action* AudioEngine::findAction (const juce::String& id)
{
    for (auto& a : actions) if (a.id == id) return &a;

    // per-track actions: t<uid>.record / clear / undo / toggle / double / select
    if (id.startsWithChar ('t') && id.containsChar ('.'))
    {
        auto uid = id.substring (1).upToFirstOccurrenceOf (".", false, false).getIntValue();
        auto what = id.fromFirstOccurrenceOf (".", false, false);
        static const juce::StringArray verbs { "record", "clear", "undo", "toggle", "double", "select" };
        if (uid > 0 && verbs.contains (what))
        {
            const juce::ScopedLock sl (lock);
            auto it = dynamicActions.find (id);
            if (it == dynamicActions.end())
            {
                Action a { id, what, [this, uid, what]
                {
                    auto* t = getTrackByUid (uid);
                    if (t == nullptr) return;
                    if (what == "record") { transport.playing = true; t->requestRecord(); }
                    else if (what == "clear")  t->requestClear();
                    else if (what == "undo")   t->requestUndo();
                    else if (what == "toggle") t->requestToggleClip();
                    else if (what == "double") t->requestDouble();
                    else if (what == "select") juce::MessageManager::callAsync ([this, uid] { selectTrack (uid); });
                } };
                it = dynamicActions.emplace (id, std::move (a)).first;
            }
            return &it->second;
        }
    }
    return nullptr;
}

Param* AudioEngine::findParam (const juce::String& rawId)
{
    const juce::ScopedLock sl (lock);
    auto id = rawId;
    if (id.startsWith ("sel."))
    {
        auto* t = getSelectedTrack();
        if (t == nullptr) return nullptr;
        id = t->paramPrefix() + id.substring (4);
    }

    for (auto* p : { &masterVolume, &energy, &crossfader, &swing, &riserBars })
        if (p->id == id) return p;
    for (auto& fx : masterFx) for (auto* p : fx->getParams()) if (p->id == id) return p;

    for (auto& t : tracks)
    {
        if (! id.startsWith (t->paramPrefix())) continue;
        for (auto* p : t->getMixerParams()) if (p->id == id) return p;
        if (t->instrument) for (auto* p : t->instrument->getParams()) if (p->id == id) return p;
        for (auto& fx : t->effects) for (auto* p : fx->getParams()) if (p->id == id) return p;
    }
    return nullptr;
}

juce::String AudioEngine::describeTarget (const juce::String& id)
{
    if (auto* a = findAction (id)) return a->name;
    auto prefixName = juce::String();
    if (id.startsWith ("sel.")) prefixName = "Pista seleccionada";
    else if (id.startsWith ("master.")) prefixName = "Master";
    else
    {
        auto uid = id.substring (1).upToFirstOccurrenceOf (".", false, false).getIntValue();
        if (auto* t = getTrackByUid (uid)) prefixName = t->name;
    }
    if (auto* p = findParam (id)) return prefixName + ": " + p->name;
    return id;
}

std::vector<Param*> AudioEngine::getAllParams()
{
    const juce::ScopedLock sl (lock);
    std::vector<Param*> r { &masterVolume, &energy, &crossfader, &swing, &riserBars };
    for (auto& fx : masterFx) for (auto* p : fx->getParams()) r.push_back (p);
    for (auto& t : tracks)
    {
        for (auto* p : t->getMixerParams()) r.push_back (p);
        if (t->instrument) for (auto* p : t->instrument->getParams()) r.push_back (p);
        for (auto& fx : t->effects) for (auto* p : fx->getParams()) r.push_back (p);
    }
    return r;
}

//==============================================================================
void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m)
{
    if (m.isMidiClock() || m.isActiveSense() || m.isSysEx()) return;
    auto name = source != nullptr ? source->getName() : juce::String ("Teclado");

    {
        const juce::ScopedLock sl (lock);
        if (mapper.handle (name, m)) return;
    }

    auto msg = m;
    if (msg.getTimeStamp() <= 0.0) msg.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);

    if (padsToDrums.load() && msg.isNoteOnOrOff() && (name.containsIgnoreCase ("MPD") || msg.getChannel() == 10))
        drumCollector.addMessageToQueue (msg);
    else
        collector.addMessageToQueue (msg);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const juce::ScopedLock sl (lock);
    sampleRate = device->getCurrentSampleRate();
    blockSize = device->getCurrentBufferSizeSamples();
    mix.setSize (2, maxChunk);
    liveMidi.ensureSize (4096); drumMidi.ensureSize (4096); selMidi.ensureSize (4096);
    collector.reset (sampleRate);
    drumCollector.reset (sampleRate);
    for (auto& t : tracks) t->prepare (sampleRate, maxChunk);
    for (auto& fx : masterFx) fx->prepare (sampleRate, maxChunk);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxChunk, 2 };
    energyFilter.prepare (spec); energyFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    riserFilter.prepare (spec);  riserFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    riserHpf.prepare (spec);     riserHpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    energySmooth.reset (sampleRate, 60.0f, energy.get());
}

void AudioEngine::audioDeviceStopped() {}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    const float* inPtrs[16] {};
    const int ins = juce::jmin (numIn, 16);

    for (int offset = 0; offset < numSamples; offset += maxChunk)
    {
        auto n = juce::jmin (maxChunk, numSamples - offset);
        for (int c = 0; c < ins; ++c) inPtrs[c] = in[c] != nullptr ? in[c] + offset : nullptr;
        processChunk (inPtrs, ins, n);
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr)
                juce::FloatVectorOperations::copy (out[c] + offset, mix.getReadPointer (juce::jmin (c, 1)), n);
    }
}

void AudioEngine::processChunk (const float* const* in, int numIn, int n)
{
    mix.clear (0, n);
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked()) return; // UI is swapping something: one silent chunk is better than a glitch

    BeatInfo beat;
    beat.sampleRate = sampleRate;
    beat.bpm = transport.bpm.load();
    beat.playing = transport.playing.load();
    beat.blockStartBeat = transport.positionBeats.load();
    beat.beatsPerSample = beat.playing ? beat.bpm / (60.0 * sampleRate) : 0.0;
    const double b0 = beat.blockStartBeat;
    const double b1 = b0 + n * (beat.bpm / (60.0 * sampleRate));

    // Quantised scene launch on bar lines
    if (auto ps = pendingScene.load(); ps >= 0)
    {
        const bool barLine = std::floor (b1 / beatsPerBar) > std::floor (b0 / beatsPerBar) || std::fmod (b0, (double) beatsPerBar) < 1e-6;
        if (! beat.playing || barLine) applyScene (ps);
    }

    // Riser -> drop
    float riser = 0.0f;
    if (riserActive.load())
    {
        auto prog = (float) ((b0 - riserStart) / riserLength);
        if (prog >= 1.0f)
        {
            riserActive = false;
            riserProgress = 0.0f;
            if (riserTargetScene >= 0) applyScene (riserTargetScene);
            energy.set (1.0f);
        }
        else riser = juce::jmax (0.0f, prog);
        riserProgress = riser;
    }

    // MIDI from controllers + on-screen/computer keyboard
    liveMidi.clear(); drumMidi.clear();
    collector.removeNextBlockOfMessages (liveMidi, n);
    drumCollector.removeNextBlockOfMessages (drumMidi, n);
    keyboardState.processNextMidiBuffer (liveMidi, 0, n, true);

    const int sel = selectedTrack.load();
    auto* drums = findDrumTrack();
    bool anySolo = false;
    for (auto& t : tracks) anySolo |= t->solo.get() > 0.5f;
    const float xf = crossfader.get();
    const float gainA = juce::jmin (1.0f, 2.0f * (1.0f - xf)), gainB = juce::jmin (1.0f, 2.0f * xf);
    const float sw = swing.get();

    for (auto& t : tracks)
    {
        selMidi.clear();
        if (t->uid == sel) selMidi.addEvents (liveMidi, 0, n, 0);
        if (t.get() == drums || (drums == nullptr && t->uid == sel)) selMidi.addEvents (drumMidi, 0, n, 0);

        auto side = (int) std::round (t->xfade.get());
        t->sceneGain = side == 1 ? gainA : side == 2 ? gainB : 1.0f;
        t->swing = sw;
        t->process (n, beat, selMidi, in, numIn);

        const bool audible = t->mute.get() < 0.5f && (! anySolo || t->solo.get() > 0.5f);
        if (audible)
            for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, t->getOutput(), c, 0, n);
    }

    // Metronome
    if (transport.metronome.load() && beat.playing)
    {
        for (int i = 0; i < n; ++i)
        {
            auto b = beat.beatAt (i);
            auto prev = i == 0 ? b0 - 1e-9 : beat.beatAt (i - 1);
            if (std::floor (b) > std::floor (prev) || (i == 0 && std::fmod (b0, 1.0) < 1e-9))
            {
                metroEnv = 1.0f;
                metroFreq = ((long long) std::floor (b + 1e-9)) % beatsPerBar == 0 ? 1600.0f : 1000.0f;
            }
            if (metroEnv > 0.0001f)
            {
                metroPhase += metroFreq / sampleRate;
                auto s = (float) std::sin (juce::MathConstants<double>::twoPi * metroPhase) * metroEnv * 0.3f;
                metroEnv *= 0.9985f;
                mix.addSample (0, i, s); mix.addSample (1, i, s);
            }
        }
    }

    // Master: energy filter (calm = dark/muffled, 100% = open) + riser
    juce::AudioBuffer<float> view (mix.getArrayOfWritePointers(), 2, n);
    {
        for (int i = 0; i < n; ++i)
        {
            auto e = energySmooth.next (energy.get());
            if ((i & 15) == 0)
            {
                energyFilter.setCutoffFrequency (180.0f * std::pow (110.0f, e));
                riserFilter.setCutoffFrequency (400.0f + riser * riser * 8000.0f);
                riserFilter.setResonance (2.0f);
                riserHpf.setCutoffFrequency (20.0f + riser * riser * 900.0f);
            }
            for (int c = 0; c < 2; ++c)
            {
                auto x = mix.getSample (c, i);
                if (e < 0.985f) x = energyFilter.processSample (c, x);
                if (riser > 0.0f)
                {
                    x = riserHpf.processSample (c, x);
                    x += riserFilter.processSample (c, rng.nextFloat() * 2.0f - 1.0f) * riser * riser * 0.35f;
                }
                mix.setSample (c, i, x);
            }
        }
    }

    for (auto& fx : masterFx)
        if (fx->isOn()) fx->process (view, beat);

    mix.applyGain (0, n, masterVolume.get());
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        auto l = juce::jlimit (-1.0f, 1.0f, mix.getSample (0, i));
        auto r = juce::jlimit (-1.0f, 1.0f, mix.getSample (1, i));
        mix.setSample (0, i, l); mix.setSample (1, i, r);
        pl = juce::jmax (pl, std::abs (l)); pr = juce::jmax (pr, std::abs (r));
    }
    masterPeakL = juce::jmax (masterPeakL.load(), pl);
    masterPeakR = juce::jmax (masterPeakR.load(), pr);

    if (recorderActive.load())
    {
        const juce::ScopedTryLock wl (writerLock);
        if (wl.isLocked() && threaded) threaded->write (mix.getArrayOfReadPointers(), n);
    }

    if (beat.playing) transport.positionBeats = b1;
}

} // namespace mcl
