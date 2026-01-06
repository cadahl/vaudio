#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "va_msg.h"

#define QUEUE_LENGTH 16
#define ITEM_SIZE sizeof(struct va_msg)

static struct {
    QueueHandle_t queue;
    StaticQueue_t static_queue;
    uint8_t queue_storage_area[QUEUE_LENGTH * ITEM_SIZE];
} state = { 0 };

void va_msg_init(void) {
    state.queue = xQueueCreateStatic(16, sizeof(struct va_msg), state.queue_storage_area, &state.static_queue);
}

IRAM_ATTR bool va_msg_get(struct va_msg *msg) {
    return xQueueReceive(state.queue, msg, 0) == pdTRUE;
}

IRAM_ATTR void va_msg_post_ptr(enum va_msg_type msg_type, void *ptr) {
    struct va_msg msg = {
        .type = msg_type,
        .ptr = ptr,
    };
    xQueueSend(state.queue, &msg, 0);
}

IRAM_ATTR void va_msg_post_i(enum va_msg_type msg_type, int i) {
    struct va_msg msg = {
        .type = msg_type,
        .i = i,
    };
    xQueueSend(state.queue, &msg, 0);
}

IRAM_ATTR void va_msg_post_b(enum va_msg_type msg_type, bool b) {
    struct va_msg msg = {
        .type = msg_type,
        .b = b,
    };
    xQueueSend(state.queue, &msg, 0);
}
