#include <unistd.h>
#include <string.h>
#include "u8g2/u8g2.h"

#include "va_app.h"
#include "va_audio.h"
#include "va_debug.h"
#include "va_io.h"
#include "va_math.h"
#include "va_menu.h"
#include "va_props.h"
#include "va_screen.h"
#include "va_screen_idle.h"
#include "va_screen_play.h"

#define TAG "va_menu"

#define MAX_LINES 4
#define PAGE_STACK_DEPTH 4
#define MAX_TEXT_LENGTH 33

bool is_empty(const char *s) {
    return !s[0];
}

typedef void (*action_t)(void);

static void select_bonded_device(void);
static void remove_bonded_device(void);

typedef const char *(*va_menu_prop_string_mapper)(int32_t value);

enum va_menu_value {
    VA_MENU_VALUE_NONE = 0,
    VA_MENU_VALUE_BLUETOOTH_BONDED_NAME_0,
    VA_MENU_VALUE_BLUETOOTH_BONDED_NAME_1,
    VA_MENU_VALUE_BLUETOOTH_BONDED_NAME_2,
    VA_MENU_VALUE_BLUETOOTH_BONDED_NAME_3,
};

struct menu_line_def {
    uint32_t screen_mask;
    char text[MAX_TEXT_LENGTH];
    enum va_menu_page_id target_page_id;
    enum va_prop_id prop_id;
    va_menu_prop_string_mapper string_mapper;
    action_t action;
};

struct menu_page_def {
    enum va_menu_page_id back_page_id;
    int nbr_lines;
    char title[MAX_TEXT_LENGTH];
    struct menu_line_def lines[MAX_LINES+1];
};

static void request_prop_updates(void) {
    va_props_request_update(VA_PROP_ID_FREE_HEAP_SIZE);
    va_props_request_update(VA_PROP_ID_LOWEST_FREE_HEAP_SIZE);
    va_props_request_update(VA_PROP_ID_RESET_REASON);
}

static IRAM_ATTR const char *volume_string_mapper(int32_t volume) {
    return va_audio_get_volume_str(volume);
}

static IRAM_ATTR const char *vis_type_string_mapper(int32_t value) {
    enum va_screen_play_vis_type vis_type = value;
    switch (vis_type) {
        case VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_NONE:
            return "Ingen";
        case VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_DOTS:
            return "Våg (punkter)";
        case VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED:
            return "Våg (fylld)";
        case VA_SCREEN_PLAY_VIS_TYPE_WAVEFORM_FILLED_ABS:
            return "Absvåg (fylld)";
        default:
            return "FELAKTIG";
    }
}

static IRAM_ATTR const char *idle_mode_string_mapper(int32_t value) {
    enum va_screen_idle_mode idle_mode = value;
    switch (idle_mode) {
        case VA_SCREEN_IDLE_MODE_BLANK:
            return "Släckt skärm";
        case VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER:
            return "Logo";
        case VA_SCREEN_IDLE_MODE_LOGO_VERTICAL_ENTER_LEAVE_WITH_HLINES:
            return "Logo+linjer";
        default:
            return "FELAKTIG";
    }
}

static IRAM_ATTR const char *seconds_string_mapper(int32_t seconds) {
    static char str[5] = {0};
    int hundreds = (seconds / 100) % 10;
    int tens = (seconds / 10) % 10;
    int ones = seconds % 10;
    str[0] = seconds >= 100 ? ('0' + hundreds) : ' ';
    str[1] = seconds >= 10 ? ('0' + tens) : ' ';
    str[2] = '0' + ones;
    str[3] = 's';
    str[4] = 0;
    return str;
}

static const struct menu_page_def page_defs[VA_MENU_PAGE_COUNT] = {
    [VA_MENU_PAGE_NONE] = { 0 },
    [VA_MENU_PAGE_MAIN] = {
        .back_page_id = VA_MENU_PAGE_NONE,
        .title = "Meny",
        .lines = {
            { .text = "Avbryt anslutning", .screen_mask = VA_SCREEN_CONNECT, .action = va_audio_toggle_discoverable, },
            { .text = "Koppla bort enhet", .screen_mask = VA_SCREEN_PLAY, .target_page_id = VA_MENU_PAGE_DISCONNECT_CONFIRM, },
            { .text = "Information", .target_page_id = VA_MENU_PAGE_INFORMATION, .action = request_prop_updates },
            { .text = "Inställningar", .target_page_id = VA_MENU_PAGE_SETTINGS, },
        },
    },
    [VA_MENU_PAGE_DISCONNECT_CONFIRM] = {
        .back_page_id = VA_MENU_PAGE_MAIN,
        .title = "Koppla bort enhet?",
        .lines = {
            { .text = "Nej", .target_page_id = VA_MENU_PAGE_MAIN, },
            { .text = "Ja", .action = va_audio_disconnect },
        },
    },
    [VA_MENU_PAGE_INFORMATION] = {
        .back_page_id = VA_MENU_PAGE_MAIN,
        .title = "Information",
        .lines = {
            { .text = "Version", .prop_id = VA_PROP_ID_VERSION },
            { .text = "Anledning till omstart", .prop_id = VA_PROP_ID_RESET_REASON },
            { .text = "Ledigt RAM", .prop_id = VA_PROP_ID_FREE_HEAP_SIZE, },
            { .text = "Ledigt RAM (lägst)", .prop_id = VA_PROP_ID_LOWEST_FREE_HEAP_SIZE, },
        },
    },
    [VA_MENU_PAGE_SETTINGS] = {
        .back_page_id = VA_MENU_PAGE_MAIN,
        .title = "Inställningar",
        .lines = {
            { .text = "Bluetooth", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH, },
            { .text = "Ljud", .target_page_id = VA_MENU_PAGE_SETTINGS_AUDIO, },
            { .text = "Övrigt", .target_page_id = VA_MENU_PAGE_SETTINGS_MISC, },
        },
    },
    [VA_MENU_PAGE_SETTINGS_MISC] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS,
        .title = "Övriga inställningar",
        .lines = {
            { .text = "Viloläge", .prop_id = VA_PROP_ID_IDLE_MODE, .string_mapper = idle_mode_string_mapper },
            { .text = "Stäng meny efter", .prop_id = VA_PROP_ID_MENU_HIDE_TIME, .string_mapper = seconds_string_mapper },
            { .text = "Dimma skärm efter", .prop_id = VA_PROP_ID_DIMMING_TIME, .string_mapper = seconds_string_mapper },
            { .text = "Släck skärm efter", .prop_id = VA_PROP_ID_SCREENOFF_TIME, .string_mapper = seconds_string_mapper },
        }
    },
    [VA_MENU_PAGE_SETTINGS_AUDIO] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS,
        .title = "Ljudinställningar",
        .lines = {
            { .text = "Volym", .target_page_id = VA_MENU_PAGE_SETTINGS_AUDIO_VOLUME, },
            { .text = "Utgångar", .target_page_id = VA_MENU_PAGE_SETTINGS_AUDIO_OUTPUT, },
            { .text = "Visuell effekt", .target_page_id = VA_MENU_PAGE_SETTINGS_AUDIO_VIS, },
        }
    },
    [VA_MENU_PAGE_SETTINGS_AUDIO_OUTPUT] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_AUDIO,
        .title = "Extern styrning",
        .lines = {
            { .text = "Växla V/H", .prop_id = VA_PROP_ID_SWAP_LR },
            { .text = "Fasvänd V", .prop_id = VA_PROP_ID_INVERT_L },
            { .text = "Fasvänd H", .prop_id = VA_PROP_ID_INVERT_R },
            { .text = "Slutsteg alltid på", .prop_id = VA_PROP_ID_REMOTE_ENABLE_ALWAYS },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS,
        .title = "Bluetoothinställningar",
        .lines = {
            { .text = "Kända enheter", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED },
            { .text = "Tidsgräns för anslutning", .prop_id = VA_PROP_DISCOVERABLE_TIME, .string_mapper = seconds_string_mapper },
            { .text = "Enhetsnamn", .prop_id = VA_PROP_ID_ANNOUNCED_BT_NAME },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH,
        .title = "Kända enheter",
        .lines = {
            { .text = "Rad 1", .prop_id = VA_PROP_ID_BT_BONDED_DEVICE_NAME_0, .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0, .action = select_bonded_device },
            { .text = "Rad 2", .prop_id = VA_PROP_ID_BT_BONDED_DEVICE_NAME_1, .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1, .action = select_bonded_device },
            { .text = "Rad 3", .prop_id = VA_PROP_ID_BT_BONDED_DEVICE_NAME_2, .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2, .action = select_bonded_device },
            { .text = "Rad 4", .prop_id = VA_PROP_ID_BT_BONDED_DEVICE_NAME_3, .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3, .action = select_bonded_device }
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH,
        .lines = {
            { .text = "Glöm enhet", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0_CONFIRM },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0_CONFIRM] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0,
        .title = "Glöm enhet?",
        .lines = {
            { .text = "Nej", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0 },
            { .text = "Ja", .action = remove_bonded_device },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH,
        .lines = {
            { .text = "Glöm enhet", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1_CONFIRM },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1_CONFIRM] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1,
        .title = "Glöm enhet?",
        .lines = {
            { .text = "Nej", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1 },
            { .text = "Ja", .action = remove_bonded_device },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH,
        .lines = {
            { .text = "Glöm enhet", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2_CONFIRM },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2_CONFIRM] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2,
        .title = "Glöm enhet?",
        .lines = {
            { .text = "Nej", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2 },
            { .text = "Ja", .action = remove_bonded_device },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH,
        .lines = {
            { .text = "Glöm enhet", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3_CONFIRM },
        }
    },
    [VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3_CONFIRM] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3,
        .title = "Glöm enhet?",
        .lines = {
            { .text = "Nej", .target_page_id = VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3 },
            { .text = "Ja", .action = remove_bonded_device },
        }
    },
    [VA_MENU_PAGE_SETTINGS_AUDIO_VOLUME] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_AUDIO,
        .title = "Volyminställningar",
        .lines = {
            { .text = "Rattens känslighet", .prop_id = VA_PROP_ID_VOLUME_ROTENC_SPEED },
            { .text = "Initial volym", .prop_id = VA_PROP_ID_VOLUME_DEFAULT, .string_mapper = volume_string_mapper },
            { .text = "Varna över volym", .prop_id = VA_PROP_ID_VOLUME_LIMIT, .string_mapper = volume_string_mapper },
            { .text = "Dölj dB efter", .prop_id = VA_PROP_ID_VOLUME_DB_HIDE_TIME, .string_mapper = seconds_string_mapper },
        }
    },
    [VA_MENU_PAGE_SETTINGS_AUDIO_VIS] = {
        .back_page_id = VA_MENU_PAGE_SETTINGS_AUDIO,
        .title = "Inst. för visuell effekt",
        .lines = {
            { .text = "Effekt", .prop_id = VA_PROP_ID_PLAY_VIS_TYPE, .string_mapper = vis_type_string_mapper },
        }
    },
};

static struct {
    uint32_t nav_time_ms;
    struct {
        int32_t selected_line_index;
        enum va_menu_page_id page_id;
        struct menu_page_def page;
    } prev, current;
    struct {
        bool active;
        enum va_prop_id prop_id;
        enum va_prop_id prop_type;
        union va_prop_value prop_value;
    } edit;
    char temp_str[128];
    int32_t bonded_bt_source;
} state = { 0 };

static void select_bonded_device(void) {
    VA_LOGD(TAG, "select_bonded_device: %d", state.current.selected_line_index);
    state.bonded_bt_source = state.current.selected_line_index;
}

static void remove_bonded_device(void) {
    VA_LOGD(TAG, "remove_bonded_device: %d", state.bonded_bt_source);
    va_audio_remove_bonded_device(state.bonded_bt_source);
}

static void begin_edit(enum va_prop_id prop_id) {
    const enum va_prop_type prop_type = va_props_get_type(prop_id);
    if (prop_type == VA_PROP_TYPE_BOOL) {
        state.edit.prop_value.b = va_props_get_bool(prop_id);
    } else if (prop_type == VA_PROP_TYPE_I32) {
        state.edit.prop_value.i = va_props_get_i32(prop_id);
    } else {
        VA_LOGE(TAG, "Can't edit prop of type %d.", prop_type);
        return;
    }
    state.edit.active = true;
    state.edit.prop_id = prop_id;
    state.edit.prop_type = prop_type;
}

static void end_edit(bool save) {
    if (save) {
        switch (state.edit.prop_type) {
            case VA_PROP_TYPE_BOOL:
                va_props_set_bool(state.edit.prop_id, state.edit.prop_value.b);
                break;
            case VA_PROP_TYPE_I32:
                va_props_set_i32(state.edit.prop_id, state.edit.prop_value.i);
                break;
            default:
                VA_LOGE(TAG, "Can't save prop of type %d", state.edit.prop_type);
                break;
        }
    }

    state.edit.active = false;
    state.edit.prop_id = VA_PROP_ID_NONE;
    state.edit.prop_type = VA_PROP_TYPE_NONE;
    state.edit.prop_value.i = 0;
}

static void edit(bool increase) {
    if (state.current.page_id == VA_MENU_PAGE_NONE) {
        return;
    }
    const enum va_prop_id prop_id = state.edit.prop_id;
    const enum va_prop_type prop_type = state.edit.prop_type;
    if (prop_type == VA_PROP_TYPE_BOOL) {
        state.edit.prop_value.b = !state.edit.prop_value.b;
    } else if (prop_type == VA_PROP_TYPE_I32) {
        const int32_t value = state.edit.prop_value.i;
        const int32_t min = va_props_get_min_i32(prop_id);
        const int32_t max = va_props_get_max_i32(prop_id);
        const int32_t step = va_props_get_step_i32(prop_id);
        if (increase) {
            VA_LOGD(TAG, "value %d, step %d, max %d", value, step, max);
            if ((value + step) <= max) {
                state.edit.prop_value.i = value + step;
            }
        } else {
            VA_LOGD(TAG, "value %d, step %d, min %d", value, step, min);
            if ((value - step) >= min) {
                state.edit.prop_value.i = value - step;
            }
        }        
    }
}

static void set_page(enum va_menu_page_id page_id) {
    VA_DBG_ASSERT(page_id >= VA_MENU_PAGE_NONE && page_id < VA_MENU_PAGE_COUNT, ESP_ERR_INVALID_ARG);
    if (page_id == state.current.page_id) {
        return;
    }
    state.nav_time_ms = va_app_get_time_ms();
    state.prev.selected_line_index = state.current.selected_line_index;
    state.current.selected_line_index = 0;
    state.prev.page = state.current.page;
    state.prev.page_id = state.current.page_id;
    state.current.page = page_defs[page_id];
    state.current.page_id = page_id;

    struct menu_line_def *lines = state.current.page.lines;
    int line_index = 0;
    for (int page_def_line_index = 0; page_def_line_index < MAX_LINES; ++page_def_line_index) {
        if (is_empty(lines[page_def_line_index].text) && !lines[page_def_line_index].prop_id) {
            break;
        }

        const uint32_t screen_mask = lines[page_def_line_index].screen_mask;
        if (!screen_mask || (screen_mask & va_screen_get())) {
            lines[line_index] = lines[page_def_line_index];

            if (page_id == VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED) {
                const char *bonded_device_name = va_props_get_string(lines[line_index].prop_id);
                if (strlen(bonded_device_name) > 0) {
                    strcpy(lines[line_index].text, bonded_device_name);
                    lines[line_index].prop_id = 0;
                    ++line_index;
                }
            } else {
                ++line_index;
            }
        }

    }
    state.current.page.nbr_lines = line_index;

    switch (page_id) {
        case VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_0:
            strcpy(state.current.page.title, va_props_get_string(VA_PROP_ID_BT_BONDED_DEVICE_NAME_0));
            break;
        case VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_1:
            strcpy(state.current.page.title, va_props_get_string(VA_PROP_ID_BT_BONDED_DEVICE_NAME_1));
            break;
        case VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_2:
            strcpy(state.current.page.title, va_props_get_string(VA_PROP_ID_BT_BONDED_DEVICE_NAME_2));
            break;
        case VA_MENU_PAGE_SETTINGS_BLUETOOTH_BONDED_3:
            strcpy(state.current.page.title, va_props_get_string(VA_PROP_ID_BT_BONDED_DEVICE_NAME_3));
            break;
        default:
            break;
    }
}

IRAM_ATTR bool va_menu_is_active(void) {
    return state.current.page_id != VA_MENU_PAGE_NONE;
}

static void on_select(void) {
    if (state.current.page_id == VA_MENU_PAGE_NONE) {
        return;
    }

    const int selected_line_index = state.current.selected_line_index;

    if (selected_line_index < 0 || selected_line_index >= state.current.page.nbr_lines) {
        return;
    }

    const struct menu_line_def *line_def = &state.current.page.lines[selected_line_index];

    if (line_def->target_page_id != VA_MENU_PAGE_NONE) {
        if (line_def->action) {
            line_def->action();
        }
        set_page(line_def->target_page_id);
        return;
    }

    if (line_def->prop_id != VA_PROP_ID_NONE && !va_props_is_readonly(line_def->prop_id)) {
        if (state.edit.active) {
            end_edit(true);
        } else {
            begin_edit(line_def->prop_id);
        }
    }

    if (line_def->action) {
        line_def->action();
        set_page(VA_MENU_PAGE_NONE);
    }
}

IRAM_ATTR void va_menu_update(void) {
    if (state.current.page_id == VA_MENU_PAGE_NONE) {
        if (va_io_input_is_down_event(VA_IO_INPUT_MENU)) {
            va_app_wake_up();
            set_page(VA_MENU_PAGE_MAIN);
            return;
        }
    }

    const int32_t time_since_last_input_ms = va_app_get_time_ms() - va_app_get_last_input_time_ms();
    if (time_since_last_input_ms >= (1000 * va_props_get_i32(VA_PROP_ID_MENU_HIDE_TIME))) {
        set_page(VA_MENU_PAGE_NONE);
        return;
    }

    if (va_io_input_is_down_event(VA_IO_INPUT_MENU)) {
        va_app_wake_up();
        if (state.edit.active) {
            end_edit(false);
        }
        set_page(VA_MENU_PAGE_NONE);
    } else if (va_io_input_is_down_event(VA_IO_INPUT_BACK)) {
        va_app_wake_up();
        if (state.edit.active) {
            end_edit(false);
        } else {
            set_page(state.current.page.back_page_id);
        }
    } else if (va_io_input_is_down_event(VA_IO_INPUT_SEL)) {
        va_app_wake_up();
        on_select();
    } else if (va_io_input_is_down_event(VA_IO_INPUT_UP)) {
        va_app_wake_up();
        if (state.edit.active) {
            edit(true);
        } else {
            if (state.current.selected_line_index > 0) {
                --state.current.selected_line_index;
            }
        }
    } else if (va_io_input_is_down_event(VA_IO_INPUT_DOWN)) {
        va_app_wake_up();
        if (state.edit.active) {
            edit(false);
        } else {
            if (state.current.selected_line_index < state.current.page.nbr_lines-1) {
                ++state.current.selected_line_index;
            }
        }
    }
}

static inline const char *int_to_value_text(int value) {
    char *temp_str = state.temp_str;

    if (value >= 10000) {
        *temp_str++ = '0' + ((value / 10000) % 10);
    }
    if (value >= 1000) {
        *temp_str++ = '0' + ((value / 1000) % 10);
    }
    if (value >= 100) {
        *temp_str++ = '0' + ((value / 100) % 10);
    }
    if (value >= 10) {
        *temp_str++ = '0' + ((value / 10) % 10);
    }
    *temp_str++ = '0' + (value % 10);
    *temp_str++ = 0;
    return state.temp_str;
}

static inline const char *bool_to_value_text(bool value) {
    return value ? "Ja" : "Nej";
}

static const char* get_line_value_text(const struct menu_page_def *page, int line_index) {
    if (line_index < 0 || line_index >= page->nbr_lines) {
        return "";
    }

    const enum va_prop_id prop_id = page->lines[line_index].prop_id;
    if (prop_id == VA_PROP_ID_NONE) {
        return "";
    } 

    const enum va_prop_type prop_type = va_props_get_type(prop_id);
    const va_menu_prop_string_mapper string_mapper = page->lines[line_index].string_mapper;
    const bool is_prop_being_edited = state.edit.active && state.edit.prop_id == prop_id;

    switch (prop_type) {
        case VA_PROP_TYPE_BOOL: {
            const bool value = is_prop_being_edited ? state.edit.prop_value.b : va_props_get_bool(prop_id);
            return bool_to_value_text(value);
            break;
        }
        case VA_PROP_TYPE_I32: {
            const int value = is_prop_being_edited ? state.edit.prop_value.i : va_props_get_i32(prop_id);
            if (string_mapper) {
                return string_mapper(value);
            }
            return int_to_value_text(value);
            break;
        }
        case VA_PROP_TYPE_STRING: {
            return va_props_get_string(prop_id);
        }
        default:
            return "";
    }
}

#define MARGIN_X (0)
#define MARGIN_Y (0)
#define MARGIN_TEXT_X (MARGIN_X+1)
#define MARGIN_TEXT_Y (MARGIN_Y+2)
#define LINE_HEIGHT 12
#define TRANSITION_TIME_MS 180

static IRAM_ATTR void draw_page(u8g2_t *u8g2, const struct menu_page_def *page, int pos_x, int pos_y, int32_t selected_line_index) {
    if (!page) {
        return;
    }

    const bool blink = va_app_get_time_ms() & 256;

    const int box_w = 128-MARGIN_X*2;
    const int box_h = 64-MARGIN_Y*2;

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, pos_x + MARGIN_X, pos_y + MARGIN_Y, box_w, box_h);
    
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_NokiaSmallPlain_tf);

    int x = pos_x + MARGIN_TEXT_X;
    int y = pos_y + MARGIN_TEXT_Y + LINE_HEIGHT;

    u8g2_DrawHLine(u8g2, pos_x + MARGIN_X, pos_y + MARGIN_Y + LINE_HEIGHT + 2, box_w);
    u8g2_DrawUTF8(u8g2, pos_x + MARGIN_X, pos_y + MARGIN_Y + LINE_HEIGHT, page->title);
    y += LINE_HEIGHT;

    x += 2;

    for (int i = 0; i < page->nbr_lines; ++i) {
        const bool is_selected = selected_line_index == i;

        if (is_selected) {
            u8g2_DrawBox(u8g2, MARGIN_X, y - LINE_HEIGHT + 3, box_w, LINE_HEIGHT-1);
            u8g2_SetDrawColor(u8g2, 0);
        }

        const char *value_text = get_line_value_text(page, i);

        if (!is_empty(page->lines[i].text)) {
            u8g2_DrawUTF8(u8g2, x, y, page->lines[i].text);

            if (value_text) {
                const int w = u8g2_GetUTF8Width(u8g2, value_text);

                if (is_selected && state.edit.active && blink) {
                    u8g2_DrawBox(u8g2, pos_x + 128 - w - 2, y - LINE_HEIGHT + 2, w+2, LINE_HEIGHT);
                    u8g2_SetDrawColor(u8g2, 1);
                }

                u8g2_DrawUTF8(u8g2, pos_x + 128 - w - 1, y, value_text);
            }
        } else {
            u8g2_DrawUTF8(u8g2, x, y, value_text);
        }


        if (is_selected) {
            u8g2_SetDrawColor(u8g2, 1);
        }

        y += LINE_HEIGHT;
    }
}

IRAM_ATTR void va_menu_draw(u8g2_t *u8g2) {
    const uint32_t time_ms = va_app_get_time_ms();
    const int time_since_nav_ms = time_ms - state.nav_time_ms;

    int transition = va_smoothstep(0, TRANSITION_TIME_MS, time_since_nav_ms) >> (16-7);

    if (state.prev.page_id != VA_MENU_PAGE_NONE) {
        if (state.current.page_id == VA_MENU_PAGE_NONE) {
            const int prev_page_y = -(transition>>1);
            if (prev_page_y > -64 && prev_page_y < 64) {
                draw_page(u8g2, &state.prev.page, 0, prev_page_y, state.prev.selected_line_index);
            }
        } else {
            if (state.current.page_id < state.prev.page_id) {
                const int prev_page_x = transition;
                if (prev_page_x >= 0 && prev_page_x < 128) {
                    draw_page(u8g2, &state.prev.page, prev_page_x, 0, state.prev.selected_line_index);
                }
            } else if (state.current.page_id > state.prev.page_id) {
                const int prev_page_x = -transition;
                if (prev_page_x > -128 && prev_page_x < 128) {
                    draw_page(u8g2, &state.prev.page, prev_page_x, 0, state.prev.selected_line_index);
                }
            }
        }
    }
    if (state.current.page_id != VA_MENU_PAGE_NONE) {
        if (state.prev.page_id == VA_MENU_PAGE_NONE) {
            const int page_y = (transition >> 1) - 64;
            if (page_y > -64 && page_y < 64) {
                draw_page(u8g2, &state.current.page, 0, page_y, state.current.selected_line_index);
            }
        } else {
            if (state.current.page_id < state.prev.page_id) {
                const int page_x = transition - 128;
                if (page_x > -128 && page_x < 128) {
                    draw_page(u8g2, &state.current.page, page_x, 0, state.current.selected_line_index);
                }
            } else if (state.current.page_id > state.prev.page_id) {
                const int page_x = 128 - transition;
                if (page_x >= 0 && page_x < 128) {
                    draw_page(u8g2, &state.current.page, page_x, 0, state.current.selected_line_index);
                }
            } else {
                draw_page(u8g2, &state.current.page, 0, 0, state.current.selected_line_index);
            }
        }
    }
}
