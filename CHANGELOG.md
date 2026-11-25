# ND-07 // MONOLITH · FM-1 · historial de cambios

## v1.9.3: los macros del driver

- **ENGINE D es el editor de macros**, cuadro por cuadro a 60 Hz: VOL, PITCH y TIMBRE de 16 pasos cada uno, con su
  largo (una vez o en loop), y SWEEP. K1 el macro, K2 el paso, K3 el valor, K4 el largo; la pantalla los dibuja como
  barras y marca el paso que suena.
- **TIMBRE** es de cada motor: el duty o el ruido (DMG y las ondas CHIP de ND07), el MIXER (YM2149), el nivel de los
  moduladores en pasos de 3 dB (YM2612).
- ARP y STEPS ya no son perillas: son macros. Las sesiones anteriores, los assets y los NEW SFX los convierten solos
  (el mismo arpegio, el mismo paso de volumen); STEPS a 30 y 15 Hz queda a 60 Hz.
- El formato de la sesión crece (los macros se guardan); las de la 1.6 a la 1.9.2 se cargan.

## v1.9.2: medir contra lo real

- `tools/compare.py`: dos grabaciones de las mismas notas, lado a lado. Encuentra las notas solo (sirve para una
  toma tocada a mano) y dice por cada par el tono en cents, el nivel, el brillo, los armónicos 1 a 8, el ruido, el
  ataque y el decaimiento; marca lo que se separa y puede dibujar los espectros.
- `tests/scripts/ref.txt`: la toma de referencia (SYNTH, luego cada motor en C4 y C5), igual en el simulador y en
  el FM-1. `docs/GRABAR.md`: cómo grabarla con el SSL2 y Reaper, y la toma contra los chips de verdad.
- Sin cambios de sonido.

## v1.9.1: lo que pasaba después del chip

- **YM2149:** las tres salidas amarradas, como en el Atari ST y el MSX: una red de resistencias (el modelo de MAME,
  con los resistores medidos), no una suma. Tres canales arriba dan 1.89 veces uno; el acorde y los *digi-drums* se
  aprietan como en el ST.
- **YM2612:** la envolvente de la nota viaja dentro del chip como atenuación de los portadores, así que una nota que
  se apaga pasa por el DAC de 9 bits y su *ladder* como en la consola; después, el pasa-bajos del Mega Drive modelo
  1 (~3.9 kHz, el de Genesis Plus GX).
- **SPC700:** el S-DSP a sus 32 kHz (cada muestra calculada y sostenida), el eco a 32 kHz con el FIR en el orden del
  hardware y su redondeo, y el filtro de salida del SNES (dos polos a 16 kHz). El eco llega ahora a 512 ms.
- **PRINT:** los mismos filtros de salida, con el del Mega Drive corregido (antes ~6.4 kHz).
- `tests/chips_test.c`: la red del YM2149, los dos filtros y la atenuación del YM2612.
- Con ENGINE ND07, el sonido idéntico al de la 1.9.

## v1.9: 01 ENGINE sin repeticiones

- Cada cosa del chip en un solo lugar: el chip en K1, su consola arriba de la pantalla, sus perillas en A y B, su DAC
  en C, el driver en D. Fuera el nombre repetido en la pantalla, los cartuchos de chips y la etiqueta del motor en el
  driver.
- **PRINT ya no escoge chip:** imprime por el DAC del motor. Sus perillas son PRINT (la mezcla, 0 % = apagado), RATE
  y GRIT.
- **Con ND07 la página tiene dos juegos:** A, con la onda o el sample de 03 tal cual suena, y el driver (B). No quedan
  juegos vacíos.
- Las sesiones anteriores se cargan; un PRINT que estaba apagado sigue apagado. Sin cambios de sonido en las pruebas.

## v1.8: el motor primero, los chips con sus propios números

- **01 ENGINE abre el flujo.** Primero escoges con qué se hace el sonido (ND07 o el chip de una consola) y después el
  asset o el synth: 01 ENGINE · 02 ASSETS · 03 OSC & SAMPLE. Cargar un asset, una receta o un HAPPY ACCIDENT ya no
  cambia el motor; un asset de ruido le pide al chip su ruido.
- **Sin chips repetidos.** SOUND CHIPS ya no vive en ASSETS: es **PRINT**, el juego C de ENGINE (PRINT, RATE, GRIT,
  MIX), y cuando está encendido sigue al motor. El eco del PRINT se fue (el SPC700 trae el suyo); el TONE es ahora el
  filtro de salida real de la consola (el del Mega Drive modelo 1). El driver de 60 Hz pasa al juego D.
- **NEW SFX** (PRESET) es el juego B de 02 ASSETS. **DUTY** y **NOISE** viven dentro del motor DMG, donde están en el
  hardware; en las ondas CHIP de ND07 los pone COLOR.
- **Cada chip con su aritmética:** el DAC medido del YM2149 y sus periodos enteros de tono, ruido y envolvente; el
  periodo de 11 bits y los divisores de ruido del DMG; las ROMs logsin y exp del YM2612, su tabla de tasas de
  envolvente, el TOTAL LEVEL en pasos de 0.75 dB, el F-NUMBER y el DAC de 9 bits con su *ladder*; la tabla gaussiana
  del SNES y los cuatro filtros del BRR (el codificador escoge el mejor por bloque, también en PRINT); las 32
  velocidades del LFO del SCSP. Las cuadradas ponen sus flancos entre muestras: sin aliasing.
- `tests/chips_test.c` compara los chips contra los números del hardware.
- Las sesiones de la 1.6 y la 1.7 se cargan (el DMG traduce su PULSE y NOISE de antes).

## v1.7: arranque y actualización

- **Pantalla de inicio:** el monolito se dibuja, dos órbitas lo rodean y se escribe "ND-07 // MONOLITH" con la versión. Dura 1.8 s y cualquier botón o perilla la salta.
- **Pantalla de actualización propia:** al instalar un `.fwsc` desde la página, el FM-1 muestra los cinco pasos (PACKAGE, CHECK HEAD, LOADER, CONFIRM, RESTART), el monolito llenándose y "DO NOT UNPLUG". Si algo falla dice UPDATE FAILED con su código.
- **Más ligero:** la curva del 3-BAND EQ se calcula solo cuando cambia una perilla (antes, en cada cuadro); fuera código que ya no se usaba. Sin cambios de sonido.

## v1.6: el chip como motor

- **03 CHIP ENGINE:** la voz puede salir del chip de una consola en lugar de 02: **YM2149** (tres cuadradas, ruido, la envolvente de 32 pasos, BUZZER y SYNCBUZ, SID, DIGI), **YM2612** (FM de 4 operadores, 8 algoritmos, feedback, CH3 con pitch por operador, SSG-EG, el canal DAC y el escalón del *ladder*), **SPC700** (BRR de 4 bits, interpolación gaussiana, PMON, eco con FIR de 8 tomas), **SCSP** (PCM de 16 bits con FM desde cualquier onda, PLFO, ALFO y programas del DSP) y **DMG** (dos pulsos, la wave RAM, el ruido de 15 o 7 bits, la envolvente por hardware y el *zombie mode*). **ND07** es el motor de siempre. Cada motor tiene sus siete perillas y su pantalla. Ver el README.
- Lo que era CHIP OSC queda dentro: el *driver* de 60 Hz (SWEEP, ARP, ARP SPD, STEPS) en C, para cualquier motor; DUTY, NOISE y PRESET en D. SEL genera un NEW SFX con el motor que tengas.
- **09 3-BAND EQ** en lugar de VOWEL + SHIFT: shelf de graves, campana y shelf de agudos (SVF de Simper), ±15 dB, la frecuencia de cada banda, el Q de la campana y OUT. Los assets que usaban la boca (MONSTER, UI ERR, ENGINE) se rehicieron con el EQ y un LFO al OUT.
- **TRANSIENT** es ahora el juego D de 06 ENVELOPES y **SPACE** el juego C de 13 MULTIBAND REVERB: 19 páginas.
- **RTPC y CHAOS PAD + KEYS quedan escondidas.** Ver el final del README.
- Sin cambios de sonido en lo que no se tocó: con ENGINE ND07, las pruebas de audio dan lo mismo bit a bit que la 1.5 (salvo las que pasan por el HAPPY ACCIDENT, que ahora también sortea el EQ). Las sesiones anteriores se cargan; lo que era VOWEL + SHIFT empieza plano.

## v1.5

- **SOUND CHIPS pasa a 01 ASSETS** (juegos B y C): la consola se escoge al empezar, junto con el asset. Vuelven a ser 21 páginas.
- **Asset SYNTH:** un saw abierto, sostenido y seco, con un poco de vibrato; el mejor punto de partida para oír qué hace cada chip.
- **02 OSC & SAMPLE:** el oscilador como wavetable visto de lado (el ciclo al frente y detrás la misma onda con menos COLOR) y el sample como tallos con punta.
- **10 VOWEL + SHIFT:** una boca que dice la vocal, se abre con el sonido y tiembla con GROWL.
- **11 RES MOD FILTER:** de vuelta la respuesta rellena con la topografía encima.
- ASSETS en cuatro columnas.

## v1.4

- **22 SOUND CHIPS:** la última página imprime todo el sonido a través del chip de una consola: YM2149 (Atari ST, MSX), YM2612 (Mega Drive), S-DSP (Super NES), SCSP (Saturn) y DMG (Game Boy). Ver el README.
- **Samples de fábrica empacados sin pérdida:** los mismos 16 bits, 136 KB menos de flash. Se desempacan en RAM al escoger el slot.
- **Pantallas como juguetes de vectores:** un objeto por página en líneas de un pixel que se mueve con el sonido, las perillas y las notas (ver la tabla del README). Sustituye el campo con estelas de la 1.3: casi la misma flash y 26 KB menos de RAM.
- **Pulido:** nombres de perillas sin abreviaturas raras (BODY, DOWN, UP, KEY TRK, RING MIX, VALUE, REVERSE), mensajes que caben en el pie, comentarios del código al día, README reorganizado y este historial aparte; fuera los archivos y workflows de prueba que sobraban.
- El sonido de todas las pruebas es idéntico bit a bit al de la 1.2. Las sesiones de la 1.1, 1.2 y 1.3 se cargan.

## v1.3: el orden del trabajo y pantallas en movimiento

**Las páginas siguen el flujo de un SFX de juego**, un botón por paso:

| Paso | Botón | Páginas |
| --- | --- | --- |
| Empezar y crear la fuente | **HOME** | 01 ASSETS · 02 OSC & SAMPLE · 03 CHIP OSC |
| Material | **ARP** | 04 GRAIN ENGINE · 05 SAMPLE LAB |
| Dar forma | **ENV** | 06 ENVELOPES · 07 TRANSIENT |
| Color | **EDIT** | 08 RESONATOR · 09 DESTROY · 10 VOWEL + SHIFT · 11 RES MOD FILTER |
| Pegar | **GLO** | 12 OTT MULTIBAND |
| Colocar | **FX** | 13 DELAY + FLANGER · 14 MULTIBAND REVERB · 15 SPACE + MOTION |
| Mover (lo que el juego controla) | **LFO** | 16 LFO 1 + 2 · 17 RTPC · 18 CHAOS PAD + KEYS |
| Variar y entregar | **SEQ** | 19 VARIATION + MORPH · 20 PHYSICS TRIGGER · 21 BATCH + FIRE TEST |

- Al encender estás en **01 ASSETS**: escoges un punto de partida y de ahí avanzas botón por botón.
- El RESONATOR pasó de HOME a EDIT (colorea, no genera). LFO y SEQ cambiaron de lugar: primero se mueve el sonido y al
  final se varía y se entrega. BATCH es la última página: la entrega.
- La cadena de audio no cambió de orden y el sonido es idéntico (todas las pruebas de audio dan el mismo resultado bit a
  bit que la 1.2). El HAPPY ACCIDENT da los mismos resultados que antes. Las sesiones de la 1.1 y la 1.2 se cargan.

**Pantallas como juguetes de vectores.** Cada página tiene un objeto propio dibujado en líneas finas de un pixel, en
los colores del tema, que se mueve con lo que suena, con las perillas y con las notas; en silencio sigue respirando
despacio. La salida se analiza en cada cuadro (FFT de 256 puntos, 24 bandas de 60 Hz a 11 kHz, el nivel, los ataques y
el último segundo de ambos). El objeto se dibuja primero y las palabras de la página encima. Con HAPPY ACCIDENT todo
gira más rápido un momento. Las referencias se tomaron como inspiración (pantallas del OP-1, plugins de Kentaro,
HUDs, geometría de alambre, láminas científicas), no como copia.

| Página | Objeto |
| --- | --- |
| ASSETS | un espectrómetro pequeño a la derecha de la línea de estado |
| OSC & SAMPLE | A/B: el ciclo del oscilador enrollado en un anillo que gira; el sample como su contorno. D: un anillo de trazos por voz, girando más rápido cuanto más desafinan |
| CHIP OSC | el sonido en bloques de 8 bits; en reposo camina una onda cuadrada |
| GRAIN ENGINE | cada grano un aro que nace donde lee, sube y se encoge (SIZE su tamaño; reversa en blanco); las orillas del SPRAY punteadas |
| SAMPLE LAB | el slot enrollado en un anillo que gira, la herramienta como signo al centro (<<, [ ], cuñas, resorte, flechas, flecha de regreso, el monolito); lo que la herramienta toca, resaltado en el anillo; dónde suena |
| ENVELOPES | el ADSR con nodos; la etapa donde está la voz, brillante; la voz como un aro que crece con la envolvente |
| TRANSIENT | cada golpe una onda desde el ataque; la ganancia como una aguja |
| RESONATOR | el cuerpo golpeado, visto desde arriba en ángulo: un aro por modo, apilados como el material (METAL placas hexagonales, GLASS una copa, WOOD tablas, STRING cuerdas, BELL una campana, DRUM un tambor, TUBE un tubo); cada aro tiembla mientras su modo suena |
| DESTROY | una esfera de alambre pasando por el shaper: HARD la cuadra, FOLD la dobla, RECTIFY la vuelve domo; BITS ajusta sus puntos a una retícula, RATE los sostiene; se hincha con el sonido |
| VOWEL + SHIFT | una topografía del espectro (5 contornos, el más nuevo afuera); SHIFT la gira |
| RES MOD FILTER | el último segundo del espectro como familia de curvas que se aleja, bajo la respuesta del filtro |
| OTT | sólidos de alambre por banda (cilindro, prisma, pirámide); una flecha: sube la ganancia que da, baja la que quita |
| DELAY + FLANGER | los ecos como losas hacia un punto de fuga (FDBK cuántas, TIME cuánto giran) |
| REVERB | la sala como túnel: SIZE separa sus marcos, DECAY cuánto siguen encendidos; el techo es el decay de HIGH y el piso el de LOW; el sonido entra marco por marco |
| SPACE | frentes de onda desde la fuente; al pasar se juntan delante (Doppler) |
| LFO 1 + 2 | un armonógrafo con las últimas 32 posiciones de la pluma; la forma de cada LFO, chica, junto a su destino |
| CHAOS PAD | el campo por sus esquinas, el rastro del dedo; en SPRING, la línea que lo regresa al centro |
| VARIATION | A: la semilla como constelación. B: A es un cubo, B un octaedro, y entre los dos viaja la mezcla, transformándose según MORPH |
| PHYSICS | cada golpe un tallo con cabeza; al tocar, una pelota salta de golpe en golpe |
| BATCH | cada golpe nuevo, una onda desde su barra |

- **El valor protagonista** (idea del lenguaje del OP-1, con formas propias): al girar una perilla, su valor ocupa la
  pantalla 1.2 s, grande, con numerales de trazo fino dibujados, en el color de esa perilla (naranja si tiene lock).
- Costo: la pantalla se redibuja a 25 cuadros por segundo mientras algo suena y a 15 en reposo, fuera del audio. Las
  líneas nítidas cuestan menos que el campo con estelas de antes: el firmware ocupa casi lo mismo de flash (+0.6 KB) y
  usa 26 KB menos de RAM.

## v1.2: 15 ASSETS, puntos de partida para sound design

Una página nueva, la primera del botón **SEQ**: 18 sonidos base de videojuego listos para tocar, cada uno armado con los
módulos que ya tiene el ND-07 (no son samples: son patches, y todo se puede editar).

| Familia | Assets |
| --- | --- |
| Impactos | **EXPLODE** (ruido que se cierra, un cuerpo grave del resonador y un cuarto), **IMPACT** (metal golpeado, el tono cayendo), **HIT** (un puñetazo: seno que cae rápido, saturado), **DEBRIS** (escombros: PHYSICS DEBRIS sobre golpes de madera, cada uno una variación) |
| Armas | **GUNSHOT** (chasquido de ruido brillante, recortado y comprimido, con rebote), **LASER** (sierra que cae dos octavas, con flanger y eco), **ZAP** (descarga: ruido plegado, bitcrush, ring mod, un pasabanda que salta) |
| Movimiento | **WHOOSH** (ruido que crece por un pasabanda que abre y cierra), **RISER** (subida SHEPARD sin fin, el filtro abriéndose) |
| UI | **UI OK** (positivo: sube una quinta), **UI ERR** (negativo: grave, con zumbido, baja un poco), **UI TICK** (seno agudo y corto), **PICKUP** (la moneda: pulso de 25 % que salta una cuarta) |
| Juego | **POWERUP** (arpegio mayor de 8 bits que sube), **MAGIC** (FM metálico a tres voces, resonador de vidrio, ecos largos), **MONSTER** (voz grave con VOWEL que se mueve y GROWL), **ALARM** (sirena de dos tonos mientras sostienes la tecla), **ENGINE** (motor: con RTPC, INTENS lo acelera) |

- **Cómo:** (en la 1.3: **HOME** → 01 ASSETS.) **K1** escoge (la cuadrícula marca cuál), **SEL** lo carga y lo toca. Arriba dice
  LOADED, o **EDITED** en naranja cuando ya le moviste algo. Las teclas lo tocan en cualquier nota.
- **Cargar un asset** pone todo en su valor de fábrica y luego los ajustes del asset; no toca tus samples, la
  SAMPLE LAB, el BATCH, el CHAOS PAD ni el tema. Se puede deshacer con **UNDO**.
- **De ahí a variaciones:** cada asset trae su **VARY** (K2) y su familia de **HAPPY ACCIDENT** (K3 HA KIND: IMPACT,
  WEAPON, WHOOSH, UI, MAGIC...): el HAPPY ACCIDENT muta sin salirse del tipo de sonido. Luego **SEQ ×3 → BATCH** con
  MATCH saca 8–16 variaciones grabadas a un nivel parejo. **K4 OUT** es su volumen.
- **Niveles:** los 18 suenan más o menos igual de fuerte (los 300 ms más fuertes cerca de −15 dBFS, picos debajo de
  −1.5 dB), comprobado en `tests/assets_test.c`, que además los deja como WAV en `build/host/assets/`.
- Las páginas de SEQ y LFO se recorren un número (15 VARIATION ahora es 16, y así hasta 21 CHAOS PAD); **SEQ** llega
  primero a ASSETS y una vez más a VARIATION. La cadena de la cabecera tiene un trazo más.
- Corregido: el sample podía leer 1 muestra después de su final al dar la vuelta un loop (sin efecto audible, pero
  fuera del arreglo). El audio de todo lo demás no cambió (mismo resultado bit a bit).

La sesión de la 1.1 sí se carga (el asset empieza en EXPLODE, sin cargar).

## v1.1: tema EVA-01 (Acid Eva-01)

Un tema nuevo, el quinto, y el de arranque en una sesión nueva: la gráfica "acid tech" con la paleta de **01 TEST**.
**Morado** = estructura (el número de página, las líneas, los mensajes), **lima** = lo que tocas, **verde** = la señal,
**naranja** = lock o aviso. Bloques planos, marcas de corte en las esquinas del display, sin marcos.

- **Cabecera:** el número de página en un bloque morado (lima durante el HAPPY ACCIDENT), el nombre en grande, y debajo
  la cadena (un trazo por módulo; lima = la página actual) y los juegos A B C D.
- **Perillas:** cuatro celdas con su color (K1 lima, K2 verde, K3 morado, K4 hueso). La perilla que acabas de girar
  **llena su celda** 1.2 s, negro sobre color. Arriba: la perilla (arco de puntos; una lista: sus posiciones) y una barra
  (naranja = lock; verde = LFO, morado = RTPC / MOD ENV, hueso = pad o aftertouch). Los números van grandes con la
  unidad chica; las palabras en un corte medio. Una celda vacía va rayada.
- **Pie:** SEL en lima con lo que hace en esa página; los mensajes (LOCKED, HAPPY ACCIDENT, ...) en una barra morada.
- **Texto:** tres fuentes nuevas de Inter (SIL OFL, `assets/fonts/`), antialiasing de 4 bits, generadas con
  `tools/gen_eva_font.py`. `cv_text` ahora mezcla los bordes con lo que hay debajo: el texto se ve limpio también sobre
  bloques de color. Solo mayúsculas, cifras y signos (unos 21 KB de flash); los textos del display siguen en Terminus.
- **Que todo quepa:** `tests/eva_fit_test.c` recorre cada parámetro en cada valor (más de 14 000) y cada nombre: nada
  se corta. En los otros temas tampoco se cortan ya los nombres largos (TARGET 1, 10 MOD SRC, CHIP PLS...).
- Detalles: el SHIFT arriba de 1 kHz se muestra como `+1.25k`; KEYS = VARIANT (antes VARIATNS); la página 18 se llama
  LFO 1 + 2. La fuente de 32 px (24 KB) se cambió por la de 16 px dibujada al doble: el firmware no creció por las
  fuentes nuevas.

El audio no cambió (mismo resultado bit a bit). La sesión de la 1.0 sí se carga, con su tema: **SEL + OCT+** hasta
que diga EVA-01.

## v1.0

**09 VOWEL + SHIFT** (botón **EDIT**, segunda página; en la cadena después de DESTROY y antes del filtro):

| Juego | Perillas | Qué hace |
| --- | --- | --- |
| A | VOWEL, SIZE, RES, V MIX | Filtro de formantes: tres band-pass en los formantes de **U O A E I**, con morph continuo entre ellas (VOWEL acepta LFO, MOD ENV, RTPC y el pad: una boca que habla). **SIZE** mueve los formantes ±12 st (abajo = criatura más grande). **RES** = qué tan estrechos. La pantalla dibuja la respuesta real. |
| B | SHIFT, FDBK, S TIME, S MIX | **Frequency shifter**: mueve todas las frecuencias los mismos Hz (0.2 Hz a 2 kHz, ±; fino cerca de 0). Suena metálico, inarmónico, robótico. Con **FDBK** y **S TIME** (1–90 ms) la salida vuelve a entrar: la espiral que sube o baja sin fin (barber pole), sci-fi, energía inestable. La pantalla muestra los parciales de la última nota (gris) y a dónde van (color), y los ecos del feedback. |
| C | GROWL, G RATE | Un trémolo áspero e irregular (15–120 Hz) antes de los formantes: garganta, gruñido, motor. |

Apagado (V MIX, S MIX y GROWL en 0) no cuesta nada y el sonido es idéntico. Corre en flash, como la fuente; su costo
real se ve en el CPU del pie de pantalla.

**Del FM-1 a la computadora:** en `web/index.html`, pestaña SAMPLES, **⇩ DESCARGAR EL SLOT (.wav)** baja el slot
activo tal como suena (una grabación, un BOUNCE, lo que hiciste en SAMPLE LAB o un sonido de fábrica): 16-bit, mono,
22.05 kHz, con nombre y fecha (`ND07_USER_3_20261117_134500.wav`). Sirve para bocetos y para pasar material a REAPER;
para el asset final graba la salida USB a 44.1 kHz. Por SysEx: `LEN` (6) y `READ` (7) (ver la tabla de MIDI).

La sesión de la 0.9 no se carga (20 páginas, parámetros nuevos).

## v0.9: herramientas creativas

| Herramienta | Dónde | Qué hace |
| --- | --- | --- |
| **MOD ENV** | 06 ENVELOPES, juego C | Una segunda envolvente de un disparo por nota: sube en M ATK, baja en M DEC (curva), y mueve **M AMT** (±) de cualquier parámetro (**M TGT**). La caída de pitch de un impacto, el barrido de un láser, un filtro que se abre. La pantalla dibuja la curva y el valor en vivo. |
| **PHYSICS TRIGGER** | 16 (SEQ ×3) | Una tecla toca un patrón de golpes: **BOUNCE** (rebotes cada vez más juntos y más débiles), **DEBRIS** (muchos al principio, luego sueltos, cada uno a su nivel), **ROLL** (parejo e inquieto), **STUTTER** (ametralladora), **SCATTER** (al azar mientras sostienes la tecla: gotas, chispas, ambientes). HITS, TIME, FALL (cuánto baja el nivel), PITCH (cuánto cambia la afinación hacia el final), JITTER. El patrón sale de la SEED: misma semilla, mismo patrón. Con VARY, cada golpe es una variación. SEL lo toca con la última nota. |
| **SAMPLE LAB** | 05 (ARP ×2) | Trabaja el slot activo en su sitio, sin gastar CPU al tocar: REVERSE, CROP (a la región del loop de 01 C), FADE IN / FADEOUT (sobre AMOUNT), 1/2 SPD (una octava abajo, el doble de largo, hasta 1.48 s), 2x SPD, MAX LVL (normalizar) y REVERT (el slot como está guardado). SEL aplica; SAVE lo guarda en flash. Con BOUNCE primero, cualquier sonido del motor se vuelve material. Pre-verb: REVERSE, BOUNCE con reverb, REVERSE. |
| **HA KIND** (recetas) | 14, juego B | El HAPPY ACCIDENT (botón REC) dentro de una receta: FREE (como siempre), IMPACT, WHOOSH, UI, MAGIC, MONSTER, WEAPON, AMBIENT. Cada toque da un sonido nuevo de ese tipo; respeta los locks y HA AMT; no cambia de slot. |
| **GESTURE** | 19, juego B · SEL | Graba un movimiento del Chaos Pad (hasta 4 s) y cada nota lo repite: 1-SHOT o LOOP. Se arma con SEL en la página 19 o con **● GESTO** en la página del teléfono; se guarda con la sesión. |

**Teléfono como Chaos Pad:** `web/index.html` ahora tiene el botón ● GESTO, pantalla completa y mantiene la pantalla
encendida. Ver "Conectar el teléfono".

Eran 19 páginas en la 0.9 (en la 1.0 son 20: desde VOWEL + SHIFT, las siguientes suben un número); la sesión de la 0.8 no se carga. *(Las secciones de versiones anteriores, abajo, citan los números
de página de su momento; la tabla de módulos tiene los actuales.)*

## v0.8: interfaz rediseñada y correcciones

**La pantalla, de arriba a abajo:**

- **Mapa de la cadena** (arriba, en lugar del rótulo "// MONOLITH"): una marca por módulo, agrupadas por botón. Alta y
  de color = el módulo está actuando sobre el sonido; subrayada = la página en la que estás. De un vistazo sabes qué
  está encendido sin recorrer páginas.
- **Página:** el número en una pastilla, el nombre y los juegos de perillas (A B C D; sólo si hay más de uno).
- **Perillas según lo que son:** las listas se dibujan como sus posiciones en fila (las de dos opciones, como un
  interruptor; en OFF / BYPASS quedan apagadas); las ± se llenan desde el centro; el resto, desde la izquierda. El valor
  va en blanco y el nombre en gris. **Naranja con candado** = con lock. **Punto + valor en color** = algo la está
  modulando (LFO, RTPC, pad o aftertouch) y el número es su valor en ese instante.
- **Pie:** qué hace **SEL** en esta página (REC ARM, NEW SFX, PLAY PASS, NEW SEED, BATCH / FIRE / STOP, SWEEP,
  RANDOMIZE), candado + número de locks, la octava si no es 0, flecha + pasos de UNDO, un cuadro naranja si el sample
  grabado no está en flash, el CPU (en el aparato) y un **medidor de salida** (−48..0 dBFS, los últimos 3 dB en
  naranja, con retención de pico de 1 s).

**Orden:**

- **MORPH** ya no es una página con una sola perilla: está en **13 VARIATION + MORPH, juego B** (MORPH, HA AMT, EVOLVE),
  junto a BATCH, que es quien usa A y B. **SEL + OCT− / OCT+** guardan A / B en ese juego. Son 17 páginas.
- **LAYER** pasó de la envolvente a **01 OSC & SAMPLE, juego D** (es parte de la fuente), con su pantalla: cada capa,
  su afinación y su nivel.
- **Lista de destinos** (LFO, RTPC, pad, aftertouch): sólo lo que el sonido oye. Fuera 18 entradas que no hacían nada
  (semilla, modos de batch, REC, MATCH…).
- Los avisos "SEL: …" y los textos repetidos salieron de las pantallas (están en el pie); el CHAOS PAD ya no encima
  sus textos; SPACE dibuja anillos de distancia y muestra nivel, low-pass real y reverb; BATCH muestra los huecos de
  los golpes por venir.

**Corregido:**

- Al terminar un BATCH con RESEED, el aviso "NEW SEED" se perdía.
- Teclas = variaciones: soltar una tecla cortaba la nota aunque otra siguiera presionada.
- **BOUNCE** ahora deja OUT de modo que el sample suene al nivel con el que se grabó (hasta donde alcanza OUT 100 %).
- Tras un OVERDUB los puntos del loop se vuelven a buscar en los cruces por cero del sample nuevo.
- SPACE al encenderse ya no parte de un estado viejo del filtro (un clic posible).
- La pantalla ya no se redibuja 20 veces por segundo en reposo en 7 de las páginas; sí mientras haya sonido (también
  en las colas de reverb).
- La sesión de la 0.7 no se carga (cambió la lista de destinos).

## v0.7: un pilar para assets de juego

Tomado como inspiración de la versión de REAPER, pensado para lo que un motor de juego hace con un sonido.
**Las páginas cambiaron de número** (agrupadas por botón; tabla abajo, ya con el orden de la 0.8). La sesión de la 0.6 no se carga.

| Qué | Dónde | Para qué |
| --- | --- | --- |
| **Teclas = variaciones** | 13 VARIATION · KEYS: VARIANT | Cada tecla dispara la variación *n* de la última nota: misma semilla, mismo resultado, siempre. SEL: semilla nueva |
| **Semilla, HA AMT, EVOLVE** | 13 VARIATION | VARY dosifica el azar por golpe; HA AMT cuánto mueve el HAPPY ACCIDENT; EVOLVE empuja 3 parámetros a cada nota (pasos con UNDO) |
| **LFO HIT RND / AMP ENV** | 15 LFO · SHAPE | Un valor al azar por golpe (o por variación) y la envolvente como fuente de modulación |
| **FIRE TEST** | 14 BATCH, juego B · SEL | Dispara a RATE (30..1200 rpm) desde un pool de variaciones: RANDOM, NO REPT o SHUFFLE, como un random container |
| **Medidor + MATCH** | 14 BATCH, juego C | Pico de cada golpe en dBFS, SPREAD entre el más fuerte y el más débil, la media; con MATCH ON, al terminar el BATCH ajusta OUT para que la media caiga en TARGET |
| **START > LOOP > STOP** | 01, juego C · LOOP: SUSTAIN | Mientras sostienes, el loop (L POS, L LEN, XFADE, en cruces por cero); al soltar sigue hasta el final del sample |
| **REC: OVERDUB / BOUNCE** | 01, juego B · REC | OVERDUB suma la toma nueva al slot (mismo nivel, normalizado). BOUNCE graba el sonido al slot y deja la cadena seca (SAMPLE, efectos en 0, con UNDO) para seguir diseñando encima |
| **LAYER 2 / 3** | 01, juego D | Capas de la misma fuente: L PITCH (p. ej. −12 st, cuerpo), DETUNE (una doble desafinada), L LEVEL |
| **SHEPARD** | 01 · WAVE 15 | Seis senos en octavas bajo una campana: subida (COLOR > 50 %) o bajada (< 50 %) sin fin. Cargas, alarmas, tensión |
| **RTPC** | 16 RTPC | Un valor 0..100 % (INTENS) por una curva (LINEAR, EXP, LOG, S-CURVE) hacia 3 destinos con su cantidad. SEL: barrido 0 → 100 % en SWEEP con la nota sostenida (motor acelerando, algo cargándose) |
| **SPACE + MOTION** | 12 SPACE | DIST (nivel, absorción del aire, más reverb), OCCLUDE (una pared: apagado y −12 dB), PASS-BY (cada nota pasa de lado a lado en ese tiempo) con DOPPLER en semitonos. SEL: tocar una pasada |

## v0.6: 14 CHIP OSC

Sonido de consola de 8 bits para SFX retro (botón **HOME**, segunda página: 02 desde la 0.7).

- **Tres ondas nuevas** en WAVE (o con ALGORITHM): **CHIP PLS** (el pulso del NES con sus 4 anchos: **DUTY** 12.5 /
  25 / 50 / 75 %), **CHIP TRI** (el triángulo de 32 escalones de 4 bits) y **CHIP NSE** (ruido de registro LFSR de 15
  bits, como el del NES; **NOISE** LONG o SHORT 93, el ruido "metálico" periódico; COLOR = qué tan rápido corre).
- **El cuadro de 60 Hz** (sobre cualquier fuente, también samples y granos): en una consola el driver de sonido
  cambiaba todo una vez por cuadro de video, y eso es la mitad del sonido de 8 bits.
  - **SWEEP** ±: el pitch sube o baja cada cuadro (saltos, láseres, caídas). Fino cerca de 0, hasta 2/3 de octava por cuadro.
  - **ARP**: OCT, 5TH, 4TH, MAJOR, MINOR, TRILL, OCT DN (en ciclo) y UP 4TH / UP 5TH / UP OCT (suben una vez y se
    quedan: la moneda). **ARP SPD** = cuadros por paso (1 = 60 Hz).
  - **STEPS**: el volumen en 16 niveles que cambian a 60 / 30 / 15 Hz, como el volumen de 4 bits del NES.
- **PRESET + SEL**: elige el tipo (**COIN, JUMP, LASER, HIT, POWERUP, EXPLODE, BLIP**) y cada toque de **SEL** hace
  un sonido nuevo de ese tipo, con valores al azar dentro de su receta, como sfxr. Toca solo la fuente, el envelope,
  TUNE y esta página; respeta los locks; el resto de la cadena (resonador, DESTROY, OTT, reverb) sigue sumándose.
  Combínalo con **VARY + BATCH** para sacar 8–16 variaciones de una moneda en una sola toma.
- La pantalla dibuja el pitch de los primeros 0.8 s (SWEEP + ARP) tal como va a sonar.
- Con SWEEP 0, ARP OFF y STEPS OFF no cuesta nada: el resultado es idéntico al de antes.

**Corregido:** el "audition" (la nota corta de C4 que suena tras HAPPY ACCIDENT o al aleatorizar un módulo) no sonaba:
la nota se soltaba antes del ataque. Ahora dura 150 ms.

## v0.5

- **13 CHAOS PAD + KEYS** (botón LFO, tercera página): un pad XY como el Kaoss Pad de Korg, desde el teléfono o la
  computadora. **X TGT / Y TGT** = cualquier parámetro (la misma lista que los LFOs), **X AMT / Y AMT** = cuánto y hacia
  dónde (±). **PAD**: SPRING (al soltar vuelve a como estaba), LATCH (se queda), PLAY (tocar el pad dispara la última
  nota: el pad como instrumento). Por MIDI: **CC16 = X, CC17 = Y, CC18 = tocar/soltar**. La pantalla muestra el pad, el
  dedo y a dónde va cada eje. SEL en esta página: destinos al azar.
- **Aftertouch como fuente de modulación:** **AT TGT / AT AMT**. Llega por MIDI (channel pressure y poly pressure) o desde
  las teclas del FM-1 (abajo).
- **Velocity desde las teclas del FM-1 (beta, OFF de fábrica):** **KEY VEL** = OFF / VEL / VEL+AT, **SENS** = cuánta fuerza
  hace falta para 127. El FM-1 no trae velocity: sus teclas son domos con una pastilla de carbón, y el chip puede medir
  la resistencia de 20 de las 27 teclas (las filas PA7 y PA8; las 7 de la fila de arriba no tienen convertidor y suenan
  con velocity fija). Es el método de [Velo / MONK-1](https://github.com/charlesvestal/fm1-monk) (Charles Vestal),
  simplificado. Con KEY VEL encendido cada nota espera unos 6 ms a su medición. **Sin probar en hardware:** si algo raro
  pasa con las teclas o los LEDs, apágalo.
- **Página web `web/index.html`** (Web MIDI, sin instalar nada):
  - **CHAOS PAD:** el pad XY a pantalla completa, una tira de presión (aftertouch) y un teclado de 13 notas con
    velocity según dónde toques la tecla.
  - **SAMPLES:** arrastra cualquier audio (WAV, MP3, AIFF, OGG, FLAC), se convierte a mono 22.05 kHz, se recorta,
    se normaliza, lo escuchas tal como sonará y se manda a uno de los 8 slots por SysEx (y se guarda en flash).
- Limpieza: fuera el osciloscopio y la tabla de saturación que ya no se usaban (~2 KB de flash y un poco de CPU en la interrupción de audio), sin avisos del
  compilador en la app.

## Tabla de páginas de la v0.6 (para comparar)



| # | Botón | Módulo | Perillas A | Perillas B |
| --- | --- | --- | --- | --- |
| 01 | HOME | OSC & SAMPLE | SOURCE (OSC / SAMPLE / MIX), WAVE (14 ondas), SAMPLE (slot 1..8), BLEND | TUNE, COLOR, STRETCH, LOOP |
| 02 | HOME ×2 | **RESONATOR** | MATERL, PITCH, DECAY, RES MIX | DAMP, SPREAD, HIT POS, KEYTRK |
| 03 | ENV | AMP ENVELOPE | ATTACK, DECAY, SUSTAIN, RELEASE | GLIDE, VELO, PITCH, OUT |
| 04 | ENV ×2 | **TRANSIENT** | ATTACK, SUSTAIN, TIME, OUT | — |
| 05 | EDIT | RES MOD FILTER | FREQ, RES, MODE, SLOPE | DEPTH, RATE, MOD SRC, ANALOG |
| 06 | EDIT ×2 | **DESTROY** | DRIVE, SHAPE, BITS, RATE | RING HZ, RNG MIX, TONE, MIX |
| 07 | ARP | GRAIN ENGINE (sobre el sample) | POS, SIZE, DENS, SPRAY | REV %, GRAIN, FREEZE, RND PIT |
| 08 | SEQ | OTT MULTIBAND (A B C D) | A: DEPTH, TIME, UPWD, DOWNWD · B: IN, OUT, STYLE, PLACE | C: LOW, MID, HIGH, THRESH · D: X-LO, X-HI, SOLO |
| 09 | FX | DELAY + FLANGER | TIME, FDBK, DLY MIX, ROUTE | RATE, DEPTH, REGEN, FLG MIX |
| 10 | GLO | MULTIBAND REVERB | SIZE, DECAY, PRE-DLY, REV MIX | LOW x, HIGH x, X-LO, X-HI |
| 11 | LFO | LFO x2 | SHAPE, RATE, MIX, TARGET (LFO 1) | SHAPE, RATE, MIX, TARGET (LFO 2) |
| 12 | LFO ×2 | **MORPH + BATCH** | MORPH, VARY, BATCH (modo), COUNT | SPACE, GATE |
| 13 | LFO ×3 | **CHAOS PAD + KEYS** (A B C) | A: X TGT, X AMT, Y TGT, Y AMT | B: PAD, KEY VEL, SENS · C: AT TGT, AT AMT |
| 14 | HOME ×3 | **CHIP OSC** | DUTY, SWEEP, ARP, ARP SPD | STEPS, NOISE, PRESET |


## v0.4

*(Los números de página de las secciones de v0.4 y v0.5 son los de entonces; la tabla de arriba tiene los actuales.)*

- **02 RESONATOR:** un banco de 6 resonadores (modos) que convierte cualquier fuente en un objeto golpeado. **MATERL:**
  METAL (barra libre), GLASS, WOOD, STRING (armónico), BELL (hum, prime, tercera menor, quinta, nominal), DRUM
  (membrana circular), TUBE (armónicos impares). PITCH ±24 semitonos; con **KEYTRK** ON sigue la tecla, OFF queda en
  C3. DECAY 20 ms–8 s; **DAMP** apaga más rápido los modos altos (madera = mucho, vidrio = poco); **SPREAD** abre o
  cierra la distancia entre modos (50 % = el material real); **HIT POS** es dónde se golpea (cerca del borde = brillante,
  al centro = los modos pares casi se apagan). La mejor fuente es algo corto con ruido: NOISE con un decay corto, ZAP, GLITCH o tus
  grabaciones. La pantalla muestra dónde caen los modos y cuánto suenan.
- **04 TRANSIENT:** transient shaper de tres seguidores de envolvente. **ATTACK** ±100 sube o corta el golpe (−100 lo
  vuelve un swell); **SUSTAIN** ±100 alarga o seca la cola; **TIME** escala los tiempos (0.1x–10x); OUT compensa.
  Debajo de ~−70 dB no actúa: no levanta el ruido de fondo.
- **06 DESTROY:** DRIVE hasta +36 dB hacia SOFT (tanh), HARD (clip), FOLD (wavefolder) o RECTIFY (rectificado de onda
  completa: sube una octava); **BITS** 16→2; **RATE** reduce el sample rate (44.1 kHz → 223 Hz, con aliasing, a
  propósito); **RING HZ / RNG MIX** ring mod 20 Hz–4.7 kHz; **TONE** low-pass de 12 dB; **MIX** dry/wet. Radios,
  robots, UI retro, glitches, armas láser.
- **12 MORPH + BATCH:**
  - **MORPH A↔B:** guarda dos patches y muévete entre ellos. En la página 12: **SEL + OCT-** guarda **A**, **SEL + OCT+**
    guarda **B**. Gira **MORPH**: cada parámetro continuo viaja de A a B (los de lista cambian al 50 %), y **los
    parámetros con lock no se mueven**. MORPH acepta LFO (un saw lento = una carga de energía) y CC11 por MIDI. Al
    editar otra perilla, el panel queda en la mezcla actual y el morph se suelta (la cabecera dice MORPH mientras está
    activo). Flujo rápido: guarda A, ☢ HAPPY ACCIDENT, guarda B, gira MORPH.
  - **VARY:** cada nota sale un poco distinta: pitch, filtro, decay, el resonador, el drive, el volumen, la posición de
    los granos y hasta 1 % del inicio del sample. Es el round-robin de Wwise/FMOD hecho en el sonido. 0 % = apagado.
  - **BATCH** (toca **SEL** en la página 12): toca COUNT golpes (4, 8, 12, 16) con la última nota que tocaste. **SPACE
    AUTO** espera a que la cola baje de −60 dB antes del siguiente golpe (las colas largas no se enciman), o fija
    0.5–7.5 s; **GATE** es cuánto se sostiene cada nota. Modos: **RANDOM** (cada golpe con su VARY), **A>B** (los golpes
    van de A a B en pasos iguales, sin azar: de chico a grande, de limpio a destruido) y **A>B+RND** (los dos). Graba
    una sola toma en REAPER por USB y córtala por silencios (Dynamic Split). SEL otra vez lo detiene.

## Antes de la v0.4

- **Osciladores:** SINE, TRI, SAW, SQUARE, PULSE, SUPRSAW, NOISE, SUB-SAW, FORMANT, METALFM, PADSCAN (antialias
  PolyBLEP). **COLOR** cambia según la onda: ancho de pulso, detune del supersaw, brillo del ruido, morph de los
  wavetables, o qué parte del sample recorre PADSCAN.
- **Samples:** 8 slots con sonidos de fábrica (BOOM, METAL, ZAP, GLITCH, WHOOSH, VOX AH, BELL, DRONE), afinados en C4.
  **Grabar en un slot:** en la página 01, toca **SEL** (REC ARMED), toca una nota: graba la salida del FM-1 (todo
  el procesamiento, reverb incluida) hasta 1.48 s o hasta 0.6 s de silencio. **SAVE** lo guarda en flash.
  **SAMPLE LAB → TOOL FACTORY** regresa el slot a su sonido de fábrica.
- **OTT:** el algoritmo del preset OTT de Ableton (el que copia el OTT de Xfer): 3 bandas Linkwitz-Riley de 24 dB/oct en
  88.3 Hz y 2.5 kHz, detección RMS, compresión hacia abajo (66.7:1 / ∞:1) y hacia arriba (1:4.17) por banda. DEPTH,
  TIME, UPWD, DOWNWD, IN, OUT, LOW/MID/HIGH, THRESH, SOLO, STYLE (OTT, SMOOTH, PUNCH, AGGR) y PLACE (PRE FX / POST FX).
- **Delay + Flanger** (4 rutas), **reverb multibanda** (LOW / MID / HIGH con su propio decay) y **2 LFOs** asignables
  a cualquier parámetro (ahora también a los módulos nuevos y a MORPH).

## Optimizaciones de CPU (v0.4)


- **Motor por bloques y en RAM:** la cadena procesa bloques de 64 muestras, módulo por módulo (cada uno revisa sus
  interruptores una vez por bloque y mantiene su estado en registros), y las etapas pesadas (resonador, transient,
  DESTROY, filtro, OTT, delay, flanger, reverb y salida) corren desde la RAM (`.ram_text`, ~12 KB) en vez de leerse de
  la flash por el caché XIP. No hacen ninguna llamada, como exige `tools/build.py`. Los osciladores se quedan en flash
  por tamaño. El resultado es idéntico bit a bit al motor muestra por muestra (patches sin azar). Ganancia medida en
  el host: −12 %; la de la RAM solo se ve en el aparato.
- **Todo el motor es mono:** −9 a −17 % según el patch.
- **Las colas ya no cargan la cadena de la voz:** ~90 ms después de que la voz queda en silencio, resonador, transient,
  DESTROY, filtro y OTT PRE dejan de calcularse; solo siguen delay y reverb. Un one-shot con su cola de reverb bajó de
  ~1210 a **~565** (−53 %), que es el caso típico de SFX.
- **Reposo:** tras ~1.1 s de silencio total, el motor entero se duerme hasta la siguiente nota.
- Los coeficientes de cada módulo solo se recalculan cuando se mueve una de sus perillas (o su LFO).
- Reverb: retroalimentación en 32 bits y modulación cada 8 pasos; limitador: una división cada 4 muestras.
- **Corregido** (también estaba en la v0.3): al quedar en silencio, el filtro y los crossovers del OTT se quedaban con un
  DC de unos pocos LSB que el OTT subía ~20 dB y mandaba al delay y la reverb. Ahora se sueltan.

Con todo encendido a la vez (MIX + supersaw + sample estirado + OTT POST + delay + flanger + reverb + 2 LFOs + los tres
módulos nuevos) son ~1790 (la v0.3, con menos módulos, gastaba ~1920 en su peor caso). El pie de pantalla muestra el CPU real en el aparato; si pasa de ~90 % y
cruje, usa la tabla de arriba.
