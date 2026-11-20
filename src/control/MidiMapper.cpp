#include "control/MidiMapper.h"

namespace mcl
{

MidiMapper::Mode MidiMapper::guessMode (Source s, const Param* p, const Action* a)
{
    if (a != nullptr) return Mode::Trigger;
    if (p == nullptr) return Mode::Absolute;
    static const juce::StringArray switches { "Mute", "Solo", "On", "Monitor", "Freeze", "Ping Pong" };
    if (p->name == "Hold") return Mode::Momentary;
    if (switches.contains (p->name)) return Mode::Toggle;
    return s == Source::Note ? Mode::Momentary : Mode::Absolute;
}

void MidiMapper::startLearn (const juce::String& targetId)
{
    { const juce::ScopedLock sl (lock); learnTarget = targetId; lastLearn = {}; }
    sendChangeMessage();
}

void MidiMapper::cancelLearn()
{
    { const juce::ScopedLock sl (lock); learnTarget = {}; }
    sendChangeMessage();
}

void MidiMapper::removeMappingsFor (const juce::String& target)
{
    { const juce::ScopedLock sl (lock);
      mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const Mapping& m) { return m.target == target; }), mappings.end()); }
    sendChangeMessage();
}

void MidiMapper::removeMapping (int index)
{
    { const juce::ScopedLock sl (lock); if (juce::isPositiveAndBelow (index, (int) mappings.size())) mappings.erase (mappings.begin() + index); }
    sendChangeMessage();
}

void MidiMapper::addMapping (const Mapping& m)
{
    { const juce::ScopedLock sl (lock);
      // a physical control drives only one target
      mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const Mapping& o)
          { return o.source == m.source && o.number == m.number && o.channel == m.channel && o.device == m.device; }), mappings.end());
      mappings.push_back (m); }
    sendChangeMessage();
}

juce::String MidiMapper::describeMapping (const Mapping& m) const
{
    auto src = juce::String (m.source == Source::CC ? "CC " : "Nota ") + juce::String (m.number);
    if (m.channel > 0) src << " ch" << m.channel;
    if (m.device.isNotEmpty()) src << " (" << m.device << ")";
    return src;
}

juce::String MidiMapper::mappingLabelFor (const juce::String& target) const
{
    const juce::ScopedLock sl (lock);
    for (auto& m : mappings) if (m.target == target) return describeMapping (m);
    return {};
}

bool MidiMapper::handle (const juce::String& device, const juce::MidiMessage& m)
{
    const bool isCC = m.isController();
    const bool isNote = m.isNoteOnOrOff();
    if (! isCC && ! isNote) return false;

    const auto source = isCC ? Source::CC : Source::Note;
    const int number = isCC ? m.getControllerNumber() : m.getNoteNumber();
    const int channel = m.getChannel();

    juce::String learnId;
    {
        const juce::ScopedLock sl (lock);
        lastMessage = device + ": " + (isCC ? "CC " : "Nota ") + juce::String (number) + " ch" + juce::String (channel)
                      + " = " + juce::String (isCC ? m.getControllerValue() : m.getVelocity());
        learnId = learnTarget;
    }

    if (learnId.isNotEmpty())
    {
        if (isNote && ! m.isNoteOn()) return true; // wait for a note-on
        Mapping mp;
        mp.device = device;
        mp.source = source;
        mp.channel = channel;
        mp.number = number;
        mp.target = learnId;
        auto* p = findParam ? findParam (learnId) : nullptr;
        auto* a = findAction ? findAction (learnId) : nullptr;
        mp.mode = guessMode (source, p, a);
        addMapping (mp);
        {
            const juce::ScopedLock sl (lock);
            lastLearn = describeMapping (mp) + "  ->  " + (describeTarget ? describeTarget (learnId) : learnId);
            learnTarget = {};
        }
        sendChangeMessage();
        return true;
    }

    std::vector<Mapping> matches;
    {
        const juce::ScopedLock sl (lock);
        for (auto& mp : mappings)
            if (mp.source == source && mp.number == number
                && (mp.channel == 0 || mp.channel == channel)
                && (mp.device.isEmpty() || device.containsIgnoreCase (mp.device)))
                matches.push_back (mp);
    }
    if (matches.empty()) return false;

    for (auto& mp : matches)
    {
        auto* p = findParam ? findParam (mp.target) : nullptr;
        auto* a = (p == nullptr && findAction) ? findAction (mp.target) : nullptr;
        const float v = isCC ? (float) m.getControllerValue() / 127.0f : (m.isNoteOn() ? m.getFloatVelocity() : 0.0f);
        const bool press = isCC ? m.getControllerValue() >= 64 : m.isNoteOn();

        switch (mp.mode)
        {
            case Mode::Absolute:  if (p) p->setNormalised (v); break;
            case Mode::Relative:
                if (p && isCC)
                {
                    auto raw = m.getControllerValue();
                    auto delta = raw < 64 ? raw : raw - 128; // two's complement (jog wheels)
                    p->setNormalised (p->getNormalised() + (float) delta / 200.0f);
                }
                break;
            case Mode::Momentary: if (p) p->set (press ? p->maxValue : p->minValue); break;
            case Mode::Toggle:
                if (p && press) p->set (p->get() > (p->minValue + p->maxValue) * 0.5f ? p->minValue : p->maxValue);
                break;
            case Mode::Trigger:
                if (press)
                {
                    if (a && a->trigger) a->trigger();
                    else if (p) p->set (p->get() > 0.5f ? 0.0f : 1.0f);
                }
                break;
        }
    }
    return true;
}

void MidiMapper::loadDefaultMappings()
{
    std::vector<Mapping> m;
    // Akai MPD218 factory preset 1: knobs send CC 3, 9, 12, 13, 14, 15, 16, 17
    const int mpdKnobs[] = { 3, 9, 12, 13, 14, 15, 16, 17 };
    const char* knobTargets[] = { "master.energy", "sel.inst.cutoff", "sel.inst.reso", "sel.inst.keymorph",
                                  "sel.inst.detune", "sel.volume", "master.crossfader", "master.volume" };
    for (int i = 0; i < 8; ++i)
        m.push_back ({ "MPD218", Source::CC, 0, mpdKnobs[i], knobTargets[i], Mode::Absolute });

    // Any keyboard: mod wheel -> energy
    m.push_back ({ {}, Source::CC, 0, 1, "master.energy", Mode::Absolute });
    setMappings (std::move (m));
}

juce::ValueTree MidiMapper::toValueTree() const
{
    juce::ValueTree v ("MAPPINGS");
    const juce::ScopedLock sl (lock);
    for (auto& mp : mappings)
    {
        juce::ValueTree c ("MAP");
        c.setProperty ("device", mp.device, nullptr);
        c.setProperty ("src", (int) mp.source, nullptr);
        c.setProperty ("ch", mp.channel, nullptr);
        c.setProperty ("num", mp.number, nullptr);
        c.setProperty ("target", mp.target, nullptr);
        c.setProperty ("mode", (int) mp.mode, nullptr);
        v.appendChild (c, nullptr);
    }
    return v;
}

void MidiMapper::fromValueTree (const juce::ValueTree& v)
{
    std::vector<Mapping> m;
    for (auto c : v)
        m.push_back ({ c.getProperty ("device").toString(), (Source) (int) c.getProperty ("src"), (int) c.getProperty ("ch"),
                       (int) c.getProperty ("num"), c.getProperty ("target").toString(), (Mode) (int) c.getProperty ("mode") });
    setMappings (std::move (m));
}

} // namespace mcl
