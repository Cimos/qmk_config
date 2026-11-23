// Custom RGB effect: a signal pulse runs along the keys like current along a
// PCB trace. It snakes through the four rows (left to right, then back), across
// both halves, with a white head and a fading tail in the current colour over a
// dim background. Driven by the shared RGB timer, so both halves stay in step.
RGB_MATRIX_EFFECT(PCB_TRACE)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#    define PCB_TRACE_PATH 1024  // 4 rows x 256 units
#    define PCB_TRACE_TAIL 96    // tail length in x units
#    define PCB_TRACE_HEAD 12    // white part at the front

static bool PCB_TRACE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t head = (uint16_t)((g_rgb_timer * (rgb_matrix_config.speed / 16 + 1)) / 16) % PCB_TRACE_PATH;

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t  row = g_led_config.point[i].y / 17;
        if (row > 3) row = 3;
        uint16_t x    = g_led_config.point[i].x;
        uint16_t pos  = row * 256 + ((row & 1) ? (224 - x) : x);
        uint16_t dist = (head - pos + PCB_TRACE_PATH) % PCB_TRACE_PATH;

        hsv_t hsv = rgb_matrix_config.hsv;
        if (dist < PCB_TRACE_TAIL) {
            hsv.v = (uint16_t)hsv.v * (PCB_TRACE_TAIL - dist) / PCB_TRACE_TAIL;
            if (dist < PCB_TRACE_HEAD) hsv.s = 0;
        } else {
            hsv.v = hsv.v / 10;
        }
        rgb_t rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
