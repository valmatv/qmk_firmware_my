// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H

/*
 * IBM Model M US 101-key layout.
 *
 * Keys not present on a standard Model M are KC_NO:
 *   - F13–F24 (terminal-specific extra row)
 *   - Vol Down / Vol Up / Mute (media keys added post-Model M)
 *   - KC_INT1–INT5 / KC_NUHS / KC_NUBS (Japanese/ISO variants)
 *   - KC_LGUI / KC_RGUI / KC_APP   (Windows keys, added 1994)
 *   - KC_PCMM / KC_PEQL            (numpad comma/equal, not on 101-key)
 *
 * Layers 1–4 are fully transparent; program them in VIA.
 */

#define _TRNS_LAYER \
    LAYOUT( \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, \
        KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS, \
        KC_TRNS, KC_TRNS, KC_TRNS \
    )

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [0] = LAYOUT(
        /* F13–F24 row: not present on Model M */
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,
        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,

        /* Esc */
        KC_ESC,

        /* F1–F12 */
        KC_F1,   KC_F2,   KC_F3,   KC_F4,
        KC_F5,   KC_F6,   KC_F7,   KC_F8,
        KC_F9,   KC_F10,  KC_F11,  KC_F12,

        /* Print Screen / Scroll Lock / Pause  |  Vol keys absent on Model M */
        KC_PSCR, KC_SCRL, KC_PAUS,
        KC_NO,   KC_NO,   KC_NO,

        /* Number row  (INT3 / ¥ absent on US Model M) */
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,
        KC_6,    KC_7,    KC_8,    KC_9,    KC_0,
        KC_MINS, KC_EQL,  KC_NO,   KC_BSPC,

        /* Navigation top  |  Numpad top */
        KC_INS,  KC_HOME, KC_PGUP,
        KC_NUM,  KC_PSLS, KC_PAST, KC_PMNS,

        /* QWERTY row */
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,
        KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,
        KC_LBRC, KC_RBRC, KC_BSLS,

        /* Navigation mid  |  Numpad mid */
        KC_DEL,  KC_END,  KC_PGDN,
        KC_P7,   KC_P8,   KC_P9,   KC_PPLS,

        /* Home row  (NUHS / non-US # absent on US Model M) */
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,
        KC_H,    KC_J,    KC_K,    KC_L,
        KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,

        /* Numpad home  (PCMM / numpad comma absent on 101-key) */
        KC_P4,   KC_P5,   KC_P6,   KC_NO,

        /* Shift row  (NUBS / non-US \ and INT1 / RO absent on US Model M) */
        KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,
        KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,
        KC_NO,   KC_RSFT,

        /* Up arrow  |  Numpad shift row */
        KC_UP,
        KC_P1,   KC_P2,   KC_P3,   KC_PENT,

        /* Bottom row  (LGUI / INT5 / INT4 / INT2 / RGUI / APP absent on Model M) */
        KC_LCTL, KC_NO,   KC_LALT, KC_NO,
        KC_SPC,
        KC_NO,   KC_NO,   KC_RALT, KC_NO,   KC_NO,   KC_RCTL,

        /* Navigation bottom  |  Numpad bottom  (PEQL absent on 101-key) */
        KC_LEFT, KC_DOWN, KC_RGHT,
        KC_P0,   KC_PDOT, KC_NO
    ),

    [1] = _TRNS_LAYER,
    [2] = _TRNS_LAYER,
    [3] = _TRNS_LAYER,
    [4] = _TRNS_LAYER,
};
