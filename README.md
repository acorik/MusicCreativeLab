# Music Creative Lab

Estación de música en directo para Windows: loops sincronizados al BPM, sintes que cambian con cada nota, baterías de 16 pads, efectos para la voz, escenas para pasar de calma a drop, MIDI learn y control desde el Stream Deck.

Pensado para tocar en vivo estilo Fred again.. / UK garage con:

| Aparato | Qué hace en la app |
|---|---|
| **Akai MPD218** | Los pads tocan la batería y los knobs ya vienen mapeados (energía, cutoff, key morph, volumen…) |
| **Numark Platinum FX** | MIDI learn de cualquier control. El crossfader mezcla pistas A/B como en un set de DJ |
| **Stream Deck MK2** | Atajos de teclado (R, 1‑8, B, Espacio, F13‑F24…) o mensajes OSC al puerto 9000 |
| **Monocaster G1 Neo + Shure SM7B** | Pista de micro con cadena de voz (gate, EQ, compresor, chorus, delay, reverb) y looper de voz |

## Descargar

Ve a **Actions → Windows build + installer** (o a **Releases** cuando haya una versión etiquetada) y descarga `MusicCreativeLab-Setup-x.y.z.exe`. Es un instalador normal: siguiente, siguiente, listo. No necesita nada más.

## Qué trae la v0.1

- **Motor de audio nativo** (C++/JUCE) con WASAPI exclusivo y ASIO (si la compilación encuentra el SDK), con bloques de 128‑256 muestras.
- **Lab Synth**: synth polifónico virtual‑analógico con unison, filtro, LFO, glide y **Key Morph** (cada nota cambia un poco de color). 10 presets: Fred Pad, Glass Keys, Garage Bass, Emo Lead, Pluck Chops, Sub Boom, Supersaw Drop, Choir Air, Warm Organ Stab y Dreamy Arp.
- **Lab Drums**: 16 pads sintetizados con la distribución de la MPD218 y 3 kits (UK Garage 2‑Step, Soft Fabric y Heavy Drop). Se puede cargar un sample propio en cualquier pad.
- **16 efectos**: Filter, EQ, Compressor, Gate, Limiter, Reverb (con freeze), Delay sincronizado ping‑pong, Chorus, Phaser, Auto‑pan, Widener, **Pump** (sidechain), **Stutter**, Saturator, Bitcrusher y Robot.
- **VST3**: escáner de los plugins que tengas instalados, que se pueden usar como instrumentos o efectos, con ventana para su interfaz y sus parámetros mapeables por MIDI.
- **Looper por pista**: grabación de largo fijo (1‑16 compases) o **modo libre**, en el que la grabación empieza con el primer golpe y el loop se cierra solo tras 2 compases de silencio. Además: overdub, deshacer, duplicar y cuantización con swing.
- **Piano roll**: editar mientras suena, cuantizar, transponer y humanizar.
- **Mixer estilo FL**: canales con fader, pan, medidor, mute, solo y una cadena de efectos por canal, más el master.
- **Escenas** que cambian en el siguiente compás, botón **RISER → DROP**, knob de **Energía** (filtro del master) y **crossfader A/B**.
- **REC SET**: graba todo lo que suena en un WAV para tus vídeos.
- **3 demos** listas para tocar y **10 tutoriales** dentro de la app.

## Sobre los VST de pago

Los plugins comerciales (Serum, Omnisphere, Kontakt…) **no se pueden incluir** en el instalador, porque redistribuirlos es ilegal. La app trae sus propios sintes y efectos. Si tienes VST3 instalados, el escáner los encuentra, y el botón *VST gratis recomendados* te lleva a las webs oficiales de plugins gratuitos buenos (Surge XT, Vital, Spitfire LABS, Valhalla Supermassive…).

## Compilar tú mismo

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64   # opcional: -DMCL_ASIO_SDK_DIR=C:/asiosdk
cmake --build build --config Release
```

JUCE se descarga solo. También compila en Linux para desarrollo, y `--selftest` renderiza las demos sin tarjeta de sonido.

Más información en [docs/GUIA.md](docs/GUIA.md) y [docs/ROADMAP.md](docs/ROADMAP.md).

Licencia: GPLv3. Usa JUCE 8 (AGPLv3).
