#include <stdint.h>
#include <stdbool.h>
#include <esp_attr.h>
#include "u8g2/u8g2.h"
#include "va_app.h"
#include "va_audio.h"
#include "va_msg.h"
#include "va_io.h"
#include "va_props.h"

#define ANSLUT_TILL_STR "Anslut med Bluetooth till"
#define ANSLUTER_STR "Ansluter..."
#define BEKRAFTA_KOD_STR "Kod för koppling är"

static struct {
    bool display_pairing_code;
    char pairing_code_chars[6];
} state = { 0 };

void va_screen_connect_init(void) {
}

IRAM_ATTR void va_screen_connect_on_message(struct va_msg *msg) {
    switch (msg->type) {
        case VA_MSG_BLUETOOTH_PAIR_CODE: {
            state.display_pairing_code = true;
            const int i = msg->i;
            state.pairing_code_chars[0] = '0' + ((i / 100000) % 10);
            state.pairing_code_chars[1] = '0' + ((i / 10000) % 10);
            state.pairing_code_chars[2] = '0' + ((i / 1000) % 10);
            state.pairing_code_chars[3] = '0' + ((i / 100) % 10);
            state.pairing_code_chars[4] = '0' + ((i / 10) % 10);
            state.pairing_code_chars[5] = '0' + (i % 10);
            break;
        }
        case VA_MSG_BLUETOOTH_PAIR_COMPLETED:
        case VA_MSG_BLUETOOTH_PAIR_FAILED:
            state.display_pairing_code = false;
            break;
        default:
            break;
    }
}

IRAM_ATTR void va_screen_connect_update() {
    if (va_io_input_is_down_event(VA_IO_INPUT_BACK)) {
        va_app_wake_up();
        va_audio_toggle_discoverable();
    }
}

IRAM_ATTR void va_screen_connect_draw(u8g2_t *u8g2, bool volume_changed) {
    const enum va_audio_connection_state connection_state = va_audio_get_connection_state();

    u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);
    if (state.display_pairing_code) {
        int w = u8g2_GetUTF8Width(u8g2, BEKRAFTA_KOD_STR);
        u8g2_DrawUTF8(u8g2, (128 - w) / 2, 24, BEKRAFTA_KOD_STR);
        w = u8g2_GetUTF8Width(u8g2, "000000");
        int x = (128 - w) / 2;
        for (int i = 0; i < 6; ++i) {
            x += u8g2_DrawGlyph(u8g2, x, 45, state.pairing_code_chars[i]);
        }
    } else if (connection_state == VA_AUDIO_CONNECTION_STATE_DISCONNECTED) {
        int w = u8g2_GetUTF8Width(u8g2, ANSLUT_TILL_STR);
        u8g2_DrawUTF8(u8g2, (128 - w) / 2, 24, ANSLUT_TILL_STR);

        w = 14 + u8g2_GetUTF8Width(u8g2, va_props_get_string(VA_PROP_ID_ANNOUNCED_BT_NAME));
        u8g2_SetFont(u8g2, u8g2_font_siji_t_6x10);
        u8g2_DrawGlyph(u8g2, (128 - w) / 2, 45, 0xE00B);

        u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);
        u8g2_DrawUTF8(u8g2, (128 - w) / 2 + 14, 45, va_props_get_string(VA_PROP_ID_ANNOUNCED_BT_NAME));
    } else if (connection_state == VA_AUDIO_CONNECTION_STATE_CONNECTING) {
        int w = u8g2_GetUTF8Width(u8g2, ANSLUTER_STR);
        u8g2_DrawUTF8(u8g2, (128 - w) / 2, 62, ANSLUTER_STR);
    }
}
