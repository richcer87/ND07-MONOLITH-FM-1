# ND-07 // MONOLITH · FM-1 · v1.5

Firmware para el **M-VAVE FM-1**: un sampler y sintetizador para diseñar efectos de sonido de videojuegos. Es la
versión de bolsillo de la estación ND-07 // MONOLITH para REAPER, escrita en C entero para el chip JieLi AC79 (sin FPU).

21 páginas en el orden del trabajo: escoges un **ASSET** de partida (y, si quieres, la consola en la que va a sonar: **SOUND CHIPS**), armas la fuente (osciladores, samples, granos,
chip de 8 bits), le das forma, color y espacio, la mueves como la movería el juego (LFOs, **RTPC**, **CHAOS PAD**), la
varías y la entregas (**VARIATION**, **PHYSICS**, **BATCH**). Además: HAPPY ACCIDENT, locks de parámetros, 16 pasos de UNDO y 5 temas (EVA-01 por defecto).

Construido sobre [Dinghy](https://github.com/zvenson/dinghy) (zvenson): USB, MIDI, actualizaciones, rescate USB,
pantalla y flash. Licencia GPL-3.0-only.

> **Estado: 1.5.** La 0.9 corre en un FM-1 "New" (con todos los módulos encendidos y el Chaos Pad, ~30 % de CPU); lo
> nuevo de la 1.0 a la 1.5 está probado en el simulador de host (audio WAV y capturas). El `.fwsc` se compila en
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
convierte a mono 22.05 kHz, lo escuchas tal como sonará y se manda a uno de los 8 slots por SysEx).

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
la siguiente página del mismo botón. **SELECT** recorre las 21 páginas. Al encender estás en **01 ASSETS**.

| Paso | Botón | Páginas |
| --- | --- | --- |
| Empezar y crear la fuente | **HOME** | 01 ASSETS (y SOUND CHIPS) · 02 OSC & SAMPLE · 03 CHIP OSC |
| Material | **ARP** | 04 GRAIN ENGINE · 05 SAMPLE LAB |
| Dar forma | **ENV** | 06 ENVELOPES · 07 TRANSIENT |
| Color | **EDIT** | 08 RESONATOR · 09 DESTROY · 10 VOWEL + SHIFT · 11 RES MOD FILTER |
| Pegar | **GLO** | 12 OTT MULTIBAND |
| Colocar | **FX** | 13 DELAY + FLANGER · 14 MULTIBAND REVERB · 15 SPACE + MOTION |
| Mover (lo que controla el juego) | **LFO** | 16 LFO 1 + 2 · 17 RTPC · 18 CHAOS PAD + KEYS |
| Variar y entregar | **SEQ** | 19 VARIATION + MORPH · 20 PHYSICS TRIGGER · 21 BATCH + FIRE TEST |

| # | Página | Perillas A | Perillas B (C, D) |
| --- | --- | --- | --- |
| 01 | **ASSETS** | ASSET, VARY, HA KIND, OUT | B (SOUND CHIPS): CHIP, RATE, GRIT, MIX · C: ECHO, E TIME, TONE |
| 02 | OSC & SAMPLE | SOURCE, WAVE (15 ondas), SAMPLE, BLEND | B: TUNE, COLOR, STRETCH, REC · C: LOOP, L POS, L LEN, XFADE · D: VOICES, L PITCH, DETUNE, L LEVEL |
| 03 | CHIP OSC | DUTY, SWEEP, ARP, ARP SPD | STEPS, NOISE, PRESET |
| 04 | GRAIN ENGINE | POS, SIZE, DENS, SPRAY | REVERSE, GRAIN, FREEZE, RND PIT |
| 05 | SAMPLE LAB | TOOL, AMOUNT | — |
| 06 | ENVELOPES | ATTACK, DECAY, SUSTAIN, RELEASE | B: GLIDE, VELO, PITCH, OUT · C: M ATK, M DEC, M AMT, M TGT |
| 07 | TRANSIENT | ATTACK, SUSTAIN, TIME, OUT | — |
| 08 | RESONATOR | BODY, PITCH, DECAY, RES MIX | DAMP, SPREAD, HIT POS, KEY TRK |
| 09 | DESTROY | DRIVE, SHAPE, BITS, RATE | RING HZ, RING MIX, TONE, MIX |
| 10 | VOWEL + SHIFT | VOWEL, SIZE, RES, V MIX | B: SHIFT, FDBK, S TIME, S MIX · C: GROWL, G RATE |
| 11 | RES MOD FILTER | FREQ, RES, MODE, SLOPE | DEPTH, RATE, MOD SRC, ANALOG |
| 12 | OTT MULTIBAND | A: DEPTH, TIME, UP, DOWN · B: IN, OUT, STYLE, PLACE | C: LOW, MID, HIGH, THRESH · D: X-LO, X-HI, SOLO |
| 13 | DELAY + FLANGER | TIME, FDBK, DLY MIX, ROUTE | RATE, DEPTH, REGEN, FLG MIX |
| 14 | MULTIBAND REVERB | SIZE, DECAY, PRE-DLY, REV MIX | LOW x, HIGH x, X-LO, X-HI |
| 15 | SPACE + MOTION | DIST, OCCLUDE, PASS-BY, DOPPLER | — |
| 16 | LFO 1 + 2 | SHAPE, RATE, MIX, TARGET (LFO 1) | SHAPE, RATE, MIX, TARGET (LFO 2) |
| 17 | RTPC | VALUE, SWEEP, CURVE | B: TARGET 1, AMT 1, TARGET 2, AMT 2 · C: TARGET 3, AMT 3 |
| 18 | CHAOS PAD + KEYS | X TGT, X AMT, Y TGT, Y AMT | B: PAD, GESTURE, KEY VEL, SENS · C: AT TGT, AT AMT |
| 19 | VARIATION + MORPH | VARY, SEED, KEYS, RESEED | MORPH, HA AMT, EVOLVE, HA KIND |
| 20 | PHYSICS TRIGGER | TRIGGER, HITS, TIME, FALL | PITCH, JITTER |
| 21 | BATCH + FIRE TEST | BATCH, COUNT, SPACE, GATE | B: RATE, POOL, ORDER · C: MATCH, TARGET |

La cadena de audio (mono): FUENTE (osc / sample / granos) × ENV → RESONATOR → TRANSIENT → DESTROY → VOWEL + SHIFT →
FILTER → OTT (PRE) → DELAY + FLANGER → REVERB → OTT (POST) → SOUND CHIPS → SPACE → limitador → salida.

## SOUND CHIPS (ASSETS B y C)

Junto con el asset escoges la consola: los juegos de perillas B y C de **01 ASSETS** pasan todo lo que suena por el chip de sonido de una consola, como si el juego corriera ahí:
la frecuencia de muestreo del chip (sin filtro antialias, como el hardware), su convertidor y su carácter. Apagado por
defecto; HAPPY ACCIDENT, los ASSETS y las recetas no lo tocan, así que puedes escoger un chip y recorrer sonidos con él.
Para oírlo bien empieza con el asset **SYNTH**: un saw abierto, sostenido y seco, con un poco de vibrato. Con SEL en
B o C, un chip al azar.

| CHIP | Consolas | Qué emula | GRIT |
| --- | --- | --- | --- |
| **YM2149** | Atari ST, MSX | El registro de volumen usado como DAC (los *digi-drums* del ST): niveles logarítmicos | 32 pasos de 1.5 dB → 16 pasos de hasta 6 dB |
| **YM2612** | Mega Drive / Genesis | El canal DAC de 8 bits, la salida de 9 bits y el escalón del *ladder* alrededor de cero | 9 → 6 bits, más escalón |
| **S-DSP** | Super NES | BRR (ADPCM de 4 bits en bloques de 16, filtro 1), interpolación suave tipo gaussiana y el eco con FIR de 8 tomas | BRR más grueso |
| **SCSP** | Saturn | PCM lineal con interpolación lineal y el DSP como un eco corto difuso | 16 → 8 bits |
| **DMG** | Game Boy | El canal de onda: samples de 4 bits, el volumen 100 / 50 / 25 % y el capacitor de salida (pasa-altos) | 16 → 8 → 4 niveles |

- **RATE:** la frecuencia del chip, en el rango que usaban sus juegos (YM2149 4–22 kHz, YM2612 4–26 kHz, S-DSP 8–32
  kHz, SCSP 11–44 kHz, DMG 2–16 kHz). Bajo = más aliasing, más época.
- **MIX:** contra el sonido antes del chip. **TONE:** el filtro analógico de salida de la consola (1.5–20 kHz).
- **ECHO / E TIME:** solo en S-DSP y SCSP, que traían eco en el hardware; E TIME en pasos de 16 ms, como el del SNES.
- En pantalla: el chip como pieza (DIP o cuadrada) con sus pines activos, la salida como sale de su DAC (escalones; en
  YM2149 y DMG una regla con sus niveles; en S-DSP los bloques BRR) y los cinco chips como etiquetas de cartucho.

## Pantallas

Cada página tiene un objeto propio en líneas de un pixel que se mueve con el sonido, las perillas y las notas, y que
en silencio sigue respirando. La salida se analiza en cada cuadro (FFT de 256 puntos en 24 bandas, el nivel, los
ataques y el último segundo de ambos). Al girar una perilla, su celda se ilumina 1.2 s con el valor; la pantalla no se tapa. Con HAPPY ACCIDENT todo gira más rápido un momento.

| Página | Objeto |
| --- | --- |
| ASSETS | un espectrómetro pequeño en la línea de estado; en B y C, el chip como pieza con sus pines, la salida en escalones y los cinco chips como etiquetas de cartucho |
| OSC & SAMPLE | el oscilador como wavetable visto de lado: el ciclo al frente y detrás la misma onda con menos COLOR; el sample como tallos con punta; en D, un anillo de trazos por voz |
| CHIP OSC | el sonido en bloques de 8 bits |
| GRAIN ENGINE | cada grano un aro que nace donde lee, sube y se encoge; las orillas del SPRAY punteadas |
| SAMPLE LAB | el slot enrollado en un anillo, la herramienta como signo al centro, lo que toca resaltado |
| ENVELOPES | el ADSR con nodos; la etapa activa en blanco; la voz como un aro que crece |
| TRANSIENT | una onda desde el ataque en cada golpe; la ganancia como aguja |
| RESONATOR | el cuerpo golpeado, un aro por modo, apilados como el material; cada aro tiembla mientras suena |
| DESTROY | una esfera de alambre a través del shaper: HARD la cuadra, FOLD la dobla, BITS la ajusta a una retícula |
| VOWEL + SHIFT | una boca que dice la vocal (U chica y redonda, A abierta, I ancha y delgada), se abre con el sonido y tiembla con GROWL; la voz saliendo |
| RES MOD FILTER | la respuesta del filtro rellena, como en una caja de filtro, con la topografía del espectro encima |
| OTT | cilindro, prisma y pirámide de alambre; flechas de lo que sube y baja la ganancia |
| DELAY + FLANGER | los ecos como losas hacia un punto de fuga |
| REVERB | la sala como túnel de marcos: SIZE los separa, DECAY los mantiene encendidos |
| SPACE | frentes de onda desde la fuente; se juntan delante al pasar (Doppler) |
| LFO 1 + 2 | un armonógrafo con el rastro de la pluma |
| CHAOS PAD | el rastro del dedo; en SPRING, la línea que lo regresa al centro |
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
| **SELECT** | página anterior / siguiente (las 21) | juego de perillas A B C D de la página |
| **K1..K4** | los parámetros de la página | lock / unlock de ese parámetro |
| **ALGORITHM** | onda del oscilador | tema (EVA-01, 01 TEST, 02 PROD…) |
| **PRESETS** | slot de sample | asset anterior / siguiente: se carga y suena (desde cualquier página) |
| **PLAY** | un C4 sostenido | **STOP ALL**: detiene BATCH, FIRE TEST, PHYSICS, el barrido RTPC y toda nota |
| **REC** | ☢ HAPPY ACCIDENT (en la familia de HA KIND) | **UNDO** |
| **SAVE** | guardar sesión (con A y B, y el sample grabado) | borrar todos los locks |
| **OCT− / OCT+** | octava | **RESET** / **RANDOMIZE** el módulo de la página (en ASSETS: un asset al azar) |
| Teclas **F3..G5** | tocar (sostener = nota sostenida, monofónico, última nota gana, glide legato) | |
| **SEL (toque)** | la acción de la página: **01** A: cargar y tocar el asset, B y C: un chip al azar · **02** REC ARM / STOP · **03** un sonido nuevo del PRESET · **05** aplicar la herramienta · **15** tocar una pasada · **17** barrido RTPC · **18** grabar un gesto · **19** A: semilla nueva, B: guardar A, luego B · **20** tocar el patrón · **21** BATCH (A, C) o FIRE TEST (B) / detener · las demás: aleatorizar el módulo (respeta locks) | |

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
| Delay / Flanger | ~80 / ~30 | MIX 0 = 0 |
| LAYER 2 / 3 (0.7, estimado) | la FUENTE ×2 / ×3 (con SUPRSAW, lo más caro) | VOICES MONO = 0 · en SAMPLE sólo con STRETCH 1.00x |
| SHEPARD (0.7, estimado) | ~ como SUPRSAW | — |
| SPACE, RTPC (0.7) | muy poco | DIST, OCCLUDE, PASS-BY en 0 / VALUE 0 = 0 |

SOUND CHIPS cuesta poco (un muestreo, un convertidor y un filtro por muestra; el eco del S-DSP, ocho multiplicaciones); en el aparato se ve en el pie de pantalla.

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
sh tests/run_host.sh tests/scripts/chip.txt        # 03 CHIP OSC: un preset de cada tipo
sh tests/run_host.sh tests/scripts/variation.txt   # 19 VARIATION: teclas = variaciones con semilla
sh tests/run_host.sh tests/scripts/sustain.txt     # 02 LOOP SUSTAIN: START > LOOP > STOP
sh tests/run_host.sh tests/scripts/motion.txt      # 17 RTPC (barrido) y 15 SPACE (PASS-BY)
sh tests/run_host.sh tests/scripts/layers.txt      # 02 D: LAYER 3 y la onda SHEPARD
sh tests/run_host.sh tests/scripts/bounce.txt      # REC BOUNCE y BATCH con MATCH
sh tests/run_host.sh tests/scripts/overdub.txt     # REC OVERDUB
sh tests/run_host.sh tests/scripts/physics.txt     # 20 PHYSICS TRIGGER: BOUNCE, DEBRIS, SCATTER
sh tests/run_host.sh tests/scripts/lab.txt         # 05 SAMPLE LAB: REVERSE, 1/2 SPD, REVERT
sh tests/run_host.sh tests/scripts/modenv.txt      # 06 C MOD ENV y las recetas del HAPPY ACCIDENT
sh tests/run_host.sh tests/scripts/assets.txt      # 01 ASSETS: escoger, SEL carga y toca, HAPPY ACCIDENT en la familia
cc -O2 -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o /tmp/at tests/assets_test.c -lm && /tmp/at   # los 19 como WAV + niveles
sh tests/run_host.sh tests/scripts/screens.txt     # todas las páginas y juegos (capturas para revisar el tema)
python3 tools/gen_screens_tour.py                  # regenera screens.txt si cambian las páginas
sh tests/run_host.sh tests/scripts/eva_states.txt  # EVA-01: perilla recién girada, lock, HAPPY ACCIDENT, modulación
python3 tools/gen_eva_font.py                      # regenera firmware/gen/eva_font.h (fuentes del tema EVA-01)
sh tests/run_host.sh tests/scripts/gesture.txt     # 18 GESTURE: grabado (SEL y CC19) y repetido por la nota
sh tests/run_host.sh tests/scripts/vox.txt         # 10 VOWEL + SHIFT: shift arriba/abajo, la espiral, U O A E I
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
