#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "va_app.h"
#include "va_audio.h"
#include "va_debug.h"
#include "va_disp.h"
#include "va_io.h"
#include "va_menu.h"
#include "va_msg.h"
#include "va_props.h"
#include "va_screen.h"
#include "va_screen_connect.h"
#include "va_screen_idle.h"
#include "va_screen_play.h"
#include "va_screen_splash.h"

#define TAG "va_app"

static struct {
    uint32_t time_ms;
    int32_t delta_time_ms;
    uint32_t last_input_time_ms;
} state = { 0 };

static TaskHandle_t app_task;

IRAM_ATTR uint32_t va_app_get_time_ms(void) {
    return state.time_ms;
}

IRAM_ATTR int32_t va_app_get_delta_time_ms(void) {
    return state.delta_time_ms;
}

IRAM_ATTR uint32_t va_app_get_last_input_time_ms(void) {
    return state.last_input_time_ms;
}

IRAM_ATTR void va_app_wake_up() {
    state.last_input_time_ms = state.time_ms;
}

IRAM_ATTR void va_app_run(void *params) {
    VA_LOGI(TAG, "initializing");

    va_msg_init();
    va_props_init();    
    va_screen_init();
    
    va_disp_init();
    ESP_ERROR_CHECK(va_audio_init());
    ESP_ERROR_CHECK(va_io_init());

    va_props_set_string(VA_PROP_ID_VERSION, "1.01");

    VA_LOGI(TAG, "initialized");

    int64_t last_time_us = 0;

    while (true) {
        int64_t time_us = esp_timer_get_time();
        const int64_t ideal_time_us = last_time_us + 16667;

#define DELAY_LOOP_OVERHEAD 500

/*        while ((ideal_time_us - time_us) > (4000 + DELAY_LOOP_OVERHEAD)) {
            vTaskDelay(4);
            time_us = esp_timer_get_time();
        }

        while ((ideal_time_us - time_us) > (2000 + DELAY_LOOP_OVERHEAD)) {
            vTaskDelay(2);
            time_us = esp_timer_get_time();
        }

        while ((ideal_time_us - time_us) > (1000 + DELAY_LOOP_OVERHEAD)) {
            vTaskDelay(1);
            time_us = esp_timer_get_time();
        }
*/
        while (time_us < ideal_time_us) {
            time_us = esp_timer_get_time();
        }

        if ((int32_t)(time_us - ideal_time_us) > 150) {
            VA_LOGI(TAG, "Frame wait overrun, %d us.", (int32_t)(time_us-ideal_time_us));
        }

        #define TIME_DIV_FIXED_BITS 8
        state.time_ms = ((time_us << TIME_DIV_FIXED_BITS) / 1000) >> TIME_DIV_FIXED_BITS;
        state.delta_time_ms = (((time_us - last_time_us) << TIME_DIV_FIXED_BITS) / 1000) >> TIME_DIV_FIXED_BITS;

        last_time_us = time_us;

        va_io_update();

        struct va_msg msg;
        while (va_msg_get(&msg)) {
            va_audio_on_message(&msg);
            va_screen_on_message(&msg);
        }

        va_audio_update();
        va_menu_update();

        if (!va_menu_is_active()) {
            va_screen_update();
        }

        va_disp_update();

        struct u8g2_struct *u8g2 = va_disp_get_u8g2();
        u8g2_ClearBuffer(u8g2);
        va_screen_draw(u8g2);
        va_menu_draw(u8g2);
        u8g2_SendBuffer(u8g2);
    }
}

void app_main() {
    esp_log_level_set("*", ESP_LOG_INFO);

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init(); 
    }
    ESP_ERROR_CHECK(err);

    xTaskCreate(va_app_run, "app", 10000, NULL, 15, &app_task);
}
