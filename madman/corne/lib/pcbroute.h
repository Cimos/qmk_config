#pragma once
#include <stdint.h>

// 128x32 frame buffer in SSD1306 page order (byte = 8 vertical pixels).
#define PCB_FB_SIZE 512

// Routing area width in pixels (the rest of the screen is left blank for text).
void           pcb_init(uint8_t width, uint32_t seed);
// Advance the animation by one tick; steps = pixels of trace to lay this tick.
void           pcb_tick(uint8_t steps);
const uint8_t *pcb_fb(void);
