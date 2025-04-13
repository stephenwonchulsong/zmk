# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library_amend()

zephyr_library_sources_ifdef(CONFIG_INPUT_TPS43 input_iqs5xx.c)

target_sources_ifdef(CONFIG_ZMK_INPUT_IQS5XX_IDLE_SLEEPER app PRIVATE zmk_iqs5xx_idle_sleeper.c)
