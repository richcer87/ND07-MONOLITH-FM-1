# ND-07 // MONOLITH · FM-1 · v1.9.2

Firmware para el **M-VAVE FM-1**: un sampler y sintetizador para diseñar efectos de sonido de videojuegos. Es la
versión de bolsillo de la estación ND-07 // MONOLITH para REAPER, escrita en C entero para el chip JieLi AC79 (sin FPU).

19 páginas en el orden del trabajo: escoges el **ENGINE** (el motor de siempre o el chip de una consola: YM2149, YM2612,
SPC700, SCSP y DMG, con su propia aritmética), un **ASSET** de partida, armas la fuente (osciladores, samples, granos),
le das forma, color y espacio, la mueves como la movería el juego (LFOs), la varías y la entregas (**VARIATION**, **PHYSICS**, **BATCH**).
Además: HAPPY ACCIDENT, locks de parámetros, 16 pasos de UNDO y 5 temas (EVA-01 por defecto).

Construido sobre [Dinghy](https://github.com/zvenson/dinghy) (zvenson): USB, MIDI, actualizaciones, rescate USB,
pantalla y flash. Licencia GPL-3.0-only.

> **Estado: 1.9.2.** La 0.9 corre en un FM-1 "New" (con todos los módulos encendidos y el Chaos Pad, ~30 % de CPU); lo
> nuevo de la 1.0 a la 1.9.2 está probado en el simulador de host (audio WAV y capturas). El `.fwsc` se compila en
> GitHub Actions con la toolchain de JieLi. Instalar firmware es bajo tu propio riesgo: el
> [FM-1 Firmware Hub](https://thefirmwarehub.com) te regresa al firmware oficial, y mantener **OCT-** al encender entra
> al rescate USB. Las sesiones de la 1.1 en adelante se cargan.
>
> **Datos de otros firmwares:** ND-07 no lee lo que otro firmware personalizado haya guardado en la flash (sesiones,
> samples) y puede sobrescribirlo. Lo más limpio es instalarlo sobre el firmware oficial.

Lo que cambió en cada versión está en [CHANGELOG.md](CHANGELOG.md).

## Instalar y actualizar

**Lo más sencillo: la página del instalador** (Chrome o Edge, sin instalar nada):
`https://richcer87.github.io/ND07-MONOLITH-FM-1/install.html`

1. Conecta el FM-1 por USB y presiona **Conectar FM-1**.
2. La página ya trae **la última compilación de `main`**. También puedes arrastrar cualquier `.fwsc` de ND-07.
3. Presiona **Instalar**. Tarda alrededor de un minuto y el FM-1 se reinicia solo. Si algo se corta, vuelve a conectar
   y presiona Instalar otra vez: termina la escritura que quedó a medias.

El ciclo completo de una actualización: haces merge (o push) en `main` → GitHub Actions compila el firmware, corre las
pruebas y publica la página con ese `.fwsc` (unos 5 minutos) → abres la página → Instalar.

> **Una sola vez:** en GitHub, **Settings → Pages → Build and deployment → Source: GitHub Actions**. Hasta entonces la
> página no se publica (el firmware se sigue compilando igual y queda en Actions → **nd07-fwsc**).

**En Windows, con doble clic:** `tools/ND07-Update.cmd` (junto a `tools/fm1_install.py`). Descarga la última
compilación y la instala; si arrastras un `.fwsc` encima, instala ese. Necesita Python 3 (instala `mido` y
`python-rtmidi` la primera vez). A mano: `py tools\fm1_install.py nd07.fwsc`.

La página usa el protocolo de actualización de SLOOP (`web/fm1ota.js`, `web/fm1pkg.js`, Leo Kuroshita, GPL-3.0),
dentro de un solo archivo (`python3 tools/make_install_page.py` lo genera desde `web/install.src.html`), así que
también funciona abierta desde el disco. Solo acepta paquetes de ND-07 (`FM-1_4707N`) que traen el cargador de
actualización, y revisa los CRC del paquete antes de escribir.

**¿Y el FM-1 Firmware Hub?** Para publicar ND-07 para otros, el Hub es lo mejor: instala con un clic desde el
navegador, revisa el paquete y te deja volver al oficial. Pero no sirve para iterar: solo lista firmware probado en un
FM-1 real, lee el repositorio una vez por noche y ofrece la última *release* (las que crea `git tag vX.Y && git push
--tags`, con atestación de compilación). El envío empieza en `https://thefirmwarehub.com/submit/`; antes revisa
"Is my identity free?" para `FM-1_4707N`. `fmhub.json` ya tiene la ficha.

**Emulador:** [joakimeriksson.github.io/fm1-emu](https://joakimeriksson.github.io/fm1-emu/) abre un `.fwsc` sin el
aparato.

### La página web: Chaos Pad y samples

`web/index.html` (Web MIDI, sin instalar nada) tiene dos partes: **CHAOS PAD** (el pad XY a pantalla completa, una tira
de presión y un teclado de 13 notas con velocity según dónde tocas) y **SAMPLES** (arrastras cualquier audio, se
convierte a mono 22.05 kHz, lo escuchas tal como sonará y se manda a uno de los 8 slots por SysEx). El pad y la tira mueven
parámetros solo cuando la página CHAOS PAD está a la vista (ver el final de este archivo).

| | Cómo |
| --- | --- |
| **Android** (lo mejor) | Cable USB-C a USB-C (o adaptador OTG) del teléfono al FM-1. Abre la página en **Chrome**, CONECTAR, acepta el permiso MIDI. El pad XY, la tira de presión, el teclado y ● GESTO funcionan con el dedo; ⛶ = pantalla completa. |
| **Computadora** | Chrome o Edge (Windows, Mac, Linux), cable USB normal. |
| **iPhone / iPad** | Safari no tiene Web MIDI. Usa una app de MIDI con pad XY (TouchOSC, Lemur, MIDI Designer…) con un adaptador USB y mándale CC16, CC17 y CC18. |
| **Inalámbrico** | El FM-1 no tiene Bluetooth ni WiFi usables en este firmware. Desde la computadora puedes rebotar: teléfono → (WiFi, p. ej. TouchOSC) → computadora → USB → FM-1. |

La página necesita **https** o **localhost** (regla de los navegadores para MIDI). Lo más fácil: en tu repositorio,
Settings → Pages → "Deploy from a branch", `main`, carpeta `/ (root)`, y ábrela en
`https://TU_USUARIO.github.io/TU_REPO/web/`. También funciona abriéndola con `python3 -m http.server` en la computadora.

## El flujo de trabajo

Cada botón es un paso. Presiónalo otra vez para el siguiente juego de perillas (A B C D) y, después del último, para
la siguiente página del mismo botón. **SELECT** recorre las 19 páginas. Al encender estás en **01 ENGINE**: primero
escoges con qué se hace el sonido, luego el asset o el synth.

| Paso | Botón | Páginas |
| --- | --- | --- |
| Motor, punto de partida y fuente | **HOME** | 01 ENGINE · 02 ASSETS · 03 OSC & SAMPLE |
| Material | **ARP** | 04 GRAIN ENGINE · 05 SAMPLE LAB |
| Dar forma | **ENV** | 06 ENVELOPES (D: TRANSIENT) |
| Color | **EDIT** | 07 RESONATOR · 08 DESTROY · 09 3-BAND EQ · 10 RES MOD FILTER |
| Pegar | **GLO** | 11 OTT MULTIBAND |
| Colocar | **FX** | 12 DELAY + FLANGER · 13 MULTIBAND REVERB (C: SPACE) |
| Mover (lo que controla el juego) | **LFO** | 14 LFO 1 + 2 |
| Variar y entregar | **SEQ** | 15 VARIATION + MORPH · 16 PHYSICS TRIGGER · 17 BATCH + FIRE TEST |

| # | Página | Perillas A | Perillas B (C, D) |
| --- | --- | --- | --- |
| 01 | **ENGINE** | ENGINE y las tres primeras del motor | B: las otras cuatro del motor · C: PRINT, RATE, GRIT · D: SWEEP, ARP, ARP SPD, STEPS (con ND07 solo A y el driver, como B) |
| 02 | **ASSETS** | ASSET, VARY, HA KIND, OUT | B: PRESET (SEL: un NEW SFX de ese tipo con el motor) |
| 03 | OSC & SAMPLE | SOURCE, WAVE (15 ondas), SAMPLE, BLEND | B: TUNE, COLOR, STRETCH, REC · C: LOOP, L POS, L LEN, XFADE · D: VOICES, L PITCH, DETUNE, L LEVEL |
| 04 | GRAIN ENGINE | POS, SIZE, DENS, SPRAY | REVERSE, GRAIN, FREEZE, RND PIT |
| 05 | SAMPLE LAB | TOOL, AMOUNT | — |
| 06 | ENVELOPES | ATTACK, DECAY, SUSTAIN, RELEASE | B: GLIDE, VELO, PITCH, OUT · C: M ATK, M DEC, M AMT, M TGT · D (TRANSIENT): ATTACK, SUSTAIN, TIME, OUT |
| 07 | RESONATOR | BODY, PITCH, DECAY, RES MIX | DAMP, SPREAD, HIT POS, KEY TRK |
| 08 | DESTROY | DRIVE, SHAPE, BITS, RATE | RING HZ, RING MIX, TONE, MIX |
| 09 | **3-BAND EQ** | LOW, MID, HIGH (±15 dB), MID HZ | LOW HZ, HIGH HZ, MID Q, OUT |
| 10 | RES MOD FILTER | FREQ, RES, MODE, SLOPE | DEPTH, RATE, MOD SRC, ANALOG |
| 11 | OTT MULTIBAND | A: DEPTH, TIME, UP, DOWN · B: IN, OUT, STYLE, PLACE | C: LOW, MID, HIGH, THRESH · D: X-LO, X-HI, SOLO |
| 12 | DELAY + FLANGER | TIME, FDBK, DLY MIX, ROUTE | RATE, DEPTH, REGEN, FLG MIX |
| 13 | MULTIBAND REVERB | SIZE, DECAY, PRE-DLY, REV MIX | B: LOW x, HIGH x, X-LO, X-HI · C (SPACE): DIST, OCCLUDE, PASS-BY, DOPPLER |
| 14 | LFO 1 + 2 | SHAPE, RATE, MIX, TARGET (LFO 1) | SHAPE, RATE, MIX, TARGET (LFO 2) |
| 15 | VARIATION + MORPH | VARY, SEED, KEYS, RESEED | MORPH, HA AMT, EVOLVE, HA KIND |
| 16 | PHYSICS TRIGGER | TRIGGER, HITS, TIME, FALL | PITCH, JITTER |
| 17 | BATCH + FIRE TEST | BATCH, COUNT, SPACE, GATE | B: RATE, POOL, ORDER · C: MATCH, TARGET |

La cadena de audio (mono): ENGINE (la fuente de 03 o un chip) × ENV → RESONATOR → TRANSIENT → DESTROY → EQ → FILTER →
OTT (PRE) → DELAY + FLANGER → REVERB → OTT (POST) → PRINT → SPACE → limitador → salida.

## ENGINE (página 01)

Lo primero es el motor. Con **ENGINE** (K1 en A) la voz la hace el chip de una consola, con sus partes y sus trucos;
**ND07** es el motor de siempre (03 OSC & SAMPLE). Los ASSETS, el HAPPY ACCIDENT y las recetas suenan sobre el motor
que escogiste y no lo cambian; un asset de ruido (EXPLODE, HIT, DEBRIS...) le pide al chip su ruido. Cada motor trae
sus siete perillas (A y B; al cambiar de motor empiezan en un punto que ya suena), y 03 le sigue prestando el sample,
sus puntos de loop y el SUSTAIN. La envolvente de 06 sigue siendo el VCA, así que con SUSTAIN al 100 % oyes la
envolvente del chip tal cual.

Cada cosa del chip está en un solo lugar: el chip en K1, su consola arriba de la pantalla, sus perillas en A y B, su
DAC en C y el driver en D. Con **ND07** no hay chip: la página muestra solo A (con la onda o el sample de 03, tal
cual suena) y el driver, que pasa a ser B.

| Motor | Consola | Perillas | Qué hace |
| --- | --- | --- | --- |
| **YM2149** | Atari ST, MSX | MIXER, NOISE, CHORD, ENV, E RATE, SID, DIGI | Tres cuadradas, el ruido LFSR de 17 bits y una envolvente de 32 pasos (8 formas). MIXER: TONE, NOISE, TONE+NS, **BUZZER** (la envolvente a la frecuencia de la nota: el bajo del Atari) y **SYNCBUZ** (la cuadrada reinicia la envolvente). CHORD reparte los tres canales (unísono, octava, quinta, acordes). **SID**: el volumen se abre y cierra cerca del tono (el PWM de SID-voice). **DIGI**: el sample por el registro de volumen (los *digi-drums*). |
| **YM2612** | Mega Drive / Genesis | ALGO, FDBK, RATIO, MOD LVL, MOD DEC, CH3, SSG-EG | FM de 4 operadores con los 8 algoritmos y feedback en el operador 1. **CH3**: cada operador con su propio pitch, fuera de la serie armónica (campanas, metal, motores). **SSG-EG**: las envolventes de los moduladores en loop, normal o invertida. ALGO **DAC PCM**: el canal 6 tocando el sample a 8 bits. |
| **SPC700** | Super NES | SRC RATE, BRR, PMON, ECHO, E TIME, E FDBK, FIR | El sample guardado a SRC RATE (22–4 kHz), codificado a **BRR** (4 bits, bloques de 16; GRIT lo engorda) y leído con la interpolación gaussiana del S-DSP. **PMON**: el pitch movido por otra voz. El **eco** con su FIR de 8 tomas (filtros como los que ponían los juegos), E TIME en pasos de 16 ms y feedback. |
| **SCSP** | Saturn | FM SRC, DEPTH, RATIO, LFO RATE, PLFO, ALFO, DSP | El sample como PCM de 16 bits con su fase modulada por cualquier onda (seno, saw, cuadrada, ruido o el mismo sample). El LFO del slot: **PLFO** al pitch, **ALFO** al nivel. **DSP**: un programa de su DSP (CHORUS, FLANGER, MULTITAP, WATER: lo de "bajo el agua"). |
| **DMG** | Game Boy | DUTY, PULSE 2, WAVE, W VOL, NOISE, ENV, ZOMBIE | Dos pulsos (DUTY: 12.5, 25, 50 o 75 %), el canal de onda de 32 pasos de 4 bits con su volumen 100/50/25 %, el ruido LFSR (NOISE: LONG de 15 bits o SHORT de 7), la envolvente de volumen por hardware (64 Hz) y **ZOMBIE**: el volumen empujado hacia arriba hasta dar la vuelta. Sale por el capacitor de salida. |

**Con la aritmética de cada chip.** Lo que el chip hace con números se hace con sus números:

- **YM2149:** la tabla de su DAC medida (no la teórica de 1.5 dB); la nota pasa por su periodo de tono de 12 bits a
  2 MHz / 16, así que arriba desafina como el chip (un C7 suena a 2083 Hz y no a 2093); el ruido y la envolvente
  también con sus periodos enteros.
- **DMG:** la nota por su periodo de 11 bits, 131072 / (2048 − x) (un C7 suena a 2080.5 Hz); el ruido con uno de sus
  ocho divisores; el capacitor de salida con su constante real.
- **YM2612:** cada operador con las dos ROMs del chip (el seno en logaritmo y la exponencial, como en el *die*), la
  atenuación en pasos de 0.094 dB, el TOTAL LEVEL en pasos de 0.75 dB, el DECAY con la tabla de tasas del chip, la
  nota en su F-NUMBER de 11 bits, el feedback de 4π y el DAC de 9 bits con su *ladder*: no hay escalón entre −1 y +1
  (la distorsión a volumen bajo).
- **SPC700:** la tabla gaussiana de 512 entradas leída del SNES y los cuatro filtros del decodificador BRR, bit a bit
  (de snes_spc, LGPL); el codificador prueba los cuatro en cada bloque y se queda con el que regresa más cerca.
- **SCSP:** el LFO en sus 32 velocidades (0.17–172 Hz) y PLFO / ALFO en sus 8 profundidades.
- Las cuadradas (YM2149, DMG) ponen sus flancos donde caen, entre muestras, y no en la muestra más cercana: el chip
  no tiene aliasing a 44.1 kHz y aquí tampoco.

**Y lo que pasaba después del chip** (la 1.9.1), porque eso también es el sonido que recuerdas:

- **YM2149:** las tres salidas amarradas en un solo cable, como en el Atari ST y el MSX. Es una red de resistencias y
  no una suma: tres canales arriba suenan 1.89 veces uno, no 3. Usa el modelo de MAME con los resistores medidos de
  fpgaarcade. Por eso las notas juntas se aprietan y los *digi-drums* del ST distorsionan como en el ST.
- **YM2612:** la envolvente de la nota va dentro del chip, como atenuación de los portadores (en pasos de 0.094 dB),
  en lugar de un VCA después. Una nota que se apaga cruza el DAC de 9 bits y su *ladder* como en la consola, con su
  aspereza a volumen bajo. Después, el pasa-bajos del Mega Drive modelo 1 (~3.9 kHz), el de las placas con YM2612
  discreto.
- **SPC700:** el S-DSP corre a sus 32 kHz: cada muestra se calcula a 32 kHz y se sostiene, el eco gira a 32 kHz con
  su FIR en el orden del hardware (FIR 0 a la más vieja), siete tomas que dan la vuelta a 16 bits y la última
  recortada, y la salida pasa por el filtro del SNES después del DAC (dos polos a 16 kHz).
- **PRINT** usa los mismos filtros de salida (Mega Drive, SNES).

Lo que queda fuera, con franqueza: el motor es **mono** (una voz de cada chip), así que el paneo duro del DMG y la
inversión de fase L/R del S-DSP no aplican. Del SCSP no están los 32 slots ni el DSP programable completo: son una voz
y cuatro programas fijos. Del YM2612, un canal y no seis, y corre a 44.1 kHz y no a sus 53 kHz. No es un emulador
ciclo a ciclo. Y una nota honesta: no puedo escuchar audio; la fidelidad sale de la documentación del hardware y del
código de los emuladores hechos a partir del silicio y de mediciones, y de pruebas (`tests/chips_test.c`) que
comparan contra sus números. El oído final es el tuyo: `docs/GRABAR.md` dice cómo grabar el FM-1 y
`tools/compare.py` mide la grabación contra el simulador o contra el chip de verdad.

**C (PRINT):** imprime todo lo que suena, efectos incluidos, por el DAC del mismo chip, al final de la cadena: como
si el SFX terminado se guardara en la consola y sonara de ahí. Usa la frecuencia de muestreo del chip (sin filtro
antialias, como el hardware), su DAC y su filtro de salida. **PRINT** es la mezcla (0 % = apagado, por defecto),
**RATE** la frecuencia y **GRIT** el DAC más grueso. No hay que escoger chip: es el del motor.

| DAC | Consolas | Qué emula | GRIT |
| --- | --- | --- | --- |
| **YM2149** | Atari ST, MSX | El registro de volumen usado como DAC (los *digi-drums* del ST), con los niveles medidos | 32 pasos de la envolvente → 16 del registro de volumen |
| **YM2612** | Mega Drive / Genesis | El canal DAC de 8 bits por el DAC de 9 bits con su *ladder*, y el filtro de salida del modelo 1 (~6.4 kHz) | 9 → 6 bits |
| **S-DSP** | Super NES | BRR (los cuatro filtros, el mejor por bloque) y la interpolación gaussiana con su tabla | BRR más grueso |
| **SCSP** | Saturn | PCM lineal con interpolación lineal | 16 → 8 bits |
| **DMG** | Game Boy | El canal de onda: samples de 4 bits, el volumen 100 / 50 / 25 % y el capacitor de salida (pasa-altos) | 16 → 8 → 4 niveles |

- **RATE:** la frecuencia del chip, en el rango que usaban sus juegos (YM2149 4–22 kHz, YM2612 4–26 kHz, S-DSP 8–32
  kHz, SCSP 11–44 kHz, DMG 2–16 kHz). Bajo = más aliasing, más época.
- **D:** el *driver* de 60 Hz (SWEEP, ARP, ARP SPD, STEPS), como lo hacían los drivers de música de los juegos; sirve
  con cualquier motor.

**02 ASSETS B (NEW SFX):** PRESET escoge el tipo (COIN, JUMP, LASER, HIT, POWERUP, EXPLODE, BLIP) y SEL genera uno
nuevo con el motor que tengas. En 03 OSC & SAMPLE, las ondas CHIP del motor ND07 toman de COLOR el ancho del pulso
(12.5, 25, 50, 75 %) y el reloj del ruido (de 75 % para arriba, el ruido corto).

## Pantallas

Cada página tiene un objeto propio en líneas de un pixel que se mueve con el sonido, las perillas y las notas, y que
en silencio sigue respirando. La salida se analiza en cada cuadro (FFT de 256 puntos en 24 bandas, el nivel, los
ataques y el último segundo de ambos). Al girar una perilla, su celda se ilumina 1.2 s con el valor; la pantalla no se tapa. Con HAPPY ACCIDENT todo gira más rápido un momento.

| Página | Objeto |
| --- | --- |
| Inicio | el monolito se dibuja por sus orillas, dos órbitas y polvo alrededor, "ND-07" escrito con cursor y la versión; cualquier botón la salta |
| Actualización | los cinco pasos de la instalación, el monolito llenándose de verde y "DO NOT UNPLUG"; en naranja si falla, con su código |
| ENGINE | en A y B el chip como lo dibujaba su manual: los canales del YM2149 y la forma de su envolvente, el algoritmo del YM2612 con sus operadores encendidos por su envolvente, el bloque BRR y las tomas del FIR del SPC700, el slot y el programa del DSP del SCSP, los canales del DMG en segmentos de LCD y su wave RAM; con ND07, la onda o el sample de 03 tal cual; en C (PRINT) el DAC como pieza con sus pines y la salida en escalones; en D el pitch de los primeros 0.8 s y el sonido en bloques de 8 bits |
| ASSETS | un espectrómetro pequeño en la línea de estado; en B los siete tipos de NEW SFX y el motor en el que va a sonar |
| OSC & SAMPLE | el oscilador como wavetable visto de lado: el ciclo al frente y detrás la misma onda con menos COLOR; el sample como tallos con punta; en D, un anillo de trazos por voz |
| GRAIN ENGINE | cada grano un aro que nace donde lee, sube y se encoge; las orillas del SPRAY punteadas |
| SAMPLE LAB | el slot enrollado en un anillo, la herramienta como signo al centro, lo que toca resaltado |
| ENVELOPES | el ADSR con nodos; la etapa activa en blanco; la voz como un aro que crece; en D (TRANSIENT) una onda desde el ataque en cada golpe y la ganancia como aguja |
| RESONATOR | el cuerpo golpeado, un aro por modo, apilados como el material; cada aro tiembla mientras suena |
| DESTROY | una esfera de alambre a través del shaper: HARD la cuadra, FOLD la dobla, BITS la ajusta a una retícula |
| 3-BAND EQ | la respuesta (LOW, MID, HIGH sumadas) rellena, con una manija por banda, sobre el espectro en vivo en la misma escala |
| RES MOD FILTER | la respuesta del filtro rellena, como en una caja de filtro, con la topografía del espectro encima |
| OTT | cilindro, prisma y pirámide de alambre; flechas de lo que sube y baja la ganancia |
| DELAY + FLANGER | los ecos como losas hacia un punto de fuga |
| REVERB | la sala como túnel de marcos: SIZE los separa, DECAY los mantiene encendidos; en C (SPACE) frentes de onda desde la fuente que se juntan delante al pasar (Doppler) |
| LFO 1 + 2 | un armonógrafo con el rastro de la pluma |
| VARIATION | la semilla como constelación; en MORPH, un cubo que se vuelve octaedro |
| PHYSICS | un tallo por golpe y una pelota que salta entre ellos |
| BATCH | una onda en cada golpe nuevo |

## Panel

Una sola regla: **cada control solo hace su trabajo; con SEL sostenido hace su segundo trabajo, el mismo en todas las
páginas**. Un toque de SEL es la acción de la página (el pie de pantalla dice cuál). Si sostienes SEL sin tocar nada,
el pie muestra los segundos trabajos uno tras otro, y al soltar no pasa nada.

| Control | Solo | Con SEL sostenido |
| --- | --- | --- |
| Botones de página | su paso del trabajo; otra vez: perillas A/B…, luego la siguiente página del botón | (lo mismo) |
| **SELECT** | página anterior / siguiente (las 19) | juego de perillas A B C D de la página |
| **K1..K4** | los parámetros de la página | lock / unlock de ese parámetro |
| **ALGORITHM** | onda del oscilador | tema (EVA-01, 01 TEST, 02 PROD…) |
| **PRESETS** | slot de sample | asset anterior / siguiente: se carga y suena (desde cualquier página) |
| **PLAY** | un C4 sostenido | **STOP ALL**: detiene BATCH, FIRE TEST, PHYSICS, el barrido RTPC y toda nota |
| **REC** | ☢ HAPPY ACCIDENT (en la familia de HA KIND) | **UNDO** |
| **SAVE** | guardar sesión (con A y B, y el sample grabado) | borrar todos los locks |
| **OCT− / OCT+** | octava | **RESET** / **RANDOMIZE** el módulo de la página (en ASSETS: un asset al azar) |
| Teclas **F3..G5** | tocar (sostener = nota sostenida, monofónico, última nota gana, glide legato) | |
| **SEL (toque)** | la acción de la página: **01** A: cargar y tocar el asset, B y C: un chip al azar · **02** REC ARM / STOP · **03** NEW SFX: un sonido nuevo del PRESET con el motor · **05** aplicar la herramienta · **13** C: tocar una pasada · **15** A: semilla nueva, B: guardar A, luego B · **16** tocar el patrón · **17** BATCH (A, C) o FIRE TEST (B) / detener · las demás: aleatorizar el módulo (respeta locks) | |

| Entrada | |
| --- | --- |
| **MIDI in** | notas con velocity, pitch bend ±2, CC1 = MIX del LFO1, **CC11 = MORPH**, CC74 = FREQ del filtro, **CC16 / CC17 / CC18 = pad X / Y / tocar**, **CC19 = armar la grabación de gesto**, channel y poly pressure = aftertouch, CC123 = all notes off |
| **SysEx** | `F0 7D 4E 37 cmd … F7`: samples desde `web/index.html` (HELLO, BEGIN slot largo, DATA μ-law en 7 bits, END guardar) y de vuelta (LEN: largo y nombre del slot activo; READ pos: 128 muestras de 16 bits) |

## Tus propios samples de fábrica

Pon `01_NOMBRE.wav` … `08_NOMBRE.wav` en `samples/` (cualquier frecuencia, mono o estéreo, PCM 8/16/24-bit; NOMBRE
hasta 8 letras), corre `python3 tools/gen_samples.py` y haz push. Se convierten a 22.05 kHz y 16 bits, máximo 1.48 s,
y se guardan empacados sin pérdida (el script comprueba que se desempacan igual). Los sonidos afinados deben estar en
C4. Los actuales se generan por síntesis y sus WAV están en `samples/` para escucharlos.

## CPU

Medido en el host (instrucciones por muestra; la referencia de motores que caben en el FM-1 es ~1000–1500). Lo que
cuesta cada módulo encendido, sobre el patch por defecto (~1015). **Todo el motor es mono** (la salida manda la misma
señal a L y R): delay, flanger, reverb y OTT procesan un solo canal.

| Módulo | Costo | Cómo bajarlo |
| --- | --- | --- |
| OTT (PRE o POST FX) | ~270 | STYLE BYPASS o DEPTH 0 = 0 |
| Filtro 24 dB con ANALOG | ~265 | 12 dB: ~150 · **ANALOG 0** (lineal): −105 |
| Reverb | ~250 | REV MIX 0 = 0 |
| RESONATOR | ~210 | RES MIX 0 = 0 |
| SUPRSAW | +170 sobre SAW | — |
| DESTROY (todo encendido) | ~110 | cada parte apagada no cuesta |
| TRANSIENT | ~95 | ATTACK y SUSTAIN en 0 = 0 |
| 3-BAND EQ | ~50 por banda | una banda en 0 dB no cuesta; todo plano = 0 |
| ENGINE (un chip) | en lugar del oscilador: DMG +25; YM2149 +100; SCSP +160 (con DSP); YM2612 +225; SPC700 +310 (con eco; el codificador BRR prueba sus cuatro filtros) | ENGINE ND07 = 0 |
| Delay / Flanger | ~80 / ~30 | MIX 0 = 0 |
| LAYER 2 / 3 (0.7, estimado) | la FUENTE ×2 / ×3 (con SUPRSAW, lo más caro) | VOICES MONO = 0 · en SAMPLE sólo con STRETCH 1.00x |
| SHEPARD (0.7, estimado) | ~ como SUPRSAW | — |
| SPACE (0.7) | muy poco | DIST, OCCLUDE, PASS-BY en 0 = 0 |

PRINT (ENGINE C) cuesta poco (un muestreo, un convertidor y un filtro por muestra); en el aparato se ve en el pie de pantalla.

## Límites de esta versión

- **Monofónico.** Polifonía multiplicaría el costo de CPU; podría venir como modo paráfono de 2–4 voces.
- **El FM-1 no tiene entrada de audio:** grabar = re-samplear su propia salida, o subir un archivo con `web/index.html`.
  Audio en vivo de la computadora al FM-1 (USB audio de entrada) existe en otro firmware (zp12) pero ocupa el lugar
  del USB audio de salida; queda como opción futura.
- **Un solo núcleo.** El AC791N tiene dos; usar el segundo (como JIANT o Melodee) toca el arranque, el linker y las
  interrupciones, y sin el aparato no hay forma de saber si es estable. Con el motor en bloques y en RAM el CPU alcanza
  (peor caso ~1790 instrucciones por muestra); el diseño está en `docs/DUALCORE.md` para cuando haga falta.
- Samples a 22.05 kHz; las grabaciones del usuario se guardan en flash en 8-bit μ-law (1.48 s por slot). Los de
  fábrica son 16-bit, empacados sin pérdida.
- El BATCH se cronometra desde el loop principal: el inicio de cada golpe puede variar unos milisegundos (no afecta el
  corte por silencios).

## Desarrollo

```sh
sh tests/run_host.sh tests/scripts/nd07_v04.txt    # resonador, transient, DESTROY, MORPH y BATCH: build/host/nd07.wav y PNG
sh tests/run_host.sh tests/scripts/nd07_tour.txt   # el recorrido de los módulos anteriores
sh tests/run_host.sh tests/scripts/pad.txt         # el CHAOS PAD y el aftertouch por MIDI
sh tests/run_host.sh tests/scripts/import.txt      # un sample por SysEx, como lo manda la página web
sh tests/run_host.sh tests/scripts/chip.txt        # 02 ASSETS B: un NEW SFX de cada tipo; en DMG y YM2149
sh tests/run_host.sh tests/scripts/engines.txt     # 01 ENGINE: los cinco chips, una nota cada uno
cc -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o /tmp/ct tests/chips_test.c -lm && /tmp/ct   # los chips contra los números del hardware
sh tests/run_host.sh tests/scripts/variation.txt   # 15 VARIATION: teclas = variaciones con semilla
sh tests/run_host.sh tests/scripts/sustain.txt     # 03 LOOP SUSTAIN: START > LOOP > STOP
sh tests/run_host.sh tests/scripts/motion.txt      # RTPC (barrido) y 13 C SPACE (PASS-BY)
sh tests/run_host.sh tests/scripts/layers.txt      # 03 D: LAYER 3 y la onda SHEPARD
sh tests/run_host.sh tests/scripts/bounce.txt      # REC BOUNCE y BATCH con MATCH
sh tests/run_host.sh tests/scripts/overdub.txt     # REC OVERDUB
sh tests/run_host.sh tests/scripts/physics.txt     # 16 PHYSICS TRIGGER: BOUNCE, DEBRIS, SCATTER
sh tests/run_host.sh tests/scripts/lab.txt         # 05 SAMPLE LAB: REVERSE, 1/2 SPD, REVERT
sh tests/run_host.sh tests/scripts/modenv.txt      # 06 C MOD ENV y las recetas del HAPPY ACCIDENT
sh tests/run_host.sh tests/scripts/assets.txt      # 02 ASSETS: escoger, SEL carga y toca, HAPPY ACCIDENT en la familia
cc -O2 -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o /tmp/at tests/assets_test.c -lm && /tmp/at   # los 19 como WAV + niveles
sh tests/run_host.sh tests/scripts/screens.txt     # todas las páginas y juegos (capturas para revisar el tema)
python3 tools/gen_screens_tour.py                  # regenera screens.txt si cambian las páginas
sh tests/run_host.sh tests/scripts/eva_states.txt  # EVA-01: perilla recién girada, lock, HAPPY ACCIDENT, modulación
python3 tools/gen_eva_font.py                      # regenera firmware/gen/eva_font.h (fuentes del tema EVA-01)
sh tests/run_host.sh tests/scripts/gesture.txt     # GESTURE del pad: grabado (SEL y CC19) y repetido por la nota
sh tests/run_host.sh tests/scripts/eq.txt          # 09 3-BAND EQ: LOW, MID, HIGH sobre ruido
sh tests/run_host.sh tests/scripts/export.txt      # el slot de vuelta como WAV (SysEx LEN / READ)
cc -O2 -Wall -o /tmp/sense_test tests/sense_test.c && /tmp/sense_test   # la velocity con presiones sintéticas
python3 tools/make_install_page.py && node tests/web_install_test.mjs   # la página del instalador (contra un FM-1 simulado)
python3 tools/gen_nd07_tables.py                  # tablas (seno, tanh, filtros, dB, μ-law...)
python3 tools/gen_samples.py                      # los samples de fábrica
```

`firmware/src/nd07.c` es la app. Reglas del chip: solo enteros, sin división de 64 bits, sin constantes que parezcan
direcciones de ROM (`docs/RULES.md`); revisado compilando a RISC-V de 32 bits sin FPU (-O1, -O2, -Os). Probado con 300
patches aleatorios con todos los módulos: sin pasarse del techo del limitador, sin DC ni inestabilidad del resonador.

La identidad del paquete es `FM-1_4707N` (4707 = N D 0 7; `FM-1_950` ya es de CS-1). Antes de publicar, revisa
"Is my identity free?" en el Firmware Hub. `fmhub.json` tiene la ficha para el Hub.

## Para quien lee hasta el final

Hay dos páginas que no aparecen: **RTPC** (un valor del juego, barrido o a mano, sobre tres destinos con curva) y
**CHAOS PAD + KEYS** (el pad XY por MIDI, la velocity de las teclas, el aftertouch y un gesto grabado). Se muestran con
**SEL + LFO** y se esconden igual; la sesión lo recuerda. Escondidas no hacen nada (el pad, el gesto, el aftertouch y
el RTPC dejan de mover parámetros); al mostrarlas, quedan en el botón LFO como 15 y 16, y las de SEQ se corren a 17–19.
