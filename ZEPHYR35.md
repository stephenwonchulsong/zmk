# Zephyr35

This branch is the **latest ZMK that builds against Zephyr 3.5**.

ZMK `main` migrated to Zephyr 4.1 (`feat!: Move to zephyr v4.1`) and can no
longer build on Zephyr 3.5, for two independent reasons:

1. **Keymap discovery** moved to `app/boards/post_boards_shields.cmake`, a
   hardware-model-v2 hook that Zephyr 3.5 never invokes. Without it no keymap is
   added to the build and shields that reference behaviors (e.g. `&none` via
   `input/processors.dtsi`) fail with "undefined node label". On this branch the
   discovery lives in `app/keymap-module/modules/modules.cmake`, a module hook
   Zephyr 3.5 does call.
2. The **C sources use Zephyr-4.1 APIs** absent in 3.5 (`INPUT_CALLBACK_DEFINE`
   3-arg form, `BT_LE_ADV_CONN_FAST_2`, newer util macros, ~40+ files).

So the newest ZMK that is genuinely compatible with Zephyr 3.5 is the **v0.3**
line, which this branch is based on.

## Build configuration

- `app/west.yml` pins `zephyr` to `v3.5.0+zmk-fixes`.
- Boards use the legacy name **`seeeduino_xiao_ble`** (not the HWMv2
  `xiao_ble//zmk`).
- The Cirque touchpad needs the out-of-tree module:
  `-DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"`.

Example (369ulptp), from `app/`:

```sh
west build -d build/369ulptp_l  -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_left  -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"
west build -d build/369ulptp_r  -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_right -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"
west build -d build/369ulptp_tp -p -b seeeduino_xiao_ble -- -DSHIELD=369ulptp_tp    -DZMK_EXTRA_MODULES="/workspaces/zmk/app/cirque-input-module-main"
```

See `app/boards/shields/36chambers/369ulptp/build.sh` for the full per-board
build + settings-reset flow.
