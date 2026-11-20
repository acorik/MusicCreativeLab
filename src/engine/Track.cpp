#include "engine/Track.h"

namespace mcl
{

juce::String clipStateName (ClipState s)
{
    switch (s)
    {
        case ClipState::Empty:          return "Vacio";
        case ClipState::Armed:          return "Armado";
        case ClipState::WaitingForNote: return "Esperando nota";
        case ClipState::Recording:      return "Grabando";
        case ClipState::Playing:        return "Loop";
        case ClipState::Overdub:        return "Overdub";
        case ClipState::Stopped:        return "Parado";
    }
    return {};
}

static juce::String pid (int uid, const char* n) { return "t" + juce::String (uid) + "." + n; }

Track::Track (int u, TrackKind k, juce::String n)
    : uid (u), kind (k), name (std::move (n)),
      volume       (pid (u, "volume"),  "Volume",   0.0f, 1.25f, 0.8f),
      pan          (pid (u, "pan"),     "Pan",     -1.0f, 1.0f, 0.0f),
      mute         (pid (u, "mute"),    "Mute",     0.0f, 1.0f, 0.0f),
      solo         (pid (u, "solo"),    "Solo",     0.0f, 1.0f, 0.0f),
      monitor      (pid (u, "monitor"), "Monitor",  0.0f, 1.0f, k == TrackKind::Audio ? 0.0f : 1.0f),
      loopBars     (pid (u, "loopbars"),"Loop Bars",0.0f, 4.0f, 2.0f),
      quantize     (pid (u, "quantize"),"Quantize", 0.0f, 4.0f, 3.0f),
      recMode      (pid (u, "recmode"), "Rec Mode", 0.0f, 1.0f, 0.0f),
      inputChannel (pid (u, "input"),   "Input",    0.0f, 7.0f, 0.0f),
      xfade        (pid (u, "xfade"),   "XFade A/B",0.0f, 2.0f, 0.0f)
{
    static const juce::uint32 palette[] = { 0xffff7a45, 0xff5ad1ff, 0xffb57bff, 0xff6fe38a, 0xffffd34d,
                                            0xffff5fa2, 0xff4de0c4, 0xffa0a8ff, 0xffff9f5a, 0xff9ee35f };
    xfade.withChoices ({ "Ninguno", "Lado A", "Lado B" });
    recMode.withChoices ({ "Fijo", "Libre" });
    monitor.withChoices ({ "Off", "On" });
    loopBars.withChoices ({ "1 comp", "2 comp", "4 comp", "8 comp", "16 comp" });
    quantize.withChoices ({ "Off", "1/4", "1/8", "1/16", "1/32" });
    colour = juce::Colour (palette[(size_t) u % std::size (palette)]);
    clip.notes.reserve (4096);
    undoSnapshot.reserve (4096);
    pendingOffs.reserve (512);
}

Track::~Track() = default;

std::vector<Param*> Track::getMixerParams()
{
    return { &volume, &pan, &mute, &solo, &monitor, &loopBars, &quantize, &recMode, &inputChannel, &xfade };
}

void Track::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    output.setSize (2, maxBlock);
    monoIn.setSize (1, maxBlock);
    midiScratch.ensureSize (4096);
    volSmooth.reset (sr, 15.0f, volume.get());
    panSmooth.reset (sr, 15.0f, pan.get());
    if (instrument) instrument->prepare (sr, maxBlock);
    for (auto& fx : effects) fx->prepare (sr, maxBlock);
    if (kind == TrackKind::Audio && loopAudio.getNumSamples() < (int) (sr * 64.0))
    {
        loopAudio.setSize (1, (int) (sr * 64.0)); // 16 bars at 60 BPM
        loopAudio.clear();
        loopLenSamples = 0;
    }
}

void Track::panic()
{
    pendingOffs.clear();
    if (instrument) instrument->allNotesOff();
    for (auto& h : held) h.on = false;
    heldCount = 0;
}

double Track::clipLocal (double absBeat) const
{
    auto len = juce::jmax (0.25, clip.lengthBeats);
    auto p = std::fmod (absBeat - clipOrigin, len);
    return p < 0.0 ? p + len : p;
}

MidiClip Track::getClipCopy()
{
    const juce::SpinLock::ScopedLockType sl (clipLock);
    return clip;
}

void Track::setClipFromUI (const MidiClip& c, bool startPlaying)
{
    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        clip.notes = c.notes;
        clip.lengthBeats = c.lengthBeats;
    }
    clipLenUI = c.lengthBeats;
    hasContentFlag = ! c.notes.empty();
    if (startPlaying && ! c.notes.empty())
    {
        auto st = state();
        if (st == ClipState::Empty || st == ClipState::Stopped) { clipOrigin = 0.0; setState (ClipState::Playing); }
    }
}

//==============================================================================
void Track::handleCommands (const BeatInfo& beat)
{
    const double now = beat.blockStartBeat;

    if (cmdClear.exchange (false))
    {
        { const juce::SpinLock::ScopedLockType sl (clipLock); clip.notes.clear(); }
        loopLenSamples = 0;
        hasContentFlag = false;
        panic();
        setState (ClipState::Empty);
    }

    if (cmdUndo.exchange (false))
    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        clip.notes.assign (undoSnapshot.begin(), undoSnapshot.end());
        hasContentFlag = ! clip.notes.empty();
    }

    if (cmdDouble.exchange (false) && kind == TrackKind::Instrument)
    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        if (clip.lengthBeats <= 32.0)
        {
            auto count = clip.notes.size();
            for (size_t i = 0; i < count; ++i) { auto n = clip.notes[i]; n.start += clip.lengthBeats; clip.notes.push_back (n); }
            clip.lengthBeats *= 2.0;
        }
    }

    if (cmdToggle.exchange (false))
    {
        auto st = state();
        if (st == ClipState::Playing || st == ClipState::Overdub) { setState (ClipState::Stopped); panic(); }
        else if (st == ClipState::Stopped && hasContentFlag) setState (ClipState::Playing);
    }

    if (cmdRecord.exchange (false))
    {
        auto st = state();
        const bool content = hasContentFlag.load();

        auto snapshot = [this]
        {
            const juce::SpinLock::ScopedLockType sl (clipLock);
            undoSnapshot.assign (clip.notes.begin(), clip.notes.end());
        };

        switch (st)
        {
            case ClipState::Empty:
            case ClipState::Stopped:
                if (content && st == ClipState::Stopped) { snapshot(); setState (ClipState::Overdub); break; }
                snapshot();
                freeMode = kind == TrackKind::Instrument && recMode.get() > 0.5f;
                if (freeMode)
                {
                    const juce::SpinLock::ScopedLockType sl (clipLock);
                    clip.notes.clear();
                    clip.lengthBeats = 4.0 * 64.0;
                    setState (ClipState::WaitingForNote);
                }
                else
                {
                    auto len = loopBarsFromIndex ((int) std::round (loopBars.get())) * beatsPerBar;
                    auto inBar = std::fmod (now, (double) beatsPerBar);
                    recordStart = (! beat.playing || inBar < 0.5) ? now - inBar : Transport::nextBar (now);
                    if (kind == TrackKind::Audio)
                    {
                        loopLenBeats = len;
                        loopLenSamples = juce::jmin (loopAudio.getNumSamples(), (int) (len * beat.samplesPerBeat()));
                        loopAudio.clear();
                    }
                    const juce::SpinLock::ScopedLockType sl (clipLock);
                    clip.notes.clear();
                    clip.lengthBeats = len;
                    clipOrigin = recordStart;
                    clipLenUI = len;
                    setState (recordStart <= now ? ClipState::Recording : ClipState::Armed);
                }
                break;

            case ClipState::Playing:
                snapshot();
                setState (ClipState::Overdub);
                break;

            case ClipState::Overdub:
                setState (ClipState::Playing);
                break;

            case ClipState::Recording:
                if (freeMode) finishFreeRecording (now);
                else setState (ClipState::Playing);
                break;

            case ClipState::Armed:
            case ClipState::WaitingForNote:
                setState (content ? ClipState::Playing : ClipState::Empty);
                break;
        }
    }
}

void Track::finishFreeRecording (double endBeat)
{
    // close held notes
    for (int p = 0; p < 128; ++p)
        if (held[p].on)
        {
            const juce::SpinLock::ScopedLockType sl (clipLock);
            clip.notes.push_back ({ p, held[p].absStart - clipOrigin, juce::jmax (0.1, endBeat - held[p].absStart), held[p].vel });
            held[p].on = false;
        }
    heldCount = 0;

    auto raw = endBeat - clipOrigin;
    auto overshoot = std::fmod (raw, (double) beatsPerBar);
    if (overshoot < 0.75 && raw > beatsPerBar) raw -= overshoot;   // pressed just after the bar line
    auto bars = juce::jmax (1, (int) std::ceil (raw / beatsPerBar - 0.02));
    if (bars == 3) bars = 4;
    else if (bars > 4 && bars < 8) bars = 8;
    else if (bars > 8) bars = 16;

    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        clip.lengthBeats = bars * beatsPerBar;
        clip.notes.erase (std::remove_if (clip.notes.begin(), clip.notes.end(),
                                          [&] (const Note& n) { return n.start >= clip.lengthBeats || n.start < -0.5; }),
                          clip.notes.end());
        for (auto& n : clip.notes) if (n.start < 0.0) n.start = 0.0;
        clipLenUI = clip.lengthBeats;
        hasContentFlag = ! clip.notes.empty();
    }
    freeMode = false;
    setState (hasContentFlag ? ClipState::Playing : ClipState::Empty);
}

void Track::recordMidi (const juce::MidiBuffer& live, const BeatInfo& beat, int numSamples)
{
    auto qIdx = (int) std::round (quantize.get());
    quant.grid = gridFromIndex (qIdx);
    quant.strength = 1.0f;
    quant.swing = swing.load();

    for (const auto meta : live)
    {
        auto m = meta.getMessage();
        if (! (m.isNoteOn() || m.isNoteOff())) continue;
        auto b = beat.beatAt (meta.samplePosition);
        auto st = state();

        if (st == ClipState::Armed && b >= recordStart) { setState (ClipState::Recording); st = ClipState::Recording; }

        if (st == ClipState::WaitingForNote && m.isNoteOn() && beat.playing)
        {
            clipOrigin = std::round (b); // the first hit defines the downbeat
            if (clipOrigin > b + 0.5) clipOrigin -= 1.0;
            setState (ClipState::Recording);
            st = ClipState::Recording;
        }

        if (st != ClipState::Recording && st != ClipState::Overdub) continue;

        auto p = m.getNoteNumber();
        if (m.isNoteOn())
        {
            if (! held[p].on) ++heldCount;
            held[p] = { true, b, m.getFloatVelocity() };
            lastEventBeat = b;
        }
        else if (held[p].on)
        {
            held[p].on = false;
            heldCount = juce::jmax (0, heldCount - 1);
            auto len = juce::jmax (0.05, b - held[p].absStart);
            double local = freeMode ? held[p].absStart - clipOrigin : clipLocal (held[p].absStart);
            if (quant.grid > 0.0)
            {
                local = MidiClip::quantizeBeat (local, quant);
                len = juce::jmax (quant.grid * 0.5, len);
            }
            const juce::SpinLock::ScopedLockType sl (clipLock);
            if (! freeMode)
            {
                if (local >= clip.lengthBeats) local -= clip.lengthBeats;
                if (local < 0.0) local += clip.lengthBeats;
            }
            if (clip.notes.size() < clip.notes.capacity())
                clip.notes.push_back ({ p, juce::jmax (freeMode ? -0.5 : 0.0, local), len, held[p].vel });
            hasContentFlag = true;
            lastEventBeat = b;
            lastNoteEndBeat = b;
        }
    }

    // state transitions at the end of the block
    auto endBeat = beat.beatAt (numSamples);
    auto st = state();
    if (st == ClipState::Armed && endBeat >= recordStart) setState (ClipState::Recording);

    if (st == ClipState::Recording && ! freeMode && endBeat >= clipOrigin + clip.lengthBeats)
    {
        // loop completed: close held notes at the loop end and start playing
        for (int p = 0; p < 128; ++p)
            if (held[p].on)
            {
                const juce::SpinLock::ScopedLockType sl (clipLock);
                if (clip.notes.size() < clip.notes.capacity())
                    clip.notes.push_back ({ p, clipLocal (held[p].absStart), juce::jmax (0.1, endBeat - held[p].absStart), held[p].vel });
                held[p].on = false;
            }
        heldCount = 0;
        setState (ClipState::Playing);
    }

    // SILENCE DETECTION: in free mode, two bars without playing closes the loop
    if (st == ClipState::Recording && freeMode && heldCount == 0 && hasContentFlag
        && endBeat - lastEventBeat >= 2.0 * beatsPerBar)
        finishFreeRecording (lastNoteEndBeat);
}

void Track::scheduleClip (juce::MidiBuffer& out, const BeatInfo& beat, int numSamples)
{
    auto st = state();
    const bool active = beat.playing && (st == ClipState::Playing || st == ClipState::Overdub
                                         || (st == ClipState::Recording && ! freeMode));
    const double b0 = beat.blockStartBeat;
    const double d = numSamples * beat.beatsPerSample;

    if (active)
    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        auto len = juce::jmax (0.25, clip.lengthBeats);
        auto p0 = clipLocal (b0);
        for (auto& n : clip.notes)
        {
            auto off = n.start - p0;
            if (off < 0.0) off += len;
            if (off < d)
            {
                auto s = juce::jlimit (0, numSamples - 1, (int) (off / beat.beatsPerSample));
                out.addEvent (juce::MidiMessage::noteOn (1, n.pitch, juce::jlimit (0.01f, 1.0f, n.velocity)), s);
                if (pendingOffs.size() < pendingOffs.capacity())
                    pendingOffs.push_back ({ n.pitch, b0 + off + juce::jmax (0.02, n.length) });
            }
        }
        clipPos = p0;
    }

    for (size_t i = 0; i < pendingOffs.size();)
    {
        auto& po = pendingOffs[i];
        if (! active || po.endBeat < b0 + d)
        {
            auto s = active ? juce::jlimit (0, numSamples - 1, (int) ((po.endBeat - b0) / beat.beatsPerSample)) : 0;
            out.addEvent (juce::MidiMessage::noteOff (1, po.pitch), s);
            po = pendingOffs.back();
            pendingOffs.pop_back();
        }
        else ++i;
    }
}

void Track::processAudioLoop (int numSamples, const BeatInfo& beat, const float* mono)
{
    auto* L = output.getWritePointer (0);
    auto* R = output.getWritePointer (1);
    const bool mon = monitor.get() > 0.5f;
    const auto spb = beat.samplesPerBeat();

    for (int i = 0; i < numSamples; ++i)
    {
        auto in = mono != nullptr ? mono[i] : 0.0f;
        auto b = beat.beatAt (i);
        auto st = state();

        if (st == ClipState::Armed && b >= recordStart) { setState (ClipState::Recording); st = ClipState::Recording; }

        float play = 0.0f;
        if (loopLenSamples > 0 && beat.playing && st != ClipState::Armed && st != ClipState::Empty && st != ClipState::Stopped)
        {
            auto rel = b - clipOrigin;
            if (rel >= 0.0 || st != ClipState::Recording)
            {
                auto idx = (int) std::fmod (std::fmod (rel * spb, (double) loopLenSamples) + loopLenSamples, (double) loopLenSamples);
                auto* data = loopAudio.getWritePointer (0);
                if (st == ClipState::Recording)
                {
                    data[idx] = in;
                    hasContentFlag = true;
                    if (rel >= loopLenBeats) setState (ClipState::Playing);
                }
                else
                {
                    play = data[idx];
                    if (st == ClipState::Overdub) data[idx] = data[idx] * 0.98f + in;
                }
                clipPos = std::fmod (rel, loopLenBeats);
            }
        }

        auto s = play + (mon ? in : 0.0f);
        L[i] += s;
        R[i] += s;
    }
    clipLenUI = loopLenBeats > 0 ? loopLenBeats : loopBarsFromIndex ((int) std::round (loopBars.get())) * beatsPerBar;
}

void Track::process (int n, const BeatInfo& beat, const juce::MidiBuffer& liveMidi,
                     const float* const* input, int numInputs)
{
    output.clear (0, n);
    handleCommands (beat);

    juce::AudioBuffer<float> view (output.getArrayOfWritePointers(), 2, n);

    if (kind == TrackKind::Instrument)
    {
        midiScratch.clear();
        recordMidi (liveMidi, beat, n);
        midiScratch.addEvents (liveMidi, 0, n, 0);
        scheduleClip (midiScratch, beat, n);

        for (const auto meta : midiScratch)
            if (meta.getMessage().isNoteOn()) { activity = 1.0f; break; }

        if (instrument) instrument->render (view, midiScratch, beat);
    }
    else
    {
        const float* mono = nullptr;
        auto ch = (int) std::round (inputChannel.get());
        if (input != nullptr && ch < numInputs && input[ch] != nullptr)
        {
            monoIn.copyFrom (0, 0, input[ch], n);
            mono = monoIn.getReadPointer (0);
        }
        processAudioLoop (n, beat, mono);
    }

    for (auto& fx : effects)
        if (fx->isOn()) fx->process (view, beat);

    // volume + constant-power pan
    float pkL = 0.0f, pkR = 0.0f;
    auto* L = output.getWritePointer (0);
    auto* R = output.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        auto g = volSmooth.next (volume.get() * sceneGain.load());
        auto p = panSmooth.next (pan.get());
        auto gl = g * std::sqrt (0.5f * (1.0f - p)) * 1.41421356f;
        auto gr = g * std::sqrt (0.5f * (1.0f + p)) * 1.41421356f;
        L[i] *= gl;
        R[i] *= gr;
        pkL = juce::jmax (pkL, std::abs (L[i]));
        pkR = juce::jmax (pkR, std::abs (R[i]));
    }
    peakL = juce::jmax (peakL.load(), pkL);
    peakR = juce::jmax (peakR.load(), pkR);
}

} // namespace mcl
