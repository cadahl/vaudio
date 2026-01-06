
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "freertos/queue.h"
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <esp_task.h>
#include "drv/mcp23017.h"

#include "va_app.h"
#include "va_debug.h"
#include "va_io.h"
#include "va_msg.h"

#define TAG "va_io"

#define VA_IO_INPUT_COUNTER_MAX 4
#define VA_IO_INPUT_COUNTER_HIGH 4
#define VA_IO_INPUT_COUNTER_LOW 1

static const int8_t rot_encoder_valid_states[] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

struct pin_def {
    gpio_num_t internal_gpio_num;
    mcp23017_pin_t exp_gpio_pin;
};

static void scan_i2c(void);
static void rot_encoder_timer_handler(void *arg);

static struct pin_def input_defs[VA_IO_INPUT_COUNT] = {
    [VA_IO_INPUT_MENU] = { .internal_gpio_num = GPIO_NUM_4 },
    [VA_IO_INPUT_CONN] = { .internal_gpio_num = GPIO_NUM_27 },
    [VA_IO_INPUT_MUTE] = { .internal_gpio_num = GPIO_NUM_19 },
    [VA_IO_INPUT_UP] = { .internal_gpio_num = GPIO_NUM_17 },
    [VA_IO_INPUT_DOWN] = { .internal_gpio_num = GPIO_NUM_16 },
    [VA_IO_INPUT_SEL] = { .exp_gpio_pin = MCP23017_PIN2 },
    [VA_IO_INPUT_BACK] = { .exp_gpio_pin = MCP23017_PIN1 },
    [VA_IO_INPUT_ROT_A] = { .internal_gpio_num = GPIO_NUM_33 },
    [VA_IO_INPUT_ROT_B] = { .internal_gpio_num = GPIO_NUM_32 },
};

static struct pin_def output_defs[VA_OUTPUT_COUNT] = {
    [VA_OUTPUT_DAC_ENABLE] = { .internal_gpio_num = GPIO_NUM_25 },
    [VA_OUTPUT_REMOTE_ENABLE] = { .exp_gpio_pin = MCP23017_PIN0 },
};

static const esp_timer_create_args_t rot_encoder_timer_args = {
        .name = "rot_encoder",
        .arg = NULL,
        .callback = rot_encoder_timer_handler,
        .dispatch_method = ESP_TIMER_TASK
};

static struct {
    uint32_t input_bits_cur;
    uint32_t input_bits_prev;
    uint8_t input_counters[VA_IO_INPUT_COUNT];

    uint8_t exp_port_a_input;
    uint8_t exp_port_b_input;
    uint8_t exp_port_a_output;
    uint8_t exp_port_b_output;

    i2c_bus_handle_t i2c_bus;
    mcp23017_handle_t expander_handle;
    bool i2c_scan_delay;
    uint32_t i2c_scan_delay_start_ms;

    struct {
        esp_timer_handle_t timer;
        uint8_t code;
        uint16_t store;
    } rot_encoder;

} state = {
    .exp_port_a_input = 0xFF,
    .exp_port_b_input = 0xFF,
};

static IRAM_ATTR esp_err_t get_input_state(enum va_io_input input, bool *value) {
    const mcp23017_pin_t exp_gpio_pin = input_defs[input].exp_gpio_pin;

    if (exp_gpio_pin == MCP23017_NOPIN) {
        const gpio_num_t internal_gpio_num = input_defs[input].internal_gpio_num;
        *value = gpio_get_level(internal_gpio_num) == 0;
        return ESP_OK;
    }

    if(exp_gpio_pin >= MCP23017_PIN0 && exp_gpio_pin <= MCP23017_PIN7) {
        *value = (state.exp_port_a_input & exp_gpio_pin) == 0;
        return ESP_OK;
    }
    
    if(exp_gpio_pin >= MCP23017_PIN8 && exp_gpio_pin <= MCP23017_PIN15) {
        *value = (state.exp_port_b_input & (exp_gpio_pin >> 8)) == 0;
        return ESP_OK;
    }

    VA_LOGE(TAG, "Invalid exp gpio pin %d.", exp_gpio_pin);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t va_io_init(void) {
    VA_LOGI(TAG, "initializing...");

    gpio_config_t io_conf ;
    memset(&io_conf, 0, sizeof(gpio_config_t));
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 0;
    for (int i = 0; i < VA_IO_INPUT_COUNT; ++i) {
        if (input_defs[i].exp_gpio_pin == MCP23017_NOPIN) {
           io_conf.pin_bit_mask |= 1ULL << ((uint64_t)input_defs[i].internal_gpio_num);
        }
    }
    VA_LOGI(TAG, "io_conf.pin_bit_mask = %x", io_conf.pin_bit_mask);
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed to configure input GPIOs: %04x", err);
    }

    err = esp_timer_create(&rot_encoder_timer_args, &state.rot_encoder.timer);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed creating encoder timer: %04x", err);
    } else {
        err = esp_timer_start_periodic(state.rot_encoder.timer, 1000);
        if (err != ESP_OK) {
            VA_LOGE(TAG, "Failed starting encoder timer: %04x", err);
        }
    }

    memset(&io_conf, 0, sizeof(gpio_config_t));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 0;
    for (int i = 0; i < VA_OUTPUT_COUNT; ++i) {
        if (output_defs[i].exp_gpio_pin == MCP23017_NOPIN) {
           io_conf.pin_bit_mask |= 1ULL << ((uint64_t)output_defs[i].internal_gpio_num);
        }
    }
    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed to configure output GPIOs: %d");
    }

    static const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = 22,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    state.i2c_bus = i2c_bus_create(I2C_NUM_1, &conf);
    if (state.i2c_bus == NULL) {
        VA_LOGE(TAG, "Failed to create i2c bus.");
    }

    return err;
}

static esp_err_t init_expander(void) {
    state.expander_handle = mcp23017_create(state.i2c_bus, 0x20);
    if (state.expander_handle == NULL) {
        VA_LOGE(TAG, "Failed to create device.");
        return ESP_FAIL;
    }

    esp_err_t err = mcp23017_check_present(state.expander_handle);
    if (err == ESP_OK) {
        VA_LOGI(TAG, "Device is present");
    } else {
        VA_LOGE(TAG, "Device is not present: %d", err);
        state.i2c_scan_delay = true;
        state.i2c_scan_delay_start_ms = va_app_get_time_ms();
        return ESP_FAIL;
    }

    err = mcp23017_set_io_dir(state.expander_handle, 0xFE, MCP23017_GPIOA);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed to set io dir: %d", err);
        return ESP_FAIL;
    }

    err = mcp23017_set_pullup(state.expander_handle, 0xFFFE);
    if (err != ESP_OK) {
        VA_LOGE(TAG, "Failed to set pullup: %d", err);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void deinit_expander(void) {
    if (state.expander_handle) {
        mcp23017_delete(&state.expander_handle);
    }
    state.expander_handle = NULL;
    state.exp_port_a_input = 0xFF;
    state.exp_port_b_input = 0xFF;
    state.exp_port_a_output = 0;
    state.exp_port_b_output = 0;
}

static void scan_i2c(void) {
    if (state.expander_handle) {
        return;
    }
    if (state.i2c_scan_delay) {
        const int32_t delay_delta_ms = va_app_get_time_ms() - state.i2c_scan_delay_start_ms;
        if (delay_delta_ms < 1000 * 15) {
            return;
        }
    }
    state.i2c_scan_delay = false;
    VA_LOGI(TAG, "scan_i2c: Scanning...");
    uint8_t found_addresses[16];
    int nbr_found = i2c_bus_scan(state.i2c_bus, found_addresses, sizeof(found_addresses));
    
    for (int i = 0; i < nbr_found; ++i) {
        if (found_addresses[i] == 0x20) {
            esp_err_t err = init_expander();
            if (err != ESP_OK) {
                
            }
            return;
        }
    }

    state.i2c_scan_delay = true;
    state.i2c_scan_delay_start_ms = va_app_get_time_ms();
}

IRAM_ATTR void va_io_update(void) {
    scan_i2c();
    esp_err_t err;

    if (state.expander_handle) {
        err = mcp23017_read_io(state.expander_handle, MCP23017_GPIOA, &state.exp_port_a_input);
        if (err == ESP_OK) {
            err = mcp23017_read_io(state.expander_handle, MCP23017_GPIOB, &state.exp_port_b_input);
        }
        if (err != ESP_OK) {
            VA_LOGE(TAG, "Failed reading from expander: %d", err);
            deinit_expander();
        }
    }

    state.input_bits_prev = state.input_bits_cur;

    for (int i = 0; i < VA_IO_INPUT_COUNT; ++i) {
        int c = state.input_counters[i];
        bool value = false;
        err = get_input_state(i, &value);
        if (err != ESP_OK) {
            VA_LOGE(TAG, "Failed reading input.");
            continue;
        }
        if (value) {
            if (c < VA_IO_INPUT_COUNTER_MAX) {
                ++c;
            }
        } else {
            if (c > 0) {
                --c;
            }
        }
        state.input_counters[i] = c;

        const uint32_t input_mask = 1UL << i;
        if (c <= VA_IO_INPUT_COUNTER_LOW) {
            state.input_bits_cur &= ~input_mask;
        }
        if (c >= VA_IO_INPUT_COUNTER_HIGH) {
            state.input_bits_cur |= input_mask;
        }
    }

//    VA_LOGI(TAG, "state.input_bits_cur = %p", state.input_bits_cur);
}

IRAM_ATTR bool va_io_input_is_down_event(enum va_io_input input) {
    const bool was_down = state.input_bits_prev & (1ULL << input);
    const bool is_down = state.input_bits_cur & (1ULL << input);
    return !was_down && is_down;
}

esp_err_t va_io_set_output(enum va_io_output output, bool enable) {
    if (output < 0 || output >= VA_OUTPUT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const struct pin_def *output_def = &output_defs[output];

    if (output_def->exp_gpio_pin == MCP23017_NOPIN) {
        return gpio_set_level(output_def->internal_gpio_num, enable ? 1 : 0);
    }

    if (!state.expander_handle) {
        return ESP_FAIL;
    }

    if (output_def->exp_gpio_pin >= MCP23017_PIN0 && output_def->exp_gpio_pin <= MCP23017_PIN7) {
        const uint8_t prev_value = state.exp_port_a_output;
        const uint8_t mask = output_def->exp_gpio_pin;
        uint8_t value = prev_value;

        if (enable) {
            value |= mask;
        } else {
            value &= ~mask;
        }

        if (value == prev_value) {
            return ESP_OK;
        }

        state.exp_port_a_output = value;
        return mcp23017_write_io(state.expander_handle, value, MCP23017_GPIOA);
    } else if (output_def->exp_gpio_pin >= MCP23017_PIN8 && output_def->exp_gpio_pin <= MCP23017_PIN15) {
        const uint8_t prev_value = state.exp_port_b_output;
        const uint8_t mask = output_def->exp_gpio_pin >> 8;
        uint8_t value = prev_value;

        if (enable) {
            value |= mask;
        } else {
            value &= ~mask;
        }

        if (value == prev_value) {
            return ESP_OK;
        }

        state.exp_port_b_output = value;
        return mcp23017_write_io(state.expander_handle, value, MCP23017_GPIOB);
    } else {
        VA_LOGE(TAG, "Invalid exp gpio pin %d", output_def->exp_gpio_pin);
        return ESP_ERR_INVALID_ARG;
    }
}

static IRAM_ATTR void rot_encoder_timer_handler(void *arg) {
    uint8_t code = state.rot_encoder.code;
    code <<= 2;
    code |= gpio_get_level(input_defs[VA_IO_INPUT_ROT_A].internal_gpio_num);
    code |= gpio_get_level(input_defs[VA_IO_INPUT_ROT_B].internal_gpio_num) << 1;
    code &= 0xf;
    state.rot_encoder.code = code;

    if (!rot_encoder_valid_states[code]) {
        return;
    }

    uint16_t store = state.rot_encoder.store;
    store = (store << 4) | code;

 //   VA_LOGI(TAG, "store %04x, code %02x", store, code);

    if (store == 0xe817 || store == 0x17e8) {
        va_msg_post_i(VA_MSG_INPUT_WHEEL_CHANGED, 1);
    } else if (store == 0xd42b || store == 0x2bd4) {
        va_msg_post_i(VA_MSG_INPUT_WHEEL_CHANGED, -1);
    }

    state.rot_encoder.store = store;
}
