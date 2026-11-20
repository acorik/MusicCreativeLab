# Roadmap

Proyecto vivo: cada versión añade funciones, arregla bugs y pule la interfaz.

## v0.1: base (hecho)
- Motor nativo con WASAPI/ASIO, mixer, transporte, metrónomo y tap tempo
- Lab Synth (Key Morph), Lab Drums (16 pads y samples), 16 efectos
- Looper por pista: fijo, libre con detección de silencios, overdub, deshacer y x2
- Piano roll, escenas, riser → drop, energía, crossfader A/B, REC SET
- VST3: escáner y hosting
- MIDI learn, mapeo por defecto de la MPD218, OSC y atajos para el Stream Deck
- 3 demos, 10 tutoriales, instalador .exe con CI de GitHub

## v0.2: sonido y directo
- [ ] Varias "clips" por pista (A/B/C/D) que se lanzan como en un clip launcher
- [ ] Sidechain real (el kick ducka los pads) además del Pump
- [ ] Librería de samples de batería incluida (CC0) y pads con choke groups
- [ ] Arpegiador y modo "acordes con un dedo" para tocar progresiones emotivas
- [ ] Efectos de voz: pitch shift / harmonizer y vocoder
- [ ] Sends de reverb/delay globales (buses de efectos)

## v0.3: control total
- [ ] Plugin oficial para el Stream Deck (SDK de Elgato) con estados en vivo (botón rojo al grabar, nombre de escena)
- [ ] Plantilla de mapeo completa para la Numark Platinum FX (jog → scratch/stutter, pads → escenas)
- [ ] Bancos B/C/D de la MPD218 para lanzar escenas y clips
- [ ] Feedback de LEDs a los controladores
- [ ] Automatización grabada de knobs dentro del loop

## v0.4: interfaz y contenido
- [ ] Temas de color y modo "escenario" a pantalla completa
- [ ] Visualizador para grabar vídeos (forma de onda y espectro)
- [ ] Exportar stems por pista
- [ ] Más demos y presets (Fred again.. style, 2‑step, breaks, ambient)
- [ ] Acentos/ñ correctos en toda la interfaz

## Ideas abiertas
- Sincronización MIDI Clock / Ableton Link
- Plugins CLAP y LV2
- Versión del motor como plugin VST3 para usarlo dentro de FL Studio
