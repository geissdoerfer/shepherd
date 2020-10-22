#ifndef PRU1_DEBUG_ROUTINES_H
#define PRU1_DEBUG_ROUTINES_H

// Debug Code - Config
#define DEBUG_GPIO_EN   0   // state1= gpio-checking, state2=writing data, state0=loop&event-routines
#define DEBUG_EVENT_EN  0   // state1=Event1, s2=e2, s3=e3 (expensive part)
#define DEBUG_LOOP_EN   0

// Debug Code, state-changes add ~7 ticks (s0 -  s2), ~6 ticks (s3)
#define DEBUG_STATE_0       write_r30(read_r30() & ~(DEBUG_P0 | DEBUG_P1))
#define DEBUG_STATE_1       write_r30((read_r30() | DEBUG_P0) & ~DEBUG_P1)
#define DEBUG_STATE_2       write_r30((read_r30() | DEBUG_P1) & ~DEBUG_P0)
#define DEBUG_STATE_3       write_r30(read_r30() | (DEBUG_P1 | DEBUG_P0))

#if DEBUG_GPIO_EN > 0
#define DEBUG_GPIO_STATE_0  DEBUG_STATE_0
#define DEBUG_GPIO_STATE_1  DEBUG_STATE_1
#define DEBUG_GPIO_STATE_2  DEBUG_STATE_2
#else
#define DEBUG_GPIO_STATE_0
#define DEBUG_GPIO_STATE_1
#define DEBUG_GPIO_STATE_2
#endif

#if DEBUG_EVENT_EN > 0
#define DEBUG_EVENT_STATE_0  DEBUG_STATE_0
#define DEBUG_EVENT_STATE_1  DEBUG_STATE_1
#define DEBUG_EVENT_STATE_2  DEBUG_STATE_2
#define DEBUG_EVENT_STATE_3  DEBUG_STATE_3
#else
#define DEBUG_EVENT_STATE_0
#define DEBUG_EVENT_STATE_1
#define DEBUG_EVENT_STATE_2
#define DEBUG_EVENT_STATE_3
#endif

#if DEBUG_LOOP_EN > 0
// "print" number by toggling debug pins bitwise, lowest bitvalue first
static void inline shift_gpio(const uint32_t number)
{
    const uint32_t gpio_off = read_r30() & ~(DEBUG_P0 | DEBUG_P1);
    const uint32_t gpio_one = gpio_off | (DEBUG_P0 | DEBUG_P1);
    const uint32_t gpio_zero = gpio_off | DEBUG_P0;
    uint32_t value = number << 1u;
    while (value >>= 1u)
    {
        write_r30(gpio_off);
        write_r30((value & 1u) ? gpio_one : gpio_zero);
    }
    write_r30(gpio_off);
    __delay_cycles(8);
}

// analyze ticks between fn-calls (=time in loop), and output values for min, mean, max on debug pins
static void inline debug_loop_delays(const uint32_t shepherd_state)
{
    static uint32_t ticks_last = 0;
    static uint32_t ticks_max = 0;
    static uint32_t ticks_min = 0xFFFFFFFF;
    static uint32_t ticks_sum = 0;
    static uint32_t ticks_count = 0;

    if (shepherd_state == STATE_RUNNING)
    {
        const uint32_t ticks_current = CT_IEP.TMR_CNT;
        if (ticks_last > ticks_current)
        {
            ticks_last = ticks_current;
            return;
        }
        // this following part should be around 11-14 instructions
        const uint32_t ticks_diff = ticks_current - ticks_last;
        if (ticks_diff > ticks_max) ticks_max = ticks_diff;
        if (ticks_diff < ticks_min) ticks_min = ticks_diff;
        ticks_sum += ticks_diff;
        ticks_count += 1;

        if (ticks_count == (1u << 20u))
        {
            _GPIO_ON(DEBUG_P0 | DEBUG_P1);
            __delay_cycles(10);
            _GPIO_OFF(DEBUG_P0 | DEBUG_P1);
            __delay_cycles(8);

            shift_gpio(ticks_min);
            shift_gpio(ticks_sum>>20u);
            shift_gpio(ticks_max);

            ticks_sum = 0;
            ticks_count = 0;
        }
    }
    ticks_last = CT_IEP.TMR_CNT;
}
#endif


#endif //PRU1_DEBUG_ROUTINES_H
