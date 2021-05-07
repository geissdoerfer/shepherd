#ifndef INT_OPTIMIZED_H
#define INT_OPTIMIZED_H

#include <stdint.h>

// short-cuts for fastest data-types on architecture
// - pru needs 7 cycles and branch when int8 or int16 or int32 is used together in an OP
// - perfect for mixed usage and hinting minimal container size (that is needed per design)

#define int8_ft     int32_t
#define uint8_ft    uint32_t
#define int16_ft    int32_t
#define uint16_ft   uint32_t
#define bool_ft     uint32_t

/*
const uint32_t cuint32_1 = 1;
const uint16_t cuint16_1 = 1;
const uint8_t  cuint8_1 = 1;
const uint16_ft cuint16f_1 = 1;
const uint8_ft  cuint8f_1 = 1;
const int32_t cint32_1 = 1;
const int16_t cint16_1 = 1;
const int8_t  cint8_1 = 1;
const int16_ft cint16f_1 = 1;
const int8_ft  cint8f_1 = 1;
*/

#endif //INT_OPTIMIZED_H
