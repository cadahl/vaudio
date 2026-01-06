#ifndef VA_SCREEN_H
#define VA_SCREEN_H

struct va_msg;
struct u8g2_struct;

enum va_screen {
    VA_SCREEN_SPLASH = 1,
    VA_SCREEN_IDLE = 2,
    VA_SCREEN_CONNECT = 4,
    VA_SCREEN_PLAY = 8,
};

void va_screen_init(void);
void va_screen_update(void);
void va_screen_on_message(struct va_msg *msg);
void va_screen_draw(struct u8g2_struct *u8g2);

enum va_screen va_screen_get(void);
void va_screen_set(enum va_screen screen);

#endif
