# ND-07 // MONOLITH · FM-1 edition · v1.3

Firmware personalizado para el **M-VAVE FM-1**: la estación de SFX ND-07 // MONOLITH (REAPER, Lua ReaImGui + JSFX),
**sampler y sintetizador para diseño de SFX de videojuegos**. 21 páginas (**18 ASSETS** de partida, resonador modal, transient shaper, DESTROY, **VOWEL + SHIFT**,
OTT multibanda, **VARIATION** con semilla, **BATCH + FIRE TEST**, **RTPC**, **SPACE + MOTION**, MORPH A↔B, **CHAOS PAD**
desde el teléfono, **CHIP OSC** de 8 bits, loop START > LOOP > STOP), HAPPY ACCIDENT, locks de parámetros,
UNDO de 16 pasos y 5 temas (EVA-01 por defecto), en C entero para el chip JieLi AC79 (sin FPU).

Construido sobre [Dinghy](https://github.com/zvenson/dinghy) (zvenson): USB, MIDI, actualizaciones, rescate USB,
pantalla y flash. Licencia GPL-3.0-only.

> **Estado: 1.3.** La 0.9 corre en un FM-1 "New" (con todos los módulos encendidos y el Chaos Pad, ~30 % de CPU); lo nuevo de la 1.0 a la 1.3 está probado en el simulador de host (audio WAV + capturas). El `.fwsc` se
> compila en GitHub Actions con la toolchain de JieLi. Pruébalo primero en el emulador. Instalar firmware es bajo tu
> propio riesgo; el [FM-1 Firmware Hub](https://thefirmwarehub.com) te regresa al firmware oficial, y mantener **OCT-**
> al encender entra al rescate USB. La sesión guardada de versiones anteriores no se carga (cambió el formato): arranca
> con los valores de fábrica.
>
> **Datos de otros firmwares:** ND-07 no lee lo que otro firmware personalizado haya guardado en la flash (sesiones,
> samples) y puede sobrescribirlo. Lo más limpio es instalarlo sobre el firmware oficial. Para volver, el Hub reinstala
> el oficial.

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

## Nuevo en v1.3: el orden del trabajo y pantallas en movimiento

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

**Pantallas en movimiento.** Cada página tiene una figura abstracta que se mueve con lo que suena, minimalista: líneas
y puntos de un pixel con estelas que se desvanecen, en los colores del tema. La salida se analiza 25 veces por segundo
(FFT de 256 puntos, 24 bandas de 60 Hz a 11 kHz, el nivel y los ataques); la figura se dibuja en un campo que se
realimenta (gira, se acerca o se aleja, se desvanece) y por eso deja espirales, túneles y rastros. En silencio sigue
respirando despacio.

- **Nada se encima:** la figura solo ocupa lo que la gráfica de la página deja vacío y nunca toca su texto ni sus
  curvas (un borde oscuro de un pixel alrededor de todo lo que la página dibuja). Cada página le da un escenario:
  en SPACE el plano de la izquierda, en RTPC la caja de la curva, en BATCH el espacio entre el título y las lecturas.
- La línea de cada cuadro va nítida; sus estelas, más tenues. Donde la gráfica llena la página (OSC & SAMPLE,
  SAMPLE LAB) solo quedan las estelas.

| Página | Figura |
| --- | --- |
| ASSETS | un espectrómetro pequeño a la derecha de la línea de estado; la cuadrícula queda quieta |
| OSC & SAMPLE | el sonido contra sí mismo un instante después (un retrato de fase), solo estelas |
| CHIP OSC | el retrato en una retícula de pasos de 8 bits |
| GRAIN ENGINE | los granos como un vórtice |
| SAMPLE LAB | el slot enrollado en un anillo, solo estelas |
| ENVELOPES | un radar: la envolvente barrida en círculo, pétalos |
| TRANSIENT | cada golpe, una onda que se expande |
| RESONATOR | los modos como cuerpos en órbita: cada uno más lejos y más rápido, encendidos mientras suenan |
| DESTROY | el retrato como nube de puntos, doblada en su borde (DRIVE) y sacudida |
| VOWEL + SHIFT | un anillo de 48 puntos que el espectro empuja; SHIFT lo gira (arriba hacia afuera, abajo hacia adentro) |
| RES MOD FILTER | el anillo de puntos, dentro de un aro que marca dónde corta el filtro |
| OTT | tres anillos, cada banda respirando |
| DELAY + FLANGER | un túnel: los ecos van hacia adentro, girados por TIME, más largos con FDBK |
| REVERB | una nebulosa de polvo donde están las bandas; DECAY la hace durar |
| SPACE | ondas desde donde está la fuente |
| LFO 1 + 2 | un armonógrafo: LFO 1 de lado a lado, LFO 2 de arriba abajo (la pluma tiene peso) |
| RTPC | crestas del espectro que suben más rápido cuanto más alto el valor |
| CHAOS PAD | el dedo, en remolino |
| VARIATION | una constelación de la semilla: 12 estrellas unidas, que destellan con cada nota |
| PHYSICS | una pelota por golpe que cae y rebota |
| BATCH | una cascada (waterfall) del espectro que se aleja |

- **Capas de historia, nítidas** (la idea de los plugins de Kentaro, que dibujan el pasado del sonido como copias
  más tenues en vez de difuminarlo; las formas son nuestras):
  - VOWEL + SHIFT y RES MOD FILTER: una **topografía** del espectro: 5 contornos cerrados, uno por instantánea (unas 8
    por segundo); el más nuevo afuera y brillante, los viejos adentro y tenues; SHIFT la hace girar.
  - DELAY: los ecos como **losas** que se alejan hacia un punto de fuga sobre su eje: tantas como deja FDBK, cada una
    más chica, un poco más girada (TIME) y tan fuerte como sonaba hace ese tiempo.
  - ENVELOPES: un **dial** con marcas: la salida enrollada alrededor como un anillo dentado, la envolvente como el
    arco de adentro.
- **El valor protagonista** (idea tomada del lenguaje de las pantallas del OP-1, con formas propias): al girar una
  perilla, su valor ocupa la pantalla 1.2 s, grande, con numerales de trazo fino dibujados (no una fuente), en el color
  de esa perilla (naranja si tiene lock); la gráfica queda como sombra debajo y el nombre arriba.
- **OTT y REVERB como sólidos de alambre:** LOW un cilindro, MID un prisma, HIGH una pirámide. En OTT miden lo fuerte
  que suena cada banda, con la ganancia del compresor debajo; en REVERB, cuánto dura cada banda.
- Costo: la pantalla se redibuja a 25 cuadros por segundo mientras algo suena y a 15 en reposo. El audio solo copia
  sus muestras; todo lo demás corre en el loop principal, fuera del audio. En el host cuesta ~0.1 ms por cuadro; en el
  FM-1 hay que verlo en el CPU del pie de pantalla.
- Memoria: el campo usa 13 KB del pool; la copia de la salida, la línea nítida y el borde, 9 KB.

## Nuevo en v1.2: 15 ASSETS, puntos de partida para sound design

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

## Nuevo en v1.1: tema EVA-01 (Acid Eva-01)

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

## Nuevo en v1.0

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

## Nuevo en v0.9: herramientas creativas

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

## Nuevo en v0.8: interfaz rediseñada y correcciones

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

## Nuevo en v0.7: un pilar para assets de juego

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

## Nuevo en v0.6: 14 CHIP OSC

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
| **Android** (lo mejor) | Cable USB-C a USB-C (o adaptador OTG) del teléfono al FM-1. Abre la página en **Chrome**, CONECTAR, acepta el permiso MIDI. El pad XY, la tira de presión, el teclado y ● GESTO funcionan con el dedo; ⛶ = pantalla completa. |
| **Computadora** | Chrome o Edge (Windows, Mac, Linux), cable USB normal. |
| **iPhone / iPad** | Safari no tiene Web MIDI. Usa una app de MIDI con pad XY (TouchOSC, Lemur, MIDI Designer…) con un adaptador USB y mándale CC16, CC17 y CC18. |
| **Inalámbrico** | El FM-1 no tiene Bluetooth ni WiFi usables en este firmware. Desde la computadora puedes rebotar: teléfono → (WiFi, p. ej. TouchOSC) → computadora → USB → FM-1. |

La página necesita **https** o **localhost** (regla de los navegadores para MIDI). Lo más fácil: en tu repositorio,
Settings → Pages → "Deploy from a branch", `main`, carpeta `/ (root)`, y ábrela en
`https://TU_USUARIO.github.io/TU_REPO/web/`. También funciona abriéndola con `python3 -m http.server` en la computadora.

## Los módulos

Cada botón de página lleva a uno o dos módulos: presiónalo otra vez para el siguiente juego de perillas (A/B…) y,
después del último, para el siguiente módulo del mismo botón. **SELECT** recorre las 21 páginas. *(Tabla en el orden de la 1.3.)*

| # | Botón | Módulo | Perillas A | Perillas B (C) |
| --- | --- | --- | --- | --- |
| 01 | HOME | **ASSETS** | ASSET, VARY, HA KIND, OUT | — |
| 02 | HOME ×2 | OSC & SAMPLE (A B C D) | SOURCE, WAVE (15 ondas), SAMPLE, BLEND | B: TUNE, COLOR, STRETCH, REC · C: LOOP, L POS, L LEN, XFADE · D: VOICES, L PITCH, DETUNE, L LEVEL |
| 03 | HOME ×3 | **CHIP OSC** | DUTY, SWEEP, ARP, ARP SPD | STEPS, NOISE, PRESET |
| 04 | ARP | GRAIN ENGINE (sobre el sample) | POS, SIZE, DENS, SPRAY | REV %, GRAIN, FREEZE, RND PIT |
| 05 | ARP ×2 | **SAMPLE LAB** | TOOL, AMOUNT | — |
| 06 | ENV | ENVELOPES (A B C) | ATTACK, DECAY, SUSTAIN, RELEASE | B: GLIDE, VELO, PITCH, OUT · C: M ATK, M DEC, M AMT, M TGT |
| 07 | ENV ×2 | **TRANSIENT** | ATTACK, SUSTAIN, TIME, OUT | — |
| 08 | EDIT | **RESONATOR** | MATERL, PITCH, DECAY, RES MIX | DAMP, SPREAD, HIT POS, KEYTRK |
| 09 | EDIT ×2 | **DESTROY** | DRIVE, SHAPE, BITS, RATE | RING HZ, RNG MIX, TONE, MIX |
| 10 | EDIT ×3 | **VOWEL + SHIFT** (A B C) | VOWEL, SIZE, RES, V MIX | B: SHIFT, FDBK, S TIME, S MIX · C: GROWL, G RATE |
| 11 | EDIT ×4 | RES MOD FILTER | FREQ, RES, MODE, SLOPE | DEPTH, RATE, MOD SRC, ANALOG |
| 12 | GLO | OTT MULTIBAND (A B C D) | A: DEPTH, TIME, UPWD, DOWNWD · B: IN, OUT, STYLE, PLACE | C: LOW, MID, HIGH, THRESH · D: X-LO, X-HI, SOLO |
| 13 | FX | DELAY + FLANGER | TIME, FDBK, DLY MIX, ROUTE | RATE, DEPTH, REGEN, FLG MIX |
| 14 | FX ×2 | MULTIBAND REVERB | SIZE, DECAY, PRE-DLY, REV MIX | LOW x, HIGH x, X-LO, X-HI |
| 15 | FX ×3 | **SPACE + MOTION** | DIST, OCCLUDE, PASS-BY, DOPPLER | — |
| 16 | LFO | LFO 1 + 2 | SHAPE, RATE, MIX, TARGET (LFO 1) | SHAPE, RATE, MIX, TARGET (LFO 2) |
| 17 | LFO ×2 | **RTPC** (A B C) | INTENS, SWEEP, CURVE | B: DEST 1, AMT 1, DEST 2, AMT 2 · C: DEST 3, AMT 3 |
| 18 | LFO ×3 | **CHAOS PAD + KEYS** (A B C) | A: X TGT, X AMT, Y TGT, Y AMT | B: PAD, GESTURE, KEY VEL, SENS · C: AT TGT, AT AMT |
| 19 | SEQ | **VARIATION + MORPH** | VARY, SEED, KEYS, RESEED | MORPH, HA AMT, EVOLVE, HA KIND |
| 20 | SEQ ×2 | **PHYSICS TRIGGER** | TRIGGER, HITS, TIME, FALL | PITCH, JITTER |
| 21 | SEQ ×3 | **BATCH + FIRE TEST** (A B C) | BATCH (modo), COUNT, SPACE, GATE | B: RATE, POOL, ORDER · C: MATCH, TARGET |

<details><summary>Tabla de la v0.6 (para comparar)</summary>

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

</details>

La cadena de audio (mono): FUENTE (osc / sample / granos) × ENV → RESONATOR → TRANSIENT → DESTROY → FILTER → OTT (PRE) →
DELAY + FLANGER → REVERB → OTT (POST) → SPACE (aire / pared / distancia) → salida. LAYER suma sus capas en la FUENTE.

### Nuevo en v0.4

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

### Lo que ya estaba

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
| **SEL (toque)** | la acción de la página: **01** cargar y tocar el asset · **02** REC ARM / STOP · **03** un sonido nuevo del PRESET · **05** aplicar la herramienta · **15** tocar una pasada · **17** barrido RTPC · **18** grabar un gesto · **19** A: semilla nueva, B: guardar A, luego B · **20** tocar el patrón · **21** BATCH (A, C) o FIRE TEST (B) / detener · las demás: aleatorizar el módulo (respeta locks) | |

Cambios de la 1.3: el tema pasó de SEL + OCT+ a **SEL + ALGORITHM**; SEL + OCT+ ahora aleatoriza el módulo (el par
de SEL + OCT−, reset). Guardar A / B en VARIATION B pasó a **SEL** (A primero, luego B; con los dos, el más cercano a
MORPH). Regresar el slot a su sonido de fábrica pasó de SEL + PLAY a **SAMPLE LAB → TOOL FACTORY**. Son nuevos SEL +
SELECT, SEL + PRESETS y SEL + PLAY (stop all).

| **MIDI in** | notas con velocity, pitch bend ±2, CC1 = MIX del LFO1, **CC11 = MORPH**, CC74 = FREQ del filtro, **CC16 / CC17 / CC18 = pad X / Y / tocar**, **CC19 = armar la grabación de gesto**, channel y poly pressure = aftertouch, CC123 = all notes off |
| **SysEx** | `F0 7D 4E 37 cmd … F7`: samples desde `web/index.html` (HELLO, BEGIN slot largo, DATA μ-law en 7 bits, END guardar) y de vuelta (LEN: largo y nombre del slot activo; READ pos: 128 muestras de 16 bits) |

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
| LAYER 2 / 3 (0.7, estimado) | la FUENTE ×2 / ×3 (con SUPRSAW, lo más caro) | VOICES MONO = 0 · en SAMPLE sólo con STRETCH 1.00x |
| SHEPARD (0.7, estimado) | ~ como SUPRSAW | — |
| SPACE, RTPC (0.7) | muy poco | DIST, OCCLUDE, PASS-BY en 0 / INTENS 0 = 0 |

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
sh tests/run_host.sh tests/scripts/nd07_v04.txt    # resonador, transient, DESTROY, MORPH y BATCH: build/host/nd07.wav y PNG
sh tests/run_host.sh tests/scripts/nd07_tour.txt   # el recorrido de los módulos anteriores
sh tests/run_host.sh tests/scripts/pad.txt         # el CHAOS PAD y el aftertouch por MIDI
sh tests/run_host.sh tests/scripts/import.txt      # un sample por SysEx, como lo manda la página web
sh tests/run_host.sh tests/scripts/chip.txt        # 02 CHIP: un preset de cada tipo
sh tests/run_host.sh tests/scripts/variation.txt   # 14 VARIATION: teclas = variaciones con semilla
sh tests/run_host.sh tests/scripts/sustain.txt     # 01 LOOP SUSTAIN: START > LOOP > STOP
sh tests/run_host.sh tests/scripts/motion.txt      # 18 RTPC (barrido) y 13 SPACE (PASS-BY)
sh tests/run_host.sh tests/scripts/layers.txt      # 01 D: LAYER 3 y la onda SHEPARD
sh tests/run_host.sh tests/scripts/bounce.txt      # REC BOUNCE y BATCH con MATCH
sh tests/run_host.sh tests/scripts/overdub.txt     # REC OVERDUB
sh tests/run_host.sh tests/scripts/physics.txt     # 16 PHYSICS TRIGGER: BOUNCE, DEBRIS, SCATTER
sh tests/run_host.sh tests/scripts/lab.txt         # 05 SAMPLE LAB: REVERSE, 1/2 SPD, REVERT
sh tests/run_host.sh tests/scripts/modenv.txt      # 06 C MOD ENV y las recetas del HAPPY ACCIDENT
sh tests/run_host.sh tests/scripts/assets.txt      # 15 ASSETS: escoger, SEL carga y toca, HAPPY ACCIDENT en la familia
cc -O2 -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o /tmp/at tests/assets_test.c -lm && /tmp/at   # los 18 como WAV + niveles
sh tests/run_host.sh tests/scripts/screens.txt     # todas las páginas y juegos (capturas para revisar el tema)
python3 tools/gen_screens_tour.py                  # regenera screens.txt si cambian las páginas
sh tests/run_host.sh tests/scripts/eva_states.txt  # EVA-01: perilla recién girada, lock, HAPPY ACCIDENT, modulación
python3 tools/gen_eva_font.py                      # regenera firmware/gen/eva_font.h (fuentes del tema EVA-01)
sh tests/run_host.sh tests/scripts/gesture.txt     # 20 GESTURE: grabado (SEL y CC19) y repetido por la nota
sh tests/run_host.sh tests/scripts/vox.txt         # 09 VOWEL + SHIFT: shift arriba/abajo, la espiral, U O A E I
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
