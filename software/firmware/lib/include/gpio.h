#ifndef __GPIO_H_
#define __GPIO_H_

#ifdef __GNUC__ \
                \
// NOTE: gcc can't use registers directly, so this is a workaround to allow gcc & cgt
#include <pru/io.h>

#else // CGT

volatile register uint32_t	__R30;
volatile register uint32_t	__R31;
#define read_r30()     		(__R30)
#define write_r30(pin_mask)	__R30 = (pin_mask)
#define read_r31()		(__R31)
#define write_r31(pin_mask)	__R31 = (pin_mask)

#endif

#if defined(PRU0)

    #define P8_11   (15U)
    #define P8_12   (14U)

    #define P9_25   (7U)
    #define P9_27   (5U)
    #define P9_28   (3U)
    #define P9_29   (1U)
    #define P9_30   (2U)
    #define P9_31   (0U)
    #define P9_41B  (6U)
    #define P9_42B  (4U)

#elif defined(PRU1)

    #define P8_20   (3U)
    #define P8_21   (12U)

    #define P8_27   (8U)
    #define P8_28   (10U)
    #define P8_29   (9U)
    #define P8_30   (11U)

    #define P8_39   (6U)
    #define P8_40   (7U)
    #define P8_41   (4U)
    #define P8_42   (5U)
    #define P8_43   (2U)
    #define P8_44   (3U)
    #define P8_45   (0U)
    #define P8_46   (1U)

#else
    #error "PRU0 / PRU1 not specified"
#endif

#define BIT_SHIFT(position)    (1U << (position))

#ifdef __GNUC__
#define GPIO_TOGGLE(pin_mask)	write_r30(read_r30() ^ (pin_mask))
#define GPIO_ON(pin_mask)	write_r30(read_r30() | (pin_mask))
#define GPIO_OFF(pin_mask)	write_r30(read_r30() & ~(pin_mask))
#else // CGT
#define GPIO_TOGGLE(pin_mask)	__R30 ^= (pin_mask)
#define GPIO_ON(pin_mask)	__R30 |= (pin_mask)
#define GPIO_OFF(pin_mask)	__R30 &= ~(pin_mask)
#endif

#endif /* __GPIO_H_ */
