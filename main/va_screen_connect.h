#ifndef VA_SCREEN_CONNECT_H
#define VA_SCREEN_CONNECT_H

#include <stdint.h>

struct u8g2_struct;
struct va_msg;

void va_screen_connect_init(void);
void va_screen_connect_on_message(struct va_msg *msg);
void va_screen_connect_update(void);
void va_screen_connect_draw(struct u8g2_struct *u8g2);

#endif
