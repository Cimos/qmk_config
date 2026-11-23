#pragma once
#include <stdbool.h>
#include <stdint.h>

// Virtual board: up to 256x32, stored in SSD1306 page order with a 256-byte
// page stride (byte = 8 vertical pixels). Each screen shows a 128px slice.
#define PCB_MAX_W   256
#define PCB_PAGES   4
#define PCB_FB_SIZE (PCB_MAX_W * PCB_PAGES)

// Start a new board. Same width + seed + tick sequence gives the same picture,
// which is how the two halves stay in step.
void           pcb_init(uint16_t width, uint32_t seed);
// Advance one tick, laying up to `steps` pixels of trace.
void           pcb_tick(uint8_t steps);
// True once the board has been routed full and wiped; start the next with pcb_init.
bool           pcb_done(void);
// Start of the 128-byte run for page `page` beginning at column `x0`.
const uint8_t *pcb_page(uint8_t page, uint16_t x0);
