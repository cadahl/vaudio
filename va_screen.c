#include <esp_attr.h>
#include "va_screen.h"
#include "va_screen_idle.h"
#include "va_screen_splash.h"
#include "va_screen_play.h"
#include "va_screen_connect.h"

#define TAG "va_screen"

static struct {
    enum va_screen screen;
} state = { 0 };

void va_screen_init(void) {
    state.screen = VA_SCREEN_SPLASH;

    va_screen_idle_init();
    va_screen_connect_init();
    va_screen_play_init();
    va_screen_splash_init();
}

IRAM_ATTR void va_screen_on_message(struct va_msg *msg) {
    va_screen_idle_on_message(msg);
    va_screen_connect_on_message(msg);
    va_screen_play_on_message(msg);
    va_screen_splash_on_message(msg);
}

IRAM_ATTR void va_screen_update(void) {
    switch (state.screen) {
        case VA_SCREEN_PLAY:
            va_screen_play_update();
            break;
        case VA_SCREEN_CONNECT:
            va_screen_connect_update();
            break;
        case VA_SCREEN_IDLE:
            va_screen_idle_update();
            break;
        case VA_SCREEN_SPLASH:
            va_screen_splash_update();
            break;
        default:
            break;
    }
}

IRAM_ATTR void va_screen_draw(struct u8g2_struct* u8g2) {
    switch (state.screen) {
        case VA_SCREEN_SPLASH:
            va_screen_splash_draw(u8g2);
            break;
        case VA_SCREEN_IDLE:
            va_screen_idle_draw(u8g2);
            break;
        case VA_SCREEN_CONNECT:
            va_screen_connect_draw(u8g2);
            break;
        case VA_SCREEN_PLAY:
            va_screen_play_draw(u8g2);
            break;
        default:
            break;
    }
}

IRAM_ATTR enum va_screen va_screen_get(void) {
    return state.screen;
}

void va_screen_set(enum va_screen screen) {
    if (screen == state.screen) {
        return;
    }
    state.screen = screen;
    switch (screen) {
        case VA_SCREEN_IDLE:
            va_screen_idle_enter();
            break;
        default:
            break;
    }
}
