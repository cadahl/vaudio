#ifndef VA_PROPS_H
#define VA_PROPS_H

#include <stdbool.h>
#include <stdint.h>

struct va_msg;

#define VA_PROPS_BDADDR_LEN 6

enum va_prop_type {
    VA_PROP_TYPE_NONE = 0,
    VA_PROP_TYPE_BOOL,
    VA_PROP_TYPE_I32,
    VA_PROP_TYPE_STRING,
    VA_PROP_TYPE_BDADDR,
};

enum va_prop_id {
    VA_PROP_ID_NONE = 0,
    VA_PROP_ID_ANNOUNCED_BT_NAME,
    VA_PROP_ID_IDLE_MODE,
    VA_PROP_ID_VOLUME_ROTENC_SPEED,
    VA_PROP_ID_VOLUME_DEFAULT,
    VA_PROP_ID_VOLUME_LIMIT,
    VA_PROP_ID_VOLUME_DB_HIDE_TIME,
    VA_PROP_ID_PLAY_VIS_TYPE,
    VA_PROP_ID_REMOTE_ENABLE_ALWAYS,
    VA_PROP_ID_SWAP_LR,
    VA_PROP_ID_INVERT_L,
    VA_PROP_ID_INVERT_R,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_0,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_1,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_2,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_3,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_4,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_5,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_6,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_7,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_0,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_1,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_2,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_3,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_4,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_5,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_6,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_7,
    VA_PROP_ID_MENU_HIDE_TIME,
    VA_PROP_ID_DIMMING_TIME,
    VA_PROP_ID_SCREENOFF_TIME,
    VA_PROP_DISCOVERABLE_TIME,
    VA_PROP_ID_RESET_REASON,
    VA_PROP_ID_FREE_HEAP_SIZE,
    VA_PROP_ID_LOWEST_FREE_HEAP_SIZE,
    VA_PROP_ID_VERSION,
    VA_PROP_ID_COUNT,
};

union va_prop_value {
    bool b;
    int32_t i;
    char *s;
    uint8_t bdaddr[VA_PROPS_BDADDR_LEN];
};

void va_props_init(void);

void va_props_request_update(enum va_prop_id prop_id);

bool va_props_is_readonly(enum va_prop_id prop_id);
enum va_prop_type va_props_get_type(enum va_prop_id prop_id);
int32_t va_props_get_min_i32(enum va_prop_id prop_id);
int32_t va_props_get_max_i32(enum va_prop_id prop_id);
int32_t va_props_get_step_i32(enum va_prop_id prop_id);

bool va_props_get_bool(enum va_prop_id prop_id);
void va_props_set_bool(enum va_prop_id prop_id, bool value);

int32_t va_props_get_i32(enum va_prop_id prop_id);
void va_props_set_i32(enum va_prop_id prop_id, int32_t value);

const char *va_props_get_string(enum va_prop_id prop_id);
void va_props_set_string(enum va_prop_id prop_id, const char *value);

void va_props_get_bdaddr(enum va_prop_id prop_id, uint8_t bdaddr[VA_PROPS_BDADDR_LEN]);
void va_props_set_bdaddr(enum va_prop_id prop_id, const uint8_t value[VA_PROPS_BDADDR_LEN]);

#endif
