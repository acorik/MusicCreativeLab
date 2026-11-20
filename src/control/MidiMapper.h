#pragma once
#include "core/Param.h"

namespace mcl
{

// MIDI learn + mapping for any controller (Akai MPD218, Numark Platinum FX,
// Stream Deck via a MIDI plugin, keyboards...).
class MidiMapper : public juce::ChangeBroadcaster
{
public:
    enum class Source { CC = 0, Note = 1 };
    enum class Mode   { Absolute = 0, Toggle = 1, Momentary = 2, Trigger = 3, Relative = 4 };

    struct Mapping
    {
        juce::String device;   // empty = any device
        Source source = Source::CC;
        int channel = 0;       // 0 = any
        int number = 0;
        juce::String target;   // param id or action id ("sel." = selected track)
        Mode mode = Mode::Absolute;
    };

    // Resolvers supplied by the engine
    std::function<Param*  (const juce::String& id)> findParam;
    std::function<Action* (const juce::String& id)> findAction;
    std::function<juce::String (const juce::String& id)> describeTarget;

    // Returns true if the message was consumed by a mapping (or by learn)
    bool handle (const juce::String& device, const juce::MidiMessage& m);

    void startLearn (const juce::String& targetId);
    void cancelLearn();
    bool isLearning() const { const juce::ScopedLock sl (lock); return learnTarget.isNotEmpty(); }
    juce::String getLearnTarget() const { const juce::ScopedLock sl (lock); return learnTarget; }
    juce::String getLastLearnMessage() const { const juce::ScopedLock sl (lock); return lastLearn; }

    // Learn mode for the whole UI: when on, clicking any control selects it as target
    std::atomic<bool> learnModeUI { false };

    std::vector<Mapping> getMappings() const { const juce::ScopedLock sl (lock); return mappings; }
    void setMappings (std::vector<Mapping> m) { const juce::ScopedLock sl (lock); mappings = std::move (m); sendChangeMessage(); }
    void removeMappingsFor (const juce::String& target);
    void removeMapping (int index);
    void addMapping (const Mapping& m);
    juce::String describeMapping (const Mapping& m) const;
    juce::String mappingLabelFor (const juce::String& target) const;

    void loadDefaultMappings();   // MPD218 knobs, generic keyboard mod wheel...

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);

    // last message seen (for the MIDI monitor)
    juce::String getLastMessageText() const { const juce::ScopedLock sl (lock); return lastMessage; }

private:
    static Mode guessMode (Source s, const Param* p, const Action* a);
    mutable juce::CriticalSection lock;
    std::vector<Mapping> mappings;
    juce::String learnTarget, lastLearn, lastMessage;
    std::map<juce::String, bool> toggleLatch;
};

} // namespace mcl
