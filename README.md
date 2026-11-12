# ND-07 // MONOLITH · FM-1 edition · v0.3

Firmware personalizado para el **M-VAVE FM-1**: la estación de SFX ND-07 // MONOLITH (REAPER, Lua ReaImGui + JSFX),
**sampler y sintetizador** para diseño de SFX, con **OTT multibanda**. Cadena de 8 módulos, HAPPY ACCIDENT, locks de parámetros, UNDO de 16 pasos y los
4 temas, en C entero para el chip JieLi AC79 (sin FPU).

Construido sobre [Dinghy](https://github.com/zvenson/dinghy) (zvenson): USB, MIDI, actualizaciones, rescate USB,
pantalla y flash. Licencia GPL-3.0-only.

> **Estado: 0.3, sin probar en hardware.** Compila y corre en el simulador de host (audio WAV + capturas). El `.fwsc` se
> compila en GitHub Actions con la toolchain de JieLi. Pruébalo primero en el emulador. Instalar firmware es bajo tu
> propio riesgo; el [FM-1 Firmware Hub](https://thefirmwarehub.com) te regresa al firmware oficial, y mantener **OCT-**
> al encender entra al rescate USB.

## Probarlo en el emulador online

1. Sube esta carpeta a un repositorio de GitHub (rama `main`).
2. En **Actions**, espera el workflow `release` y descarga el artifact **nd07-fwsc**; extrae `nd07.fwsc`.
3. Abre [joakimeriksson.github.io/fm1-emu](https://joakimeriksson.github.io/fm1-emu/), abre tu `nd07.fwsc` en la página
   y presiona Start.

Para una versión publicada: `git tag v0.3 && git push --tags`.

## Los módulos

| # | Botón | Módulo | Perillas A | Perillas B |
| --- | --- | --- | --- | --- |
| 01 | HOME | OSC & SAMPLE | SOURCE (OSC / SAMPLE / MIX), WAVE, SAMPLE (slot 1..8), BLEND | TUNE, COLOR, STRETCH, LOOP |
| 02 | ENV | AMP ENVELOPE | ATTACK, DECAY, SUSTAIN, RELEASE | GLIDE, VELO, PITCH, OUT |
| 03 | EDIT | RES MOD FILTER | FREQ, RES, MODE, SLOPE | DEPTH, RATE, MOD SRC, ANALOG |
| 04 | ARP | GRAIN ENGINE (sobre el sample) | POS, SIZE, DENS, SPRAY | REV %, GRAIN, FREEZE, RND PIT |
| 05 | SEQ | OTT MULTIBAND (4 juegos: A B C D) | A: DEPTH, TIME, UPWD, DOWNWD · B: IN, OUT, STYLE, PLACE | C: LOW, MID, HIGH, THRESH · D: X-LO, X-HI, SOLO |
| 06 | FX | DELAY + FLANGER | TIME, FDBK, DLY MIX, ROUTE | RATE, DEPTH, REGEN, FLG MIX |
| 07 | GLO | MULTIBAND REVERB | SIZE, DECAY, PRE-DLY, REV MIX | LOW x, HIGH x, X-LO, X-HI |
| 08 | LFO | LFO x2 | SHAPE, RATE, MIX, TARGET (LFO 1) | SHAPE, RATE, MIX, TARGET (LFO 2) |

- **Osciladores:** SINE, TRI, SAW, SQUARE, PULSE, SUPRSAW, NOISE, SUB-SAW, FORMANT, METALFM, PADSCAN (antialias
  PolyBLEP). **COLOR** cambia según la onda: ancho de pulso, detune del supersaw, brillo del ruido, morph de los
  wavetables, o qué parte del sample recorre PADSCAN.
- **Samples:** 8 slots con sonidos de fábrica (BOOM, METAL, ZAP, GLITCH, WHOOSH, VOX AH, BELL, DRONE), afinados en C4.
  **Grabar en un slot:** en la página 01, toca **SEL** (REC ARMED), toca una nota: graba la salida del FM-1 (todo
  el procesamiento, reverb incluida) hasta 1.48 s o hasta 0.6 s de silencio. **SAVE** lo guarda en flash.
  **SEL + PLAY** regresa el slot a su sonido de fábrica.
- **MIX:** BLEND = cuánto sample entra (0 % solo oscilador, 100 % solo sample).
- **OTT:** el algoritmo del preset OTT de Ableton (el que copia el OTT de Xfer): 3 bandas con crossovers Linkwitz-Riley
  de 24 dB/oct en 88.3 Hz y 2.5 kHz (ajustables en el juego D), detección RMS, compresión hacia abajo (66.7:1 / ∞:1) y
  hacia arriba (1:4.17) por banda, con sus umbrales, tiempos y ganancias de entrada/salida originales. Los controles de
  Xfer: DEPTH (dry/wet, con el dry alineado en fase), TIME (0.1x–10x; 1.00x = los tiempos del preset), UPWD %, DOWNWD %,
  IN, OUT y la ganancia de cada banda (LOW/MID/HIGH), más THRESH (mueve los umbrales de las tres bandas), SOLO por banda,
  STYLE (OTT o las curvas SMOOTH/PUNCH/AGGR de la versión JSFX) y PLACE: **PRE FX** (antes del delay y la reverb) o
  **POST FX** (aplasta también las colas, en estéreo). Con DEPTH 0 la respuesta es plana (±0.02 dB medido).
- **Delay + Flanger, ROUTE:** PARALL. (en paralelo), FLG>DLY, DLY>FLG, IN FDBK (el flanger dentro del feedback: cada
  repetición se flanchea más). Mover TIME desliza la cinta.
- **Reverb multibanda:** DECAY es el tiempo de los medios; LOW x y HIGH x multiplican el decay de graves y agudos
  (25..200 %) con cortes en X-LO / X-HI. Salida estéreo.
- **LFOs:** cada uno con forma (SINE, TRI, SAW UP/DN, SQUARE, S&H, DRIFT), RATE 0.05..30 Hz, **MIX** (0 % = sin
  efecto, 100 % = barre todo el rango del parámetro) y **TARGET**: cualquier parámetro, incluso el otro LFO.
  La perilla modulada muestra un punto que se mueve. Por defecto LFO1 → PITCH (vibrato) y LFO2 → FREQ del filtro.

## Panel

| Control | Función |
| --- | --- |
| Botones de página | página 01..08 (presiona otra vez: perillas A/B) |
| **SELECT** | página anterior / siguiente |
| **K1..K4** | parámetros · **SEL sostenido + girar** = lock/unlock |
| **ALGORITHM / PRESETS** | onda del oscilador / slot de sample |
| Teclas **F3..G5** | tocar (sostener = nota sostenida, monofónico, última nota gana, glide legato) · **OCT- / OCT+** octava |
| **PLAY** | un C4 sostenido |
| **REC** | ☢ HAPPY ACCIDENT · **SEL + REC** = UNDO |
| **SAVE** | guardar sesión (y el sample grabado) · **SEL + SAVE** = borrar todos los locks |
| **SEL (toque)** | página 01: REC ARM / STOP · páginas 02..08: aleatorizar el módulo (respeta locks) |
| Botón de página otra vez | siguiente juego de perillas (A/B, y en el OTT A/B/C/D) |
| **SEL + OCT-** / **SEL + OCT+** / **SEL + PLAY** | reset del módulo / siguiente tema / slot a fábrica |
| **MIDI in** | notas con velocity, pitch bend ±2, CC1 = MIX del LFO1, CC74 = FREQ del filtro, CC123 = all notes off |

## Tus propios samples de fábrica

Pon `01_NOMBRE.wav` … `08_NOMBRE.wav` en `samples/` (cualquier frecuencia, mono o estéreo, PCM 8/16/24-bit; NOMBRE hasta 8
letras), corre `python3 tools/gen_samples.py` y haz push. Se convierten a 22.05 kHz μ-law, máximo 1.48 s. Los sonidos
afinados deben estar en C4. Los actuales se generan por síntesis y sus WAV están en `samples/` para escucharlos.

## Calidad (v0.3)

- Samples de fábrica a **16-bit** con dither TPDF (antes 8-bit μ-law); las grabaciones se decodifican a 16-bit en RAM.
- **Parámetros suaves y de alta resolución:** todo el motor lee los parámetros en Q8 con un glide de ~6 ms: girar una
  perilla o modular con un LFO ya no produce escalones (zipper); el pitch y el TUNE se modulan en fracciones de
  semitono (vibratos y sirenas continuos).
- Reverb con dos líneas moduladas (menos resonancia metálica); grabación con filtro anti-alias al bajar a 22.05 kHz.
- Sin DC en ninguna configuración, y el limitador ya no se pasa del techo (-0.2 dBFS).

## Límites de esta versión

- **Monofónico.** Polifonía multiplicaría el costo de CPU; podría venir como modo paráfono de 2–4 voces.
- **El FM-1 no tiene entrada de audio:** grabar = re-samplear su propia salida. Subir WAVs desde la computadora en
  tiempo real (por MIDI SysEx con una página web) es posible para una versión futura.
- Samples a 22.05 kHz; las grabaciones del usuario se guardan en flash en 8-bit μ-law (1.48 s por slot: la flash libre
  son 256 KB para 8 slots). Los de fábrica son 16-bit.
- Se quitaron el Seamless Looper y la exportación UCS/BWF de la versión REAPER; el FM-1 es entrada de audio USB para
  grabarlo en REAPER.

## Desarrollo

```sh
sh tests/run_host.sh tests/scripts/nd07_tour.txt   # simulador: build/host/nd07.wav y capturas PNG
python3 tools/gen_nd07_tables.py                  # tablas (seno, tanh, filtros, dB, μ-law...)
python3 tools/gen_samples.py                      # los samples de fábrica
```

`firmware/src/nd07.c` es la app. Reglas del chip: solo enteros, sin división de 64 bits, sin constantes que parezcan
direcciones de ROM (`docs/RULES.md`); revisado compilando a RISC-V de 32 bits sin FPU. Medido en el host: ~1330
instrucciones por muestra con el patch por defecto (OTT incluido) y ~1790 con todo encendido a la vez (MIX + supersaw +
sample estirado + granos + OTT POST estéreo + delay + flanger + reverb + 2 LFOs); la referencia de motores que caben en
el FM-1 es 1000–1500. Delay, flanger y reverb no gastan CPU con su MIX en 0, y el OTT en PRE FX cuesta la mitad que en
POST. El pie de pantalla muestra el CPU real en el aparato: si una combinación extrema pasa de ~90 % y cruje, avísame.

La identidad del paquete es `FM-1_95N`; revisa la lista del Firmware Hub antes de publicar.
