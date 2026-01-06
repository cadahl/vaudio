#ifndef VA_DEBUG_H
#define VA_DEBUG_H

#include <esp_log.h>

#ifndef NDEBUG
#define VA_DBG_ASSERT(cond, err) \
    if (!(cond)) { \
        ESP_ERROR_CHECK(err); \
    }
#else
#define VA_DBG_ASSERT(cond, err)
#endif

#define VA_LOGD(tag, format, ...) do { \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG, tag, "%s: " format, __FUNCTION__, ##__VA_ARGS__); \
    } while(0);

#define VA_LOGI(tag, format, ...) do { \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, tag, "%s: " format, __FUNCTION__, ##__VA_ARGS__); \
    } while(0);

#define VA_LOGW(tag, format, ...) do { \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, tag, "%s: " format, __FUNCTION__, ##__VA_ARGS__); \
    } while(0);

#define VA_LOGE(tag, format, ...) do { \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, tag, "%s: " format, __FUNCTION__, ##__VA_ARGS__); \
    } while(0);

#endif
