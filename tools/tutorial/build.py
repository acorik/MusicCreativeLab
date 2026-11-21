import html
I='img/'
def fig(name, cap, cls=''): return f'<figure class="{cls}"><img src="{I}{name}.png"><figcaption>{cap}</figcaption></figure>'
def legend(items): return '<ol class="leg">'+''.join(f'<li><b>{a}</b> {b}</li>' for a,b in items)+'</ol>'
def tip(t): return f'<div class="tip"><b>JARVIS dice:</b> {t}</div>'
def warn(t): return f'<div class="warn"><b>Ojo:</b> {t}</div>'
P=[]
P.append('''<section class="cover">
<div class="logo">MUSIC CREATIVE LAB</div>
<h1>Guia ilustrada</h1>
<p class="sub">De cero a tu primer set en directo: pistas, loops, synths, bateria, efectos, piano roll, escenas, MPD218, Numark, Stream Deck y tu SM7B.</p>
<img src="img/02_pantalla_principal.png" class="coverimg">
<p class="ver">Version 0.1 &middot; Noviembre 2026 &middot; Todas las capturas son reales, sacadas de la app</p>
</section>''')
P.append('''<section><h2>Indice</h2><ol class="toc">
<li>Instalar y configurar el audio</li><li>La pantalla principal</li><li>La barra de arriba</li><li>Una pista por dentro (el looper)</li>
<li>Tu primer loop de bateria (paso a paso)</li><li>Escenas, Energy y el Riser &gt; Drop</li><li>El synth y Key Morph</li><li>La bateria (16 pads)</li>
<li>Efectos</li><li>El Mixer</li><li>El Piano Roll</li><li>Navegador, presets y VST</li><li>Anadir pistas</li><li>MIDI Learn: controla todo con tus aparatos</li>
<li>Controladores: MPD218, Numark y Stream Deck</li><li>Tu voz: el SM7B con efectos</li><li>Grabar tu set (REC SET)</li><li>Guardar, atajos y trucos</li><li>Mini diccionario musical</li></ol>
<p>Si tienes prisa: lee el 1, el 2 y el 5, y luego pulsa la demo <i>"1. Calma -&gt; UK Garage"</i>. Lo demas lo vas descubriendo tocando.</p></section>''')

P.append(f'''<section><h2>1. Instalar y configurar el audio</h2>
<h3>Instalar</h3><ol>
<li>Entra en la pagina de la compilacion: <a href="https://github.com/acorik/MusicCreativeLab/actions/runs/44805152006">github.com/acorik/MusicCreativeLab/actions</a> (la ultima ejecucion verde).</li>
<li>Abajo, en <b>Artifacts</b>, descarga el instalador (un .zip). Descomprimelo.</li>
<li>Ejecuta <b>MusicCreativeLab-Setup-(version).exe</b> y dale a Siguiente hasta el final. Si Windows dice "Windows protegio su PC", pulsa <i>Mas informacion &gt; Ejecutar de todas formas</i> (sale porque el programa no esta firmado, es normal).</li>
<li>Abre <b>Music Creative Lab</b> desde el menu Inicio.</li></ol>
<h3>Elegir tu tarjeta de sonido (Monocaster G1 Neo)</h3><ol>
<li>Conecta la Monocaster por USB <b>antes</b> de abrir la app.</li>
<li>Pulsa el boton <b>Audio</b> (arriba a la derecha, numero 16 en la seccion 3).</li>
<li>En <i>Tipo</i> elige <b>ASIO</b> si aparece tu interfaz; si no, <b>Windows Audio</b>. En <i>Dispositivo</i> elige la Monocaster tanto para salida como para entrada.</li>
<li>Tamano de buffer: empieza en <b>256</b>. Si oyes chasquidos, sube a 512. Si notas retraso al tocar, baja a 128.</li></ol>
{tip("Mientras no elijas tarjeta veras arriba el texto 'SIN AUDIO: pulsa Audio'. En las capturas sale porque las hice en un ordenador sin altavoces, tranquilo.")}
{warn("No mezcles: si usas ASIO, cierra otros programas que usen la misma interfaz (OBS, Discord...) o pueden pelearse por ella.")}
<h3>La primera vez</h3><p>La app abre la pestana <b>TUTORIALES</b>: son 10 mini guias dentro del programa. Cada una tiene el boton <i>Cargar la demo de este tutorial</i> para probarlo al momento.</p>
{fig("01_tutoriales","Pestana Tutoriales: lista de guias a la izquierda, texto en el centro y boton para cargar la demo.")}
</section>''')

P.append(f'''<section><h2>2. La pantalla principal</h2>
<p>Todo esta en una sola pantalla. No hay ventanas escondidas: lo que ves es lo que suena.</p>
{fig("02_pantalla_principal","Pagina LIVE con la demo 1 sonando.")}
{legend([("Navegador.","Demos, pistas listas (instrumento + efectos), efectos y tus VST. Doble clic para anadir."),
("Pestanas.","LIVE (tocar), MIXER (volumenes), PIANO ROLL (editar notas), CONTROLADORES (mapeos) y TUTORIALES."),
("Escenas.","Botones que cambian varias pistas a la vez (intro, build, drop...)."),
("Inspector.","Los mandos de la pista seleccionada: su instrumento y sus efectos."),
("Teclado.","Puedes tocar con el raton o con el teclado del ordenador (A W S E D F...)."),
("PLAY.","Arranca y para la musica (tecla Espacio)."),
("Energy.","El 'mando magico': de 0 (oscuro, filtrado) a 1 (todo abierto, potente)."),
("MIDI LEARN.","Para asignar cualquier mando a tus controladores."),
("Audio.","Configurar tarjeta de sonido.")])}
<p>Cada <b>fila</b> de LIVE es una <b>pista</b>: un instrumento (synth, bateria) o tu micro. Haz clic en una fila para seleccionarla; el Inspector de la derecha cambia para mostrar sus mandos.</p></section>''')

P.append(f'''<section><h2>3. La barra de arriba</h2>
{fig("03_barra_superior","Barra superior (partida en dos para que se lea bien).")}
{legend([("Archivo.","Nuevo proyecto, abrir, guardar, demos y la carpeta de sets grabados."),("PLAY.","Empieza a sonar."),("STOP.","Para. Pulsado dos veces vuelve al principio."),
("REC.","Graba o hace overdub en la pista seleccionada (tecla R)."),("Metro.","Metronomo (clic en cada pulso)."),("Tap.","Pulsalo al ritmo de una cancion y la app adivina el BPM."),
("BPM.","Velocidad. UK garage suele ir a 130-140. Usa - y + para ajustar."),("Posicion.","Compas . pulso. '4 . 1' = compas 4, pulso 1."),
("Energy.","Filtro general: bajalo para un break oscuro, subelo para el drop."),("Swing.","Hace que el ritmo 'cojee' con estilo; clave en garage (0.1-0.25)."),
("Riser Bars.","Cuantos compases dura la subida del Riser."),("Crossfader.","Mezcla entre las pistas del lado A y del lado B, como un DJ."),
("REC SET.","Graba TODO lo que suena a un archivo WAV (para YouTube/TikTok)."),("Panico.","Si una nota se queda pillada, esto calla todo."),
("MIDI LEARN.","Modo de asignar controles."),("Audio.","Tarjeta de sonido."),("Estado.","Avisos y uso de CPU.")])}
{tip("Los mandos con un puntito azul ya estan asignados a un knob de tu MPD218.")}</section>''')

P.append(f'''<section><h2>4. Una pista por dentro (el looper)</h2>
<p>Cada pista tiene su propio <b>looper</b>: grabas un trozo y se repite solo, siempre a tempo.</p>
{fig("04_fila_pista","Fila de la pista Keys.","wide")}
{legend([("REC.","1a vez: graba. Si ya hay loop: overdub (anade notas encima sin borrar)."),("Play/Stop.","Para o reanuda solo este loop."),
("Undo.","Deshace el ultimo overdub."),("x2.","Duplica el loop (4 compases pasan a 8) para hacer variaciones."),("Borrar.","Vacia la pista."),
("M (Mute).","Silencia la pista."),("S (Solo).","Solo suena esta pista."),
("Modo.","<i>Largo fijo</i>: tu eliges cuantos compases. <i>Libre (silencios)</i>: empieza con tu primer golpe y se cierra solo cuando dejas 2 compases de silencio."),
("Cuantizacion.","Coloca tus notas en la rejilla al grabar. 1/16 es lo normal; 'Sin cuant.' deja tus notas tal cual."),
("Compases.","Largo del loop en modo fijo: 1, 2, 4, 8 o 16.")])}
<p>El recuadro de abajo muestra las notas del loop y una linea que avanza (el cabezal). El color del borde te dice el estado: <b style="color:#e0405a">rojo</b> grabando, <b style="color:#ff9f5a">naranja</b> overdub, color de la pista = sonando.</p>
</section>''')

P.append(f'''<section><h2>5. Tu primer loop de bateria (paso a paso)</h2>
<p>Lo que pediste: grabar bateria a cualquier tempo y que el programa entienda los silencios. Asi se hace:</p>
<h3>Paso 1. Proyecto nuevo</h3><p><b>Archivo &gt; Nuevo proyecto</b>. Te crea 5 pistas vacias: Bateria, Pad, Bajo, Keys y Micro.</p>
{fig("18_proyecto_nuevo","Proyecto nuevo: todo vacio y listo para grabar.")}
<h3>Paso 2. Modo libre y REC</h3><p>En la pista Bateria pon el modo en <b>Libre (silencios)</b> y pulsa <b>REC</b>. La pista se queda esperando: no empieza a grabar hasta que des el primer golpe, asi no hay huecos raros al principio.</p>
{fig("19_esperando_nota","Esperando tu primer golpe.","wide")}
<h3>Paso 3. Toca</h3><p>Toca los pads de la MPD218 (o las teclas del ordenador). Bombo, hi-hat, clap, hi-hat... El recuadro rojo crece mientras tocas: 1, 2, 4... compases.</p>
{fig("20_grabando","Grabando en modo libre: ves las notas aparecer y los pads se iluminan a la derecha.")}
<h3>Paso 4. Deja de tocar</h3><p>Cuando dejas <b>2 compases en silencio</b>, la app entiende que has terminado: cierra el loop, lo redondea al largo mas logico (1, 2, 4, 8 o 16 compases) y lo pone a sonar. Las notas quedan como las tocaste (con la cuantizacion que elegiste).</p>
{fig("21_loop_cerrado","Loop cerrado: 2 compases, ya sonando en bucle.","wide")}
<p>Si quieres retocarlo, ve al <b>PIANO ROLL</b>: ahi ves cada golpe y lo puedes mover.</p>
{fig("22_piano_roll_drums","Tu loop de bateria en el piano roll.")}
{tip("Si te equivocas: Undo (o Z). Si quieres empezar de cero: Borrar. Si quieres anadir mas golpes encima: REC otra vez = overdub.")}
</section>''')

P.append(f'''<section><h2>6. Escenas, Energy y el Riser &gt; Drop</h2>
<p>Una <b>escena</b> es una "foto" de que pistas suenan. Pulsar una escena cambia todo a la vez, siempre al empezar el siguiente compas para que no se descuadre.</p>
{fig("05_escenas","Columna de escenas de la demo 1: Intro calma, Build, DROP garage y Breakdown emo.","narrow")}
<ul><li><b>Lanzar:</b> clic en el nombre (o teclas 1 a 8).</li><li><b>Guardar:</b> deja sonando lo que quieras (con mutes) y pulsa <i>Guardar</i> en una escena vacia.</li></ul>
<h3>El Riser &gt; Drop</h3><p>Pulsa <b>RISER &gt; DROP</b> (tecla B): durante unos compases (Riser Bars) la musica sube de tension, el Energy se abre... y al terminar salta a la siguiente escena con el drop. Es tu transicion de calma a UK garage potente con un solo boton.</p>
{fig("06_riser","Riser en marcha: el boton se llena de izquierda a derecha y el Energy sube a 1.")}
{tip("Pon el knob 1 de la MPD218 en Energy (ya viene asi) y mueve tu mismo la tension; el Riser es la version automatica.")}
</section>''')

P.append(f'''<section><h2>7. El synth (Lab Synth) y Key Morph</h2>
<div class="cols"><div>{fig("07_inspector_synth","Inspector de la pista Keys.")}</div><div>
{legend([("Cambiar instrumento.","Pon otro synth, la bateria o un VST tuyo."),("Cutoff.","Brillo. Bajo = oscuro y suave; alto = brillante."),
("Key Morph.","La magia que pediste: cada tecla suena distinta. Las notas graves suenan mas calidas y las agudas mas brillantes y abiertas, asi cada acorde transmite mas emocion."),
("Attack.","Cuanto tarda en entrar el sonido. 0 = golpe; alto = entra suave como un pad.")])}
<p>El desplegable de arriba tiene <b>10 presets</b>: Fred Pad, Glass Keys, Garage Bass, Emo Lead, Pluck Chops, Sub Boom, Supersaw Drop, Choir Air, Warm Organ Stab y Dreamy Arp.</p>
<p>Otros mandos utiles:</p><ul><li><b>Wave A/B, A/B Mix:</b> forma de onda (el "color" basico).</li><li><b>Unison/Detune:</b> mas voces desafinadas = sonido gordo.</li>
<li><b>Reso:</b> pitido en el filtro (cuidado, sube poco).</li><li><b>Decay/Sustain/Release:</b> como se apaga la nota.</li><li><b>LFO:</b> movimiento automatico.</li><li><b>Glide:</b> las notas se deslizan.</li></ul>
</div></div>
{tip("Todos estos mandos se pueden mover en directo mientras el loop suena. Prueba Key Morph y Cutoff a la vez con dos knobs de la MPD218: asi pasas de calma a brillante sin parar la musica.")}
</section>''')

P.append(f'''<section><h2>8. La bateria (16 pads)</h2>
<div class="cols"><div>{fig("08_inspector_drums","Inspector de una pista de bateria.")}</div><div>
<p>Los 16 pads estan colocados <b>igual que en tu MPD218</b> (banco A): abajo a la izquierda el 1 (Kick).</p>
<ul><li><b>Clic en un pad:</b> suena y lo selecciona.</li><li><b>Clic derecho en un pad:</b> cargar tu propio sample (WAV, AIFF o FLAC) o elegir otro sonido.</li>
<li>El desplegable de arriba cambia el kit: <b>UK Garage 2-Step</b>, <b>Soft Fabric</b> (calma) o <b>Heavy Drop</b> (potente).</li></ul>
{legend([("Punch.","Da pegada a todo el kit.")])}
<p>Debajo: Kit Level y los mandos del pad seleccionado (volumen, afinacion, duracion y panorama).</p>
</div></div></section>''')

P.append(f'''<section><h2>9. Efectos</h2>
<div class="cols"><div>{fig("09_efectos","Efectos de la pista Pad, con Pump (sidechain) seleccionado.")}</div><div>
<p>Cada pista tiene su <b>cadena de efectos</b> (abajo del Inspector). El sonido pasa por ellos de izquierda a derecha.</p>
{legend([("+ Efecto.","Anade uno de la lista (o un VST de efecto)."),("Mandos del efecto.","Aqui Depth: cuanto 'bombea' el pad con el bombo, el sonido tipico de garage.")])}
<p><b>Los 16 efectos incluidos:</b> Filter, EQ 3-band, Compressor, Noise Gate, Limiter, Reverb, Delay (sync), Chorus/Doubler, Phaser, Saturator/Distortion, Bitcrusher/Lo-Fi, Pump (sidechain), Stutter/Beat Repeat, Auto-Pan/Tremolo, Stereo Widener y Robot (Ring Mod).</p>
<ul><li>Clic en un efecto = ver sus mandos. Boton verde ON = encender/apagar.</li><li>Para quitar, apagar o reordenar un efecto: en el MIXER, clic derecho sobre el efecto (Activar/desactivar, Subir, Bajar, Eliminar).</li><li>Desde el Navegador: doble clic en un efecto lo anade a la pista seleccionada.</li></ul>
<p>"Efectos infinitos": encadena los que quieras y suma los VST gratis de la seccion 12.</p>
</div></div></section>''')

P.append(f'''<section><h2>10. El Mixer</h2>
{fig("10_mixer","Pestana MIXER: un canal por pista, como en FL Studio.")}
<ul><li><b>Fader:</b> volumen de la pista. <b>Medidor:</b> lo fuerte que suena (si llega arriba y se pone rojo, baja).</li>
<li><b>Pan:</b> izquierda/derecha. <b>M/S:</b> mute y solo.</li><li><b>FX:</b> la cadena de efectos; <b>+ FX</b> anade uno.</li>
<li><b>XF: A / B:</b> en que lado del crossfader esta cada pista.</li><li><b>MASTER:</b> todo junto. Lleva un Limiter al final para que nunca sature.</li></ul>
{tip("Truco DJ: pon la bateria calma en A y la de garage en B, y pasa de una a otra con el crossfader (knob 7 de la MPD218).")}
</section>''')

P.append(f'''<section><h2>11. El Piano Roll</h2>
{fig("11_piano_roll","Piano roll de la pista Keys.")}
<ul><li><b>Clic</b> en un hueco: crea nota. <b>Arrastrar:</b> mover. <b>Borde o Shift:</b> cambiar largo. <b>Alt + arrastrar:</b> velocidad (fuerza). <b>Doble clic / clic derecho:</b> borrar.</li>
<li><b>Compases y Rejilla:</b> largo del loop y tamano de la cuadricula.</li>
<li><b>Cuantizar (1):</b> pone las notas en su sitio exacto. <b>Duplicar x2 (2):</b> copia el loop detras.</li>
<li><b>-1 oct / +1 oct / -1 / +1:</b> sube o baja todo. <b>Humanizar:</b> lo vuelve un poco imperfecto (mas "humano").</li></ul>
<p>Lo mejor: el piano roll <b>sigue sonando en bucle</b> mientras editas, y puedes mover los mandos del synth en el Inspector a la vez para cambiar el sonido en tiempo real.</p>
</section>''')

P.append(f'''<section><h2>12. Navegador, presets y VST</h2>
<div class="cols"><div>{fig("12_navegador","Navegador (columna izquierda).","")}</div><div>
<ul><li><b>DEMOS:</b> 3 proyectos para tocar ya: <i>Calma -&gt; UK Garage</i> (132 BPM), <i>Emo 2-Step nocturno</i> (138) y <i>Ambient Lab</i> (96).</li>
<li><b>PISTAS:</b> 15 presets: instrumento + efectos ya ajustados. Doble clic y aparece una pista nueva.</li>
<li><b>EFECTOS:</b> doble clic = se anade a la pista seleccionada.</li><li><b>Buscar:</b> escribe para filtrar.</li></ul>
{legend([("Escanear mis VST.","Busca los VST3 instalados en tu PC (C:\\Program Files\\Common Files\\VST3). Con <b>+ Carpeta</b> anades otra carpeta."),
("VST gratis recomendados.","Lista de VST gratis y legales (Vital, Surge XT, Dexed, Spitfire LABS, Decent Sampler, TAL, Valhalla Supermassive...). Los descargas tu desde su web y le das a Escanear.")])}
{warn("No puedo meter en la app VST de pago (Serum, Omnisphere, Kontakt...): la licencia no deja repartirlos. Si los compras y los instalas, la app los carga sin problema.")}
</div></div></section>''')

P.append(f'''<section><h2>13. Anadir pistas</h2>
{fig("13_menu_pista","Boton + Pista con su menu.")}
<p>Pulsa <b>+ Pista</b> (1) al final de las filas: eliges <i>Synths y bajos</i>, <i>Baterias (MPD218)</i>, <i>Micro / voz</i>, <i>Mis instrumentos VST</i>, o una pista vacia (synth o audio).</p>
<p><b>Clic derecho en el nombre</b> de una pista: renombrar, abrir piano roll, subir/bajar, lado del crossfader (A/B) o eliminar.</p>
</section>''')

P.append(f'''<section><h2>14. MIDI Learn: controla todo con tus aparatos</h2>
<p>Cualquier mando, boton o fader de la app se puede asignar a un knob, pad o fader fisico. Dos formas:</p>
<h3>Forma rapida: clic derecho en un mando</h3>
{fig("14_menu_learn","Clic derecho en Cutoff.")}
<ul><li><b>MIDI Learn:</b> mueve un knob de tu controlador y queda asignado a ESTE mando de ESTA pista.</li>
<li><b>MIDI Learn para la pista SELECCIONADA:</b> el knob controla el Cutoff de la pista que tengas seleccionada en cada momento. Con 8 knobs controlas todas las pistas.</li>
<li><b>Copiar direccion OSC:</b> para el Stream Deck (seccion 15).</li><li><b>Valor por defecto:</b> lo devuelve a su sitio.</li></ul>
<h3>Modo Learn</h3>
{fig("15_learn_activo","MIDI LEARN encendido (1): los mandos se recuadran y el que esta esperando se marca (2).")}
<p>Pulsa <b>MIDI LEARN</b>, haz clic en un mando de la app, mueve el control fisico... listo. Repite con todos los que quieras y vuelve a pulsar MIDI LEARN para salir.</p>
</section>''')

P.append(f'''<section><h2>15. Controladores: MPD218, Numark y Stream Deck</h2>
{fig("16_controladores","Pestana CONTROLADORES: mapeos activos, acciones y la ayuda de cada aparato.")}
<p>Aqui ves todos los mapeos (puedes quitar el que quieras), el monitor MIDI (que llega en tiempo real) y la lista de <b>Acciones</b>: clic en una y pulsa un pad = ese pad hace esa accion (Play, Grabar, Escena 3, Riser...).</p>
<h3>Akai MPD218 (ya viene configurado)</h3><ul>
<li><b>Pads (banco A):</b> van directos a la pista de bateria (notas 36-51). Con el boton naranja <i>Pads MPD218 -&gt; bateria</i> lo activas/desactivas.</li>
<li><b>Knobs 1-8:</b> Energy, Cutoff, Reso, Key Morph, Detune, Volumen de la pista seleccionada, Crossfader y Master.</li>
<li>Bancos B y C de pads: libres para que les pongas acciones (ej. banco B = escenas 1-8).</li></ul>
<h3>Numark Platinum FX</h3><ul>
<li>Pulsa <b>MODO LEARN</b>, haz clic en el mando de la app y mueve el control de la Numark. Ideas: crossfader de la Numark = Crossfader; faders de canal = volumen de pistas; botones de efectos = Riser y mutes; jog = Energy.</li>
</ul>
<h3>Stream Deck MK2</h3><ol>
<li><b>Lo facil:</b> en el software del Stream Deck anade botones <i>Sistema &gt; Tecla de acceso rapido</i> con las teclas de abajo (la app tiene que estar delante).</li>
<li><b>Pro (funciona aunque la app este en segundo plano):</b> plugin OSC gratuito de la Store, enviando a <b>127.0.0.1 puerto 9000</b> mensajes como <code>/mcl/action/transport.play</code>, <code>/mcl/action/scene.1</code>, <code>/mcl/action/master.riser</code> o <code>/mcl/param/master.energy 0.7</code>.</li></ol>
<table><tr><th>Tecla</th><th>Hace</th><th>Tecla</th><th>Hace</th></tr>
<tr><td>Espacio</td><td>Play / pausa</td><td>1 ... 8</td><td>Lanzar escena</td></tr>
<tr><td>Esc</td><td>Stop (2 veces = inicio)</td><td>B</td><td>Riser &gt; Drop</td></tr>
<tr><td>R</td><td>Grabar / overdub</td><td>M</td><td>Metronomo</td></tr>
<tr><td>Z</td><td>Deshacer overdub</td><td>N</td><td>Tap tempo</td></tr>
<tr><td>X</td><td>Duplicar loop</td><td>Q</td><td>Panico</td></tr>
<tr><td>V</td><td>Parar/reanudar loop</td><td>F5</td><td>REC SET</td></tr>
<tr><td>Supr</td><td>Borrar loop</td><td>F13-F20</td><td>Escenas 1-8 (ideal Stream Deck)</td></tr>
<tr><td>Flechas</td><td>Cambiar pista / octava</td><td>F21 F22 F23 F24</td><td>Grabar, Riser, Play, Deshacer</td></tr>
<tr><td>A W S E D F...</td><td>Tocar notas como un piano</td><td></td><td></td></tr></table>
{tip("Las teclas F13-F24 no existen en tu teclado, por eso son perfectas para el Stream Deck: nunca chocan con nada.")}
</section>''')

P.append(f'''<section><h2>16. Tu voz: el SM7B con efectos</h2>
{fig("17_micro","Pista Micro seleccionada con el Monitor encendido y su cadena de efectos.")}
{legend([("Fila Micro.","Es una pista de audio: su loop graba sonido real (voz, guitarra...). El boton Monitor verde = te oyes con efectos."),
("Input.","Que entrada de la Monocaster usa (0 = entrada 1)."),("Monitor.","Encender para oirte. Apagalo si te acoplas o no lo necesitas.")])}
<p><b>Cadena que viene puesta:</b> Noise Gate (quita ruido de fondo) &gt; EQ 3-band &gt; Compressor (nivela) &gt; Chorus/Doubler &gt; Delay &gt; Reverb. Tambien tienes el preset <i>Micro voz robot</i> con el efecto Robot.</p>
{warn("El SM7B necesita mucha ganancia. Sube la ganancia en la Monocaster hasta que, hablando normal, el medidor llegue a la mitad o 3/4. Si se queda corto, un booster tipo Cloudlifter ayuda. Usa auriculares para no acoplar.")}
<p>Puedes grabar loops de voz igual que con los instrumentos: REC en la pista Micro (modo fijo de compases).</p>
</section>''')

P.append(f'''<section><h2>17. Grabar tu set (REC SET)</h2>
{fig("23_rec_set","REC SET encendido (1) y el aviso GRABANDO SET (2).")}
<ol><li>Pulsa <b>REC SET</b> (o F5). Se pone rojo y arriba sale <i>GRABANDO SET</i>.</li><li>Toca tu set: escenas, riser, voz, todo.</li>
<li>Vuelve a pulsar REC SET para terminar.</li><li><b>Archivo &gt; Abrir carpeta de sets grabados</b>: tu WAV esta en <i>Musica\\Music Creative Lab\\Sets</i>, listo para meterlo en tu editor de video.</li></ol>
{tip("Graba con OBS a la vez para tener la imagen y usa este WAV como audio: suena mucho mejor que capturar el escritorio.")}
</section>''')

P.append(f'''<section><h2>18. Guardar, atajos y trucos</h2><ul>
<li><b>Archivo &gt; Guardar</b> (Ctrl+S). Los proyectos (.mcl) van a <i>Musica\\Music Creative Lab\\Proyectos</i>. Doble clic en un .mcl lo abre.</li>
<li><b>Autoguardado:</b> si se cierra la app, al abrirla vuelve todo como estaba.</li>
<li><b>Ctrl+O</b> abre un proyecto. El Undo (Z) es por pista: deshace el ultimo overdub.</li>
<li><b>Rendimiento:</b> en tu PC (i7 + 32 GB) va sobrado. Si un VST pesado da chasquidos, sube el buffer a 512.</li>
<li><b>Flujo recomendado para un directo:</b> 1) graba pad calmado, 2) bajo, 3) bateria calma en libre, 4) guarda escena "Intro", 5) graba bateria garage y synth del drop, guarda escena "Drop", 6) en el directo: Intro &gt; Energy subiendo &gt; Riser &gt; Drop.</li></ul>
<h2>19. Mini diccionario musical</h2><table class="dict">
<tr><td><b>BPM</b></td><td>Pulsos por minuto: la velocidad.</td></tr><tr><td><b>Compas</b></td><td>Grupo de 4 pulsos. Los loops miden 1, 2, 4, 8 o 16 compases.</td></tr>
<tr><td><b>Loop</b></td><td>Trozo que se repite en bucle.</td></tr><tr><td><b>Overdub</b></td><td>Grabar encima de un loop sin borrarlo.</td></tr>
<tr><td><b>Cuantizar</b></td><td>Colocar las notas exactamente en la rejilla.</td></tr><tr><td><b>Swing</b></td><td>Retrasar un poco las notas pares: da "groove".</td></tr>
<tr><td><b>Drop</b></td><td>El momento fuerte despues de una subida.</td></tr><tr><td><b>Riser</b></td><td>Subida de tension antes del drop.</td></tr>
<tr><td><b>Sidechain / Pump</b></td><td>El sonido baja cada vez que pega el bombo: efecto "respira".</td></tr><tr><td><b>Pad</b></td><td>Sonido largo y suave de fondo (y tambien los botones de la MPD).</td></tr>
<tr><td><b>VST</b></td><td>Instrumento o efecto de otra marca que se mete dentro de la app.</td></tr><tr><td><b>MIDI</b></td><td>El "idioma" con el que tus controladores hablan con el PC.</td></tr>
<tr><td><b>UK Garage / 2-Step</b></td><td>Estilo de Reino Unido, 130-140 BPM, bateria saltarina con mucho swing.</td></tr></table>
<p class="end">Esto es la version 0.1. Cada cosa que no te guste o que eches en falta, dimela y la mejoramos. Para eso estamos.</p>
</section>''')

CSS='''@page{size:A4;margin:13mm 13mm 14mm}
body{font-family:"DejaVu Sans",Arial,sans-serif;font-size:10.3pt;line-height:1.45;color:#1d1d26}
section{page-break-before:always}section.cover{page-break-before:auto;text-align:center;padding-top:20mm}
.logo{font-weight:800;letter-spacing:4px;color:#ff8c28;font-size:14pt}
.cover h1{font-size:34pt;margin:4mm 0 3mm;color:#1d1d26}.sub{font-size:12pt;color:#555;max-width:150mm;margin:0 auto 8mm}
.coverimg{width:100%;border-radius:6px;box-shadow:0 2px 10px rgba(0,0,0,.3)}.ver{color:#888;font-size:9pt;margin-top:6mm}
h2{font-size:18pt;color:#1d1d26;border-bottom:3px solid #ff8c28;padding-bottom:2mm;margin:0 0 4mm}
h3{font-size:12pt;color:#c25c00;margin:5mm 0 2mm}
figure{margin:3mm 0 4mm;text-align:center;page-break-inside:avoid}figure img{max-width:100%;border:1px solid #333;border-radius:4px}
figure.narrow img{max-height:95mm}figcaption{font-size:8.5pt;color:#666;margin-top:1mm;font-style:italic}
.cols{display:flex;gap:6mm;align-items:flex-start}.cols>div:first-child{flex:0 0 62mm}.cols>div:last-child{flex:1}
.cols figure img{max-height:200mm}
ol.leg{counter-reset:n;list-style:none;padding:0;margin:2mm 0}ol.leg li{counter-increment:n;position:relative;padding-left:9mm;margin:1.4mm 0}
ol.leg li:before{content:counter(n);position:absolute;left:0;top:0;width:6mm;height:6mm;border-radius:50%;background:#ff8c28;color:#111;font-weight:800;font-size:8.5pt;text-align:center;line-height:6mm}
.tip,.warn{border-radius:5px;padding:2.5mm 3.5mm;margin:3mm 0;page-break-inside:avoid}.tip{background:#e9f6ff;border-left:4px solid #36c8ff}
.warn{background:#fff3e6;border-left:4px solid #ff8c28}
table{border-collapse:collapse;width:100%;font-size:9pt;margin:3mm 0;page-break-inside:avoid}td,th{border:1px solid #ccc;padding:1.3mm 2mm;text-align:left}th{background:#2a2a36;color:#fff}
code{background:#eee;padding:0 1mm;border-radius:2px;font-size:8.5pt}.toc li{margin:1.2mm 0;font-size:11pt}.end{margin-top:6mm;font-weight:bold;color:#c25c00}
a{color:#0a6fb0}'''
open('/tmp/claude-0/tut/guia.html','w').write('<!doctype html><html lang="es"><head><meta charset="utf-8"><title>Music Creative Lab - Guia ilustrada</title><style>'+CSS+'</style></head><body>'+''.join(P)+'</body></html>')
