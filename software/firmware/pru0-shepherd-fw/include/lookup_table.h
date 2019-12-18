#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t lookup(int32_t table[][9], int32_t current);
void lookup_init();

#ifdef __cplusplus
}
#endif
