#pragma once

#include <stdint.h>
#include "quirc.h"

#ifdef __cplusplus
extern "C" {
#endif

void quirc_opencv_init(void);
void quirc_opencv_rectify(struct quirc* qr);

#ifdef __cplusplus
}
#endif
