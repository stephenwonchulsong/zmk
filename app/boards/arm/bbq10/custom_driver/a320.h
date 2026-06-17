/*
 * Copyright (c) 2023 ZitaoTech
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef A320_H
#define A320_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief check if the touchpad is touched
 *
 * @return true if touched
 */
bool tp_is_touched(void);

/**
 * @brief enable or disable trackpad motion reporting at runtime
 */
void a320_set_enabled(bool enabled);

/**
 * @brief query whether trackpad motion reporting is currently enabled
 *
 * @return true if enabled
 */
bool a320_is_enabled(void);

/**
 * @brief toggle trackpad motion reporting on/off
 */
void a320_toggle_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // A320_H
