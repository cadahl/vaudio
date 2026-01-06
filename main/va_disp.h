#ifndef VA_DISP_H
#define VA_DISP_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

struct u8g2_struct;

void va_disp_init(void);
void va_disp_update(void);

struct u8g2_struct *va_disp_get_u8g2(void);

#endif
