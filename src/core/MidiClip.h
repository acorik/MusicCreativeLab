#pragma once
#include <JuceHeader.h>
#include <vector>

namespace mcl
{

struct Note
{
    int pitch = 60;
    double start = 0.0;    // in beats, relative to clip start
    double length = 0.25;  // in beats
    float velocity = 0.8f;
};

struct QuantizeSettings
{
    double grid = 0.25;   // beats (0.25 = 1/16)
    float strength = 1.0f;
    float swing = 0.0f;   // 0..0.5 : delays every second grid step (UK garage loves ~0.15-0.25)
};

class MidiClip
{
public:
    std::vector<Note> notes;
    double lengthBeats = 16.0;

    void clear() { notes.clear(); }

    static double quantizeBeat (double beat, const QuantizeSettings& q)
    {
        if (q.grid <= 0.0) return beat;
        auto step = std::round (beat / q.grid);
        auto target = step * q.grid;
        if (((long long) step) % 2 != 0) target += q.swing * q.grid;
        return beat + (target - beat) * q.strength;
    }

    void quantize (const QuantizeSettings& q)
    {
        for (auto& n : notes)
        {
            n.start = quantizeBeat (n.start, q);
            if (n.start >= lengthBeats) n.start -= lengthBeats;
            if (n.start < 0.0) n.start += lengthBeats;
            n.length = juce::jmax (q.grid * 0.5, std::round (n.length / q.grid) * q.grid);
        }
        sort();
    }

    void sort()
    {
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    }

    // Double the loop (copy notes into the second half)
    void duplicate()
    {
        auto copy = notes;
        for (auto n : copy) { n.start += lengthBeats; notes.push_back (n); }
        lengthBeats *= 2.0;
        sort();
    }

    void transpose (int semitones)
    {
        for (auto& n : notes) n.pitch = juce::jlimit (0, 127, n.pitch + semitones);
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree v ("CLIP");
        v.setProperty ("length", lengthBeats, nullptr);
        for (auto& n : notes)
        {
            juce::ValueTree nv ("N");
            nv.setProperty ("p", n.pitch, nullptr);
            nv.setProperty ("s", n.start, nullptr);
            nv.setProperty ("l", n.length, nullptr);
            nv.setProperty ("v", n.velocity, nullptr);
            v.appendChild (nv, nullptr);
        }
        return v;
    }

    void fromValueTree (const juce::ValueTree& v)
    {
        notes.clear();
        lengthBeats = v.getProperty ("length", 16.0);
        for (auto nv : v)
            notes.push_back ({ (int) nv.getProperty ("p"), (double) nv.getProperty ("s"),
                               (double) nv.getProperty ("l"), (float) nv.getProperty ("v") });
        sort();
    }
};

} // namespace mcl
