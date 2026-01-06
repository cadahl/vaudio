#ifndef VA_AUDIO_H
#define VA_AUDIO_H

#include <stdint.h>
#include <esp_err.h>
#include "va_props.h"

struct va_msg;

enum va_audio_volume {
    VA_AUDIO_VOLUME_0 = 0,
    VA_AUDIO_VOLUME_1,
    VA_AUDIO_VOLUME_2,
    VA_AUDIO_VOLUME_3,
    VA_AUDIO_VOLUME_4,
    VA_AUDIO_VOLUME_5,
    VA_AUDIO_VOLUME_6,
    VA_AUDIO_VOLUME_7,
    VA_AUDIO_VOLUME_8,
    VA_AUDIO_VOLUME_9,
    VA_AUDIO_VOLUME_10,
    VA_AUDIO_VOLUME_11,
    VA_AUDIO_VOLUME_12,
    VA_AUDIO_VOLUME_13,
    VA_AUDIO_VOLUME_14,
    VA_AUDIO_VOLUME_15,
    VA_AUDIO_VOLUME_16,
    VA_AUDIO_VOLUME_17,
    VA_AUDIO_VOLUME_18,
    VA_AUDIO_VOLUME_19,
    VA_AUDIO_VOLUME_20,
    VA_AUDIO_VOLUME_21,
    VA_AUDIO_VOLUME_22,
    VA_AUDIO_VOLUME_23,
    VA_AUDIO_VOLUME_24,
    VA_AUDIO_VOLUME_25,
    VA_AUDIO_VOLUME_26,
    VA_AUDIO_VOLUME_27,
    VA_AUDIO_VOLUME_28,
    VA_AUDIO_VOLUME_29,
    VA_AUDIO_VOLUME_30,
    VA_AUDIO_VOLUME_31,
    VA_AUDIO_VOLUME_32,
    VA_AUDIO_VOLUME_33,
    VA_AUDIO_VOLUME_34,
    VA_AUDIO_VOLUME_35,
    VA_AUDIO_VOLUME_36,
    VA_AUDIO_VOLUME_37,
    VA_AUDIO_VOLUME_38,
    VA_AUDIO_VOLUME_39,
    VA_AUDIO_VOLUME_40,
    VA_AUDIO_VOLUME_41,
    VA_AUDIO_VOLUME_42,
    VA_AUDIO_VOLUME_43,
    VA_AUDIO_VOLUME_44,
    VA_AUDIO_VOLUME_45,
    VA_AUDIO_VOLUME_46,
    VA_AUDIO_VOLUME_47,
    VA_AUDIO_VOLUME_48,
    VA_AUDIO_VOLUME_49,
    VA_AUDIO_VOLUME_50,
    VA_AUDIO_VOLUME_51,
    VA_AUDIO_VOLUME_52,
    VA_AUDIO_VOLUME_53,
    VA_AUDIO_VOLUME_54,
    VA_AUDIO_VOLUME_55,
    VA_AUDIO_VOLUME_56,
    VA_AUDIO_VOLUME_57,
    VA_AUDIO_VOLUME_58,
    VA_AUDIO_VOLUME_59,
    VA_AUDIO_VOLUME_60,
    VA_AUDIO_VOLUME_61,
    VA_AUDIO_VOLUME_62,
    VA_AUDIO_VOLUME_63,
    VA_AUDIO_VOLUME_COUNT,
};

enum va_audio_connection_state {
    VA_AUDIO_CONNECTION_STATE_DISCONNECTED,
    VA_AUDIO_CONNECTION_STATE_CONNECTING,
    VA_AUDIO_CONNECTION_STATE_CONNECTED,
    VA_AUDIO_CONNECTION_STATE_DISCONNECTING
};

esp_err_t va_audio_init(void);
void va_audio_update(void);
void va_audio_on_message(struct va_msg *msg);

void va_audio_disconnect(void);

enum va_audio_connection_state va_audio_get_connection_state(void);
void va_audio_toggle_discoverable(void);

enum va_audio_volume va_audio_get_volume(void);
const char *va_audio_get_volume_str(enum va_audio_volume volume);
bool va_audio_has_volume_changed(void);

void va_audio_set_output_enable(bool enable);
bool va_audio_get_output_enable(void);

bool va_audio_is_playing(void);
bool va_audio_is_muted(void);

void va_audio_apply_volume(const int16_t *src, int nbr_samples, int32_t *dst);
void va_audio_submit_samples_for_waveform(const int16_t *src, int nbr_samples);

const char *va_audio_get_source_name(void);
const char *va_audio_get_title(void);

void va_audio_add_bonded_device(uint8_t bdaddr[VA_PROPS_BDADDR_LEN]);
void va_audio_remove_bonded_device(int32_t device_index_in_settings);

#endif
