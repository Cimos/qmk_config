// Copyright 2022 Cole Smith <cole@boadsource.xyz>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

 [0] = LAYOUT(
    KC_NUM, KC_7, KC_8, KC_9,
    KC_LSFT, KC_4, KC_5, KC_6,
    KC_MINS, KC_1, KC_2, KC_3,
    KC_PLUS, KC_0, KC_DOT, KC_ENT
)
};

void keyboard_post_init_user(void) {
    // Customise these values to desired behaviour
    debug_enable=true;
    debug_matrix=true;
    debug_keyboard=true;
    //debug_mouse=true;
  }
