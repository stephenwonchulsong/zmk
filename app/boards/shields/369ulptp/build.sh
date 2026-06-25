#!/usr/bin/env bash

APP_DIR="/workspaces/zmk/app"
OUT_DIR="/output"

declare -A results

run() {
    local label="$1"; shift
    if "$@"; then
        results["$label"]="OK"
    else
        results["$label"]="FAIL"
    fi
}

cd "$APP_DIR"

echo "=== Building 369ulptp firmware ==="
run "build: 369ulptp_left"       west build -d build/369ulptp_l       -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_left -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"
run "build: 369ulptp_right"      west build -d build/369ulptp_r       -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_right -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"
run "build: 369ulptp_tp"         west build -d build/369ulptp_tp      -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_tp -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"

echo "=== Building settings_reset firmware ==="
run "build: 369ulptp_left_reset"  west build -d build/369ulptp_l_reset  -p -b seeeduino_xiao_ble -- -DSHIELD=settings_reset
run "build: 369ulptp_right_reset" west build -d build/369ulptp_r_reset  -p -b seeeduino_xiao_ble -- -DSHIELD=settings_reset
run "build: 369ulptp_tp_reset"    west build -d build/369ulptp_tp_reset -p -b seeeduino_xiao_ble -- -DSHIELD=settings_reset

echo "=== Copying firmware to output ==="
run "copy: 369ulptp_left.uf2"       cp build/369ulptp_l/zephyr/zmk.uf2        "$OUT_DIR/369ulptp_left.uf2"
run "copy: 369ulptp_right.uf2"      cp build/369ulptp_r/zephyr/zmk.uf2        "$OUT_DIR/369ulptp_right.uf2"
run "copy: 369ulptp_tp.uf2"         cp build/369ulptp_tp/zephyr/zmk.uf2       "$OUT_DIR/369ulptp_tp.uf2"
run "copy: 369ulptp_left_reset.uf2"  cp build/369ulptp_l_reset/zephyr/zmk.uf2  "$OUT_DIR/369ulptp_left_reset.uf2"
run "copy: 369ulptp_right_reset.uf2" cp build/369ulptp_r_reset/zephyr/zmk.uf2  "$OUT_DIR/369ulptp_right_reset.uf2"
run "copy: 369ulptp_tp_reset.uf2"    cp build/369ulptp_tp_reset/zephyr/zmk.uf2 "$OUT_DIR/369ulptp_tp_reset.uf2"

echo ""
echo "=== Results ==="
ok=0; fail=0
for label in \
    "build: 369ulptp_left" "build: 369ulptp_right" "build: 369ulptp_tp" \
    "build: 369ulptp_left_reset" "build: 369ulptp_right_reset" "build: 369ulptp_tp_reset" \
    "copy: 369ulptp_left.uf2" "copy: 369ulptp_right.uf2" "copy: 369ulptp_tp.uf2" \
    "copy: 369ulptp_left_reset.uf2" "copy: 369ulptp_right_reset.uf2" "copy: 369ulptp_tp_reset.uf2"
do
    if [[ "${results[$label]}" == "OK" ]]; then
        echo "  [OK]   $label"
        ((ok++))
    else
        echo "  [FAIL] $label"
        ((fail++))
    fi
done
echo ""
echo "  $ok succeeded, $fail failed"
