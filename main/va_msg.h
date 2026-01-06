#ifndef VA_MSG_H
#define VA_MSG_H

#include <stdbool.h>

enum va_msg_type {
    VA_MSG_BLUETOOTH_PAIR_CODE,
    VA_MSG_BLUETOOTH_PAIR_COMPLETED,
    VA_MSG_BLUETOOTH_PAIR_FAILED,

    VA_MSG_INPUT_WHEEL_BTN_PRESS,
    VA_MSG_INPUT_WHEEL_BTN_RELEASE,
    VA_MSG_INPUT_WHEEL_BTN_CLICK,
    VA_MSG_INPUT_WHEEL_BTN_LONGPRESS,
    VA_MSG_INPUT_WHEEL_CHANGED,

    VA_MSG_AUDIO_CONNECTION_STATE_CHANGED,
    VA_MSG_AUDIO_IS_PLAYING_CHANGED,
    VA_MSG_AUDIO_SOURCE_NAME_CHANGED,
    VA_MSG_AUDIO_ARTIST_CHANGED,
    VA_MSG_AUDIO_TITLE_CHANGED,
    VA_MSG_AUDIO_PLAY_POS_CHANGED,
    VA_MSG_AUDIO_PLAY_DURATION_CHANGED,
    VA_MSG_AUDIO_DISCOVERABLE_CHANGED,
};

struct va_msg {
    enum va_msg_type type;
    union {
        int i;
        bool b;
        void *ptr;
    };
};

void va_msg_init(void);
bool va_msg_get(struct va_msg *msg);

void va_msg_post_ptr(enum va_msg_type msg_type, void *ptr);
void va_msg_post_i(enum va_msg_type msg_type, int i);
void va_msg_post_b(enum va_msg_type msg_type, bool b);

#endif
