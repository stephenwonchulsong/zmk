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
#define LAYER_GAME    2
#define LAYER_UPPER   3
#define LAYER_ADJUST  4

#define BRT_STATIC 90

#define BRT_BLINK_HIGH 100
#define BRT_BLINK_LOW  0

/* LOWER blink: 2 Hz → 250 ms half-period */
#define BLINK_INTERVAL_LOWER_MS  250
/* ADJUST blink: 4 Hz → 125 ms half-period */
#define BLINK_INTERVAL_ADJUST_MS 125

#define CYCLE_BRT_MIN  10
#define CYCLE_BRT_MAX  100
#define CYCLE_BRT_STEP 5
/* UPPER breathe: 1 Hz → 36 steps per full cycle → 28 ms per step */
#define CYCLE_INTERVAL_UPPER_MS 28

static bool prev_active = false;
static int prev_layer = -1;
static bool blink_on = false;
static uint8_t cycle_brightness = CYCLE_BRT_MIN;
static bool cycle_direction_up = true;

static struct k_work_delayable polling_work;
static struct k_work_delayable blink_work;
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

/* LOWER (2 Hz) and ADJUST (4 Hz): blink */
static void blink_work_handler(struct k_work *work) {
    if (prev_layer != LAYER_LOWER && prev_layer != LAYER_ADJUST) {
        set_led_brightness(0);
        return;
    }

    blink_on = !blink_on;
    set_led_brightness(blink_on ? BRT_BLINK_HIGH : BRT_BLINK_LOW);
    uint32_t interval = (prev_layer == LAYER_LOWER) ? BLINK_INTERVAL_LOWER_MS
                                                    : BLINK_INTERVAL_ADJUST_MS;
    k_work_reschedule(&blink_work, K_MSEC(interval));
}

/* UPPER (1 Hz): breathe */
static void cycle_work_handler(struct k_work *work) {
    if (prev_layer != LAYER_UPPER) {
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
    k_work_reschedule(&cycle_work, K_MSEC(CYCLE_INTERVAL_UPPER_MS));
}

static void polling_work_handler(struct k_work *work) {
    bool active = (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
    int current_layer = zmk_keymap_highest_layer_active();

    if (current_layer != prev_layer || active != prev_active) {
        prev_layer = current_layer;
        prev_active = active;

        k_work_cancel_delayable(&blink_work);
        k_work_cancel_delayable(&cycle_work);
        blink_on = false;
        cycle_brightness = CYCLE_BRT_MIN;
        cycle_direction_up = true;

        switch (current_layer) {
        case LAYER_DEFAULT:
            set_led_brightness(BRT_STATIC);
            break;

        case LAYER_LOWER:
            set_led_brightness(BRT_BLINK_LOW);
            k_work_reschedule(&blink_work, K_MSEC(BLINK_INTERVAL_LOWER_MS));
            break;

        case LAYER_GAME:
            set_led_brightness(0);
            break;

        case LAYER_UPPER:
            k_work_reschedule(&cycle_work, K_MSEC(100));
            break;

        case LAYER_ADJUST:
            set_led_brightness(BRT_BLINK_LOW);
            k_work_reschedule(&blink_work, K_MSEC(BLINK_INTERVAL_ADJUST_MS));
            break;

        default:
            set_led_brightness(0);
            break;
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
    k_work_init_delayable(&blink_work, blink_work_handler);
    k_work_init_delayable(&cycle_work, cycle_work_handler);

    k_work_reschedule(&polling_work, K_MSEC(100));
    return 0;
}

SYS_INIT(keyboardbacklight_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
