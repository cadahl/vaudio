#include <string.h>
#include <memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs.h"
#include "esp_err.h"
#include "esp_system.h"

#include "va_debug.h"
#include "va_props.h"
#include "va_screen_idle.h"
#include "va_screen_play.h"

#define TAG "va_props"

#define FMT_ERR_FAILED_TO_GET_LENGTH_OF_PROP "Failed to get length of prop '%s', error: %04x"
#define FMT_ERR_FAILED_FOR_PROP "Failed for prop '%s', error: %04x"
#define FMT_ERR_BAD_LENGTH_OF_PROP "Bad length of prop '%s': %d"

static void load_bool(const char *name, bool *value, bool default_value);
static void load_i32(const char *name, int32_t *value, int32_t default_value);
static void load_string(const char *name, char **value, const char *default_value);
static void load_bdaddr(const char *name, uint8_t value[VA_PROPS_BDADDR_LEN], const uint8_t default_value[VA_PROPS_BDADDR_LEN]);

static void save_bool(const char *name, bool value);
static void save_i32(const char *name, int32_t value);
static void save_string(const char *name, const char *value);
static void save_bdaddr(const char *name, const uint8_t value[VA_PROPS_BDADDR_LEN]);

struct prop_info {
    enum va_prop_type type;
    bool is_readonly;
    const char persist_name[16];
    union {
        struct {
            bool default_value;
        } b;
        struct {
            int32_t default_value;
            int32_t min;
            int32_t max;
            int32_t step;
        } i;
        struct {
            const char *default_value;
        } s;
        struct {
            uint8_t default_value[VA_PROPS_BDADDR_LEN];
        } bdaddr;
    };
};

static const struct prop_info prop_infos[VA_PROP_ID_COUNT] = {
    [VA_PROP_ID_ANNOUNCED_BT_NAME] = {
        .persist_name = "bt_ann_name",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "VAudio940",
        }
    },
    [VA_PROP_ID_VOLUME_ROTENC_SPEED] = {
        .persist_name = "vol_rotenc_spd",
        .type = VA_PROP_TYPE_I32,
        .i = {
            .default_value = 3,
            .min = 1,
            .max = 6,
            .step = 1,
        }
    },
    [VA_PROP_ID_VOLUME_DEFAULT] = {
        .persist_name = "vol_default",
        .type = VA_PROP_TYPE_I32,
        .i = {
            .default_value = 8,
            .min = 0,
            .max = 16,
            .step = 1,
        }
    },
    [VA_PROP_ID_VOLUME_LIMIT] = {
        .persist_name = "vol_limit",
        .type = VA_PROP_TYPE_I32,
        .i = {
            .default_value = 20,
            .min = 16,
            .max = 31,
            .step = 1,
        }
    },
    [VA_PROP_ID_VOLUME_DB_HIDE_TIME] = {
        .persist_name = "vol_db_hide_time",
        .type = VA_PROP_TYPE_I32,
        .i = {
            .default_value = 3,
            .min = 1,
            .max = 10,
            .step = 1,
        }
    },
    [VA_PROP_ID_REMOTE_ENABLE_ALWAYS] = {
        .persist_name = "rem_en_always",
        .type = VA_PROP_TYPE_BOOL,
        .b = {
            .default_value = false,
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_0] = {
        .persist_name = "bt_bnd_addr_0",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_1] = {
        .persist_name = "bt_bnd_addr_1",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_2] = {
        .persist_name = "bt_bnd_addr_2",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_3] = {
        .persist_name = "bt_bnd_addr_3",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_4] = {
        .persist_name = "bt_bnd_addr_4",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_5] = {
        .persist_name = "bt_bnd_addr_5",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_6] = {
        .persist_name = "bt_bnd_addr_6",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_ADDR_7] = {
        .persist_name = "bt_bnd_addr_7",
        .type = VA_PROP_TYPE_BDADDR,
        .bdaddr = {
            .default_value = { 0 },
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_0] = {
        .persist_name = "bt_bnd_name_0",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_1] = {
        .persist_name = "bt_bnd_name_1",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_2] = {
        .persist_name = "bt_bnd_name_2",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_3] = {
        .persist_name = "bt_bnd_name_3",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_4] = {
        .persist_name = "bt_bnd_name_4",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_5] = {
        .persist_name = "bt_bnd_name_5",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_6] = {
        .persist_name = "bt_bnd_name_6",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_BT_BONDED_DEVICE_NAME_7] = {
        .persist_name = "bt_bnd_name_7",
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "",
        }
    },
    [VA_PROP_ID_MENU_HIDE_TIME] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "menu_hide_time",
        .i = {
            .default_value = 15,
            .min = 5,
            .max = 60,
            .step = 5,
        }
    },
    [VA_PROP_ID_DIMMING_TIME] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "dimming_time",
        .i = {
            .default_value = 10,
            .min = 5,
            .max = 60,
            .step = 5,
        }
    },
    [VA_PROP_ID_SCREENOFF_TIME] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "screenoff_time",
        .i = {
            .default_value = 60,
            .min = 30,
            .max = 60*5,
            .step = 30,
        }
    },
    [VA_PROP_DISCOVERABLE_TIME] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "disc_time",
        .i = {
            .default_value = 30,
            .min = 15,
            .max = 120,
            .step = 5,
        }
    },
    [VA_PROP_ID_RESET_REASON] = {
        .is_readonly = true,
        .type = VA_PROP_TYPE_I32
    },
    [VA_PROP_ID_FREE_HEAP_SIZE] = {
        .is_readonly = true,
        .type = VA_PROP_TYPE_I32
    },
    [VA_PROP_ID_LOWEST_FREE_HEAP_SIZE] = {
        .is_readonly = true,
        .type = VA_PROP_TYPE_I32
    },
    [VA_PROP_ID_VERSION] = {
        .is_readonly = true,
        .type = VA_PROP_TYPE_STRING,
        .s = {
            .default_value = "1.0",
        }
    },
    [VA_PROP_ID_PLAY_VIS_TYPE] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "play_vis_type",
        .i = {
            .default_value = VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED_ABS,
            .min = VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_NONE,
            .max = VA_SCREEN_PLAY_VIS_TYPE_COUNT-1,
            .step = 1,
        }
    },
    [VA_PROP_ID_IDLE_MODE] = {
        .type = VA_PROP_TYPE_I32,
        .persist_name = "idle_mode",
        .i = {
            .default_value = VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER,
            .min = VA_SCREEN_IDLE_MODE_BLANK,
            .max = VA_SCREEN_IDLE_MODE_COUNT-1,
            .step = 1,
        }
    },
    [VA_PROP_ID_SWAP_LR] = {
        .type = VA_PROP_TYPE_BOOL,
        .persist_name = "swap_lr",
        .b = {
            .default_value = false
        }
    },
    [VA_PROP_ID_INVERT_L] = {
        .type = VA_PROP_TYPE_BOOL,
        .persist_name = "invert_l",
        .b = {
            .default_value = false
        }
    },
    [VA_PROP_ID_INVERT_R] = {
        .type = VA_PROP_TYPE_BOOL,
        .persist_name = "invert_r",
        .b = {
            .default_value = false
        }
    }
};

static struct {
    nvs_handle_t handle;
    struct prop_info infos[VA_PROP_ID_COUNT];
    union va_prop_value values[VA_PROP_ID_COUNT];
} state = { 0 };

void va_props_init(void) {
    VA_LOGI(TAG, "va_props_init: Loading persisted props...");

    ESP_ERROR_CHECK(nvs_open("settings", NVS_READWRITE, &state.handle));

    for (int i = 0; i < VA_PROP_ID_COUNT; ++i) {
        const char *persist_name = prop_infos[i].persist_name;
        if (!persist_name || persist_name[0] == 0) {
            continue;
        }
        switch (prop_infos[i].type) {
            case VA_PROP_TYPE_BOOL:
                load_bool(persist_name, &state.values[i].b, prop_infos[i].b.default_value);
                break;
            case VA_PROP_TYPE_I32:
                load_i32(persist_name, &state.values[i].i, prop_infos[i].i.default_value);
                break;
            case VA_PROP_TYPE_STRING:
                load_string(persist_name, &state.values[i].s, prop_infos[i].s.default_value);
                break;
            case VA_PROP_TYPE_BDADDR:
                load_bdaddr(persist_name, state.values[i].bdaddr, prop_infos[i].bdaddr.default_value);
                break;
            default:
                break;
        }
    }
}

void va_props_request_update(enum va_prop_id prop_id) {
    switch (prop_id) {
        case VA_PROP_ID_RESET_REASON:
            va_props_set_i32(prop_id, (int32_t)esp_reset_reason());
            break;
        case VA_PROP_ID_FREE_HEAP_SIZE:
            va_props_set_i32(prop_id, (int32_t)esp_get_free_heap_size());
            break;
        case VA_PROP_ID_LOWEST_FREE_HEAP_SIZE:
            va_props_set_i32(prop_id, (int32_t)esp_get_minimum_free_heap_size());
            break;
        default:
            break;
    }
}

bool va_props_is_readonly(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].is_readonly;
}

enum va_prop_type va_props_get_type(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].type;
}

int32_t va_props_get_default_value_i32(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].i.default_value;
}

int32_t va_props_get_min_i32(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].i.min;
}

int32_t va_props_get_max_i32(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].i.max;
}

int32_t va_props_get_step_i32(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    return prop_infos[prop_id].i.step;
}

int32_t va_props_get_i32(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    return state.values[prop_id].i;
}

void va_props_set_i32(enum va_prop_id prop_id, int32_t value) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_I32, ESP_ERR_INVALID_ARG);
    int32_t *stored_value = &state.values[prop_id].i;
    if (*stored_value == value) {
        return;
    }
    *stored_value = value;

    const char *persist_name = prop_infos[prop_id].persist_name;
    if (persist_name) {
        save_i32(prop_infos[prop_id].persist_name, state.values[prop_id].i);
    }
}

bool va_props_get_bool(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_BOOL, ESP_ERR_INVALID_ARG);
    return state.values[prop_id].b;
}

void va_props_set_bool(enum va_prop_id prop_id, bool value) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_BOOL, ESP_ERR_INVALID_ARG);
    bool *stored_value = &state.values[prop_id].b;
    if (*stored_value == value) {
        return;
    }
    *stored_value = value;

    const char *persist_name = prop_infos[prop_id].persist_name;
    if (persist_name) {
        save_bool(prop_infos[prop_id].persist_name, state.values[prop_id].i);
    }
}

void va_props_get_bdaddr(enum va_prop_id prop_id, uint8_t bdaddr[VA_PROPS_BDADDR_LEN]) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_BDADDR, ESP_ERR_INVALID_ARG);
    memcpy(bdaddr, state.values[prop_id].bdaddr, VA_PROPS_BDADDR_LEN);
}

void va_props_set_bdaddr(enum va_prop_id prop_id, const uint8_t value[VA_PROPS_BDADDR_LEN]) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_BDADDR, ESP_ERR_INVALID_ARG);
    uint8_t *stored_value = state.values[prop_id].bdaddr;
    if (!memcmp(stored_value, value, VA_PROPS_BDADDR_LEN)) {
        return;
    }
    memcpy(stored_value, value, VA_PROPS_BDADDR_LEN);

    const char *persist_name = prop_infos[prop_id].persist_name;
    if (persist_name) {
        save_bdaddr(persist_name, value);
    }
}

const char *va_props_get_string(enum va_prop_id prop_id) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_STRING, ESP_ERR_INVALID_ARG);
    return state.values[prop_id].s;
}

void va_props_set_string(enum va_prop_id prop_id, const char *value) {
    VA_DBG_ASSERT(prop_id > 0 && prop_id < VA_PROP_ID_COUNT, ESP_ERR_INVALID_ARG);
    VA_DBG_ASSERT(prop_infos[prop_id].type == VA_PROP_TYPE_STRING, ESP_ERR_INVALID_ARG);
    char **stored_value = &state.values[prop_id].s;
    if (*stored_value != NULL && value != NULL && !strcmp(*stored_value, value)) {
        return;
    }
    free(*stored_value);
    *stored_value = strdup(value);

    const char *persist_name = prop_infos[prop_id].persist_name;
    if (persist_name) {
        save_string(persist_name, value);
    }
}

static void save_string(const char *name, const char *value) {
    const esp_err_t err = nvs_set_str(state.handle, name, value); 
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        return;
    }
    nvs_commit(state.handle);
}

static void load_string(const char *name, char **value, const char *default_value) {
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(state.handle, name, NULL, &required_size);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            VA_LOGE(TAG, FMT_ERR_FAILED_TO_GET_LENGTH_OF_PROP, name, err);
        }
        *value = strdup(default_value);
        return;
    }
    char *temp = malloc(required_size);
    err = nvs_get_str(state.handle, name, temp, &required_size);
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        free(temp);
        *value = strdup(default_value);
        return;
    }
    *value = temp;
}

static void save_bdaddr(const char *name, const uint8_t value[VA_PROPS_BDADDR_LEN]) {
    const esp_err_t err = nvs_set_blob(state.handle, name, value, VA_PROPS_BDADDR_LEN);
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        return;
    }
    nvs_commit(state.handle);
}

static void load_bdaddr(const char *name, uint8_t value[VA_PROPS_BDADDR_LEN], const uint8_t default_value[VA_PROPS_BDADDR_LEN]) {
    size_t required_size = 0;
    esp_err_t err = nvs_get_blob(state.handle, name, NULL, &required_size);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            VA_LOGE(TAG, FMT_ERR_FAILED_TO_GET_LENGTH_OF_PROP, name);
        }
        memcpy(value, default_value, VA_PROPS_BDADDR_LEN);
        return;
    }
    if (required_size != VA_PROPS_BDADDR_LEN) {
        VA_LOGE(TAG, FMT_ERR_BAD_LENGTH_OF_PROP, name, required_size);
        memcpy(value, default_value, VA_PROPS_BDADDR_LEN);
        return;
    }
    err = nvs_get_blob(state.handle, name, value, &required_size);
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        memcpy(value, default_value, VA_PROPS_BDADDR_LEN);
        return;
    }
}

static void save_i32(const char *name, int32_t value) {
    const esp_err_t err = nvs_set_i32(state.handle, name, value); 
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        return;
    }
    nvs_commit(state.handle);
}

static void load_i32(const char *name, int32_t *value, int32_t default_value) {
    const esp_err_t err = nvs_get_i32(state.handle, name, value); 
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        }
        *value = default_value;
    }
}

static void save_bool(const char *name, bool value) {
    const esp_err_t err = nvs_set_u8(state.handle, name, value ? 1 : 0); 
    if (err != ESP_OK) {
        VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        return;
    }
    nvs_commit(state.handle);
}

static void load_bool(const char *name, bool *value, bool default_value) {
    uint8_t value_u8;
    const esp_err_t err = nvs_get_u8(state.handle, name, &value_u8);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            VA_LOGE(TAG, FMT_ERR_FAILED_FOR_PROP, name, err);
        }
        *value = default_value;
    } else {
        *value = value_u8 != 0;
    }
}
