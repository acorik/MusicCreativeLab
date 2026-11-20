# Guía rápida de Music Creative Lab

## 1. Primer arranque
1. Abre **Audio** (arriba a la derecha). Elige **ASIO** si tu Monocaster lo tiene (o ASIO4ALL / FlexASIO). Si no, elige **Windows Audio (Exclusive Mode)**. Buffer: **128 o 256**.
2. Conecta la MPD218 y la Numark antes de abrir la app (si las conectas después, se detectan en unos segundos).
3. La primera vez se abre la demo **Calma → UK Garage** y la pestaña **TUTORIALES**.

## 2. La pantalla
- **Arriba**: Play, Stop, REC (pista seleccionada), Metrónomo, Tap, BPM, posición, knobs **Energía / Swing / Riser Bars / Crossfader**, **REC SET**, Pánico y **MIDI LEARN**.
- **Izquierda (Navegador)**: demos, pistas preset, efectos y tus VST. Doble clic para añadir.
- **Centro**: **LIVE** (loops y escenas), **MIXER**, **PIANO ROLL**, **CONTROLADORES** y **TUTORIALES**.
- **Derecha (Inspector)**: el instrumento de la pista seleccionada (presets, knobs, pads) y sus efectos.
- **Abajo**: teclado. Las teclas `A W S E D F T G Y H U J K O L P` del PC tocan notas.

## 3. Grabar loops
| Modo | Cómo funciona |
|---|---|
| **Largo fijo** | Pulsa **R**: empieza en este compás (o en el siguiente) y graba 1/2/4/8/16 compases. Luego se repite solo. |
| **Libre (silencios)** | Pulsa **R**: espera tu primer golpe, grabas lo que quieras y tras **2 compases sin tocar** se cierra al compás más cercano. |
| **Overdub** | **R** sobre un loop que ya suena: lo que toques se suma. **Z** deshace. |

## 4. Atajos (también para el Stream Deck)
`Espacio` play · `Esc` stop · `R` grabar · `Z` deshacer · `X` duplicar · `V` parar/reanudar loop · `Supr` borrar loop · `↑/↓` pista · `←/→` octava · `1–8` escenas · `B` riser · `M` metrónomo · `N` tap · `Q` pánico · `F5` grabar set · `F13–F20` escenas · `F21` grabar · `F22` riser · `F23` play · `F24` deshacer

## 5. Stream Deck MK2
- **Fácil**: botones *Sistema → Tecla de acceso rápido* con los atajos de arriba (la app tiene que estar en primer plano).
- **Pro**: un plugin OSC del Stream Deck Store apuntando a `127.0.0.1:9000`:
  - `/mcl/action/transport.play`, `/mcl/action/sel.record`, `/mcl/action/scene.3`, `/mcl/action/master.riser`
  - `/mcl/param/master.energy 0.8`
  - Con clic derecho en cualquier botón o knob, *Copiar dirección OSC* te da la suya.

## 6. MPD218 (mapeo por defecto)
- Pads del banco A (notas 36–51): pista de batería.
- Knobs: K1 Energía · K2 Cutoff · K3 Resonancia · K4 Key Morph · K5 Detune · K6 Volumen de la pista seleccionada · K7 Crossfader · K8 Master.

## 7. Numark Platinum FX
En **CONTROLADORES**, activa **MODO LEARN**. Pulsa un control de la app y mueve el de la Numark. Una idea de set:
- Crossfader → *Crossfader* del master (con clic derecho en cada pista eliges lado A o B).
- Faders de canal → volumen de pistas.
- Filtro/EQ → *Energía*.
- Pads → escenas, RISER y Stutter Hold.

## 8. Voz (SM7B)
Añade la pista **Micro SM7B**. En el Inspector, *Input* = canal de la Monocaster. Activa **Monitor** con auriculares. La cadena es Gate → EQ → Compresor → Chorus → Delay → Reverb. El SM7B pide mucha ganancia: súbela en la interfaz.

## 9. Archivos
- Proyectos: `Música\Music Creative Lab\Proyectos\*.mcl`
- Sets grabados: `Música\Music Creative Lab\Sets\*.wav`
- La sesión se guarda sola cada minuto y al cerrar.
