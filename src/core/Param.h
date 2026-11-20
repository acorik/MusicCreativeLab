#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>

namespace mcl
{

// A thread-safe automatable parameter. The UI, MIDI mapping, OSC (Stream Deck)
// and the audio thread all read/write the same atomic value.
struct Param
{
    Param (juce::String idIn, juce::String nameIn, float minIn, float maxIn, float defIn,
           juce::String suffixIn = {}, float skewIn = 1.0f)
        : id (std::move (idIn)), name (std::move (nameIn)), minValue (minIn), maxValue (maxIn),
          defaultValue (defIn), suffix (std::move (suffixIn)), skew (skewIn), value (defIn) {}

    juce::String id, name;
    float minValue, maxValue, defaultValue;
    juce::String suffix;
    float skew;
    std::atomic<float> value;
    juce::StringArray choices; // optional: shows a name per integer step (e.g. "1/8", "3/16")

    Param* withChoices (juce::StringArray c) { choices = std::move (c); return this; }

    float get() const noexcept               { return value.load (std::memory_order_relaxed); }
    void  set (float v) noexcept             { value.store (juce::jlimit (minValue, maxValue, v), std::memory_order_relaxed); }

    float getNormalised() const noexcept
    {
        auto p = (get() - minValue) / (maxValue - minValue);
        return skew == 1.0f ? p : std::pow (juce::jlimit (0.0f, 1.0f, p), skew);
    }

    void setNormalised (float n) noexcept
    {
        n = juce::jlimit (0.0f, 1.0f, n);
        if (skew != 1.0f) n = std::pow (n, 1.0f / skew);
        set (minValue + n * (maxValue - minValue));
    }

    juce::NormalisableRange<double> range() const
    {
        juce::NormalisableRange<double> r (minValue, maxValue);
        r.skew = 1.0 / skew;
        return r;
    }
};

using ParamList = std::vector<Param*>;

// Something that MIDI / OSC / Stream Deck can trigger (play, record, launch scene...)
struct Action
{
    juce::String id, name;
    std::function<void()> trigger;
};

// One-pole smoother for click-free parameter changes on the audio thread
struct Smoothed
{
    float current = 0.0f, coeff = 0.999f;
    void reset (double sampleRate, float timeMs = 20.0f, float start = 0.0f)
    {
        coeff = (float) std::exp (-1.0 / (sampleRate * timeMs * 0.001));
        current = start;
    }
    float next (float target) noexcept { current = target + coeff * (current - target); return current; }
};

} // namespace mcl
