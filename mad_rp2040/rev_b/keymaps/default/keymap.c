// Copyright 2022 Cole Smith <cole@boadsource.xyz>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/* 0
 *  |   1  |   2  |
 */

 [0] = LAYOUT(
  KC_1, KC_2
)
};


// qmk compile -kb madman/mad_rp2040 -km default
