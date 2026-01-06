#include <stdbool.h>
#include <stdint.h>
#include <esp_attr.h>
#include "u8g2/u8g2.h"
#include "va_app.h"
#include "va_io.h"
#include "va_audio.h"
#include "va_math.h"
#include "va_raster.h"
#include "va_screen_idle.h"

static const int16_t stored_points[] = { -27996,-25736,-4268,-12428,10294,-32767,376,-2385,-12428,-11424,-1883,21216,16948,-376,0,32767 };

#define NBR_POINTS ((sizeof(stored_points) / sizeof(int16_t)) / 2)
static const int32_t nbr_points = NBR_POINTS;

#define NBR_LINES 8

static struct {
    enum va_screen_idle_mode mode;
    uint32_t scene_start_ms;
    int32_t frame;
    struct va_point3_fp16 *src_points;
    struct va_point3_fp16 *tmp_points;
    struct va_point3_fp16 *dst_points;
    struct va_point2_fp16 *projected_points;
    int32_t line_a;
} state = { 0 };

static void set_mode(enum va_screen_idle_mode mode) {
    state.mode = mode;
    state.scene_start_ms = va_app_get_time_ms();
}

void va_screen_idle_init(void) {    
    state.src_points = malloc(sizeof(struct va_point3_fp16) * NBR_POINTS);
    state.tmp_points = malloc(sizeof(struct va_point3_fp16) * NBR_POINTS);
    state.dst_points = malloc(sizeof(struct va_point3_fp16) * NBR_POINTS);
    state.projected_points = malloc(sizeof(struct va_point2_fp16) * NBR_POINTS);

    for (int i = 0; i < nbr_points; ++i) {
        state.src_points[i].x = stored_points[i*2 + 0];
        state.src_points[i].y = stored_points[i*2 + 1];
        state.src_points[i].z = 0;
    }
}

IRAM_ATTR void va_screen_idle_on_message(struct va_msg *msg) {    
}

void va_screen_idle_enter(void) {
    set_mode(VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER);
}

IRAM_ATTR void va_screen_idle_update(void) {
    state.mode = va_props_get_i32(VA_PROP_ID_IDLE_MODE);
    if (va_io_input_is_down_event(VA_IO_INPUT_CONN)) {
        va_app_wake_up();
        va_audio_toggle_discoverable();
    }
}

IRAM_ATTR void va_screen_idle_draw(struct u8g2_struct *u8g2) {
    const uint32_t scene_time_ms = va_app_get_time_ms() - state.scene_start_ms;
    switch (state.mode) {
        case VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER: 
        case VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER_LEAVE_WITH_HLINES: {
            if (state.mode == VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER_LEAVE_WITH_HLINES) {
                const int32_t yoffset = -(32<<16) + va_smoothstep(7*1024, 8*1024, scene_time_ms) * 64;
                const int32_t b = scene_time_ms >> 2;
                for (int i = 0; i < NBR_LINES; ++i) {
                    int32_t s = va_sin(b - i * 31);
                    const int32_t y = (yoffset + 31 * s) >> 16;
                    if (y >= 0 && y < 64) {
                        u8g2_DrawHLine(u8g2, 0, y, 128);
                    }
                }
            }

            uint32_t phase = scene_time_ms >> 10;
            uint32_t a = state.frame*4;

            struct va_matrix_fp16 mtx_rot;
            va_matrix_rot_y(&mtx_rot, a);
            va_matrix_transform_points(&mtx_rot, state.src_points, state.dst_points, nbr_points);

            int32_t pos_y = -(64 << 16);

            if (phase == 0) {
                pos_y = (64 << 16) - 64 * va_smoothstep(0, 1024, scene_time_ms);
            } else if (phase >= 7) {
                if (state.mode == VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER) {
                    pos_y = 0;
                } else {
                    pos_y = -64 * va_smoothstep(7*1024, 8*1024, scene_time_ms);
                }
            } else if (phase > 0 && phase < 7) {
                pos_y = 0;
            }
            
            va_matrix_project_points(15, 150000, state.dst_points, state.projected_points, nbr_points);

            for (int i = 0; i < nbr_points; ++i) {
                state.projected_points[i].y += pos_y;
            }

            int prev_i = nbr_points-1;
            for (int i = 0; i < nbr_points; ++i) {
                va_raster_line_draw(
                    u8g2,
                    &state.projected_points[prev_i],
                    &state.projected_points[i]);
                prev_i = i;
            }
            break;
        case VA_SCREEN_IDLE_MODE_BLANK:
        default:
            break;
        }

    }

    ++state.frame;
}
