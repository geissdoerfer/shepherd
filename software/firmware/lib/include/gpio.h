#ifndef __GPIO_H_
#define __GPIO_H_

#ifdef __GNUC__
#include <pru/io.h>
#else // NOTE: gcc can't use registers directly, so this is a workaround to allow gcc & cgt
volatile register uint32_t __R30;
volatile register uint32_t __R31;
#define read_r30()      __R30
#define write_r30(x)    __R30 = x
#define read_r31()      __R31
#define write_r31(x)    __R31 = x
#endif

#if defined(PRU0)

    #define P8_11   (1U << 15U)
    #define P8_12   (1U << 14U)

    #define P9_25   (1U << 7U)
    #define P9_27   (1U << 5U)
    #define P9_28   (1U << 3U)
    #define P9_29   (1U << 1U)
    #define P9_30   (1U << 2U)
    #define P9_31   (1U << 0U)
    #define P9_41B  (1U << 6U)
    #define P9_42B  (1U << 4U)

#elif defined(PRU1)

    #define P8_20   (1U << 3U)
    #define P8_21   (1U << 12U)

    #define P8_27   (1U << 8U)
    #define P8_28   (1U << 10U)
    #define P8_29   (1U << 9U)
    #define P8_30   (1U << 11U)

    #define P8_39   (1U << 6U)
    #define P8_40   (1U << 7U)
    #define P8_41   (1U << 4U)
    #define P8_42   (1U << 5U)
    #define P8_43   (1U << 2U)
    #define P8_44   (1U << 3U)
    #define P8_45   (1U << 0U)
    #define P8_46   (1U << 1U)

#else
    #error
#endif

#ifdef __GNUC__
#define _GPIO_TOGGLE(x) write_r30(read_r30() ^ (x))
#define _GPIO_ON(x)     write_r30(read_r30() | (x))
#define _GPIO_OFF(x)    write_r30(read_r30() & ~(x))
#else
#define _GPIO_TOGGLE(x) __R30 ^= (x)
#define _GPIO_ON(x)     __R30 |= (x)
#define _GPIO_OFF(x)    __R30 &= ~(x)
#endif

#endif /* __GPIO_H_ */
