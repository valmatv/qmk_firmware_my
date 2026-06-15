/*
Copyright 2010,2011,2012,2013,2019 Jun WAKO <wakojun@gmail.com>
Copyright 2021 Markus Fritsche <fritsche.markus@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

/* key matrix size: 8x16 = 128 universal positions */
#define MATRIX_ROWS 8
#define MATRIX_COLS 16

/* key combination to enter command mode: Shift+Alt+Ctrl */
#define IS_COMMAND() ( \
    get_mods() == (MOD_BIT(KC_LSFT) | MOD_BIT(KC_LALT) | MOD_BIT(KC_LCTL)) || \
    get_mods() == (MOD_BIT(KC_RSFT) | MOD_BIT(KC_RALT) | MOD_BIT(KC_RCTL)) \
)
