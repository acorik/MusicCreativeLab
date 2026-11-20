#include "ui/TutorialView.h"

namespace mcl
{

const std::vector<Tutorial>& getTutorials()
{
    static std::vector<Tutorial> t {
        { "1. Primeros pasos (5 min)", {
            "Bienvenido a Music Creative Lab.\n\nEsto es tu estudio de directo: cada fila de la pagina LIVE es una pista (un synth, una bateria o tu micro). "
            "Cada pista tiene un LOOP que se repite sincronizado al BPM.\n\nPulsa 'Siguiente' para configurar el audio.",
            "CONFIGURAR EL AUDIO\n\n1. Pulsa el boton 'Audio' arriba a la derecha.\n"
            "2. Tipo de dispositivo: elige ASIO si tu Monocaster tiene driver ASIO (o ASIO4ALL / FlexASIO). Si no, 'Windows Audio (Exclusive Mode)'.\n"
            "3. Salida y entrada: tu Monocaster G1 Neo.\n4. Buffer: 128 o 256 muestras. Mas bajo = menos retraso; si oyes cortes, subelo.\n\n"
            "Con tu i7 y 32 GB de RAM puedes ir a 128 sin problema.",
            "TOCAR UNA DEMO\n\n1. Pulsa 'Cargar la demo de este tutorial' (abajo).\n2. Pulsa ESPACIO o el boton Play.\n"
            "3. A la derecha de la pagina LIVE tienes las ESCENAS: pulsa 'Build' y despues 'DROP garage'. Cambian al siguiente compas, siempre a tiempo.\n"
            "4. Prueba el boton RISER > DROP: sube la tension 4 compases y explota en la siguiente escena.\n\nAhora ya has hecho tu primera transicion de calma a drop.",
        }, 0 },
        { "2. Tocar la demo epica con tus controladores", {
            "AKAI MPD218\n\nConectala por USB. Los pads tocan la bateria automaticamente (notas 36-51).\n"
            "Los knobs ya vienen mapeados:\n  K1 Energia (filtro del master: oscuro = calma, abierto = potente)\n  K2 Cutoff del synth seleccionado\n"
            "  K3 Resonancia   K4 Key Morph   K5 Detune\n  K6 Volumen de la pista seleccionada   K7 Crossfader   K8 Master",
            "CAMBIAR EL SONIDO EN TIEMPO REAL\n\nSelecciona la pista 'Keys' (clic en su fila). Mientras suena el loop, gira K2 y K4.\n\n"
            "KEY MORPH es la magia: cada nota que suena cambia un poco su color (filtro, forma de onda, estereo), asi los acordes 'respiran' y generan emocion.\n"
            "Subelo al maximo en el Pad y escucha como cada acorde es distinto.",
            "TECLADO DEL PC\n\nSi no tienes teclado MIDI, las teclas A W S E D F T G Y H U J tocan notas como un piano. Flechas izquierda/derecha cambian la octava.\n"
            "Lo que tocas va a la pista seleccionada.",
        }, 0 },
        { "3. Grabar tu primer loop (largo fijo)", {
            "1. Crea un proyecto nuevo: menu Archivo > Nuevo proyecto.\n2. Selecciona la pista 'Pad'.\n3. En su fila elige '4 comp' y 'Largo fijo'.\n"
            "4. Activa el metronomo (M) si quieres guia.",
            "5. Pulsa R (o REC). Si estabas al principio del compas empieza ya; si no, espera al siguiente compas (lo veras 'Armado').\n"
            "6. Toca unos acordes. Cuando pasen los 4 compases el loop se cierra solo y empieza a repetirse.\n"
            "7. Pulsa R otra vez para OVERDUB: lo que toques se suma al loop. R de nuevo para salir.\n8. Z deshace el ultimo overdub si te equivocas.",
            "TRUCO: la cuantizacion (1/16 por defecto) coloca tus notas en la rejilla, con el swing del master para que suene a garage. "
            "Ponla en 'Sin cuant.' si quieres que quede exactamente como tocaste.",
        }, -1 },
        { "4. Drums a tu ritmo: modo libre y silencios", {
            "Este es el modo para grabar baterias 'al tiempo que quieras'.\n\n1. Selecciona la pista de bateria.\n2. Cambia su modo a 'Libre (silencios)'.\n"
            "3. Pulsa R: la pista se pone en 'Esperando nota'.",
            "4. Toca los pads de la MPD218. La grabacion empieza EXACTAMENTE con tu primer golpe (ese es el inicio del loop).\n"
            "5. Toca el patron que quieras, con los huecos y silencios que quieras.\n"
            "6. Deja de tocar: tras 2 compases de silencio el programa detecta que has terminado, cierra el loop al compas mas cercano y lo pone en repeat.\n"
            "   (Tambien puedes pulsar R para cerrarlo tu.)",
            "Los silencios que dejaste se respetan, las notas se cuantizan a 1/16 con swing.\n"
            "Si el loop te sale de 3 compases se redondea a 4 para que cuadre con el resto. Usa x2 para duplicarlo y Z para deshacer.",
        }, 2 },
        { "5. Piano roll: editar y cambiar el synth en directo", {
            "Doble clic en una pista (o pestana PIANO ROLL) para ver sus notas.\n\n  Clic en vacio: crear nota\n  Arrastrar: mover\n  Borde derecho o Shift: cambiar largo\n"
            "  Alt + arrastrar: velocidad\n  Doble clic o clic derecho: borrar",
            "El loop sigue sonando mientras editas. Usa 'Cuantizar', '+1 oct', 'Duplicar x2' y 'Humanizar'.\n\n"
            "Mientras suena, ve al INSPECTOR (derecha) y mueve los knobs del synth: Cutoff, Attack, Key Morph, LFO... todo en tiempo real.",
        }, 0 },
        { "6. Tu voz con el Shure SM7B", {
            "1. Anade una pista 'Micro SM7B (voz)' desde el navegador o '+ Pista'.\n2. En el Inspector elige 'Input' = el canal donde esta tu SM7B en la Monocaster (normalmente 0).\n"
            "3. Pulsa 'Monitor' para oirte. USA AURICULARES para evitar acoples.",
            "CADENA DE VOZ\n\nLa pista trae: Noise Gate > EQ > Compresor > Chorus (apagado) > Delay (apagado) > Reverb.\n"
            "El SM7B necesita mucha ganancia: subela en la Monocaster, no en la app.\nActiva Delay y Chorus para voces etereas, o anade 'Robot (Ring Mod)'.",
            "LOOPS DE VOZ\n\nPulsa R en la pista del micro: graba los compases elegidos y los repite. R otra vez = overdub (capas de voces, armonias, beatbox...).\n"
            "Los efectos se aplican en directo sobre el loop: cambia la reverb y cambia el loop.",
        }, -1 },
        { "7. De calma a potente: escenas, energia, riser y crossfader", {
            "ESCENAS\n\nUna escena guarda que pistas suenan, sus volumenes y la energia. Monta un estado y pulsa 'Guardar' junto a una escena.\n"
            "Lanzalas con 1-8, con la MPD218 o el Stream Deck. Siempre entran en el siguiente compas.",
            "ENERGIA\n\nEl knob ENERGIA (arriba) es un filtro del master: bajo = sonido apagado y calido, alto = abierto.\nSubelo poco a poco durante un build.",
            "RISER > DROP\n\nPulsa B: durante N compases (Riser Bars) se sube un filtro y ruido blanco, y justo en el compas del drop se lanza la siguiente escena.",
            "CROSSFADER ESTILO DJ\n\nClic derecho en una pista > Crossfader lado A o B. El crossfader del master mezcla las pistas A (calma) con las B (drop).\n"
            "Mapea el crossfader de tu Numark Platinum FX y tendras un set que parece de DJ, pero tocado en vivo.",
        }, 0 },
        { "8. MIDI learn: MPD218, Numark y Stream Deck", {
            "MIDI LEARN\n\nClic derecho en cualquier knob o boton > 'MIDI Learn' y mueve un control de tu aparato. Listo.\n"
            "'MIDI Learn para la pista SELECCIONADA' hace que ese knob controle siempre la pista que tengas elegida.",
            "MODO LEARN\n\nEn la pestana CONTROLADORES activa 'MODO LEARN': ahora cada clic en un control lo prepara para el siguiente movimiento de tu controlador. "
            "Ideal para mapear la Numark entera en 2 minutos.\n\nAhi tambien puedes mapear ACCIONES (escenas, grabar, riser...) a pads.",
            "STREAM DECK\n\nAsigna atajos de teclado (Sistema > Tecla de acceso rapido): R grabar, 1-8 escenas, B riser, Espacio play...\n"
            "Para que funcione con la app en segundo plano usa un plugin OSC (puerto 9000). Toda la lista esta en CONTROLADORES.",
        }, -1 },
        { "9. Tus VST y los gratis recomendados", {
            "1. Navegador (izquierda) > 'Escanear mis VST'. Busca VST3 en C:\\Program Files\\Common Files\\VST3 (y carpetas que anadas con '+ Carpeta').\n"
            "2. Doble clic en un instrumento VST para crear una pista con el. Doble clic en un efecto VST para anadirlo a la pista seleccionada.\n"
            "3. 'Abrir GUI del VST' en el Inspector muestra su interfaz.",
            "VST GRATIS\n\nNo puedo incluir plugins de pago dentro del instalador (seria ilegal), pero el boton 'VST gratis recomendados' te lleva a las webs oficiales "
            "de plugins buenos y gratuitos: Surge XT, Vital, Spitfire LABS (pianos, cuerdas, guitarras), Valhalla Supermassive, Decent Sampler...\n"
            "Instalalos, pulsa 'Escanear' y apareceran aqui.",
        }, -1 },
        { "10. Grabar tu set para YouTube / TikTok", {
            "Pulsa F5 o el boton 'REC SET': todo lo que suena en el master se graba en WAV (24 bits) en\n  Musica\\Music Creative Lab\\Sets\n\n"
            "Pulsa otra vez para terminar. Puedes llevar ese WAV a tu editor de video.",
            "IDEA DE VIDEO\n\nGraba la pantalla + tus manos con la MPD218 y la Numark mientras pasas de 'Intro calma' a 'DROP garage' con el RISER. "
            "Contenido perfecto para Shorts y Reels.",
        }, 0 },
    };
    return t;
}

TutorialView::TutorialView()
{
    list.setRowHeight (30);
    addAndMakeVisible (list);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Colours::accent);
    stepLabel.setColour (juce::Label::textColourId, Colours::textDim);
    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setFont (juce::FontOptions (17.0f));
    body.setColour (juce::TextEditor::backgroundColourId, Colours::panel);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &stepLabel, &body, &prev, &next, &tryDemo })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }
    prev.onClick = [this] { if (step > 0) { --step; showStep(); } };
    next.onClick = [this] { if (step < getTutorials()[(size_t) current].steps.size() - 1) { ++step; showStep(); } else if (current + 1 < (int) getTutorials().size()) list.selectRow (current + 1); };
    tryDemo.onClick = [this] { auto d = getTutorials()[(size_t) current].demo; if (d >= 0 && onLoadDemo) onLoadDemo (d); };
    list.selectRow (0);
}

void TutorialView::showTutorial (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) getTutorials().size())) return;
    current = index;
    step = 0;
    showStep();
}

void TutorialView::showStep()
{
    auto& t = getTutorials()[(size_t) current];
    title.setText (t.title, juce::dontSendNotification);
    stepLabel.setText ("Paso " + juce::String (step + 1) + " de " + juce::String (t.steps.size()), juce::dontSendNotification);
    body.setText (t.steps[step], juce::dontSendNotification);
    tryDemo.setVisible (t.demo >= 0);
    prev.setEnabled (step > 0);
}

void TutorialView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected) { g.setColour (Colours::accent.withAlpha (0.25f)); g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f); }
    g.setColour (selected ? Colours::text : Colours::textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (getTutorials()[(size_t) row].title, 10, 0, w - 12, h, juce::Justification::centredLeft, true);
}

void TutorialView::paint (juce::Graphics& g) { g.fillAll (Colours::bg); }

void TutorialView::resized()
{
    auto r = getLocalBounds().reduced (10);
    list.setBounds (r.removeFromLeft (330));
    r.removeFromLeft (14);
    title.setBounds (r.removeFromTop (34));
    stepLabel.setBounds (r.removeFromTop (20));
    auto bottom = r.removeFromBottom (40);
    prev.setBounds (bottom.removeFromLeft (130).reduced (2));
    next.setBounds (bottom.removeFromLeft (130).reduced (2));
    tryDemo.setBounds (bottom.removeFromRight (280).reduced (2));
    body.setBounds (r.reduced (0, 6));
}

} // namespace mcl
