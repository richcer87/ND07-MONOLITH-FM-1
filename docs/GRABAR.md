# Grabar pruebas del FM-1

Sirve para comparar el FM-1 con el simulador y con los chips de verdad (o sus emuladores). Lo mide `tools/compare.py`,
que encuentra las notas solo: no hace falta tocar a tiempo, solo las mismas notas en el mismo orden.

## Preparación

- La salida del FM-1 a una entrada del SSL2 MKII (línea o instrumento, sin efectos). En Reaper una pista mono a 44.1 o
  48 kHz, 24 bits, sin plugins. Ganancia para que lo más fuerte quede cerca de −6 dBFS, nunca en rojo.
- Medio segundo de silencio al principio de cada toma (el programa mide ahí el piso de ruido).
- Exporta cada toma como WAV (24 bits o float) y adjúntala en el chat.

## Toma 1: la toma de referencia (FM-1 contra el simulador)

Es la misma que hace `tests/scripts/ref.txt` en el simulador. Comprueba que el FM-1 suena igual que el código.

1. **02 ASSETS A**: ASSET en **SYNTH**, VARY en 0 %, SEL para cargarlo (suena una vez; déjalo en la grabación).
2. **01 ENGINE A**, K1 en **ND07**. Toca **C4** un segundo, suelta, medio segundo, **C5** un segundo.
3. K1 un paso a la derecha (**YM2149**) y las mismas dos notas. Luego **YM2612**, **SPC700**, **SCSP** y **DMG**.

Sin tocar otras perillas. C4 y C5 por MIDI si tienes un teclado; si no, con las teclas del FM-1 y OCT. Después:

```
sh tests/run_host.sh tests/scripts/ref.txt                       # el simulador: build/host/nd07.wav
python3 tools/compare.py build/host/nd07.wav toma1.wav --ref --png toma1.png
```

## Toma 2: los chips de verdad (opcional, y la más valiosa)

Las mismas notas de dos sonidos muy simples, en el FM-1 y en la referencia: una consola, un emulador preciso
(Genesis Plus GX con Nuked, SameBoy, ares) o un tracker que use esos núcleos (Furnace). En el FM-1, ENGINE A y B:

| Chip | En el FM-1 | En la referencia |
| --- | --- | --- |
| YM2149 | MIXER TONE, CHORD OFF, ENV OFF, SID 0, DIGI 0 | un canal, tono, volumen 15, sin envolvente |
| DMG | DUTY 50 %, PULSE 2 OFF, WAVE OFF, NOISE OFF, ENV OFF, ZOMBIE OFF | canal 1, duty 50 %, volumen 15, sin envolvente |

En 06 ENVELOPES deja ATTACK 0, SUSTAIN 100 %, RELEASE corto, para que la envolvente no tape la del chip.
`python3 tools/compare.py referencia.wav fm1.wav --png toma2.png`

## Video

Un video corto de la pantalla mientras grabas la toma 1 (sobre todo el pie de pantalla con la CPU al pasar por
cada motor) y, si puedes, tocando algo con SPC700 y YM2612 y varios módulos encendidos: lo que más pesa.

## Qué dice el programa

Por cada par de notas: el tono (y cuántos cents se separan), el nivel, el brillo (el centroide espectral), los
armónicos 1 a 8, el ruido que no cae en los armónicos, el ataque y el decaimiento. Marca con **!** lo que se separa
más de 10 cents o de 3 dB. No escucha por ti: señala dónde escuchar.
