#ifndef VA_SCREEN_PLAY_H
#define VA_SCREEN_PLAY_H

#include <stdbool.h>
#include <stdint.h>
#include <u8g2/u8g2.h>

struct va_msg;

enum va_screen_play_vis_type {
    VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_NONE = 0,
    VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_DOTS,
    VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED,
    VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED_ABS,
    VA_SCREEN_PLAY_VIS_TYPE_COUNT,
};

void va_screen_play_init(void);
void va_screen_play_on_message(struct va_msg *msg);
void va_screen_play_update();
void va_screen_play_draw(u8g2_t *u8g2);

void va_screen_play_write_waveform_l(const int8_t *samples, int samples_size);
void va_screen_play_write_waveform_r(const int8_t *samples, int samples_size);

#endif
