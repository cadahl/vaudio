#ifndef VA_RASTER_H
#define VA_RASTER_H

#include <stdint.h>
#include "va_math.h"

struct u8g2_struct;

void va_raster_init(void);

void va_raster_clear(void);

void va_raster_line_draw(
    struct u8g2_struct *u8g2,
    const struct va_point2_fp16 *a,
    const struct va_point2_fp16 *b);

#endif
