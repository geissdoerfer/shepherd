//
// Created by ingmo on 26.08.2020.
//

#ifndef PRU1_INT_OPTIMIZED_H
#define PRU1_INT_OPTIMIZED_H

#include <stdint.h>

// short-name for fastest data-types
// - pru needs 7 cycles and branch when int8 or int16 or int32 is used together
// - perfect for mixing and showing minimal container
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

#endif //PRU1_INT_OPTIMIZED_H
