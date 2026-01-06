#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_attr.h>
#include "u8g2/u8g2.h"
#include "va_app.h"
#include "va_audio.h"
#include "va_math.h"
#include "va_msg.h"
#include "va_screen_play.h"
#include "va_io.h"
#include "va_props.h"
#include "va_cqueue.h"

#define TAG "va_screen_play"

static struct {
    enum va_screen_play_vis_type vis_type;
    struct va_cqueue waveform_cq_l;
    struct va_cqueue waveform_cq_r;
    int32_t volume_changed_timer;
    char *source_name;
    char *artist;
    char *title;
    int32_t play_pos;
    int32_t play_pos_update_time;
    int32_t play_pos_last_seconds;
    char play_pos_chars[8];
    char play_duration_chars[8];
} state = { 0 };

void va_screen_play_init(void) {
    state.source_name = strdup("");
    state.artist = strdup("");
    state.title = strdup("");
    memset(state.play_pos_chars, '0', 8); 
    memset(state.play_duration_chars, '0', 8); 

    state.vis_type = VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED_ABS;
    va_cqueue_init(&state.waveform_cq_l, 512);
    va_cqueue_init(&state.waveform_cq_r, 512);
}

void IRAM_ATTR va_screen_play_update(uint32_t time_ms) {
    if (va_io_input_is_down_event(VA_IO_INPUT_DOWN)) {
        va_app_wake_up();
        if (state.vis_type > 0) {
            --state.vis_type;
        } else {
            state.vis_type = VA_SCREEN_PLAY_VIS_TYPE_COUNT-1;
        }
    }
    if (va_io_input_is_down_event(VA_IO_INPUT_UP)) {
        va_app_wake_up();
        if (state.vis_type < VA_SCREEN_PLAY_VIS_TYPE_COUNT-1) {
            ++state.vis_type;
        } else {
            state.vis_type = 0;
        }
    }

    int32_t play_pos_update_time_delta = time_ms - state.play_pos_update_time;
    if (play_pos_update_time_delta >= 1000) {
        state.play_pos += 1000;
        state.play_pos_update_time = time_ms;

        int seconds = state.play_pos / 1000;
        int seconds_delta = seconds - state.play_pos_last_seconds;
        if (seconds_delta < -1 || seconds_delta > 0) {
            state.play_pos_last_seconds = seconds;
            int minutes = seconds / 60;
            int hours = minutes / 60;
            seconds %= 60;
            minutes %= 60;
            state.play_pos_chars[0] = '0' + (seconds % 10);
            state.play_pos_chars[1] = '0' + (seconds / 10);
            state.play_pos_chars[2] = '0' + (minutes % 10);
            state.play_pos_chars[3] = '0' + (minutes / 10);
            state.play_pos_chars[4] = '0' + (hours % 10);
            state.play_pos_chars[5] = '0' + ((hours / 10) % 10);
            state.play_pos_chars[6] = '0' + ((hours / 100) % 10);
        }
    }
}

static enum va_audio_volume clamp_volume(int32_t volume) {
    return va_clamp_i32(VA_AUDIO_VOLUME_0, VA_AUDIO_VOLUME_COUNT-1, volume);
}

static IRAM_ATTR void draw_volume_ramp(u8g2_t *u8g2, int x, int y, bool blink) {
    const enum va_audio_volume volume_limit = clamp_volume(va_props_get_i32(VA_PROP_ID_VOLUME_LIMIT));
    const enum va_audio_volume volume = va_audio_get_volume();

    for (int i = 0; i < 32; ++i) {
        if (i <= volume && (volume < volume_limit || blink)) {
            const int height = i / 4;
            u8g2_DrawVLine(u8g2, x+i, y - height, height+1+height);
        } else {
            if (i & 1) {
                u8g2_DrawPixel(u8g2, x+i, y);
            }
        }
    }
}

static IRAM_ATTR void draw_waveform(u8g2_t *u8g2, int x, int y, int width, enum va_screen_play_vis_type vis_type, struct va_cqueue *waveform_cq) {
    int8_t waveform[width];
    if (!va_cqueue_read(waveform_cq, waveform, width)) {
        return;
    }
    if (vis_type == VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED_ABS) {
        y += 8;
        for (int i = 0; i < width; ++i) {
            const int8_t ss = waveform[i] >> 2;
            const int8_t s = ss < 0 ? -ss : ss;
            u8g2_DrawVLine(u8g2, x++, y-s, s+1);
        }
    } else if (vis_type == VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED) {
        for (int i = 0; i < width; ++i) {
            const int8_t s = waveform[i] >> 3;
            if (s < 0) {
                u8g2_DrawVLine(u8g2, x++, y + s, -s+1);
            } else {
                u8g2_DrawVLine(u8g2, x++, y, s+1);
            }
        }
    } else if (vis_type == VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_DOTS) {
        for (int i = 0; i < width; ++i) {
            const int8_t s = waveform[i] >> 3;
            u8g2_DrawPixel(u8g2, x++, y + s);
        }
    }
}

static IRAM_ATTR void draw_waveforms(u8g2_t *u8g2, int x, int y) {    
    draw_waveform(u8g2, x, y, 46, state.vis_type, &state.waveform_cq_l);
    x += 48;
    draw_waveform(u8g2, x, y, 46, state.vis_type, &state.waveform_cq_r);
}

static IRAM_ATTR void draw_volume_db(u8g2_t *u8g2, int x, int y, bool blink) {
    const bool is_muted = va_audio_is_muted();
    if (!is_muted || blink) {
        const char *str = va_audio_get_volume_str(va_audio_get_volume());
        const int len = strlen(str);
        u8g2_SetFont(u8g2, u8g2_font_NokiaSmallBold_tf);
        for (int i = len-1; i >= 0; --i) {
            x -= u8g2_GetGlyphWidth(u8g2, str[i]);
            u8g2_DrawGlyph(u8g2, x, y, str[i]);
        }
    }
}

static IRAM_ATTR int draw_time(u8g2_t *u8g2, int x, int y, const char *chars) {
    x -= u8g2_GetGlyphWidth(u8g2, chars[0]);
    u8g2_DrawGlyph(u8g2, x, y, chars[0]);
    x -= u8g2_GetGlyphWidth(u8g2, chars[1]);
    u8g2_DrawGlyph(u8g2, x, y, chars[1]);
    x -= u8g2_GetGlyphWidth(u8g2, ':');
    u8g2_DrawGlyph(u8g2, x, y, ':');
    x -= u8g2_GetGlyphWidth(u8g2, chars[2]);
    u8g2_DrawGlyph(u8g2, x, y, chars[2]);
    x -= u8g2_GetGlyphWidth(u8g2, chars[3]);
    u8g2_DrawGlyph(u8g2, x, y, chars[3]);
    if (chars[4] != '0' || chars[5] != '0' || chars[6] != '0') {
        x -= u8g2_GetGlyphWidth(u8g2, ':');
        u8g2_DrawGlyph(u8g2, x, y, ':');
        x -= u8g2_GetGlyphWidth(u8g2, chars[4]);
        u8g2_DrawGlyph(u8g2, x, y, chars[4]);
        if (chars[5] != '0' || chars[6] != '0') {
            x -= u8g2_GetGlyphWidth(u8g2, chars[5]);
            u8g2_DrawGlyph(u8g2, x, y, chars[5]);
            if (chars[6] != '0') {
                x -= u8g2_GetGlyphWidth(u8g2, chars[6]);
                u8g2_DrawGlyph(u8g2, x, y, chars[6]);
            }
        }
    }
    return x;
}

IRAM_ATTR void va_screen_play_draw(u8g2_t *u8g2) {
    const bool blink = (va_app_get_time_ms() & 1023) >= 512;
    const bool alert_blink = (va_app_get_time_ms() & 127) >= 64;
    const bool is_playing = va_audio_is_playing();

    if (va_audio_has_volume_changed()) {
        state.volume_changed_timer = va_props_get_i32(VA_PROP_ID_VOLUME_DB_HIDE_TIME) * 1000;
    }
    const bool show_volume_db = state.volume_changed_timer > 0 || va_audio_is_muted();
    if (state.volume_changed_timer > 0) {
        state.volume_changed_timer -= va_app_get_delta_time_ms();
    }
    if (state.volume_changed_timer < 0) {
        state.volume_changed_timer = 0;
    }

    u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);
    u8g2_DrawUTF8(u8g2, 0, 7, state.source_name);

    if (is_playing || blink) {
        u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);
        int x = draw_time(u8g2, 128, 8, state.play_duration_chars);
        x -= u8g2_GetGlyphWidth(u8g2, '/');
        u8g2_DrawGlyph(u8g2, x, 8, '/');
        x = draw_time(u8g2, x, 8, state.play_pos_chars);
        u8g2_SetFont(u8g2, u8g2_font_siji_t_6x10);
        x -= 12;
        if (va_audio_is_playing()) {
            u8g2_DrawGlyph(u8g2, x, 8, 0xE058);
        } else {
            u8g2_DrawGlyph(u8g2, x, 8, 0xE059);
        }
    }

    draw_waveforms(u8g2, 0, 32);

    draw_volume_ramp(u8g2, 96, 32, alert_blink);
    if (show_volume_db) {
        draw_volume_db(u8g2, 128, 20, blink);
    }

    u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);
    u8g2_DrawUTF8(u8g2, 0, 54, state.artist);
    u8g2_DrawUTF8(u8g2, 0, 63, state.title);
}

IRAM_ATTR void va_screen_play_on_message(struct va_msg *msg) {
   switch (msg->type) {
        case VA_MSG_AUDIO_SOURCE_NAME_CHANGED: {
            if (state.source_name) {
                free(state.source_name);
            }
            state.source_name = (char *)msg->ptr;
            break;
        }
        case VA_MSG_AUDIO_TITLE_CHANGED:
            if (state.title) {
                free(state.title);
            }
            state.title = (char *)msg->ptr;
            break;
        case VA_MSG_AUDIO_ARTIST_CHANGED:
            if (state.artist) {
                free(state.artist);
            }
            state.artist = (char *)msg->ptr;
            break;
        case VA_MSG_AUDIO_PLAY_POS_CHANGED: {
            state.play_pos = msg->i;
            state.play_pos_update_time = va_app_get_time_ms();
            break;
        }
        case VA_MSG_AUDIO_PLAY_DURATION_CHANGED: {
            int seconds = atoi(msg->ptr) / 1000;
            int minutes = seconds / 60;
            int hours = minutes / 60;
            seconds %= 60;
            minutes %= 60;
            state.play_duration_chars[0] = '0' + (seconds % 10);
            state.play_duration_chars[1] = '0' + (seconds / 10);
            state.play_duration_chars[2] = '0' + (minutes % 10);
            state.play_duration_chars[3] = '0' + (minutes / 10);
            state.play_duration_chars[4] = '0' + (hours % 10);
            state.play_duration_chars[5] = '0' + ((hours / 10) % 10);
            state.play_duration_chars[6] = '0' + ((hours / 100) % 10);
            break;
        }
    default:
        break;
    }
}

IRAM_ATTR void va_screen_play_write_waveform_l(const int8_t *data, int data_size) {
    va_cqueue_write(&state.waveform_cq_l, (const void*)data, data_size);
}

IRAM_ATTR void va_screen_play_write_waveform_r(const int8_t *data, int data_size) {
    va_cqueue_write(&state.waveform_cq_r, (const void*)data, data_size);
}
