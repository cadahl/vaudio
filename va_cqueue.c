#include <stdbool.h>
#include <esp_system.h>
#include <esp_assert.h>
#include <stdatomic.h>
#include "va_cqueue.h"

void va_cqueue_init(struct va_cqueue *q, int capacity) {
    assert(capacity > 0);
    assert((capacity & (capacity - 1)) == 0);
    atomic_init(&q->read_pos, 0);
    atomic_init(&q->write_pos, 0);
    q->capacity = capacity;
    q->mask = capacity - 1;
    q->buf = malloc(capacity);
}

IRAM_ATTR bool va_cqueue_write(struct va_cqueue *q, const void *data, int data_size) {
    int write_pos = atomic_fetch_or_explicit(&q->write_pos, 0, memory_order_acquire);
    const int read_pos = atomic_fetch_or_explicit(&q->read_pos, 0, memory_order_acquire);
    const int used_size = write_pos - read_pos;

    if ((used_size + data_size) >= q->capacity) {
        // full
        return false;
    }

    const uint8_t *src = data;
    uint8_t *dst = q->buf;
    const int mask = q->mask;
    for (int i = 0; i < data_size; ++i) {
        dst[write_pos & mask] = src[i];
        ++write_pos;
    }

    atomic_exchange_explicit(&q->write_pos, write_pos, memory_order_release);
    return true;
}

IRAM_ATTR bool va_cqueue_read(struct va_cqueue *q, void *data, int data_size) {
    const int write_pos = atomic_fetch_or_explicit(&q->write_pos, 0, memory_order_acquire);
    int read_pos = atomic_fetch_or_explicit(&q->read_pos, 0, memory_order_acquire);
    const int used_size = write_pos - read_pos;

    if (used_size < data_size) {
        // not enough
        return false;
    }

    const uint8_t *src = q->buf;
    uint8_t *dst = data;
    const int mask = q->mask;
    for (int i = 0; i < data_size; ++i) {
        dst[i] = src[read_pos & mask];
        ++read_pos;
    }

    atomic_exchange_explicit(&q->read_pos, read_pos, memory_order_release);
    return true;
}
