#ifndef VA_SCREEN_IDLE_H
#define VA_SCREEN_IDLE_H

struct va_msg;
struct u8g2_struct;

enum va_screen_idle_mode {
    VA_SCREEN_IDLE_MODE_BLANK = 0,
    VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER,
    VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER_LEAVE_WITH_HLINES,
    VA_SCREEN_IDLE_MODE_COUNT,
};

void va_screen_idle_init(void);
void va_screen_idle_enter(void);
void va_screen_idle_on_message(struct va_msg *msg);
void va_screen_idle_update(void);
void va_screen_idle_draw(struct u8g2_struct *u8g2);

#endif
