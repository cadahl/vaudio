#ifndef VA_IO_H
#define VA_IO_H

#include <stdbool.h>
#include <esp_err.h>

enum va_io_output {
    VA_OUTPUT_DAC_ENABLE = 0,
    VA_OUTPUT_REMOTE_ENABLE,
    VA_OUTPUT_COUNT,
};

enum va_io_input {
    VA_IO_INPUT_MENU,
    VA_IO_INPUT_CONN,
    VA_IO_INPUT_MUTE,
    VA_IO_INPUT_UP,
    VA_IO_INPUT_DOWN,
    VA_IO_INPUT_SEL,
    VA_IO_INPUT_BACK,
    VA_IO_INPUT_ROT_A,
    VA_IO_INPUT_ROT_B,
    VA_IO_INPUT_COUNT
};

esp_err_t va_io_init(void);
void va_io_update(void);
bool va_io_input_is_down_event(enum va_io_input input);

esp_err_t va_io_set_output(enum va_io_output output, bool enable);

#endif
