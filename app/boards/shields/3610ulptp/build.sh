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

echo "=== Building 3610ulptp firmware ==="
run "build: 3610ulptp_left"       west build -d build/3610ulptp_l       -p -b "xiao_ble//zmk" -- -DSHIELD=3610ulptp_left
run "build: 3610ulptp_right"      west build -d build/3610ulptp_r       -p -b "xiao_ble//zmk" -- -DSHIELD=3610ulptp_right
run "build: 3610ulptp_tp"         west build -d build/3610ulptp_tp      -p -b "xiao_ble//zmk" -- -DSHIELD=3610ulptp_tp

echo "=== Building settings_reset firmware ==="
run "build: 3610ulptp_left_reset"  west build -d build/3610ulptp_l_reset  -p -b "xiao_ble//zmk" -- -DSHIELD=settings_reset
run "build: 3610ulptp_right_reset" west build -d build/3610ulptp_r_reset  -p -b "xiao_ble//zmk" -- -DSHIELD=settings_reset
run "build: 3610ulptp_tp_reset"    west build -d build/3610ulptp_tp_reset -p -b "xiao_ble//zmk" -- -DSHIELD=settings_reset

echo "=== Copying firmware to output ==="
run "copy: 3610ulptp_left.uf2"       cp build/3610ulptp_l/zephyr/zmk.uf2        "$OUT_DIR/3610ulptp_left.uf2"
run "copy: 3610ulptp_right.uf2"      cp build/3610ulptp_r/zephyr/zmk.uf2        "$OUT_DIR/3610ulptp_right.uf2"
run "copy: 3610ulptp_tp.uf2"         cp build/3610ulptp_tp/zephyr/zmk.uf2       "$OUT_DIR/3610ulptp_tp.uf2"
run "copy: 3610ulptp_left_reset.uf2"  cp build/3610ulptp_l_reset/zephyr/zmk.uf2  "$OUT_DIR/3610ulptp_left_reset.uf2"
run "copy: 3610ulptp_right_reset.uf2" cp build/3610ulptp_r_reset/zephyr/zmk.uf2  "$OUT_DIR/3610ulptp_right_reset.uf2"
run "copy: 3610ulptp_tp_reset.uf2"    cp build/3610ulptp_tp_reset/zephyr/zmk.uf2 "$OUT_DIR/3610ulptp_tp_reset.uf2"

echo ""
echo "=== Results ==="
ok=0; fail=0
for label in \
    "build: 3610ulptp_left" "build: 3610ulptp_right" "build: 3610ulptp_tp" \
    "build: 3610ulptp_left_reset" "build: 3610ulptp_right_reset" "build: 3610ulptp_tp_reset" \
    "copy: 3610ulptp_left.uf2" "copy: 3610ulptp_right.uf2" "copy: 3610ulptp_tp.uf2" \
    "copy: 3610ulptp_left_reset.uf2" "copy: 3610ulptp_right_reset.uf2" "copy: 3610ulptp_tp_reset.uf2"
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
