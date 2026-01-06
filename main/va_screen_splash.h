#ifndef VA_SCREEN_SPLASH_H
#define VA_SCREEN_SPLASH_H

#include <stdint.h>

struct u8g2_struct;

void va_screen_splash_init(void);
void va_screen_splash_on_message(struct va_msg *msg);
void va_screen_splash_update(void);
void va_screen_splash_draw(struct u8g2_struct *u8g2);

#endif
