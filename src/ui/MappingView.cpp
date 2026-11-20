#include "ui/MappingView.h"

namespace mcl
{

juce::String getShortcutHelpText()
{
    return juce::String::fromUTF8 (
        "ATAJOS DE TECLADO (usalos tambien desde el Stream Deck con la accion 'Tecla de acceso rapido')\n"
        "  Espacio      Play / pausa\n"
        "  Esc          Stop (2 veces = volver al inicio)\n"
        "  R            Grabar / overdub en la pista seleccionada\n"
        "  Z            Deshacer ultimo overdub\n"
        "  X            Duplicar loop (x2)\n"
        "  V            Parar / reanudar loop\n"
        "  Supr         Borrar loop de la pista seleccionada\n"
        "  Flechas      Arriba/abajo: cambiar de pista  ·  izq/dcha: octava del teclado\n"
        "  1 ... 8      Lanzar escena 1-8\n"
        "  B            RISER -> DROP (siguiente escena)\n"
        "  M            Metronomo    N  Tap tempo    Q  Panico\n"
        "  F5           Empezar / parar la grabacion del set en WAV\n"
        "  A W S E D F T G Y H U J K O L P   tocan notas como un piano\n"
        "  F13 ... F20  Escenas 1-8 (teclas extra, ideales para el Stream Deck)\n"
        "  F21 F22 F23 F24   Grabar · Riser · Play · Deshacer\n"
        "\n"
        "STREAM DECK MK2\n"
        "  Opcion facil: anade botones 'Sistema > Tecla de acceso rapido' con las teclas de arriba.\n"
        "  (La app tiene que estar en primer plano.)\n"
        "  Opcion pro (funciona aunque la app este en segundo plano): instala un plugin OSC gratuito\n"
        "  del Stream Deck Store y envia mensajes a 127.0.0.1, puerto que ves abajo:\n"
        "     /mcl/action/transport.play     /mcl/action/sel.record     /mcl/action/scene.1\n"
        "     /mcl/action/master.riser       /mcl/param/master.energy 0.7\n"
        "  Clic derecho en cualquier boton o knob > 'Copiar direccion OSC' para obtener la suya.\n"
        "  Opcion MIDI: con un plugin MIDI del Stream Deck + loopMIDI, usa MIDI learn como con la MPD218.\n"
        "\n"
        "AKAI MPD218\n"
        "  Pads (banco A, notas 36-51) -> pista de bateria automaticamente.\n"
        "  Knobs (CC 3, 9, 12-17) -> Energia, Cutoff, Reso, Key Morph, Detune, Volumen pista, Crossfader, Master.\n"
        "  Cambia cualquier cosa con MIDI learn.\n"
        "\n"
        "NUMARK PLATINUM FX\n"
        "  Usa MODO LEARN: pulsa un control de la app y mueve el fader/knob/pad de la Numark.\n"
        "  Ideas: crossfader -> Master Crossfader (pistas lado A/B), faders de canal -> volumen de pistas,\n"
        "  EQ/filtro -> 'Energia' o el Filter del master, pads -> escenas y RISER, jog -> Stutter Hold.\n");
}

//==============================================================================
struct MappingView::MapModel : public juce::ListBoxModel
{
    explicit MapModel (AudioEngine& e) : engine (e) {}
    int getNumRows() override { cache = engine.mapper.getMappings(); return (int) cache.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) cache.size())) return;
        auto& m = cache[(size_t) row];
        if (selected) { g.setColour (Colours::panelHi); g.fillRect (0, 0, w, h); }
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (Colours::learn);
        g.drawText (engine.mapper.describeMapping (m), 8, 0, w / 2 - 10, h, juce::Justification::centredLeft, true);
        g.setColour (Colours::text);
        static const char* modes[] = { "abs", "toggle", "momentaneo", "disparo", "relativo" };
        g.drawText (engine.describeTarget (m.target) + "   [" + modes[(int) m.mode] + "]", w / 2, 0, w / 2 - 8, h, juce::Justification::centredLeft, true);
    }
    AudioEngine& engine;
    std::vector<MidiMapper::Mapping> cache;
};

struct MappingView::ActionModel : public juce::ListBoxModel
{
    explicit ActionModel (AudioEngine& e) : engine (e) {}
    int getNumRows() override { return (int) engine.getActions().size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        auto& a = engine.getActions()[(size_t) row];
        auto learning = engine.mapper.getLearnTarget() == a.id;
        if (selected || learning) { g.setColour (learning ? Colours::learn.withAlpha (0.3f) : Colours::panelHi); g.fillRect (0, 0, w, h); }
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (Colours::text);
        g.drawText (a.name, 8, 0, w - 160, h, juce::Justification::centredLeft, true);
        auto map = engine.mapper.mappingLabelFor (a.id);
        g.setColour (map.isEmpty() ? Colours::textDim : Colours::learn);
        g.drawText (map.isEmpty() ? "clic = learn" : map, w - 150, 0, 145, h, juce::Justification::centredRight, true);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        auto& a = engine.getActions()[(size_t) row];
        if (e.mods.isPopupMenu()) { ParamKnob::showLearnMenu (engine, a.id, {}, nullptr); return; }
        engine.mapper.startLearn (a.id);
    }
    AudioEngine& engine;
};

//==============================================================================
MappingView::MappingView (AudioEngine& e, OscControl& o) : engine (e), osc (o)
{
    mapModel = std::make_unique<MapModel> (e);
    actionModel = std::make_unique<ActionModel> (e);
    mapList.setModel (mapModel.get());
    actionList.setModel (actionModel.get());
    for (auto* l : { &mapList, &actionList }) { l->setRowHeight (24); addAndMakeVisible (l); }

    learnMode.setClickingTogglesState (true);
    learnMode.setColour (juce::TextButton::buttonOnColourId, Colours::learn);
    learnMode.onClick = [this] { engine.mapper.learnModeUI = learnMode.getToggleState(); if (! learnMode.getToggleState()) engine.mapper.cancelLearn(); };
    cancelLearn.onClick = [this] { engine.mapper.cancelLearn(); };
    defaults.onClick = [this] { engine.mapper.loadDefaultMappings(); };
    removeBtn.onClick = [this] { engine.mapper.removeMapping (mapList.getSelectedRow()); };
    padsToDrums.setClickingTogglesState (true);
    padsToDrums.setToggleState (engine.padsToDrums.load(), juce::dontSendNotification);
    padsToDrums.onClick = [this] { engine.padsToDrums = padsToDrums.getToggleState(); };

    for (auto* c : std::initializer_list<juce::Component*> { &learnMode, &cancelLearn, &defaults, &removeBtn, &padsToDrums, &learnStatus, &monitor, &devices, &oscLabel, &help })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
    learnStatus.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    learnStatus.setColour (juce::Label::textColourId, Colours::learn);
    monitor.setColour (juce::Label::textColourId, Colours::textDim);
    devices.setColour (juce::Label::textColourId, Colours::textDim);
    devices.setJustificationType (juce::Justification::topLeft);
    oscLabel.setColour (juce::Label::textColourId, Colours::textDim);
    help.setMultiLine (true);
    help.setReadOnly (true);
    help.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
    help.setText (getShortcutHelpText());
    engine.mapper.addChangeListener (this);
    startTimerHz (10);
}

MappingView::~MappingView() { engine.mapper.removeChangeListener (this); }

void MappingView::changeListenerCallback (juce::ChangeBroadcaster*) { mapList.updateContent(); mapList.repaint(); actionList.repaint(); }

void MappingView::timerCallback()
{
    auto target = engine.mapper.getLearnTarget();
    juce::String st;
    if (target.isNotEmpty()) st = "Esperando MIDI para: " + engine.describeTarget (target) + "  ->  mueve un knob o pulsa un pad";
    else if (engine.mapper.learnModeUI.load()) st = "Modo learn activo: haz clic en cualquier knob, boton o accion";
    else st = engine.mapper.getLastLearnMessage().isNotEmpty() ? "Ultimo mapeo: " + engine.mapper.getLastLearnMessage() : "Clic derecho en cualquier control = MIDI learn";
    learnStatus.setText (st, juce::dontSendNotification);
    learnMode.setToggleState (engine.mapper.learnModeUI.load(), juce::dontSendNotification);

    monitor.setText ("Monitor MIDI: " + engine.mapper.getLastMessageText(), juce::dontSendNotification);
    static int tick = 0;
    if (tick++ % 10 != 0) { actionList.repaint(); return; }
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices()) names.add (d.name);
    devices.setText ("Dispositivos MIDI conectados:\n  " + (names.isEmpty() ? juce::String ("(ninguno)") : names.joinIntoString ("\n  ")), juce::dontSendNotification);
    oscLabel.setText (osc.isRunning() ? "OSC escuchando en 127.0.0.1 puerto " + juce::String (osc.getPort()) + "   ultimo: " + osc.getLastMessage()
                                      : "OSC no disponible (puerto ocupado)", juce::dontSendNotification);
    actionList.repaint();
}

void MappingView::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("MAPEOS ACTIVOS", mapList.getBounds().translated (0, -20).withHeight (18), juce::Justification::left);
    g.drawText ("ACCIONES (clic para mapear a un pad / boton)", actionList.getBounds().translated (0, -20).withHeight (18), juce::Justification::left);
}

void MappingView::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (34);
    learnMode.setBounds (top.removeFromLeft (480).reduced (2));
    cancelLearn.setBounds (top.removeFromLeft (140).reduced (2));
    defaults.setBounds (top.removeFromLeft (220).reduced (2));
    padsToDrums.setBounds (top.removeFromLeft (260).reduced (2));
    learnStatus.setBounds (r.removeFromTop (26));
    monitor.setBounds (r.removeFromTop (20));
    oscLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (24);

    auto right = r.removeFromRight (r.getWidth() * 45 / 100);
    help.setBounds (right.removeFromBottom (right.getHeight() - 110).reduced (4, 0));
    devices.setBounds (right.reduced (4, 0));

    auto left = r;
    auto mapArea = left.removeFromTop (left.getHeight() / 2 - 12);
    removeBtn.setBounds (mapArea.removeFromBottom (30).removeFromLeft (220).reduced (0, 2));
    mapList.setBounds (mapArea);
    left.removeFromTop (24);
    actionList.setBounds (left);
}

} // namespace mcl
