#include "project/Project.h"
#include "dsp/DrumMachine.h"

namespace mcl
{
namespace Project
{

static juce::String key (const juce::String& s) { return s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"); }

juce::File getDefaultFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Music Creative Lab").getChildFile ("Proyectos");
    f.createDirectory();
    return f;
}

//==============================================================================
static juce::ValueTree fxToTree (Effect& fx)
{
    auto v = fx.getState();
    v.setProperty ("prefix", fx.getPrefix(), nullptr);
    return v;
}

static std::unique_ptr<Effect> fxFromTree (AudioEngine& engine, const juce::ValueTree& v, const juce::String& prefix, juce::String& err)
{
    auto type = v.getProperty ("type").toString();
    std::unique_ptr<Effect> fx;
    if (type == "plugin")
    {
        if (auto xml = juce::parseXML (v.getProperty ("desc").toString()))
        {
            juce::PluginDescription d;
            if (d.loadFromXml (*xml)) fx = engine.createPluginEffect (d, prefix, err);
            if (! fx) err << "No se pudo cargar el plugin " << d.name << "\n";
        }
    }
    else fx = createBuiltInEffect (type, prefix);
    if (fx) fx->setState (v);
    return fx;
}

juce::ValueTree save (AudioEngine& engine)
{
    juce::ValueTree root ("MCLPROJECT");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("bpm", engine.transport.bpm.load(), nullptr);
    for (auto* p : { &engine.masterVolume, &engine.energy, &engine.crossfader, &engine.swing, &engine.riserBars })
        root.setProperty (key (p->name), p->get(), nullptr);

    const juce::ScopedLock sl (engine.getLock());
    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        auto* t = engine.getTrack (i);
        juce::ValueTree tv ("TRACK");
        tv.setProperty ("uid", t->uid, nullptr);
        tv.setProperty ("kind", t->kind == TrackKind::Audio ? "audio" : "instrument", nullptr);
        tv.setProperty ("name", t->name, nullptr);
        tv.setProperty ("colour", t->colour.toString(), nullptr);
        for (auto* p : t->getMixerParams()) tv.setProperty (key (p->name), p->get(), nullptr);
        if (t->instrument) tv.appendChild (t->instrument->getState(), nullptr);
        for (auto& fx : t->effects) tv.appendChild (fxToTree (*fx), nullptr);
        auto clip = t->getClipCopy().toValueTree();
        clip.setProperty ("origin", t->clipOrigin, nullptr);
        clip.setProperty ("playing", t->getClipState() == ClipState::Playing || t->getClipState() == ClipState::Overdub, nullptr);
        tv.appendChild (clip, nullptr);
        root.appendChild (tv, nullptr);
    }

    juce::ValueTree master ("MASTERFX");
    for (auto& fx : engine.getMasterEffects()) master.appendChild (fxToTree (*fx), nullptr);
    root.appendChild (master, nullptr);

    juce::ValueTree scenes ("SCENES");
    for (auto& s : engine.scenes)
    {
        juce::ValueTree sv ("SCENE");
        sv.setProperty ("name", s.name, nullptr);
        sv.setProperty ("energy", s.energy, nullptr);
        for (auto& [uid, st] : s.tracks)
        {
            juce::ValueTree tv ("T");
            tv.setProperty ("uid", uid, nullptr);
            tv.setProperty ("active", st.active, nullptr);
            tv.setProperty ("volume", st.volume, nullptr);
            sv.appendChild (tv, nullptr);
        }
        scenes.appendChild (sv, nullptr);
    }
    root.appendChild (scenes, nullptr);
    root.appendChild (engine.mapper.toValueTree(), nullptr);
    return root;
}

juce::String load (AudioEngine& engine, const juce::ValueTree& root)
{
    if (! root.hasType ("MCLPROJECT")) return "No es un proyecto de Music Creative Lab";
    juce::String err;
    engine.stop(); engine.stop();
    engine.clearAllTracks();

    engine.transport.bpm = (double) root.getProperty ("bpm", 132.0);
    for (auto* p : { &engine.masterVolume, &engine.energy, &engine.crossfader, &engine.swing, &engine.riserBars })
        if (root.hasProperty (key (p->name))) p->set ((float) root.getProperty (key (p->name)));

    for (auto tv : root)
    {
        if (! tv.hasType ("TRACK")) continue;
        auto kind = tv.getProperty ("kind").toString() == "audio" ? TrackKind::Audio : TrackKind::Instrument;
        auto* t = engine.addTrack (kind, tv.getProperty ("name").toString(), (int) tv.getProperty ("uid"), false);
        t->colour = juce::Colour::fromString (tv.getProperty ("colour").toString());
        for (auto* p : t->getMixerParams())
            if (tv.hasProperty (key (p->name))) p->set ((float) tv.getProperty (key (p->name)));

        for (auto c : tv)
        {
            if (c.hasType ("INSTRUMENT"))
            {
                auto type = c.getProperty ("type").toString();
                std::unique_ptr<Instrument> inst;
                if (type == "plugin")
                {
                    if (auto xml = juce::parseXML (c.getProperty ("desc").toString()))
                    {
                        juce::PluginDescription d;
                        if (d.loadFromXml (*xml)) inst = engine.createPluginInstrument (d, *t, err);
                        if (! inst) err << "No se pudo cargar el instrumento " << d.name << " (se usa Lab Synth)\n";
                    }
                }
                if (! inst) inst = engine.createBuiltInInstrument (type == "plugin" ? "synth" : type, *t);
                if (type != "plugin" || inst->getTypeId() == "plugin") inst->setState (c);
                engine.setInstrument (*t, std::move (inst));
            }
            else if (c.hasType ("FX"))
            {
                auto prefix = c.getProperty ("prefix").toString();
                if (prefix.isEmpty()) prefix = t->newFxPrefix();
                t->bumpFxCounter (prefix.fromLastOccurrenceOf ("fx", false, false).getIntValue());
                if (auto fx = fxFromTree (engine, c, prefix, err)) engine.addEffect (*t, std::move (fx));
            }
            else if (c.hasType ("CLIP"))
            {
                MidiClip clip;
                clip.fromValueTree (c);
                t->setClipFromUI (clip, (bool) c.getProperty ("playing", true));
                t->clipOrigin = (double) c.getProperty ("origin", 0.0);
            }
        }
    }

    // master fx: replace defaults
    while (! engine.getMasterEffects().empty()) engine.removeMasterEffect (0);
    auto master = root.getChildWithName ("MASTERFX");
    for (auto c : master)
        if (auto fx = fxFromTree (engine, c, c.getProperty ("prefix").toString(), err)) engine.addMasterEffect (std::move (fx));
    if (engine.getMasterEffects().empty()) engine.addMasterEffect (createBuiltInEffect ("limiter", engine.newMasterFxPrefix()));

    {
        const juce::ScopedLock sl (engine.getLock());
        engine.scenes.clear();
        for (auto sv : root.getChildWithName ("SCENES"))
        {
            Scene s;
            s.name = sv.getProperty ("name").toString();
            s.energy = sv.getProperty ("energy", 0.5f);
            for (auto tv : sv) s.tracks[(int) tv.getProperty ("uid")] = { (bool) tv.getProperty ("active"), (float) tv.getProperty ("volume") };
            engine.scenes.push_back (s);
        }
    }

    auto maps = root.getChildWithName ("MAPPINGS");
    if (maps.isValid()) engine.mapper.fromValueTree (maps);

    if (auto* first = engine.getTrack (0)) engine.selectTrack (first->uid);
    engine.sendChangeMessage();
    return err;
}

bool saveToFile (AudioEngine& engine, const juce::File& f)
{
    if (auto xml = save (engine).createXml()) return xml->writeTo (f);
    return false;
}

juce::String loadFromFile (AudioEngine& engine, const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr) return "No se pudo leer " + f.getFileName();
    return load (engine, juce::ValueTree::fromXml (*xml));
}

//==============================================================================
static void setFx (Effect* fx, const juce::String& name, float v) { if (fx) if (auto* p = fx->findParam (name)) p->set (v); }

const std::vector<TrackPreset>& getTrackPresets()
{
    static std::vector<TrackPreset> presets {
        { "Fred Pad (calma)",    "Synths",   "synth", 0, { "chorus", "pump", "reverb" } },
        { "Glass Keys",          "Synths",   "synth", 1, { "delay", "reverb" } },
        { "Garage Bass",         "Bajos",    "synth", 2, { "distortion", "compressor" } },
        { "Emo Lead",            "Synths",   "synth", 3, { "delay", "reverb" } },
        { "Pluck Chops",         "Synths",   "synth", 4, { "delay", "reverb" } },
        { "Sub Boom",            "Bajos",    "synth", 5, { "compressor" } },
        { "Supersaw Drop",       "Synths",   "synth", 6, { "pump", "reverb", "widener" } },
        { "Choir Air",           "Pads",     "synth", 7, { "reverb", "widener" } },
        { "Warm Organ Stab",     "Synths",   "synth", 8, { "reverb" } },
        { "Dreamy Arp",          "Synths",   "synth", 9, { "delay", "reverb" } },
        { "Drums UK Garage",     "Baterias", "drums", 0, { "compressor" } },
        { "Drums Soft Fabric",   "Baterias", "drums", 1, { "reverb" } },
        { "Drums Heavy Drop",    "Baterias", "drums", 2, { "distortion", "compressor" } },
        { "Micro SM7B (voz)",    "Micro",    "audio", 0, {} },
        { "Micro voz robot",     "Micro",    "audio", 1, { "ringmod" } },
    };
    return presets;
}

Track* addTrackFromPreset (AudioEngine& engine, const TrackPreset& p)
{
    if (p.instrument == "audio")
    {
        auto* t = engine.addTrack (TrackKind::Audio, p.name);
        for (auto& id : p.fx) engine.addEffect (*t, id);
        return t;
    }
    auto* t = engine.addTrack (TrackKind::Instrument, p.name);
    auto inst = engine.createBuiltInInstrument (p.instrument, *t);
    inst->loadPreset (p.preset);
    engine.setInstrument (*t, std::move (inst));
    for (auto& id : p.fx)
    {
        auto* fx = engine.addEffect (*t, id);
        if (id == "reverb") setFx (fx, "Mix", p.instrument == "drums" ? 0.12f : 0.32f);
        if (id == "distortion") setFx (fx, "Drive", 0.25f);
        if (id == "pump") setFx (fx, "Depth", 0.45f);
    }
    if (p.instrument == "drums") { t->quantize.set (3.0f); t->loopBars.set (0.0f); }
    return t;
}

static Track* addPreset (AudioEngine& e, const juce::String& name, const juce::String& rename = {})
{
    for (auto& p : getTrackPresets())
        if (p.name == name)
        {
            auto* t = addTrackFromPreset (e, p);
            if (rename.isNotEmpty()) t->name = rename;
            return t;
        }
    return nullptr;
}

//==============================================================================
// Composition helpers
static void chord (MidiClip& c, double start, double len, std::initializer_list<int> notes, float vel = 0.7f)
{
    for (auto n : notes) c.notes.push_back ({ n, start, len, vel });
}

static MidiClip padClip (const std::vector<std::vector<int>>& chords, double barLen = 4.0)
{
    MidiClip c;
    c.lengthBeats = barLen * (double) chords.size();
    for (size_t i = 0; i < chords.size(); ++i)
        for (auto n : chords[i]) c.notes.push_back ({ n, i * barLen, barLen - 0.1, 0.6f });
    return c;
}

static MidiClip arpClip (const std::vector<std::vector<int>>& chords, int octaveUp, const std::vector<double>& rhythm, float swingAmt)
{
    MidiClip c;
    c.lengthBeats = 4.0 * (double) chords.size();
    int k = 0;
    for (size_t bar = 0; bar < chords.size(); ++bar)
    {
        auto& ch = chords[bar];
        for (auto r : rhythm)
        {
            auto step = (int) std::round (r / 0.25);
            auto pos = bar * 4.0 + r + ((step % 2) ? swingAmt * 0.25 : 0.0);
            auto pitch = ch[(size_t) (k++ % (int) ch.size())] + 12 * octaveUp;
            c.notes.push_back ({ pitch, pos, 0.22, 0.55f + 0.25f * (float) ((k * 7) % 3) / 2.0f });
        }
    }
    return c;
}

static MidiClip garageDrums (float swingAmt, bool heavy)
{
    MidiClip c;
    c.lengthBeats = 4.0;
    auto sw = [swingAmt] (double b) { auto step = (int) std::round (b / 0.25); return b + ((step % 2) ? swingAmt * 0.25 : 0.0); };
    auto hit = [&] (int pad, double b, float v, double len = 0.1) { c.notes.push_back ({ DrumMachine::firstNote + pad, sw (b), len, v }); };

    // 2-step kick: the 1 and the "and" of 3
    hit (0, 0.0, 1.0f); hit (0, 2.5, 0.9f);
    if (heavy) { hit (0, 1.75, 0.6f); hit (12, 0.0, 0.8f, 1.5); }
    // clap + snare on 2 and 4
    hit (3, 1.0, 0.95f); hit (3, 3.0, 0.95f);
    hit (2, 3.0, 0.5f);
    // offbeat hats + swung ghost hats
    for (double b : { 0.5, 1.5, 2.5, 3.5 }) hit (4, b, 0.75f);
    for (double b : { 0.75, 1.25, 2.25, 3.75 }) hit (4, b, 0.35f);
    hit (6, 3.5, 0.5f);
    // shaker 16ths
    for (int i = 0; i < 16; ++i) hit (5, i * 0.25, (i % 2) ? 0.3f : 0.18f);
    // rim shuffle
    hit (1, 1.75, 0.5f); hit (1, 3.25, 0.45f);
    // vox chops
    hit (13, 2.75, 0.7f, 0.2); hit (14, 3.75, 0.55f, 0.2);
    c.sort();
    return c;
}

static MidiClip softDrums()
{
    MidiClip c;
    c.lengthBeats = 4.0;
    auto hit = [&] (int pad, double b, float v) { c.notes.push_back ({ DrumMachine::firstNote + pad, b, 0.1, v }); };
    hit (0, 0.0, 0.7f); hit (0, 2.75, 0.4f);
    hit (1, 1.0, 0.5f); hit (3, 3.0, 0.6f);
    for (int i = 0; i < 8; ++i) hit (4, i * 0.5 + 0.25, 0.3f + 0.1f * (float) (i % 2));
    hit (13, 3.5, 0.4f);
    c.sort();
    return c;
}

static MidiClip bassClip (const std::vector<int>& roots)
{
    MidiClip c;
    c.lengthBeats = 4.0 * (double) roots.size();
    for (size_t bar = 0; bar < roots.size(); ++bar)
    {
        auto r = roots[bar]; auto o = bar * 4.0;
        c.notes.push_back ({ r,      o + 0.0,  0.7,  0.9f });
        c.notes.push_back ({ r + 12, o + 1.5,  0.2,  0.7f });
        c.notes.push_back ({ r,      o + 2.5,  0.45, 0.85f });
        c.notes.push_back ({ r + 12, o + 3.25, 0.2,  0.6f });
        c.notes.push_back ({ r + 7,  o + 3.5,  0.25, 0.7f });
    }
    return c;
}

static MidiClip stabClip (const std::vector<std::vector<int>>& chords)
{
    MidiClip c;
    c.lengthBeats = 4.0 * (double) chords.size();
    for (size_t bar = 0; bar < chords.size(); ++bar)
        for (double b : { 0.0, 0.75, 2.5 })
            for (auto n : chords[bar]) c.notes.push_back ({ n + 12, bar * 4.0 + b, 0.35, 0.65f });
    return c;
}

static void scene (AudioEngine& e, int index, const juce::String& name, float energy, const std::map<Track*, float>& on)
{
    e.captureScene (index);
    auto& s = e.scenes[(size_t) index];
    s.name = name;
    s.energy = energy;
    for (int i = 0; i < e.getNumTracks(); ++i)
    {
        auto* t = e.getTrack (i);
        auto it = on.find (t);
        s.tracks[t->uid] = { it != on.end(), it != on.end() ? it->second : t->volume.get() };
    }
}

juce::StringArray getDemoNames()
{
    return { "1. Calma -> UK Garage (132 BPM)", "2. Emo 2-Step nocturno (138 BPM)", "3. Ambient Lab para improvisar (96 BPM)" };
}

juce::StringArray getDemoDescriptions()
{
    return {
        "Empieza con pads tranquilos estilo Fred again.., pasa por un build y explota en un drop de UK garage. Usa las escenas 1-4 o el boton RISER.",
        "Acordes de coro y un lead emotivo que se convierten en un 2-step pesado con sub-bajo. Ideal para tocar melodias encima con la MPD218.",
        "Pads y arpegios lentos con mucho espacio. Las pistas Bateria, Bajo y Micro estan vacias para que grabes tus propios loops.",
    };
}

void newEmptyProject (AudioEngine& e)
{
    e.stop(); e.stop();
    e.clearAllTracks();
    while (! e.getMasterEffects().empty()) e.removeMasterEffect (0);
    e.addMasterEffect (createBuiltInEffect ("limiter", e.newMasterFxPrefix()));
    e.transport.bpm = 130.0;
    e.energy.set (1.0f);
    addPreset (e, "Drums UK Garage", "Bateria");
    addPreset (e, "Fred Pad (calma)", "Pad");
    addPreset (e, "Garage Bass", "Bajo");
    addPreset (e, "Glass Keys", "Keys");
    addPreset (e, "Micro SM7B (voz)", "Micro");
    if (auto* t = e.getTrack (0)) e.selectTrack (t->uid);
}

void loadDemo (AudioEngine& e, int index)
{
    e.stop(); e.stop();
    e.clearAllTracks();
    while (! e.getMasterEffects().empty()) e.removeMasterEffect (0);
    e.addMasterEffect (createBuiltInEffect ("limiter", e.newMasterFxPrefix()));
    const float sw = 0.14f;
    e.swing.set (sw);

    if (index == 0)
    {
        e.transport.bpm = 132.0;
        // Ab maj7 - Fm9 - Db maj7 - Eb add9
        std::vector<std::vector<int>> chords { { 56, 60, 63, 67 }, { 53, 56, 60, 63, 67 }, { 49, 53, 56, 60 }, { 51, 55, 58, 65 } };

        auto* drums = addPreset (e, "Drums UK Garage", "Drums Garage");
        drums->setClipFromUI (garageDrums (sw, false), true);
        auto* soft = addPreset (e, "Drums Soft Fabric", "Drums Calma");
        soft->setClipFromUI (softDrums(), true);
        auto* pad = addPreset (e, "Fred Pad (calma)", "Pad");
        pad->setClipFromUI (padClip (chords), true);
        auto* keys = addPreset (e, "Glass Keys", "Keys");
        keys->setClipFromUI (arpClip (chords, 1, { 0.0, 0.75, 1.5, 2.0, 2.75, 3.5 }, sw), true);
        auto* bass = addPreset (e, "Garage Bass", "Bajo");
        bass->setClipFromUI (bassClip ({ 44, 41, 37, 39 }), true);
        auto* drop = addPreset (e, "Supersaw Drop", "Synth Drop");
        drop->setClipFromUI (stabClip (chords), true);
        drop->volume.set (0.55f);
        auto* mic = addPreset (e, "Micro SM7B (voz)", "Micro");
        mic->monitor.set (0.0f);
        drums->xfade.set (2.0f); bass->xfade.set (2.0f); drop->xfade.set (2.0f);
        soft->xfade.set (1.0f);

        scene (e, 0, "Intro calma", 0.45f, { { pad, 0.8f }, { keys, 0.55f }, { soft, 0.7f }, { mic, 0.8f } });
        scene (e, 1, "Build", 0.7f, { { pad, 0.8f }, { keys, 0.7f }, { soft, 0.8f }, { bass, 0.6f }, { mic, 0.8f } });
        scene (e, 2, "DROP garage", 1.0f, { { drums, 0.9f }, { bass, 0.85f }, { drop, 0.55f }, { pad, 0.45f }, { keys, 0.6f }, { mic, 0.8f } });
        scene (e, 3, "Breakdown emo", 0.6f, { { pad, 0.85f }, { keys, 0.7f }, { soft, 0.6f }, { mic, 0.8f } });
        e.launchScene (0);
        e.selectTrack (keys->uid);
    }
    else if (index == 1)
    {
        e.transport.bpm = 138.0;
        // Cm9 - Ab maj7 - Eb - Bb
        std::vector<std::vector<int>> chords { { 48, 51, 55, 58, 62 }, { 44, 48, 51, 55 }, { 51, 55, 58, 62 }, { 46, 50, 53, 58 } };
        auto* choir = addPreset (e, "Choir Air", "Coro");
        choir->setClipFromUI (padClip (chords), true);
        auto* lead = addPreset (e, "Emo Lead", "Lead");
        MidiClip melody; melody.lengthBeats = 16.0;
        int mel[] = { 67, 70, 72, 70, 67, 65, 63, 62, 63, 65, 67, 70, 72, 74, 75, 74 };
        for (int i = 0; i < 16; ++i) melody.notes.push_back ({ mel[i], i * 1.0 + ((i % 4) == 3 ? 0.5 : 0.0), (i % 4) == 3 ? 0.4 : 0.85, 0.75f });
        lead->setClipFromUI (melody, true);
        auto* drums = addPreset (e, "Drums Heavy Drop", "Drums");
        drums->setClipFromUI (garageDrums (sw, true), true);
        auto* sub = addPreset (e, "Sub Boom", "Sub");
        sub->setClipFromUI (bassClip ({ 36, 32, 39, 34 }), true);
        auto* pluck = addPreset (e, "Pluck Chops", "Chops");
        pluck->setClipFromUI (arpClip (chords, 1, { 0.0, 0.5, 0.75, 1.5, 2.25, 2.5, 3.0, 3.75 }, sw), true);
        auto* mic = addPreset (e, "Micro SM7B (voz)", "Micro");
        mic->monitor.set (0.0f);

        scene (e, 0, "Nocturno", 0.4f, { { choir, 0.85f }, { mic, 0.8f } });
        scene (e, 1, "Melodia", 0.65f, { { choir, 0.8f }, { lead, 0.7f }, { pluck, 0.5f }, { mic, 0.8f } });
        scene (e, 2, "DROP pesado", 1.0f, { { drums, 0.95f }, { sub, 0.9f }, { lead, 0.65f }, { pluck, 0.6f }, { choir, 0.5f }, { mic, 0.8f } });
        scene (e, 3, "Respiro", 0.55f, { { choir, 0.85f }, { pluck, 0.55f }, { mic, 0.8f } });
        e.launchScene (0);
        e.selectTrack (lead->uid);
    }
    else
    {
        e.transport.bpm = 96.0;
        // Dmaj9 - Bm11 - Gmaj7 - A6
        std::vector<std::vector<int>> chords { { 50, 54, 57, 61, 64 }, { 47, 54, 57, 62, 64 }, { 43, 50, 54, 57, 59 }, { 45, 52, 54, 57, 61 } };
        auto* pad = addPreset (e, "Fred Pad (calma)", "Pad");
        pad->setClipFromUI (padClip (chords), true);
        auto* arp = addPreset (e, "Dreamy Arp", "Arp");
        arp->setClipFromUI (arpClip (chords, 1, { 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5 }, 0.0f), true);
        addPreset (e, "Drums Soft Fabric", "Bateria (graba tu)");
        addPreset (e, "Sub Boom", "Bajo (graba tu)");
        addPreset (e, "Micro SM7B (voz)", "Micro");
        if (auto* t = e.getTrack (2)) { t->recMode.set (1.0f); e.selectTrack (t->uid); }
        e.energy.set (0.8f);
    }
}

} // namespace Project
} // namespace mcl
