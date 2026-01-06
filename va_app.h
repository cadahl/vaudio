#ifndef VA_APP_H
#define VA_APP_H

#include <stdint.h>

uint32_t va_app_get_time_ms(void);
int32_t va_app_get_delta_time_ms(void);
uint32_t va_app_get_last_input_time_ms(void);

void va_app_wake_up(void);

#endif
