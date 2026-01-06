#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_assert.h>

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "va_app.h"
#include "va_audio.h"
#include "va_debug.h"
#include "va_math.h"
#include "va_msg.h"
#include "va_props.h"
#include "va_screen_play.h"
#include "va_screen.h"
#include "va_io.h"

#define TAG "va_audio"

static struct {
    enum va_audio_connection_state connection_state; 
    bool is_playing;
    int volume_knob;
    enum va_audio_volume volume;
    bool has_volume_changed;
    bool output_enable;
    int32_t discoverable_time_left_ms;
    int32_t wheel_delta;
    bool remote_enable;
} state = { 0 };

/* event for stack up */
enum {
    BT_APP_EVT_STACK_UP = 0,
};

static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param);
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

static bool check_bonded_device_settings(void);

esp_err_t va_audio_init(void) {
    esp_err_t err;
    VA_LOGI(TAG, "initializing");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        VA_LOGE(BT_AV_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(err));
        return err;
    }
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        VA_LOGE(BT_AV_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    if ((err = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        VA_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        VA_LOGE(BT_AV_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(err));
        return err;
    }

    /* set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    /* set default parameters for Legacy Pairing (use fixed pin code 1234) */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    const uint8_t *bda = esp_bt_dev_get_address();
    if (bda) {
        VA_LOGI(BT_AV_TAG, "Own address:[%02x:%02x:%02x:%02x:%02x:%02x]", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    }
    bt_app_task_start_up();

    /* bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    if (!check_bonded_device_settings()) {
        check_bonded_device_settings();
    }

    VA_LOGI(TAG, "initialized");

    return ESP_OK;
}

static const enum va_prop_id bonded_device_addr_prop_ids[] = {
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_0,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_1,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_2,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_3,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_4,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_5,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_6,
    VA_PROP_ID_BT_BONDED_DEVICE_ADDR_7,
}; 

static const enum va_prop_id bonded_device_name_prop_ids[] = {
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_0,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_1,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_2,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_3,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_4,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_5,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_6,
    VA_PROP_ID_BT_BONDED_DEVICE_NAME_7,
}; 

static const int max_nbr_bonded_device_settings = sizeof(bonded_device_addr_prop_ids) / sizeof(enum va_prop_id);

void va_audio_remove_bonded_device(int32_t device_index_in_settings) {
    if (device_index_in_settings < 0 || device_index_in_settings >= max_nbr_bonded_device_settings) {
        VA_LOGE(TAG, "Invalid index %d", device_index_in_settings);
        return;
    }
    esp_bd_addr_t bdaddr;
    va_props_get_bdaddr(bonded_device_addr_prop_ids[device_index_in_settings], bdaddr);
    esp_bt_gap_remove_bond_device(bdaddr);
 
    esp_bd_addr_t zero_bdaddr = { 0 };
    for (int i = device_index_in_settings; i < max_nbr_bonded_device_settings-1; ++i) {
        va_props_get_bdaddr(bonded_device_addr_prop_ids[device_index_in_settings+1], bdaddr);
        va_props_set_bdaddr(bonded_device_addr_prop_ids[device_index_in_settings], bdaddr);

        va_props_set_string(bonded_device_name_prop_ids[device_index_in_settings],
            va_props_get_string(bonded_device_name_prop_ids[device_index_in_settings+1]));
    }
    va_props_set_bdaddr(bonded_device_addr_prop_ids[max_nbr_bonded_device_settings-1], zero_bdaddr);    
    va_props_set_string(bonded_device_name_prop_ids[max_nbr_bonded_device_settings-1], "");
}

void va_audio_add_bonded_device(uint8_t bdaddr[VA_PROPS_BDADDR_LEN]) {
    const uint8_t zero_bdaddr[VA_PROPS_BDADDR_LEN] = { 0 };
    uint8_t address_in_settings[VA_PROPS_BDADDR_LEN];
    for (int i = 0; i < max_nbr_bonded_device_settings; ++i) {
        va_props_get_bdaddr(bonded_device_addr_prop_ids[i], address_in_settings);
        if (!memcmp(bdaddr, address_in_settings, VA_PROPS_BDADDR_LEN)) {            
            VA_LOGI(TAG, "Device already exists at index %d.", i);
            return;
        }
    }
    for (int i = 0; i < max_nbr_bonded_device_settings; ++i) {
        va_props_get_bdaddr(bonded_device_addr_prop_ids[i], address_in_settings);
        if (!memcmp(zero_bdaddr, address_in_settings, VA_PROPS_BDADDR_LEN)) {
            VA_LOGI(TAG, "Adding device at index %d.", i);
            va_props_set_bdaddr(bonded_device_addr_prop_ids[i], bdaddr);

            char name[32] = {0};
            sprintf(name, "Okänd #%d", i + 1);
            va_props_set_string(bonded_device_name_prop_ids[i], name);
            return;
        }
    }
    VA_LOGE(TAG, "No free slot found!");
}

static void update_bonded_device_setting(uint8_t bdaddr[VA_PROPS_BDADDR_LEN], const char *name) {
    uint8_t address_in_settings[VA_PROPS_BDADDR_LEN];
    for (int i = 0; i < max_nbr_bonded_device_settings; ++i) {
        va_props_get_bdaddr(bonded_device_addr_prop_ids[i], address_in_settings);
        if (!memcmp(address_in_settings, bdaddr, VA_PROPS_BDADDR_LEN)) {
            VA_LOGI(TAG, "Found bonded device in settings at index %d.", i);
            const char *existing_name = va_props_get_string(bonded_device_name_prop_ids[i]);
            if (strcmp(existing_name, name) != 0) {
                VA_LOGI(TAG, "Updating name from '%s' to '%s'.", existing_name, name);
                va_props_set_string(bonded_device_name_prop_ids[i], name);
            }
            return;
        }
    }
    VA_LOGI(TAG, "Device not found in settings.");
}

static bool check_bonded_device_settings(void) {
    bool valid = true;

    for (int prop_idx = 0; prop_idx < max_nbr_bonded_device_settings; ++prop_idx) {
        uint8_t address_in_settings[VA_PROPS_BDADDR_LEN];
        va_props_get_bdaddr(bonded_device_addr_prop_ids[prop_idx], address_in_settings);
        const char *name = va_props_get_string(bonded_device_name_prop_ids[prop_idx]);
        VA_LOGI(TAG, "Address %d in settings (%02x:%02x:%02x:%02x:%02x:%02x) '%s'.", prop_idx,
            address_in_settings[0], address_in_settings[1], address_in_settings[2], address_in_settings[3], address_in_settings[4], address_in_settings[5], name);
    }

    int nbr_bonded_devices_in_db = esp_bt_gap_get_bond_device_num();
    VA_LOGI(TAG, "Nbr bonded devices in db: %d", nbr_bonded_devices_in_db);

    const esp_bd_addr_t zero_bdaddr = { 0 };

    esp_bd_addr_t addresses_in_db[max_nbr_bonded_device_settings];
    esp_err_t err = esp_bt_gap_get_bond_device_list(&nbr_bonded_devices_in_db, addresses_in_db);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed getting list of bonded devices from db. Wiping devices from settings.");
        valid = false;
    } else {
        int32_t nbr_bonded_devices_in_settings = 0;
        for (int prop_idx = 0; prop_idx < max_nbr_bonded_device_settings; ++prop_idx) {
            uint8_t address_in_settings[VA_PROPS_BDADDR_LEN] = {0};
            va_props_get_bdaddr(bonded_device_addr_prop_ids[prop_idx], address_in_settings);
            if (!memcmp(zero_bdaddr, address_in_settings, VA_PROPS_BDADDR_LEN)) {
                VA_LOGD(TAG, "Address %d in settings is empty.", prop_idx);
                continue;
            } 
            bool found_in_db = false;       
            for (int db_idx = 0; db_idx < nbr_bonded_devices_in_db; ++db_idx) {
                if (!memcmp(addresses_in_db[db_idx], address_in_settings, VA_PROPS_BDADDR_LEN)) {
                    VA_LOGD(TAG, "Address %d in settings (%02x:%02x:%02x:%02x:%02x:%02x) is found in db at index %d.", prop_idx,
                        address_in_settings[0], address_in_settings[1], address_in_settings[2], address_in_settings[3], address_in_settings[4], address_in_settings[5], db_idx);
                    found_in_db = true;
                    break;
                }
            }
            if (!found_in_db) {
                VA_LOGE(TAG, "Address %d in settings (%02x:%02x:%02x:%02x:%02x:%02x) not found in db!", prop_idx,
                    address_in_settings[0], address_in_settings[1], address_in_settings[2], address_in_settings[3], address_in_settings[4], address_in_settings[5]);
                valid = false;
            }
            ++nbr_bonded_devices_in_settings;
        }

        if (nbr_bonded_devices_in_db != nbr_bonded_devices_in_settings) {
            VA_LOGE(TAG, "Number of bonded devices in settings (%d) does not match number in db (%d)!", nbr_bonded_devices_in_settings, nbr_bonded_devices_in_db);
            valid = false;
        }
    }

    if (valid) {
        VA_LOGI(TAG, "Bonded device settings are ok.");
        return true;
    } else {
        VA_LOGE(TAG, "Bonded device settings are invalid, rebuilding them.");

        for (int i = 0; i < max_nbr_bonded_device_settings; ++i) {
            va_props_set_bdaddr(bonded_device_addr_prop_ids[i], zero_bdaddr);
            va_props_set_string(bonded_device_name_prop_ids[i], "");
        }

        const int n = nbr_bonded_devices_in_db > max_nbr_bonded_device_settings ? max_nbr_bonded_device_settings : nbr_bonded_devices_in_db;
        for (int i = 0; i < n; ++i) {
            va_props_set_bdaddr(bonded_device_addr_prop_ids[i], addresses_in_db[i]);

            char name[32] = {0};
            sprintf(name, "Okänd #%d", i + 1);
            va_props_set_string(bonded_device_name_prop_ids[i], name);
        }
        return false;
    }
}

static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param) {
    switch (event) {
        case ESP_BT_DEV_NAME_RES_EVT: {
            if (param->name_res.status == ESP_BT_STATUS_SUCCESS) {
                VA_LOGI(BT_AV_TAG, "Get local device name success: %s", param->name_res.name);
            } else {
                VA_LOGE(BT_AV_TAG, "Get local device name failed, status: %d", param->name_res.status);
            }
            break;
        }
        default: {
            VA_LOGI(BT_AV_TAG, "event: %d", event);
            break;
        }
    }
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    uint8_t *bda = NULL;
    switch (event) {
        /* when authentication completed, this event comes */
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                VA_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
                ESP_LOG_BUFFER_HEX(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            } else {
                VA_LOGE(BT_AV_TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
            }
            VA_LOGI(BT_AV_TAG, "link key type of current link is: %d", param->auth_cmpl.lk_type);
            break;
        }
        case ESP_BT_GAP_ENC_CHG_EVT: {
            char *str_enc[3] = {"OFF", "E0", "AES"};
            bda = (uint8_t *)param->enc_chg.bda;
            VA_LOGI(BT_AV_TAG, "Encryption mode to [%02x:%02x:%02x:%02x:%02x:%02x] changed to %s",
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], str_enc[param->enc_chg.enc_mode]);
            break;
        }

        /* when Security Simple Pairing user confirmation requested, this event comes */
        case ESP_BT_GAP_CFM_REQ_EVT:
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %"PRIu32, param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);

            va_msg_post_i(VA_MSG_BLUETOOTH_PAIR_CODE, (int32_t)param->cfm_req.num_val);
            break;
        /* when Security Simple Pairing passkey notified, this event comes */
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %"PRIu32, param->key_notif.passkey);
            break;
        /* when Security Simple Pairing passkey requested, this event comes */
        case ESP_BT_GAP_KEY_REQ_EVT:
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
            break;

        /* when GAP mode changed, this event comes */
        case ESP_BT_GAP_MODE_CHG_EVT:
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d",
                    param->mode_chg.mode);
            break;
        /* when ACL connection completed, this event comes */
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            bda = (uint8_t *)param->acl_conn_cmpl_stat.bda;
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT Connected to [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x",
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_conn_cmpl_stat.stat);

            va_msg_post_i(VA_MSG_BLUETOOTH_PAIR_COMPLETED, 0);
            break;
        /* when ACL disconnection completed, this event comes */
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            bda = (uint8_t *)param->acl_disconn_cmpl_stat.bda;
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT Disconnected from [%02x:%02x:%02x:%02x:%02x:%02x], reason: 0x%x",
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->acl_disconn_cmpl_stat.reason);

            va_msg_post_i(VA_MSG_BLUETOOTH_PAIR_FAILED, 0);
            break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
            bda = (uint8_t *)param->read_rmt_name.bda;
            VA_LOGI(BT_AV_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT [%02x:%02x:%02x:%02x:%02x:%02x], status: 0x%x, name: '%s'",
                    bda[0], bda[1], bda[2], bda[3], bda[4], bda[5], param->read_rmt_name.stat, param->read_rmt_name.rmt_name);
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
                update_bonded_device_setting(param->read_rmt_name.bda, (const char *)&param->read_rmt_name.rmt_name[0]);
                va_msg_post_ptr(VA_MSG_AUDIO_SOURCE_NAME_CHANGED, strdup((const char *)param->read_rmt_name.rmt_name));
            }
            break;
        }
        /* others */
        default: {
            VA_LOGI(BT_AV_TAG, "event: %d", event);
            break;
        }
    }
}

static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    VA_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    switch (event) {
    /* when do the stack up, this event comes */
    case BT_APP_EVT_STACK_UP: {
        esp_bt_gap_set_device_name(va_props_get_string(VA_PROP_ID_ANNOUNCED_BT_NAME));
        esp_bt_dev_register_callback(bt_app_dev_cb);
        esp_bt_gap_register_callback(bt_app_gap_cb);

        assert(esp_avrc_ct_init() == ESP_OK);
        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
        assert(esp_avrc_tg_init() == ESP_OK);
        esp_avrc_tg_register_callback(bt_app_rc_tg_cb);

        esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
        assert(esp_avrc_tg_set_rn_evt_cap(&evt_set) == ESP_OK);

        assert(esp_a2d_sink_init() == ESP_OK);
        esp_a2d_register_callback(&bt_app_a2d_cb);
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);

        /* Get the default value of the delay value */
        esp_a2d_sink_get_delay_value();
        /* Get local device name */
        esp_bt_gap_get_device_name();

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        break;
    }
    /* others */
    default:
        VA_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

static DRAM_ATTR const int32_t vol_amp_table_10p[] = {
    0,
    52,
    58,
    66,
    74,
    83,
    93,
    104,
    117,
    131,
    147,
    165,
    185,
    207,
    233,
    261,
    293,
    328,
    369,
    414,
    464,
    521,
    584,
    655,
    735,
    825,
    926,
    1039,
    1165,
    1308,
    1467,
    1646,
    1847,
    2072,
    2325,
    2609,
    2927,
    3285,
    3685,
    4135,
    4640,
    5206,
    5841,
    6554,
    7353,
    8250,
    9257,
    10387,
    11654,
    13076,
    14672,
    16462,
    18471,
    20724,
    23253,
    26090,
    29274,
    32846,
    36854,
    41350,
    46396,
    52057,
    58409,
    65536
};

ESP_STATIC_ASSERT(sizeof(vol_amp_table_10p) / sizeof(int32_t) == VA_AUDIO_VOLUME_COUNT);

static DRAM_ATTR const char *vol_str_table[] = {
    "   TYST",
    "-31.0db",
    "-30.5dB",
    "-30.0dB",
    "-29.5dB",
    "-29.0dB",
    "-28.5dB",
    "-28.0dB",
    "-27.5dB",
    "-27.0dB",
    "-26.5dB",
    "-26.0dB",
    "-25.5dB",
    "-25.0dB",
    "-24.5dB",
    "-24.0dB",
    "-23.5dB",
    "-23.0dB",
    "-22.5dB",
    "-22.0dB",
    "-21.5dB",
    "-21.0dB",
    "-20.5dB",
    "-20.0dB",
    "-19.5dB",
    "-19.0dB",
    "-18.5dB",
    "-18.0dB",
    "-17.5dB",
    "-17.0dB",
    "-16.5dB",
    "-16.0dB",
    "-15.5dB",
    "-15.0dB",
    "-14.5dB",
    "-14.0dB",
    "-13.5dB",
    "-13.0dB",
    "-12.5dB",
    "-12.0dB",
    "-11.5dB",
    "-11.0dB",
    "-10.5dB",
    "-10.0dB",
    " -9.5dB",
    " -9.0dB",
    " -8.5dB",
    " -8.0dB",
    " -7.5dB",
    " -7.0dB",
    " -6.5dB",
    " -6.0dB",
    " -5.5dB",
    " -5.0dB",
    " -4.5dB",
    " -4.0dB",
    " -3.5dB",
    " -3.0dB",
    " -2.5dB",
    " -2.0dB",
    " -1.5dB",
    " -1.0dB",
    " -0.5dB",
    "  0.0dB",
};

ESP_STATIC_ASSERT(sizeof(vol_str_table) / sizeof(const char *) == VA_AUDIO_VOLUME_COUNT);

static inline IRAM_ATTR enum va_audio_volume clamp_volume(enum va_audio_volume volume) {
    return (enum va_audio_volume)va_clamp_i32(
        VA_AUDIO_VOLUME_0,
        VA_AUDIO_VOLUME_63,
        volume);
}

static inline IRAM_ATTR void set_volume(enum va_audio_volume volume) {
    state.volume_knob = volume << 2;
    state.volume = clamp_volume(volume);
}

static inline IRAM_ATTR void set_volume_knob(int volume_knob) {
    state.volume_knob = volume_knob;
    state.volume = clamp_volume(volume_knob >> 2);
}

IRAM_ATTR enum va_audio_volume va_audio_get_volume(void) {
    return state.volume;
}

IRAM_ATTR const char *va_audio_get_volume_str(enum va_audio_volume volume) {
    return vol_str_table[clamp_volume(volume)];
}

IRAM_ATTR bool va_audio_has_volume_changed(void) {
    return state.has_volume_changed;
}

IRAM_ATTR void va_audio_apply_volume(const int16_t * __restrict src, int nbr_samples, int32_t * __restrict dst) {
    const bool swap_lr = va_props_get_bool(VA_PROP_ID_SWAP_LR);
    const bool invert_l = va_props_get_bool(VA_PROP_ID_INVERT_L);
    const bool invert_r = va_props_get_bool(VA_PROP_ID_INVERT_R);
    const enum va_audio_volume volume0 = clamp_volume(state.output_enable ? (state.volume_knob >> 2) : VA_AUDIO_VOLUME_0);
    const enum va_audio_volume volume1 = clamp_volume(volume0 > VA_AUDIO_VOLUME_0 ? volume0 + 1 : volume0);
    const int vol_amp0 = vol_amp_table_10p[volume0];
    const int vol_amp1 = vol_amp_table_10p[volume1];
    const int vol_amp_frac = state.volume_knob & 3;
    int vol_amp = 
        vol_amp_frac == 0 ? vol_amp0 :
        vol_amp_frac == 1 ? (vol_amp0 * 3 + vol_amp1) / 4 :
        vol_amp_frac == 2 ? (vol_amp0 + vol_amp1) / 2 :
                            (vol_amp0 + vol_amp1 * 3) / 4;
    if (vol_amp < 0) {
        vol_amp = 0;
    }
    if (vol_amp > 65536) {
        vol_amp = 65536;
    }
    int m[4];
    if (swap_lr) {
        m[0] = 0;       m[1] = vol_amp;
        m[2] = vol_amp; m[3] = 0;
    } else {
        m[0] = vol_amp; m[1] = 0;
        m[2] = 0;       m[3] = vol_amp;
    }
    if (invert_l) {
        m[0] = -m[0];
        m[1] = -m[1];
    }
    if (invert_r) {
        m[2] = -m[2];
        m[3] = -m[3];
    }
    for (int i = 0; i < nbr_samples; i += 2) {
        const int32_t l = src[i];
        const int32_t r = src[i+1];
        dst[i] = l * m[0] + r * m[1];
        dst[i+1] = l * m[2] + r * m[3];
    }
}

IRAM_ATTR void va_audio_submit_samples_for_waveform(const int16_t *src, int nbr_samples) {    
    static int8_t *temp = NULL;
    static int temp_capacity = 0;
    int nbr_waveform_samples = nbr_samples/16;
    if (!temp || temp_capacity < nbr_waveform_samples) {
        free(temp);
        temp = malloc(nbr_waveform_samples);
    }

    for (int i = 0, ri = 0; i < nbr_waveform_samples; ++i, ri += 16) {
        temp[i] = src[ri] >> 8;
    }
    va_screen_play_write_waveform_l(temp, nbr_waveform_samples);

    for (int i = 0, ri = 0; i < nbr_waveform_samples; ++i, ri += 16) {
        temp[i] = src[ri + 1] >> 8;
    }
    va_screen_play_write_waveform_r(temp, nbr_waveform_samples);
}

IRAM_ATTR void va_audio_set_output_enable(bool enable) {
    if (state.output_enable == enable) {
        return;
    }
    state.output_enable = enable;
    va_io_set_output(VA_OUTPUT_DAC_ENABLE, enable);
}

IRAM_ATTR bool va_audio_get_output_enable(void) {
    return state.output_enable;
}

IRAM_ATTR bool va_audio_is_playing(void) {
    return state.is_playing;
}

IRAM_ATTR bool va_audio_is_muted(void) {
    return !state.output_enable || state.volume == 0;
}

IRAM_ATTR enum va_audio_connection_state va_audio_get_connection_state(void) {
    return state.connection_state;
}

IRAM_ATTR static void update_remote_enable(void) {
    const bool remote_enable = 
        state.connection_state == VA_AUDIO_CONNECTION_STATE_CONNECTED ||
        va_props_get_bool(VA_PROP_ID_REMOTE_ENABLE_ALWAYS);

    if (remote_enable == state.remote_enable) {
        return;
    }
    state.remote_enable = remote_enable;

    esp_err_t err = va_io_set_output(VA_OUTPUT_REMOTE_ENABLE, remote_enable);
    if (err != ESP_OK) {
        state.remote_enable = false;
    }
}

IRAM_ATTR static void update_discoverable(void) {
    if (state.discoverable_time_left_ms <= 0) {
        return;
    }
    state.discoverable_time_left_ms -= va_app_get_delta_time_ms();
    if (state.discoverable_time_left_ms > 0) {
        return;
    }
    VA_LOGI(TAG, "update_discoverable: Becoming undiscoverable.");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    state.discoverable_time_left_ms = 0;
    va_msg_post_b(VA_MSG_AUDIO_DISCOVERABLE_CHANGED, false);
}

IRAM_ATTR void va_audio_update(void) {
    update_remote_enable();
    update_discoverable();

    state.has_volume_changed = false;

    if (va_audio_get_connection_state() == VA_AUDIO_CONNECTION_STATE_CONNECTED) {
        if (va_io_input_is_down_event(VA_IO_INPUT_MUTE)) {
            va_app_wake_up();
            va_audio_set_output_enable(!va_audio_get_output_enable());
            state.has_volume_changed = true;
        }

        if (state.wheel_delta) {
            va_app_wake_up();
            state.volume_knob = state.volume_knob + state.wheel_delta * va_props_get_i32(VA_PROP_ID_VOLUME_ROTENC_SPEED);
            if (state.volume_knob < 0) {
                state.volume_knob = 0;
            }
            if (state.volume_knob >= (VA_AUDIO_VOLUME_COUNT<<2)) {
                state.volume_knob = (VA_AUDIO_VOLUME_COUNT<<2)-1;
            }
            set_volume_knob(state.volume_knob);
            state.wheel_delta = 0;
            state.has_volume_changed = true;
        }
    }
}

void va_audio_toggle_discoverable(void) {
    if (state.discoverable_time_left_ms == 0) {
        VA_LOGI(TAG, "Becoming discoverable.");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        state.discoverable_time_left_ms = va_props_get_i32(VA_PROP_DISCOVERABLE_TIME) * 1000;
        va_msg_post_b(VA_MSG_AUDIO_DISCOVERABLE_CHANGED, true);
    } else {
        state.discoverable_time_left_ms = 1;
    }
}

void va_audio_disconnect(void) {
    VA_LOGD(TAG, "va_audio_disconnect");
    bt_app_disconnect();
}

IRAM_ATTR void va_audio_on_message(struct va_msg *msg) {
    // VA_LOGI(TAG, "va_audio_on_message: %u", msg->type);
    switch (msg->type) {
        case VA_MSG_AUDIO_CONNECTION_STATE_CHANGED:
            state.connection_state = msg->i;
            switch (msg->i) {
                case VA_AUDIO_CONNECTION_STATE_CONNECTING:
                    set_volume_knob(0);
                    va_audio_set_output_enable(false);
                    break;
                case VA_AUDIO_CONNECTION_STATE_CONNECTED:
                    va_screen_set(VA_SCREEN_PLAY);
                    set_volume(va_props_get_i32(VA_PROP_ID_VOLUME_DEFAULT));
                    va_audio_set_output_enable(true);
                    break;
                case VA_AUDIO_CONNECTION_STATE_DISCONNECTING:
                    set_volume_knob(0);
                    va_audio_set_output_enable(false);
                    break;
                case VA_AUDIO_CONNECTION_STATE_DISCONNECTED:
                    va_screen_set(VA_SCREEN_IDLE);
                    set_volume_knob(0);
                    va_audio_set_output_enable(false);
                    break;
            }
            break;
        case VA_MSG_INPUT_WHEEL_CHANGED:
            VA_LOGD(TAG, "wheel delta %d", msg->i);
            state.wheel_delta += msg->i;
            break;
        case VA_MSG_AUDIO_IS_PLAYING_CHANGED:
            state.is_playing = msg->b;
            break;
        case VA_MSG_AUDIO_DISCOVERABLE_CHANGED: {
            if (msg->b) {
                va_screen_set(VA_SCREEN_CONNECT);
            } else {
                if (va_screen_get() == VA_SCREEN_CONNECT) {
                    va_screen_set(state.connection_state == VA_AUDIO_CONNECTION_STATE_CONNECTED ?
                        VA_SCREEN_PLAY : VA_SCREEN_IDLE);
                }
            }
            break;
        }
        default:
            break;
   }
}
