#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Dinghy's host tests (no hardware). Run from the repo root after building, the SMK-37 first, the FM-1 last
# (both boards write the same build/ intermediates; the tests expect the FM-1 binaries):
#   BOARD=smk37 sh build.sh; BOARD=smk37 MODEL=elite sh build.sh; BOARD=smk37 MODEL=mke sh build.sh; sh build.sh
#   sh tests/run_tests.sh
# The FM-1 app on the host (tests/run_host.sh), MIDI and the SMK-37 on the host (tests/scripts), the flash map, the
# update path (ota.c, the loader, tools/fm1_install.py; from the fm1-to-smk37 kit's reference layer,
# genkigenki/sloop-fm1 smk37) and the packages.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # (fm1pkg_make.py: the SDK files of a package)
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

[ -f build/nd07.fwsc ] || { echo "run ./build.sh first"; exit 1; }

run "FM-1 app on the host: the default script plays, screens and sound" sh tests/run_host.sh
run "SMK-37 app on the host: the default script" env BOARD=smk37 sh tests/run_host.sh
run "FM-1 MIDI: USB, the TRS jack, a clock, the channel" sh tests/run_host.sh tests/scripts/midi.txt
run "SMK-37: velocity, a pad, an outer key, the pitch wheel" env BOARD=smk37 sh tests/run_host.sh tests/scripts/smk37.txt
run "ND-07: the CHAOS PAD and the aftertouch over MIDI" sh tests/run_host.sh tests/scripts/pad.txt
run "ND-07: a sample over SysEx, as web/index.html sends it" sh tests/run_host.sh tests/scripts/import.txt
run "ND-07: 14 CHIP OSC, a preset of each kind" sh tests/run_host.sh tests/scripts/chip.txt
run "ND-07: VARIATION keys, the seeded hits" sh tests/run_host.sh tests/scripts/variation.txt
run "ND-07: SUSTAIN loop, START > LOOP > STOP" sh tests/run_host.sh tests/scripts/sustain.txt
run "ND-07: RTPC sweep, SPACE + MOTION pass-by" sh tests/run_host.sh tests/scripts/motion.txt
run "ND-07: LAYER 3, the SHEPARD wave" sh tests/run_host.sh tests/scripts/layers.txt
run "ND-07: REC BOUNCE, BATCH with level MATCH" sh tests/run_host.sh tests/scripts/bounce.txt
run "ND-07: REC OVERDUB" sh tests/run_host.sh tests/scripts/overdub.txt
run "ND-07: PHYSICS TRIGGER: BOUNCE, DEBRIS, SCATTER" sh tests/run_host.sh tests/scripts/physics.txt
run "ND-07: SAMPLE LAB: REVERSE, HALF SPD, REVERT" sh tests/run_host.sh tests/scripts/lab.txt
run "ND-07: MOD ENV, the HAPPY ACCIDENT's recipes" sh tests/run_host.sh tests/scripts/modenv.txt
run "ND-07: the pad's GESTURE recorded and replayed" sh tests/run_host.sh tests/scripts/gesture.txt
run "ND-07: VOWEL + SHIFT: shift up and down, the feedback spiral, U O A E I" sh tests/run_host.sh tests/scripts/vox.txt
run "ND-07: the slot back out as a WAV (SysEx LEN / READ)" sh tests/run_host.sh tests/scripts/export.txt
run "ND-07: 15 ASSETS: pick, SEL loads and plays, HAPPY ACCIDENT in the family, a PHYSICS asset" sh tests/run_host.sh tests/scripts/assets.txt
run "ND-07 EVA-01 theme: every page and knob set on screen" sh tests/run_host.sh tests/scripts/screens.txt
run "ND-07 EVA-01 theme: a knob just turned, a lock, HAPPY ACCIDENT, modulated" sh tests/run_host.sh tests/scripts/eva_states.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/eva_fit_test" tests/eva_fit_test.c
run "ND-07 EVA-01 theme: every name and every value of every knob fits its cell, no cut text" "$OUT/eva_fit_test"
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/assets_test" tests/assets_test.c -lm
run "ND-07: the 18 ASSETS rendered (build/host/assets), each as loud as the others, none clipped" "$OUT/assets_test"
$CC -o "$OUT/sense_test" tests/sense_test.c
run "ND-07: key velocity and aftertouch from synthetic presses" "$OUT/sense_test"

$CC -DBOARD_SMK37=1 -Ifirmware/hal -o "$OUT/smk_flash_test" tests/smk_flash_test.c
run "SMK-37 flash map: settings, blobs, owner tag, slots in the store; never the head, BTIF/USRTRIM, USR, key_mac" "$OUT/smk_flash_test"
$CC -o "$OUT/smk_wheel_test" tests/smk_wheel_test.c
run "SMK-37 pitch wheel: the bend is back to 0 at rest, however slowly the wheel was moved" "$OUT/smk_wheel_test"

$CC -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (FM-1)" "$OUT/ota_test" build/nd07.fwsc
head -c 60000 build/nd07.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_screen_test" tests/ldr_screen_test.c
run "update loader's screen (from Skiff): UPDATE MODE, WRITING and its bar, UPDATE FAILED" "$OUT/ldr_screen_test" "$OUT"
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader (FM-1): other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/nd07.fwsc

# SMK-37: the OTA staging and the loader windows on SMK-layout packages (made here from the FM-1 binaries: only the
# layout and the protocol are under test), then the real SMK packages
python3 tools/fm1pkg_make.py --board smk37 build/nd07.bin build/loader/ota.bin "$OUT/smk_new.fwsc" >/dev/null
python3 tools/fm1pkg_make.py --board smk37 "$OUT/old_app.bin" build/loader/ota.bin "$OUT/smk_old.fwsc" >/dev/null
run "SMK-37 package layout (fm1pkg_make --board smk37): PRCT 0x9D000, stock region table" \
    python3 tests/smk_pkg_test.py "$OUT/smk_new.fwsc" "SMK-37 Pro_900"
$CC -DBOARD_SMK37=1 -o "$OUT/ota_test_smk" tests/ota_test.c
run "M-UPGRADE entry, SMK-37 Pro (staging 0xBB000, parm ota-SMK-37 Pro_016)" "$OUT/ota_test_smk" "$OUT/smk_new.fwsc"
for m in 1 2; do
    $CC -DBOARD_SMK37=1 -DT_MODEL=$m -o "$OUT/ota_test_smk_m$m" tests/ota_test.c
    run "M-UPGRADE entry, SMK-37 model $m (1 Elite, 2 MKE-P37): identity and parm under its name" "$OUT/ota_test_smk_m$m" "$OUT/smk_new.fwsc"
done
$CC -DBOARD_SMK37=1 -o "$OUT/ldr_test_smk" tests/ldr_test.c
run "update loader, SMK-37: app area 0x4000-0x9CFFF, protected ranges untouched" "$OUT/ldr_test_smk" "$OUT/smk_old.fwsc" "$OUT/smk_new.fwsc"
found=0
for m in ":SMK-37 Pro_4707" "-elite:SMK-37 Elite_4707" "-mke:MKE-P37_4707"; do
    for f in build/nd07*-smk37${m%%:*}.fwsc; do
        [ -f "$f" ] || continue
        found=1
        v=$(echo "$f" | sed -n 's/.*nd07-[0-9]\.\([0-9]\).*/\1/p'); v=${v:-0}
        run "SMK-37 package $f: layout, identity ${m#*:}$v, the loader marker" python3 tests/smk_pkg_test.py "$f" "${m#*:}$v"
    done
done
[ "$found" = 1 ] || echo "(no SMK-37 packages in build/: BOARD=smk37 sh build.sh first)"

run "installer (tools/fm1_install.py): FM-1 and SMK-37, install and the update-mode resume check the model" python3 tests/install_test.py

[ "$fail" = 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit "$fail"
