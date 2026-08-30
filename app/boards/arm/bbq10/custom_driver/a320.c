/*
 * A320 optical sensor driver (polling via motion GPIO, Zephyr input subsystem)
 *
 * Module auto-detection: ZitaoTech has shipped BB-keyboard trackpad modules at
 * I2C 0x3B (reg-0x82 burst protocol) and 0x37 / 0x57 (reg-0x0A burst protocol)
 * across production runs, without the published sources always matching the
 * fitted module. Instead of trusting the devicetree address alone, this driver
 * probes the known module types at init (devicetree address first) and latches
 * whichever one ACKs. If nothing ACKs, it keeps retrying from the poll loop
 * (covers slow sensor power-up) and logs the failure.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT avago_a320

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <stdlib.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>

#include "trackpad_led.h"

LOG_MODULE_REGISTER(a320, CONFIG_A320_LOG_LEVEL);

/* =========================
 * Configurable parameters
 * ========================= */

#ifndef CONFIG_A320_POLL_INTERVAL_MS
#define CONFIG_A320_POLL_INTERVAL_MS 2
#endif

/* Interval between module-detection retries while no known module has ACKed. */
#define A320_DETECT_RETRY_MS 500
/* Log a warning every Nth failed detection retry (N * 500ms = every 10s). */
#define A320_DETECT_LOG_EVERY 20
/* After this many failed retries, scan the whole bus once: an ACK at an
 * unlisted address means an unknown module type (report it); no ACK anywhere
 * means the trackpad is unpowered or the flex/connector is faulty. */
#define A320_DETECT_SCAN_ATTEMPT 10

/* =========================
 * HID indicators
 * ========================= */

static zmk_hid_indicators_t current_indicators;
#define HID_INDICATORS_CAPS_LOCK (1 << 1)

/* =========================
 * Motion GPIO (TP_MOTION on P1.01 per Q10 Pro schematic)
 * ========================= */

#define MOTION_GPIO_NODE DT_NODELABEL(gpio1)
#define MOTION_GPIO_PIN 1
static const struct device *motion_gpio_dev;

/* =========================
 * State flags
 * ========================= */

static bool touched = false;
static bool tp_enabled = true; /* runtime trackpad on/off toggle (&tp_toggle) */
static bool ctrl_pressed = false;

/* =========================
 * Data & Config structs
 * ========================= */

struct a320_dev_config {
    struct i2c_dt_spec i2c;
};

typedef int (*a320_read_fn_t)(const struct device *dev, uint16_t addr, int16_t *dx, int16_t *dy);

struct a320_module {
    uint16_t addr;
    a320_read_fn_t read;
    const char *proto;
};

struct a320_data {
    const struct device *dev;
    struct k_work_delayable poll_work;
    const struct a320_module *active; /* NULL until a module has been detected */
    uint32_t detect_attempts;
};

/* =========================
 * Key listener (Ctrl)
 * ========================= */

static int key_listener_cb(const zmk_event_t *eh) {

    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (!ev)
        return 0;

    if (ev->position == 37) {
        ctrl_pressed = ev->state;
    }

    return 0;
}

ZMK_LISTENER(a320_key_listener, key_listener_cb);
ZMK_SUBSCRIPTION(a320_key_listener, zmk_position_state_changed);

/* =========================
 * HID indicator listener
 * ========================= */

static int hid_indicators_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);

    if (ev)
        current_indicators = ev->indicators;

    return ZMK_EV_EVENT_BUBBLE;
}

/* =========================
 * I2C read variants
 *
 * Axis mapping is chassis-specific (sensor mounting), taken from ZitaoTech's
 * Q10 sources for the 0x3B and 0x37 module types. The 0x57 type has never
 * appeared in Q10 sources; it shares the reg-0x0A protocol, so it reuses the
 * 0x37 mapping. If a detected 0x57 module moves the cursor mirrored or
 * rotated, adjust the signs in a320_read_motion_reg0a().
 * ========================= */

/* 0x3B module: burst read 3 bytes from reg 0x82. */
static int a320_read_motion_reg82(const struct device *dev, uint16_t addr, int16_t *dx,
                                  int16_t *dy) {
    const struct a320_dev_config *cfg = dev->config;
    uint8_t buf[3];
    uint8_t reg = 0x82;
    int ret;

    ret = i2c_write(cfg->i2c.bus, &reg, 1, addr);
    if (ret < 0)
        return ret;

    ret = i2c_burst_read(cfg->i2c.bus, addr, 0x82, buf, sizeof(buf));
    if (ret < 0)
        return ret;

    *dx = -(int8_t)buf[2];
    *dy = -(int8_t)buf[1];

    return 0;
}

/* 0x37 / 0x57 modules: burst read 7 bytes from reg 0x0A. */
static int a320_read_motion_reg0a(const struct device *dev, uint16_t addr, int16_t *dx,
                                  int16_t *dy) {
    const struct a320_dev_config *cfg = dev->config;
    uint8_t buf[7];
    uint8_t reg = 0x0A;
    int ret;

    ret = i2c_write(cfg->i2c.bus, &reg, 1, addr);
    if (ret < 0)
        return ret;

    ret = i2c_burst_read(cfg->i2c.bus, addr, 0x0A, buf, sizeof(buf));
    if (ret < 0)
        return ret;

    *dy = (int8_t)buf[3];
    *dx = -(int8_t)buf[1];

    return 0;
}

/* Known ZitaoTech BB-trackpad module types. */
static const struct a320_module a320_modules[] = {
    {0x3B, a320_read_motion_reg82, "reg-0x82"},
    {0x37, a320_read_motion_reg0a, "reg-0x0A"},
    {0x57, a320_read_motion_reg0a, "reg-0x0A"},
};

/* =========================
 * Module detection
 * ========================= */

static int a320_try_detect(const struct device *dev) {
    const struct a320_dev_config *cfg = dev->config;
    struct a320_data *data = dev->data;
    int16_t dx, dy;

    /* Pass 0: the devicetree-configured address. Pass 1: the other known types. */
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < ARRAY_SIZE(a320_modules); i++) {
            const struct a320_module *m = &a320_modules[i];
            bool is_dts_addr = (m->addr == cfg->i2c.addr);

            if ((pass == 0) != is_dts_addr)
                continue;

            if (m->read(dev, m->addr, &dx, &dy) == 0) {
                data->active = m;
                LOG_INF("A320 detected: addr 0x%02X (%s protocol)%s", m->addr, m->proto,
                        is_dts_addr ? "" : " -- differs from devicetree address!");
                return 0;
            }
        }
    }

    data->detect_attempts++;
    if (data->detect_attempts == 1 || (data->detect_attempts % A320_DETECT_LOG_EVERY) == 0) {
        LOG_WRN("A320: no known module ACKed (tried 0x3B 0x37 0x57, attempt %u), retrying...",
                data->detect_attempts);
    }

    if (data->detect_attempts == A320_DETECT_SCAN_ATTEMPT) {
        int found = 0;
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            uint8_t dummy;
            if (i2c_read(cfg->i2c.bus, &dummy, 1, addr) == 0) {
                LOG_WRN("A320 bus scan: device ACKs at 0x%02X -- unknown module type, "
                        "report this address",
                        addr);
                found++;
            }
        }
        if (!found) {
            LOG_ERR("A320 bus scan: nothing ACKs on the trackpad bus -> trackpad unpowered "
                    "or hardware/flex fault");
        }
    }
    return -ENODEV;
}

/* =========================
 * Poll work
 * ========================= */

static void a320_poll_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct a320_data *data = CONTAINER_OF(dwork, struct a320_data, poll_work);

    /* No module found yet: keep trying at a relaxed pace. */
    if (!data->active) {
        if (a320_try_detect(data->dev) != 0) {
            k_work_reschedule(&data->poll_work, K_MSEC(A320_DETECT_RETRY_MS));
            return;
        }
    }

    /* Trackpad disabled via &tp_toggle: stop reporting but keep polling alive. */
    if (!tp_enabled) {
        touched = false;
        k_work_reschedule(&data->poll_work, K_MSEC(CONFIG_A320_POLL_INTERVAL_MS));
        return;
    }

    int pin_state = gpio_pin_get(motion_gpio_dev, MOTION_GPIO_PIN);

    /* One-shot runtime diagnostics: prove whether the motion pin ever asserts
     * and whether reads keep succeeding once a module was detected. */
    static bool seen_low, seen_motion, logged_read_err;
    if (pin_state == 0 && !seen_low) {
        seen_low = true;
        LOG_INF("A320 motion pin asserted LOW (data ready) for the first time");
    }

    if (pin_state == 0) {

        int16_t dx = 0, dy = 0;
        int rc = data->active->read(data->dev, data->active->addr, &dx, &dy);

        if (rc < 0 && !logged_read_err) {
            logged_read_err = true;
            LOG_ERR("A320 motion read failed after detection: err=%d", rc);
        } else if (rc == 0 && !seen_motion && (dx || dy)) {
            seen_motion = true;
            LOG_INF("A320 first motion sample dx=%d dy=%d", dx, dy);
        }

        if (rc == 0 && (dx || dy)) {

            bool capslock = current_indicators & HID_INDICATORS_CAPS_LOCK;

            if (ctrl_pressed) {
                dx /= 2;
                dy /= 2;
            }

            /* ===== Normal mouse mode ===== */

            if (!capslock) {
                uint8_t brt = indicator_tp_get_last_valid_brightness();
                float factor = 0.4f + 0.01f * brt;

                dx = dx * 3 / 2 * factor;
                dy = dy * 3 / 2 * factor;
            }

            if (capslock) {
                input_report_rel(data->dev, INPUT_REL_WHEEL, -dy / 16, true, K_FOREVER);
            } else {
                input_report_rel(data->dev, INPUT_REL_X, dx, false, K_FOREVER);
                input_report_rel(data->dev, INPUT_REL_Y, dy, true, K_FOREVER);
                touched = true;
            }
        }
    } else {
        touched = false;
    }

    k_work_reschedule(&data->poll_work, K_MSEC(CONFIG_A320_POLL_INTERVAL_MS));
}

bool tp_is_touched(void) { return touched; }

/* =========================
 * Enable / disable toggle
 * ========================= */

void a320_set_enabled(bool enabled) {
    tp_enabled = enabled;
    if (!enabled) {
        touched = false;
    }
    LOG_INF("A320 trackpad %s", enabled ? "ENABLED" : "DISABLED");
}

bool a320_is_enabled(void) { return tp_enabled; }

void a320_toggle_enabled(void) { a320_set_enabled(!tp_enabled); }

/* =========================
 * Init
 * ========================= */

static int a320_init(const struct device *dev) {
    const struct a320_dev_config *cfg = dev->config;
    struct a320_data *data = dev->data;

    if (!device_is_ready(cfg->i2c.bus))
        return -ENODEV;

    motion_gpio_dev = DEVICE_DT_GET(MOTION_GPIO_NODE);
    if (!device_is_ready(motion_gpio_dev))
        return -ENODEV;

    gpio_pin_configure(motion_gpio_dev, MOTION_GPIO_PIN, GPIO_INPUT | GPIO_PULL_UP);

    data->dev = dev;
    data->active = NULL;
    data->detect_attempts = 0;

    /* First detection attempt; on failure the poll loop keeps retrying, so a
     * sensor that powers up late is still picked up. */
    a320_try_detect(dev);

    k_work_init_delayable(&data->poll_work, a320_poll_work_handler);

    k_work_schedule(&data->poll_work, K_MSEC(CONFIG_A320_POLL_INTERVAL_MS));

    LOG_INF("A320 init OK (dts addr 0x%02X, module %s)", cfg->i2c.addr,
            data->active ? "detected" : "not detected yet");

    return 0;
}

/* =========================
 * Device define
 * ========================= */

#define A320_INIT_PRIORITY CONFIG_INPUT_A320_INIT_PRIORITY

#define A320_DEFINE(inst)                                                                          \
    static struct a320_data a320_data_##inst;                                                      \
    static const struct a320_dev_config a320_cfg_##inst = {                                        \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, a320_init, NULL, &a320_data_##inst, &a320_cfg_##inst, POST_KERNEL, \
                          A320_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(A320_DEFINE)

ZMK_LISTENER(a320_hid_listener, hid_indicators_listener);
ZMK_SUBSCRIPTION(a320_hid_listener, zmk_hid_indicators_changed);
