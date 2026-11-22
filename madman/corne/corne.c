/*
Copyright 2019 @foostan
Copyright 2020 Drashna Jaelre <@drashna>

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

#include "quantum.h"
#include "split_util.h"
#include "print.h"
#include "transactions.h"
#ifdef VIA_ENABLE
#    include "raw_hid.h"
#endif
#include "lib/bongocat.h"
#include "lib/spaceship.h"

typedef union {
    bool oled_enable;
} mad_corne_oled_sync_t;

static uint32_t _oled_one_second_timer = 0;
static uint32_t _elapsed_one_second = 0;
static mad_corne_oled_sync_t g_mad_corne_oled_sync = {0};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#define ONE_SEC     (1000)
#define ONE_MIN     (60*ONE_SEC)


#ifdef CONSOLE_ENABLE
#undef OLED_TIMEOUT
#define OLED_TIMEOUT 5*ONE_SEC
#else
#undef OLED_TIMEOUT
#define OLED_TIMEOUT (5*ONE_MIN)
#endif


void oled_sync(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&g_mad_corne_oled_sync, in_data, sizeof(mad_corne_oled_sync_t));
}

void keyboard_post_init_user(void) {

    transaction_register_rpc(KEYBOARD_OLED_SYNC, oled_sync);


#ifdef CONSOLE_ENABLE
    debug_enable=true;
    debug_matrix=true;
    debug_keyboard=true;
#endif
}

__attribute__((weak)) const keypos_t PROGMEM hand_swap_config[MATRIX_ROWS][MATRIX_COLS] = {
    // Left
    {{0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4}},
    {{0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5}},
    {{0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6}},
    {{0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}},
    // Right
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}},
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}},
    {{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}},
    {{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}}
};

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    _oled_one_second_timer = timer_read32();
    _elapsed_one_second = timer_elapsed32(_oled_one_second_timer);
    return rotation;
}


static bool once_per_second(void){
    if(!is_keyboard_master()){
    return false;
    }

    _elapsed_one_second = timer_elapsed32(_oled_one_second_timer);
    if (_elapsed_one_second < (uint32_t)1000){
        return false;
    }
    _oled_one_second_timer = timer_read32();

#ifdef CONSOLE_ENABLE
    uprintf("elapsed_input_activity_ms %lu\n", timer_elapsed32(last_input_activity_time()));
    uprintf("Is left keyboard: %d\n", is_keyboard_left());
    uprintf("Is Master Keyboard: %d\n", is_keyboard_master());
#endif

    if (timer_elapsed32(last_input_activity_time()) > OLED_TIMEOUT) {
        g_mad_corne_oled_sync.oled_enable=false;
    } else {
        g_mad_corne_oled_sync.oled_enable=true;
    }

    transaction_rpc_send(KEYBOARD_OLED_SYNC, sizeof(mad_corne_oled_sync_t), &g_mad_corne_oled_sync);

    return true;
}

static const char *layer_name(void) {
    switch (get_highest_layer(layer_state)) {
        case 0:  return PSTR("BASE");
        case 1:  return PSTR("LOWR");
        case 2:  return PSTR("RAIS");
        case 3:  return PSTR("ADJ ");
        default: return PSTR("????");
    }
}

// USB half: bongo cat with WPM, layer and Caps Lock on the right
static void render_status(void) {
    led_t leds = host_keyboard_led_state();

    render_bongocat();
    oled_set_cursor(14, 0);
    oled_write_P(PSTR("WPM:"), false);
    oled_write(get_u8_str(get_current_wpm(), '0'), false);
    oled_set_cursor(17, 1);
    oled_write_P(layer_name(), false);
    oled_set_cursor(17, 2);
    oled_write_P(leds.caps_lock ? PSTR("CAPS") : PSTR("    "), false);
}

bool oled_task_kb(void) {
    if (!oled_task_user()) {
        return false;
    }

    if(once_per_second()) {
#ifdef CONSOLE_ENABLE
        print("One second has past\n");
#endif
    }

    if (g_mad_corne_oled_sync.oled_enable){
        if(!is_oled_on()){
            oled_init(OLED_ROTATION_0);
            oled_on();
        }

        if (is_keyboard_master()) {
            render_status();
        } else {
            render_spaceship();
        }
    } else {
        oled_clear();
    }

    return false;
}


bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
        return process_record_user(keycode, record);
    }

bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) {
        return false;
    }

    if (host_keyboard_led_state().caps_lock && !is_keyboard_left()) {
        rgb_matrix_set_color(53, 0xFF, 0xFF, 0xFF);
    }
    return true;
}

#ifdef VIA_ENABLE
// Lets the PC put the keyboard into the RP2040 bootloader over raw HID,
// so updates need no BOOT button. VIA command 0x0B (bootloader jump) is
// unused by QMK; require "BL" after it so stray packets cannot trigger it.
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (data[0] == 0x0B && data[1] == 'B' && data[2] == 'L') {
        raw_hid_send(data, length);
        wait_ms(50);
        reset_keyboard();
        return true;
    }
    return false;
}
#endif
