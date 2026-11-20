#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace mcl
{

constexpr int beatsPerBar = 4;

// Musical position of the current audio block, passed to every instrument / FX
struct BeatInfo
{
    double sampleRate = 48000.0;
    double bpm = 120.0;
    double blockStartBeat = 0.0;  // transport position at sample 0 of the block
    double beatsPerSample = 0.0;
    bool playing = false;

    double beatAt (int sample) const noexcept { return blockStartBeat + sample * beatsPerSample; }
    double samplesPerBeat() const noexcept    { return sampleRate * 60.0 / bpm; }
};

class Transport
{
public:
    std::atomic<double> bpm { 132.0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> metronome { false };
    std::atomic<double> positionBeats { 0.0 }; // read by UI

    double getBeat() const noexcept { return positionBeats.load(); }

    static double nextBar (double beat) noexcept
    {
        auto bar = std::floor (beat / beatsPerBar + 1e-9);
        return (bar + 1.0) * beatsPerBar;
    }

    // Tap tempo
    void tap()
    {
        auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastTap > 2000.0) tapCount = 0;
        if (tapCount > 0)
        {
            auto interval = now - lastTap;
            tapAverage = tapCount == 1 ? interval : tapAverage * 0.7 + interval * 0.3;
            bpm.store (juce::jlimit (60.0, 200.0, 60000.0 / tapAverage));
        }
        lastTap = now;
        ++tapCount;
    }

private:
    double lastTap = 0.0, tapAverage = 500.0;
    int tapCount = 0;
};

} // namespace mcl
