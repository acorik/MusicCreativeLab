#include "engine/AudioEngine.h"
#include "project/Project.h"

// Offline self-test: renders the demos without an audio device and checks
// recording logic. Run with:  "Music Creative Lab.exe" --selftest
#include <cstdarg>

namespace mcl
{
namespace
{
juce::String selfTestLog;
void logf (const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start (args, fmt);
    std::vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);
    std::fputs (buf, stdout);
    selfTestLog << buf;
}

struct FakeDevice : juce::AudioIODevice {
  FakeDevice() : AudioIODevice("fake","fake") {}
  juce::StringArray getOutputChannelNames() override { return {"L","R"}; }
  juce::StringArray getInputChannelNames() override { return {"In"}; }
  juce::Array<double> getAvailableSampleRates() override { return {48000}; }
  juce::Array<int> getAvailableBufferSizes() override { return {256}; }
  int getDefaultBufferSize() override { return 256; }
  juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
  void close() override {}
  bool isOpen() override { return true; }
  void start (juce::AudioIODeviceCallback*) override {}
  void stop() override {}
  bool isPlaying() override { return true; }
  juce::String getLastError() override { return {}; }
  int getCurrentBufferSizeSamples() override { return 256; }
  double getCurrentSampleRate() override { return 48000; }
  int getCurrentBitDepth() override { return 24; }
  juce::BigInteger getActiveOutputChannels() const override { return 3; }
  juce::BigInteger getActiveInputChannels() const override { return 1; }
  int getOutputLatencyInSamples() override { return 0; }
  int getInputLatencyInSamples() override { return 0; }
};
}

int runSelfTest()
{
    int failures = 0;

    {
      mcl::AudioEngine e; FakeDevice dev; e.audioDeviceAboutToStart(&dev);
      mcl::Project::newEmptyProject(e);
      auto* d = e.getTrack(0); e.selectTrack(d->uid);
      juce::AudioBuffer<float> out(2,256), in(1,256); in.clear();
      auto runBeats=[&](double beats){ int blocks=(int)(beats*60.0/e.transport.bpm*48000/256); for(int i=0;i<blocks;++i) e.audioDeviceIOCallbackWithContext(in.getArrayOfReadPointers(),1,out.getArrayOfWritePointers(),2,256,{}); };
      for (int mode=0; mode<2; ++mode) {
        d->requestClear(); runBeats(0.1);
        d->recMode.set((float)mode); d->loopBars.set(0);
        e.transport.positionBeats = 0.0;
        e.recordOnSelected(); runBeats(0.05);
        logf("mode %d state after rec: %s\n", mode, mcl::clipStateName(d->getClipState()).toRawUTF8());
        runBeats(1.3); // start late at beat ~1.35
        for (int k=0;k<6;++k){ e.keyboardState.noteOn(1,36+(k%3),0.9f); runBeats(0.1); e.keyboardState.noteOff(1,36+(k%3),0); runBeats(0.4); }
        logf("  during: %s pos %.2f\n", mcl::clipStateName(d->getClipState()).toRawUTF8(), e.transport.getBeat());
        runBeats(10);
        auto c = d->getClipCopy();
        logf("  after silence: %s len %.1f notes %d origin %.2f first %.3f\n", mcl::clipStateName(d->getClipState()).toRawUTF8(), c.lengthBeats, (int)c.notes.size(), d->clipOrigin, c.notes.empty()?-1.0:c.notes[0].start);
        if (d->getClipState() != mcl::ClipState::Playing || std::abs (c.lengthBeats - 4.0) > 0.01 || c.notes.size() != 6) { logf ("  FAIL loop recording\n"); ++failures; }
      }
    }
    for (int demo = 0; demo < 3; ++demo) {
    mcl::AudioEngine e; FakeDevice dev; e.audioDeviceAboutToStart(&dev);
    mcl::Project::loadDemo(e,demo);
    e.play();
    juce::AudioBuffer<float> out(2,256), in(1,256); in.clear();
    juce::AudioBuffer<float> all(2, 48000*24);
    int pos=0; juce::Time t0=juce::Time::getCurrentTime();
    for (int b=0; pos+256<=all.getNumSamples(); ++b) {
      if (pos==48000*4) e.launchScene(1);
      if (pos==48000*8) e.startRiserToNextScene();
      e.audioDeviceIOCallbackWithContext(in.getArrayOfReadPointers(),1,out.getArrayOfWritePointers(),2,256,{});
      all.copyFrom(0,pos,out,0,0,256); all.copyFrom(1,pos,out,1,0,256); pos+=256;
    }
    auto ms=(juce::Time::getCurrentTime()-t0).inMilliseconds();
    logf("demo %d: rms %.3f peak %.3f, render %lld ms for 24s, scene %d\n",demo, all.getRMSLevel(0,0,pos), all.getMagnitude(0,pos), (long long)ms, e.currentScene.load());
    auto rms = all.getRMSLevel(0,0,pos);
    if (rms < 0.02f || all.getMagnitude(0,pos) > 1.0f) { logf ("  FAIL demo level\n"); ++failures; }
    juce::File f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mcl-demo"+juce::String(demo)+".wav"); f.deleteFile();
    juce::WavAudioFormat w; std::unique_ptr<juce::AudioFormatWriter> wr(w.createWriterFor(new juce::FileOutputStream(f),48000,2,16,{},0));
    wr->writeFromAudioSampleBuffer(all,0,pos);
    // save/load roundtrip
    auto tree = mcl::Project::save(e); auto err = mcl::Project::load(e, tree);
    logf("  roundtrip tracks %d scenes %d err '%s'\n", e.getNumTracks(), (int)e.scenes.size(), err.toRawUTF8());
    if (err.isNotEmpty()) ++failures;
    }
    
    logf ("SELFTEST %s (%d failures)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("selftest.log").replaceWithText (selfTestLog);
    return failures;
}
} // namespace mcl
