// Copyright 2022 Cole Smith <cole@boadsource.xyz>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/* 0
 *  |   1  |   2  |
 */

 [0] = LAYOUT(
    KC_1, KC_5, KC_9, KC_4,
    KC_2, KC_6, KC_1, KC_5,
    KC_3, KC_7, KC_2, KC_6,
    KC_4, KC_8, KC_3, KC_7
)
};


// qmk compile -kb madman/mad_rp2040 -km default
