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

echo "=== Building 369ulp firmware ==="
run "build: 369ulp_left"       west build -d build/369ulp_l       -p -b xiao_ble//zmk -- -DSHIELD=369ulp_left
run "build: 369ulp_right"      west build -d build/369ulp_r       -p -b xiao_ble//zmk -- -DSHIELD=369ulp_right

echo "=== Building settings_reset firmware ==="
run "build: 369ulp_left_reset"  west build -d build/369ulp_l_reset  -p -b xiao_ble//zmk -- -DSHIELD=settings_reset
run "build: 369ulp_right_reset" west build -d build/369ulp_r_reset  -p -b xiao_ble//zmk -- -DSHIELD=settings_reset

echo "=== Copying firmware to output ==="
run "copy: 369ulp_left.uf2"       cp build/369ulp_l/zephyr/zmk.uf2        "$OUT_DIR/369ulp_left.uf2"
run "copy: 369ulp_right.uf2"      cp build/369ulp_r/zephyr/zmk.uf2        "$OUT_DIR/369ulp_right.uf2"
run "copy: 369ulp_left_reset.uf2"  cp build/369ulp_l_reset/zephyr/zmk.uf2  "$OUT_DIR/369ulp_left_reset.uf2"
run "copy: 369ulp_right_reset.uf2" cp build/369ulp_r_reset/zephyr/zmk.uf2  "$OUT_DIR/369ulp_right_reset.uf2"

echo ""
echo "=== Results ==="
ok=0; fail=0
for label in \
    "build: 369ulp_left" "build: 369ulp_right" \
    "build: 369ulp_left_reset" "build: 369ulp_right_reset" \
    "copy: 369ulp_left.uf2" "copy: 369ulp_right.uf2" \
    "copy: 369ulp_left_reset.uf2" "copy: 369ulp_right_reset.uf2"
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
