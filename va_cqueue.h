#ifndef VA_CQUEUE_H
#define VA_CQUEUE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

struct va_cqueue {
    _Atomic(int) read_pos;
    _Atomic(int) write_pos;
    int capacity;
    int mask;
    uint8_t *buf;
};

void va_cqueue_init(struct va_cqueue *q, int capacity);
bool va_cqueue_write(struct va_cqueue *q, const void *data, int data_size);
bool va_cqueue_read(struct va_cqueue *q, void *data, int data_size);

#endif
