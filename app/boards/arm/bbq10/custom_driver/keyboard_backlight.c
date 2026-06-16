/*
 * Copyright (c) 2023 ZitaoTech
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/activity.h>
#include <zmk/keymap.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_keyboard_backlight),
             "keyboard_backlight: No zmk_keyboard_backlight chosen node found");

static const struct device *const indiled_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_keyboard_backlight));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))
#define INDICATOR_LED_NUM_LEDS (DT_NUM_CHILD(DT_CHOSEN(zmk_keyboard_backlight)))

/* Layer indices — must match #defines in bbq10.keymap */
#define LAYER_DEFAULT 0
#define LAYER_LOWER   1
#define LAYER_UPPER   2
#define LAYER_ADJUST  3

#define BRT_STATIC 80

#define CYCLE_BRT_MIN  10
#define CYCLE_BRT_MAX  100
#define CYCLE_BRT_STEP 5
/* A full breathe cycle is (MAX-MIN)/STEP steps up + the same down = 36 steps.
 * Per-step interval = 1000 / (freq_hz * 36). */
#define CYCLE_INTERVAL_LOWER_MS  28 /* LOWER breathe:  1 Hz */
#define CYCLE_INTERVAL_UPPER_MS  14 /* UPPER breathe:  2 Hz */
#define CYCLE_INTERVAL_ADJUST_MS 7  /* ADJUST breathe: 4 Hz */

static bool prev_active = false;
static int prev_layer = -1;
static uint8_t cycle_brightness = CYCLE_BRT_MIN;
static bool cycle_direction_up = true;

static struct k_work_delayable polling_work;
static struct k_work_delayable cycle_work;

static void set_led_brightness(uint8_t level) {
    if (!device_is_ready(indiled_dev)) {
        LOG_ERR("Indicator LED device not ready");
        return;
    }
    for (int i = 0; i < INDICATOR_LED_NUM_LEDS; i++) {
        int err = led_set_brightness(indiled_dev, i, level);
        if (err < 0) {
            LOG_ERR("Failed to set LED[%d] brightness: %d", i, err);
        }
    }
}

/* Breathe interval for the layers that use it, 0 if the layer doesn't breathe. */
static uint32_t cycle_interval_for_layer(int layer) {
    switch (layer) {
    case LAYER_LOWER:  return CYCLE_INTERVAL_LOWER_MS;
    case LAYER_UPPER:  return CYCLE_INTERVAL_UPPER_MS;
    case LAYER_ADJUST: return CYCLE_INTERVAL_ADJUST_MS;
    default:           return 0;
    }
}

/* LOWER (1 Hz), UPPER (2 Hz), ADJUST (4 Hz): breathe */
static void cycle_work_handler(struct k_work *work) {
    uint32_t interval = cycle_interval_for_layer(prev_layer);
    if (!prev_active || interval == 0) {
        set_led_brightness(0);
        return;
    }

    set_led_brightness(cycle_brightness);

    if (cycle_direction_up) {
        cycle_brightness += CYCLE_BRT_STEP;
        if (cycle_brightness >= CYCLE_BRT_MAX) {
            cycle_brightness = CYCLE_BRT_MAX;
            cycle_direction_up = false;
        }
    } else {
        if (cycle_brightness < CYCLE_BRT_STEP) {
            cycle_brightness = CYCLE_BRT_MIN;
            cycle_direction_up = true;
        } else {
            cycle_brightness -= CYCLE_BRT_STEP;
        }
    }
    k_work_reschedule(&cycle_work, K_MSEC(interval));
}

static void polling_work_handler(struct k_work *work) {
    bool active = (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
    int current_layer = zmk_keymap_highest_layer_active();

    if (current_layer != prev_layer || active != prev_active) {
        prev_layer = current_layer;
        prev_active = active;

        k_work_cancel_delayable(&cycle_work);
        cycle_brightness = CYCLE_BRT_MIN;
        cycle_direction_up = true;

        if (!active) {
            /* Idle: turn the backlight off regardless of layer. */
            set_led_brightness(0);
        } else {
            switch (current_layer) {
            case LAYER_DEFAULT:
                set_led_brightness(BRT_STATIC);
                break;

            case LAYER_LOWER:
            case LAYER_UPPER:
            case LAYER_ADJUST:
                k_work_reschedule(&cycle_work, K_MSEC(100));
                break;

            default:
                set_led_brightness(0);
                break;
            }
        }
    }

    k_work_reschedule(&polling_work, K_MSEC(100));
}

static int keyboardbacklight_init(void) {
    if (!device_is_ready(indiled_dev)) {
        LOG_ERR("LED indicator device not ready");
        return -ENODEV;
    }

    prev_active = (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
    prev_layer = -1;

    k_work_init_delayable(&polling_work, polling_work_handler);
    k_work_init_delayable(&cycle_work, cycle_work_handler);

    k_work_reschedule(&polling_work, K_MSEC(100));
    return 0;
}

SYS_INIT(keyboardbacklight_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
