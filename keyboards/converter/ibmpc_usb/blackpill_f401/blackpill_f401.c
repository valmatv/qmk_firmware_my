// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H

void board_init(void) {
    /* B9 is configured as I2C1_SDA in the default board file.
     * Float it so it does not interfere even though I2C is disabled. */
    gpio_set_pin_input_high(B9);
}
