// Copyright 2022 Cole Smith <cole@boadsource.xyz>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

 [0] = LAYOUT(
// * |------+------+------+------+|     |------+------+------+------+|
       KC_2,  KC_3,  KC_4,  KC_5,         KC_6,  KC_7,  KC_8,  KC_9,
// * |------+------+------+------+|     |------+------+------+------+|
       KC_W,  KC_E,  KC_R,  KC_T,         KC_Y,  KC_U,  KC_I,  KC_O,
// * |------+------+------+------+|     |------+------+------+------+|
       KC_S,  KC_D,  KC_F,  KC_G,         KC_H,  KC_J,  KC_K,  KC_L,
// * |------+------+------+------+|     |------+------+------+------+|
       KC_Z,  KC_X,  KC_C,  KC_V,         KC_B,  KC_N,  KC_M, KC_COMM
// * |------+------+------+------+|     |------+------+------+------+|
)
};

void keyboard_post_init_user(void) {
    // Customise these values to desired behaviour
    debug_enable=true;
    debug_matrix=true;
    debug_keyboard=true;
    //debug_mouse=true;
  }
