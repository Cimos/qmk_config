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
#include "lib/pcbroute.h"

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

// PCB animation sync: the USB half owns the board and sends its seed and
// tick count every frame. The router is deterministic, so the other half
// replays the same ticks and both screens show one 256px-wide board.
typedef struct {
    uint32_t seed;
    uint32_t tick;
} pcb_sync_t;

static pcb_sync_t pcb_remote;
static bool       pcb_remote_valid = false;

void pcb_sync(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    if (in_buflen == sizeof(pcb_sync_t)) {
        memcpy(&pcb_remote, in_data, sizeof(pcb_sync_t));
        pcb_remote_valid = true;
    }
}

void keyboard_post_init_user(void) {

    transaction_register_rpc(KEYBOARD_OLED_SYNC, oled_sync);
    transaction_register_rpc(KEYBOARD_PCB_SYNC, pcb_sync);


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

// Both halves: one random PCB auto-router board 256px wide, split across the
// two screens (left half shows x 0-127, right half x 128-255).
#define PCB_WIDTH   256
#define PCB_TICK_MS 40
#define PCB_STEPS   2
#define PCB_CATCHUP 64  // most ticks the other half replays per frame

static void render_pcb(void) {
    static bool     started = false;
    static uint32_t seed    = 0;
    static uint32_t tick    = 0;
    static uint32_t last    = 0;

    if (is_keyboard_master()) {
        if (!started || pcb_done()) {
            seed = (seed * 1664525u + 1013904223u) ^ timer_read32();
            if (seed == 0) seed = 1;
            pcb_init(PCB_WIDTH, seed);
            tick    = 0;
            started = true;
        }
        if (timer_elapsed32(last) >= PCB_TICK_MS) {
            last = timer_read32();
            pcb_tick(PCB_STEPS);
            tick++;
            pcb_sync_t msg = {.seed = seed, .tick = tick};
            transaction_rpc_send(KEYBOARD_PCB_SYNC, sizeof(msg), &msg);
        }
    } else if (pcb_remote_valid) {
        // New board, or we got ahead somehow: start over from the master's seed
        if (!started || pcb_remote.seed != seed || pcb_remote.tick < tick) {
            seed = pcb_remote.seed;
            pcb_init(PCB_WIDTH, seed);
            tick    = 0;
            started = true;
        }
        for (uint8_t n = 0; tick < pcb_remote.tick && n < PCB_CATCHUP; n++) {
            pcb_tick(PCB_STEPS);
            tick++;
        }
    }

    uint16_t x0 = is_keyboard_left() ? 0 : 128;
    for (uint8_t page = 0; page < PCB_PAGES; page++) {
        oled_set_cursor(0, page);
        oled_write_raw((const char *)pcb_page(page, x0), 128);
    }
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

        render_pcb();
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

// Layer colours: on any layer above BASE, keys that do something on that
// layer light in the layer's colour and unused keys go dark. Brightness
// follows the current RGB brightness. Underglow keeps its animation.
static const hsv_t layer_colour[] = {
    [1] = {HSV_BLUE},
    [2] = {HSV_GREEN},
    [3] = {HSV_RED},
};

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }

    uint8_t layer = get_highest_layer(layer_state);
    if (layer == 0 || layer >= ARRAY_SIZE(layer_colour)) {
        return true;
    }

    hsv_t hsv = layer_colour[layer];
    hsv.v     = rgb_matrix_get_val();
    rgb_t rgb = hsv_to_rgb(hsv);

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t led = g_led_config.matrix_co[row][col];
            if (led == NO_LED || led < led_min || led >= led_max) {
                continue;
            }
            uint16_t kc = keymap_key_to_keycode(layer, (keypos_t){.row = row, .col = col});
            if (kc > KC_TRANSPARENT) {
                rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
            } else {
                rgb_matrix_set_color(led, 0, 0, 0);
            }
        }
    }
    return true;
}
