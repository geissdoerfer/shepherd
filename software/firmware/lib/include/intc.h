#ifndef __INTC_H_
#define __INTC_H_

#include "gpio.h"

/*
 * This triggers the corresponding system event in INTC
 * See PRU-ICSS Reference Guide §5.2.2.2
 */
#ifdef __GNUC__
#define INTC_TRIGGER_EVENT(x)   write_r31((1U << 5U) + ((x) - 16U))
#else // CGT
#define INTC_TRIGGER_EVENT(x)   __R31 = (1U << 5U) + ((x) - 16U)
#endif

#define INTC_CHECK_EVENT(x)     (CT_INTC.SECR0 &(1U << (x)))
#define INTC_CLEAR_EVENT(x)     (CT_INTC.SICR_bit.STS_CLR_IDX = (x))
// below is a deconstructed INTC_CHECK_EVENT, better optimizable for slow/far register read

inline uint32_t intc_get_secr0()
{
    return CT_INTC.SECR0;
}

inline uint32_t intc_check_event(const uint32_t secr0, const uint32_t pos)
{
    return ((secr0) &(1U << (pos)));
}

#endif /* __INTC_H_ */
