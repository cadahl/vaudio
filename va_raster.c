#include <esp_system.h>
#include "va_raster.h"
#include "u8g2/u8g2.h"

#define TAG "va_raster"

static struct {
    int32_t screen_width;
    int32_t screen_height;
} state = { 0 };

void va_raster_init(void) {
    state.screen_width = 128;
    state.screen_height = 64;
}

IRAM_ATTR void va_raster_line_draw(
    struct u8g2_struct *u8g2,
    const struct va_point2_fp16 *a,
    const struct va_point2_fp16 *b) {

    const va_fp16 ax = a->x + 32768;
    const va_fp16 ay = a->y + 32768;
    const va_fp16 bx = b->x + 32768;
    const va_fp16 by = b->y + 32768;

    const va_fp16 w = 128 << 16;
    const va_fp16 h = 64 << 16;

    if (ax < 0 && bx < 0) {
        return;
    }
    if (ax >= w && bx >= w) {
        return;
    }

    va_fp16 x0, y0, x1, y1;
    if (ay < by) {
        x0 = ax;
        y0 = ay;
        x1 = bx;
        y1 = by;
    } else {
        x0 = bx;
        y0 = by;
        x1 = ax;
        y1 = ay;
    }

    if (y1 < 0 || y0 >= h) {
        return;
    }

    const va_fp16 kx = ((x1 - x0) << 10) / ((y1 - y0) >> 6);

    if (y0 < 0) {
        x0 += (kx * -(y0 >> 16)) >> 16;
        y0 = 0;
    }
    
    x0 -= (kx * ((y0 & 0xFFFF) >> 12)) >> 4;

    if (y1 > (h-65536)) {
        y1 = (h-65536);
    }

    uint8_t *scr = u8g2_GetBufferPtr(u8g2);

//    VA_LOGI(TAG, "%d, %d - %d, %d\n", x0>>16, y0>>16, x1>>16, y1>>16);

    int32_t prev_px = -1;
    va_fp16 x = x0;
    const int32_t py0 = y0 >> 16;
    const int32_t py1 = y1 >> 16;
    for (va_fp16 py = py0; py <= py1; ++py) {
        const uint8_t mask = 1 << (py & 7);
        const int32_t px = x >> 16;
        const uint32_t offset = (py & ~7) * u8g2_GetBufferTileWidth(u8g2);        
        if (prev_px < 0 || (px == prev_px)) {
            scr[offset + px] |= mask;
        } else {
            if (prev_px < px) {
                uint8_t *dst = scr + offset + prev_px;
                const int32_t len = px - prev_px;
                for (int32_t i = 0; i < len; ++i) {
                    *++dst |= mask;
                }
            } else {
                uint8_t *dst = scr + offset + px;
                const int32_t len = prev_px - px;
                for (int32_t i = 0; i < len; ++i) {
                    *dst++ |= mask;
                }
            }
        }
        prev_px = px;
        x += kx;
    }
}
