#pragma once
#include "engine/AudioEngine.h"

namespace mcl
{

// OSC server so the Elgato Stream Deck (with any free OSC plugin), TouchOSC,
// a phone, etc. can drive the app over the network.
//
//   /mcl/action/<action-id>          e.g. /mcl/action/transport.play  /mcl/action/scene.2
//   /mcl/param/<param-id> <0..1>     e.g. /mcl/param/master.energy 0.7
//   /mcl/<action-id>                 shortcut for actions
class OscControl : private juce::OSCReceiver,
                   private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    explicit OscControl (AudioEngine& e) : engine (e) { addListener (this); }
    ~OscControl() override { disconnect(); }

    bool start (int portIn) { disconnect(); port = portIn; running = connect (port); return running; }
    bool isRunning() const { return running; }
    int getPort() const { return port; }
    juce::String getLastMessage() const { return last; }

private:
    void oscMessageReceived (const juce::OSCMessage& m) override
    {
        auto addr = m.getAddressPattern().toString();
        last = addr;
        if (! addr.startsWith ("/mcl/")) return;
        auto rest = addr.substring (5);

        float value = 1.0f;
        if (m.size() > 0)
        {
            if (m[0].isFloat32()) value = m[0].getFloat32();
            else if (m[0].isInt32()) value = (float) m[0].getInt32();
        }

        if (rest.startsWith ("param/"))
        {
            if (auto* p = engine.findParam (rest.substring (6))) p->setNormalised (value);
            return;
        }
        auto id = rest.startsWith ("action/") ? rest.substring (7) : rest;
        if (value > 0.0f)
            if (auto* a = engine.findAction (id)) if (a->trigger) a->trigger();
    }

    AudioEngine& engine;
    int port = 9000;
    bool running = false;
    juce::String last;
};

} // namespace mcl
