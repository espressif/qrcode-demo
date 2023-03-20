#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pgm_save(const char* path, int w, int h, const uint8_t* data);


#ifdef __cplusplus
}
#endif