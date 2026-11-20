#pragma once
#include "core/Param.h"
#include "core/Transport.h"

namespace mcl
{

class Effect
{
public:
    Effect (juce::String prefixIn) : prefix (std::move (prefixIn))
    {
        enabled = add ("On", 0.0f, 1.0f, 1.0f);
    }
    virtual ~Effect() = default;

    virtual juce::String getTypeId() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer, const BeatInfo& beat) = 0;
    virtual void reset() {}

    virtual bool hasEditor() const { return false; }
    virtual juce::AudioProcessorEditor* createEditor() { return nullptr; }
    virtual void showEditor() {}

    bool isOn() const noexcept { return enabled->get() > 0.5f; }

    virtual ParamList getParams()
    {
        ParamList l;
        for (auto& p : owned) l.push_back (p.get());
        return l;
    }

    virtual juce::ValueTree getState() const
    {
        juce::ValueTree v ("FX");
        v.setProperty ("type", getTypeId(), nullptr);
        for (auto& p : owned) v.setProperty (juce::Identifier (key (p->name)), p->get(), nullptr);
        return v;
    }

    virtual void setState (const juce::ValueTree& v)
    {
        for (auto& p : owned)
            if (v.hasProperty (juce::Identifier (key (p->name))))
                p->set ((float) v.getProperty (juce::Identifier (key (p->name))));
    }

    Param* findParam (const juce::String& name)
    {
        for (auto& p : owned) if (p->name == name) return p.get();
        return nullptr;
    }

    const juce::String& getPrefix() const { return prefix; }

protected:
    Param* add (const juce::String& name, float mn, float mx, float def, const juce::String& suffix = {}, float skew = 1.0f)
    {
        owned.push_back (std::make_unique<Param> (prefix + key (name).toLowerCase(), name, mn, mx, def, suffix, skew));
        return owned.back().get();
    }

    static juce::String key (const juce::String& s)
    {
        auto k = s.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        return k.isEmpty() ? juce::String ("p") : (juce::CharacterFunctions::isDigit (k[0]) ? "p" + k : k);
    }

    juce::String prefix;
    Param* enabled = nullptr;
    std::vector<std::unique_ptr<Param>> owned;
};

struct EffectType { juce::String id, name, category; };

const std::vector<EffectType>& getBuiltInEffectTypes();
std::unique_ptr<Effect> createBuiltInEffect (const juce::String& typeId, const juce::String& prefix);

// Tempo-synced note divisions used by delay / pump / stutter / autopan
const juce::StringArray& getSyncDivisionNames();
double syncDivisionToBeats (int index);

} // namespace mcl
