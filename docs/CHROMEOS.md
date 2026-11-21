# Music Creative Lab Lite en Chromebook (ChromeOS)

La version **Lite** es la misma app pero sin cargar VST (en ChromeOS no existen los VST de Windows).
Trae todo lo demas: loops, Lab Synth (10 presets), Lab Drums (16 pads), 16 efectos, piano roll,
escenas, Riser > Drop, MIDI learn y REC SET. Usa unos **45 MB de RAM**, asi que va bien con 4 GB.

## 1. Activa Linux (solo la primera vez)
1. Ajustes > Avanzado > Desarrolladores > **Entorno de desarrollo Linux** > Activar.
2. Dale 10 GB de disco (con 5 tambien vale). Espera a que termine; se abre una Terminal.

## 2. Descarga el instalador correcto
- Chromebook con procesador **Intel / AMD** (lo normal): `music-creative-lab-lite_..._amd64.deb`
- Chromebook con procesador **ARM / MediaTek / Snapdragon**: `music-creative-lab-lite_..._arm64.deb`

Para saber cual tienes: abre la Terminal de Linux y escribe `dpkg --print-architecture`.

Los dos se generan solos en GitHub: Actions > "ChromeOS / Linux Lite (.deb)" > la ultima ejecucion verde > Artifacts.

## 3. Instala
1. En la app **Archivos**, haz doble clic en el `.deb` > **Instalar**.
   (Si el archivo esta en Descargas y no te deja, arrastralo a "Archivos de Linux" primero.)
2. Aparece en el menu de apps, dentro de la carpeta **Aplicaciones de Linux**: "Music Creative Lab Lite".

Por Terminal tambien vale: `sudo apt install ./music-creative-lab-lite_0.1.0_amd64.deb`

## 4. Microfono y controladores
- **Micro:** Ajustes > Avanzado > Desarrolladores > Linux > activa **Permitir que Linux acceda al micro**.
- **MPD218 / Numark / Monocaster por USB:** Ajustes > Desarrolladores > Linux > **Gestionar dispositivos USB** > activa el aparato.
  Conectalo antes de abrir la app. El soporte USB-MIDI de Linux en ChromeOS depende del modelo de Chromebook:
  si no aparece, usa el teclado del ordenador (A W S E D F... y atajos) y el raton.
- **Stream Deck:** no tiene software para ChromeOS; los atajos de teclado si funcionan con cualquier teclado.

## 5. Consejos para 4 GB de RAM
- Cierra las pestanas de Chrome que no uses: Linux comparte la memoria con ellas.
- Si oyes chasquidos: boton **Audio** > sube el buffer a 1024.
- Pocas pistas con Reverb a la vez (es el efecto que mas CPU gasta).
- Los sets grabados con REC SET quedan en `Archivos de Linux/Music/Music Creative Lab/Sets`.
