#include <stdbool.h>
#include "u8g2/u8g2_esp32_hal.h"
#include "u8g2/u8g2.h"
#include <esp_lcd_panel_ssd1306.h>
#include <esp_lcd_io_spi.h>

#include "va_app.h"
#include "va_disp.h"
#include "va_props.h"

#define TAG "va_disp"

enum power_state {
    POWER_STATE_ON = 0,
    POWER_STATE_DIMMED = 1,
    POWER_STATE_OFF = 2,
};

static struct {
    enum power_state power_state;
    u8g2_t u8g2;
} state = { 0 };

static void update_power_state(void);

void va_disp_init(void) {    
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.spi.clk  = GPIO_NUM_18;
    u8g2_esp32_hal.bus.spi.mosi = GPIO_NUM_23;
    u8g2_esp32_hal.bus.spi.cs   = GPIO_NUM_5;
    u8g2_esp32_hal.dc           = GPIO_NUM_26;
    u8g2_esp32_hal.reset        = GPIO_NUM_15;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_ssd1309_128x64_noname0_f(&state.u8g2, U8G2_R0, u8g2_esp32_spi_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8g2_InitDisplay(&state.u8g2);
    u8g2_SetContrast(&state.u8g2, 0);
    u8g2_SetPowerSave(&state.u8g2, 0);

    u8g2_SetContrast(&state.u8g2, 255);
 //   u8g2_SetClipWindow(&state.u8g2, 0, 0, 128, 64);
    u8g2_ClearBuffer(&state.u8g2);
    u8g2_SendBuffer(&state.u8g2);
}

IRAM_ATTR void va_disp_update(void) {
    update_power_state();
}

IRAM_ATTR static void update_power_state(void) {
    const int seconds_since_last_input_time = (va_app_get_time_ms() - va_app_get_last_input_time_ms()) / 1000;

    enum power_state power_state = POWER_STATE_ON;
    if (seconds_since_last_input_time > va_props_get_i32(VA_PROP_ID_SCREENOFF_TIME)) {
        power_state = POWER_STATE_OFF;
    } else if (seconds_since_last_input_time > va_props_get_i32(VA_PROP_ID_DIMMING_TIME)) {
        power_state = POWER_STATE_DIMMED;
    }

    if (state.power_state == power_state) {
        return;
    }
    state.power_state = power_state;

    switch (power_state) {
        case POWER_STATE_ON:
            u8g2_SetPowerSave(&state.u8g2, 0);
            u8g2_SetContrast(&state.u8g2, 255);
            break;
        case POWER_STATE_DIMMED:
            u8g2_SetPowerSave(&state.u8g2, 0);
            u8g2_SetContrast(&state.u8g2, 0);
            break;
        case POWER_STATE_OFF:
            u8g2_SetPowerSave(&state.u8g2, 1);
            u8g2_SetContrast(&state.u8g2, 0);
            break;
        default:
            break;
    }
}

IRAM_ATTR struct u8g2_struct *va_disp_get_u8g2(void) {
    return &state.u8g2;
}
