#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <esp_attr.h>
#include "u8g2/u8g2.h"
#include "va_app.h"
#include "va_screen.h"

#define TAG "va_screen_splash"

static struct {
    bool is_first;
    uint32_t start_millis;
    uint32_t last_millis;
    int32_t logo_y;
    int32_t logo_dy;
    uint32_t remove_timer_start_millis;
    int phase;
} state = { 0 };

void va_screen_splash_init() {
    state.is_first = true;
    state.last_millis = 0xFFFFFFFF;
}

IRAM_ATTR void va_screen_splash_on_message(struct va_msg *msg) {    
}

IRAM_ATTR void va_screen_splash_update(void) {    
}

IRAM_ATTR void va_screen_splash_draw(struct u8g2_struct *u8g2) {
    if (state.is_first) {
        state.start_millis = va_app_get_time_ms();
        state.last_millis = state.start_millis;
    }

    u8g2_SetFont(u8g2, u8g2_font_maniac_tr);
    u8g2_DrawStr(u8g2, 16, state.logo_y >> 16, "VAudio");

    state.logo_y += state.logo_dy;

    switch (state.phase) {
        case 0:
            if (state.logo_y > (64 << 16)) {
                state.logo_dy = -((state.logo_dy >> 1) + (state.logo_dy >> 3));

                if (state.remove_timer_start_millis == 0 && abs(state.logo_dy) < 10000) {
                    state.remove_timer_start_millis = va_app_get_time_ms();
                }

                state.logo_y = (64 << 16) - (state.logo_y - (64 << 16));
            }
            if (state.remove_timer_start_millis > 0 && (va_app_get_time_ms() - state.remove_timer_start_millis) > 1000) {
                state.remove_timer_start_millis = va_app_get_time_ms();
                state.phase = 1;
            }
            break;
        case 1:
            if (state.logo_y > (96 << 16)) {
                va_screen_set(VA_SCREEN_IDLE);
                state.phase = 2;
            }
            break;
        default:
            break;
    }
    
    state.logo_dy += (va_app_get_time_ms() - state.last_millis) << 9;

    if ((va_app_get_time_ms() - state.start_millis) > 5000) {
        va_screen_set(VA_SCREEN_IDLE);
    }

    state.last_millis = va_app_get_time_ms();
    state.is_first = false;
}
