# ND-07 // MONOLITH · FM-1 edition · v0.6

Firmware personalizado para el **M-VAVE FM-1**: la estación de SFX ND-07 // MONOLITH (REAPER, Lua ReaImGui + JSFX),
**sampler y sintetizador para diseño de SFX de videojuegos**. 14 páginas (resonador modal, transient shaper, DESTROY,
OTT multibanda, MORPH A↔B y BATCH de variaciones, **CHAOS PAD** desde el teléfono, **CHIP OSC** de 8 bits), HAPPY ACCIDENT, locks de parámetros,
UNDO de 16 pasos y los 4 temas, en C entero para el chip JieLi AC79 (sin FPU).

Construido sobre [Dinghy](https://github.com/zvenson/dinghy) (zvenson): USB, MIDI, actualizaciones, rescate USB,
pantalla y flash. Licencia GPL-3.0-only.

> **Estado: 0.6, sin probar en hardware.** Compila y corre en el simulador de host (audio WAV + capturas). El `.fwsc` se
> compila en GitHub Actions con la toolchain de JieLi. Pruébalo primero en el emulador. Instalar firmware es bajo tu
> propio riesgo; el [FM-1 Firmware Hub](https://thefirmwarehub.com) te regresa al firmware oficial, y mantener **OCT-**
> al encender entra al rescate USB. La sesión guardada de versiones anteriores no se carga (cambió el formato): arranca
> con los valores de fábrica.
>
> **Datos de otros firmwares:** ND-07 no lee lo que otro firmware personalizado haya guardado en la flash (sesiones,
> samples) y puede sobrescribirlo. Lo más limpio es instalarlo sobre el firmware oficial. Para volver, el Hub reinstala
> el oficial.

## Probarlo en el emulador online

1. Sube esta carpeta a un repositorio de GitHub (rama `main`).
2. En **Actions**, espera el workflow `release` y descarga el artifact **nd07-fwsc**; extrae `nd07.fwsc`.
3. Abre [joakimeriksson.github.io/fm1-emu](https://joakimeriksson.github.io/fm1-emu/), abre tu `nd07.fwsc` en la página
   y presiona Start.

Para una versión publicada: `git tag v0.6 && git push --tags`.

## Nuevo en v0.6: 14 CHIP OSC

Sonido de consola de 8 bits para SFX retro (botón **HOME**, tercera página).

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

## Nuevo en v0.5

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

### Conectar el teléfono

| | Cómo |
| --- | --- |
| **Android** | Cable USB-C a USB-C (o adaptador OTG) del teléfono al FM-1. Abre la página en **Chrome**, CONECTAR, acepta el permiso MIDI. |
| **Computadora** | Chrome o Edge (Windows, Mac, Linux), cable USB normal. |
| **iPhone / iPad** | Safari no tiene Web MIDI. Usa una app de MIDI con pad XY (TouchOSC, Lemur, MIDI Designer…) con un adaptador USB y mándale CC16, CC17 y CC18. |
| **Inalámbrico** | El FM-1 no tiene Bluetooth ni WiFi usables en este firmware. Desde la computadora puedes rebotar: teléfono → (WiFi, p. ej. TouchOSC) → computadora → USB → FM-1. |

La página necesita **https** o **localhost** (regla de los navegadores para MIDI). Lo más fácil: en tu repositorio,
Settings → Pages → "Deploy from a branch", `main`, carpeta `/ (root)`, y ábrela en
`https://TU_USUARIO.github.io/TU_REPO/web/`. También funciona abriéndola con `python3 -m http.server` en la computadora.

## Los módulos

Cada botón de página lleva a uno o dos módulos: presiónalo otra vez para el siguiente juego de perillas (A/B…) y,
después del último, para el siguiente módulo del mismo botón. **SELECT** recorre las 14 páginas.

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

La cadena de audio (mono): FUENTE (osc / sample / granos) × ENV → RESONATOR → TRANSIENT → DESTROY → FILTER → OTT (PRE) →
DELAY + FLANGER → REVERB → OTT (POST) → salida.

### Nuevo en v0.4

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

### Lo que ya estaba

- **Osciladores:** SINE, TRI, SAW, SQUARE, PULSE, SUPRSAW, NOISE, SUB-SAW, FORMANT, METALFM, PADSCAN (antialias
  PolyBLEP). **COLOR** cambia según la onda: ancho de pulso, detune del supersaw, brillo del ruido, morph de los
  wavetables, o qué parte del sample recorre PADSCAN.
- **Samples:** 8 slots con sonidos de fábrica (BOOM, METAL, ZAP, GLITCH, WHOOSH, VOX AH, BELL, DRONE), afinados en C4.
  **Grabar en un slot:** en la página 01, toca **SEL** (REC ARMED), toca una nota: graba la salida del FM-1 (todo
  el procesamiento, reverb incluida) hasta 1.48 s o hasta 0.6 s de silencio. **SAVE** lo guarda en flash.
  **SEL + PLAY** regresa el slot a su sonido de fábrica.
- **OTT:** el algoritmo del preset OTT de Ableton (el que copia el OTT de Xfer): 3 bandas Linkwitz-Riley de 24 dB/oct en
  88.3 Hz y 2.5 kHz, detección RMS, compresión hacia abajo (66.7:1 / ∞:1) y hacia arriba (1:4.17) por banda. DEPTH,
  TIME, UPWD, DOWNWD, IN, OUT, LOW/MID/HIGH, THRESH, SOLO, STYLE (OTT, SMOOTH, PUNCH, AGGR) y PLACE (PRE FX / POST FX).
- **Delay + Flanger** (4 rutas), **reverb multibanda** (LOW / MID / HIGH con su propio decay) y **2 LFOs** asignables
  a cualquier parámetro (ahora también a los módulos nuevos y a MORPH).

## Panel

| Control | Función |
| --- | --- |
| Botones de página | sus módulos; presiona otra vez: perillas A/B…, luego el siguiente módulo del botón |
| **SELECT** | página anterior / siguiente (las 14) |
| **K1..K4** | parámetros · **SEL sostenido + girar** = lock/unlock |
| **ALGORITHM / PRESETS** | onda del oscilador / slot de sample |
| Teclas **F3..G5** | tocar (sostener = nota sostenida, monofónico, última nota gana, glide legato) · **OCT- / OCT+** octava |
| **PLAY** | un C4 sostenido |
| **REC** | ☢ HAPPY ACCIDENT (a veces enciende el resonador, DESTROY o el transient) · **SEL + REC** = UNDO |
| **SAVE** | guardar sesión (con A y B, y el sample grabado) · **SEL + SAVE** = borrar todos los locks |
| **SEL (toque)** | 01: REC ARM / STOP · 12: BATCH / detener · 13: destinos del pad al azar · 14: un sonido nuevo del PRESET · las demás: aleatorizar el módulo (respeta locks) |
| **SEL + OCT-** / **SEL + OCT+** | reset del módulo / siguiente tema · en la página 12: **guardar A / guardar B** |
| **SEL + PLAY** | el slot a su sonido de fábrica |
| **MIDI in** | notas con velocity, pitch bend ±2, CC1 = MIX del LFO1, **CC11 = MORPH**, CC74 = FREQ del filtro, **CC16 / CC17 / CC18 = pad X / Y / tocar**, channel y poly pressure = aftertouch, CC123 = all notes off |
| **SysEx** | `F0 7D 4E 37 cmd … F7`: samples desde `web/index.html` (HELLO, BEGIN slot largo, DATA μ-law en 7 bits, END guardar) |

## Tus propios samples de fábrica

Pon `01_NOMBRE.wav` … `08_NOMBRE.wav` en `samples/` (cualquier frecuencia, mono o estéreo, PCM 8/16/24-bit; NOMBRE hasta 8
letras), corre `python3 tools/gen_samples.py` y haz push. Se convierten a 22.05 kHz μ-law, máximo 1.48 s. Los sonidos
afinados deben estar en C4. Los actuales se generan por síntesis y sus WAV están en `samples/` para escucharlos.

## CPU (v0.4; CHIP OSC apagado no suma nada)

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

Optimizaciones de esta versión:

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

## Límites de esta versión

- **Monofónico.** Polifonía multiplicaría el costo de CPU; podría venir como modo paráfono de 2–4 voces.
- **El FM-1 no tiene entrada de audio:** grabar = re-samplear su propia salida, o subir un archivo con `web/index.html`.
  Audio en vivo de la computadora al FM-1 (USB audio de entrada) existe en otro firmware (zp12) pero ocupa el lugar
  del USB audio de salida; queda como opción futura.
- **Un solo núcleo.** El AC791N tiene dos; usar el segundo (como JIANT o Melodee) toca el arranque, el linker y las
  interrupciones, y sin el aparato no hay forma de saber si es estable. Con el motor en bloques y en RAM el CPU alcanza
  (peor caso ~1790 instrucciones por muestra); el diseño está en `docs/DUALCORE.md` para cuando haga falta.
- Samples a 22.05 kHz; las grabaciones del usuario se guardan en flash en 8-bit μ-law (1.48 s por slot). Los de
  fábrica son 16-bit.
- El BATCH se cronometra desde el loop principal: el inicio de cada golpe puede variar unos milisegundos (no afecta el
  corte por silencios).

## Desarrollo

```sh
sh tests/run_host.sh tests/scripts/nd07_v04.txt    # los módulos nuevos, MORPH y BATCH: build/host/nd07.wav y PNG
sh tests/run_host.sh tests/scripts/nd07_tour.txt   # el recorrido de los módulos anteriores
sh tests/run_host.sh tests/scripts/pad.txt         # el CHAOS PAD y el aftertouch por MIDI
sh tests/run_host.sh tests/scripts/import.txt      # un sample por SysEx, como lo manda la página web
sh tests/run_host.sh tests/scripts/chip.txt        # 14 CHIP: un preset de cada tipo
cc -O2 -Wall -o /tmp/sense_test tests/sense_test.c && /tmp/sense_test   # la velocity con presiones sintéticas
python3 tools/gen_nd07_tables.py                  # tablas (seno, tanh, filtros, dB, μ-law...)
python3 tools/gen_samples.py                      # los samples de fábrica
```

`firmware/src/nd07.c` es la app. Reglas del chip: solo enteros, sin división de 64 bits, sin constantes que parezcan
direcciones de ROM (`docs/RULES.md`); revisado compilando a RISC-V de 32 bits sin FPU (-O1, -O2, -Os). Probado con 300
patches aleatorios con todos los módulos: sin pasarse del techo del limitador, sin DC ni inestabilidad del resonador.

La identidad del paquete es `FM-1_4707N` (4707 = N D 0 7; `FM-1_950` ya es de CS-1). Antes de publicar, revisa
"Is my identity free?" en el Firmware Hub. `fmhub.json` tiene la ficha para el Hub.
