// driver to access gpio-registers from beaglebone-memory
// based on SPRUH73Q Dec2019, TODO: could be added to pssp, but needs entry in .cmd file

#ifndef PRU1_SYS_GPIO_H
#define PRU1_SYS_GPIO_H
#include <stdint.h>
#include "gpio.h"


// GPIO Registers, ch25.4
typedef struct{
    // 0h
    uint32_t GPIO_REVISION;
    uint32_t RSVD00x[3];
    // 10h
    uint32_t GPIO_SYSCONFIG;
    uint32_t RSVD01x[3];
    // 20h
    uint32_t GPIO_EOI;
    uint32_t GPIO_IRQSTATUS_RAW_0;
    uint32_t GPIO_IRQSTATUS_RAW_1;
    uint32_t GPIO_IRQSTATUS_0;
    // 30h
    uint32_t GPIO_IRQSTATUS_1;
    uint32_t GPIO_IRQSTATUS_SET_0;
    uint32_t GPIO_IRQSTATUS_SET_1;
    uint32_t GPIO_IRQSTATUS_CLR_0;
    // 40h
    uint32_t GPIO_IRQSTATUS_CLR_1;
    uint32_t GPIO_IRQWAKEN_0;
    uint32_t GPIO_IRQWAKEN_1;
    uint32_t RSVD04x[1];
    // 50h++
    uint32_t RSVD05x[4]; // 50h
    uint32_t RSVD06x[4]; // 60h
    uint32_t RSVD07x[4]; // 70h
    uint32_t RSVD08x[4]; // 80h
    uint32_t RSVD09x[4]; // 90h
    uint32_t RSVD0Ax[4]; // A0h
    uint32_t RSVD0Bx[4]; // B0h
    uint32_t RSVD0Cx[4]; // C0h
    uint32_t RSVD0Dx[4]; // D0h
    uint32_t RSVD0Ex[4]; // E0h
    uint32_t RSVD0Fx[4]; // F0h
    // 100h
    uint32_t RSVD10x[4];
    // 110h
    uint32_t RSVD100[1];
    uint32_t GPIO_SYSSTATUS;
    uint32_t RSVD108[2];
    // 120h
    uint32_t RSVD12x[4];
    // 130h
    uint32_t GPIO_CTRL;
    uint32_t GPIO_OE;               // output-enabled -> should also be sampled when starting a measurement
    uint32_t GPIO_DATAIN;           // sampled with interface clock
    uint32_t GPIO_DATAOUT;
    // 140h
    uint32_t GPIO_LEVELDETECT0;
    uint32_t GPIO_LEVELDETECT1;
    uint32_t GPIO_RISINGDETECT;     // rising-edge and falling-edge could be used to sample pins with IRQ
    uint32_t GPIO_FALLINGDETECT;
    // 150h
    uint32_t GPIO_DEBOUNCENABLE;
    uint32_t GPIO_DEBOUNCINGTIME;
    uint32_t RSVD15x[2];
    // 160h++
    uint32_t RSVD16x[4]; // 160h
    uint32_t RSVD17x[4]; // 170h
    uint32_t RSVD18x[4]; // 180h
    // 190h
    uint32_t GPIO_CLEARDATAOUT;
    uint32_t GPIO_SETDATAOUT;
} Gpio;

// pseudo-assertion to test for correct struct-size
extern uint32_t CHECK_STRUCT_Gpio__[1/(sizeof(Gpio) == 0x0198)];


// Memory Map, p182
#ifdef __GNUC__
volatile Gpio *CT_GPIO0__ = (void *)0x44E07000; // TODO: the other gnu-definitions in pssp should also not use __X
#define CT_GPIO0	(*CT_GPIO0__)
volatile Gpio *CT_GPIO1__ = (void *)0x4804C000;
#define CT_GPIO1	(*CT_GPIO1__)
volatile Gpio *CT_GPIO2__ = (void *)0x481AC000;
#define CT_GPIO2	(*CT_GPIO2__)
volatile Gpio *CT_GPIO3__ = (void *)0x481AE000;
#define CT_GPIO3	(*CT_GPIO3__)
#else
volatile __far Gpio CT_GPIO0 __attribute__((cregister("GPIO0", far), peripheral));
volatile __far Gpio CT_GPIO1 __attribute__((cregister("GPIO1", far), peripheral));
volatile __far Gpio CT_GPIO2 __attribute__((cregister("GPIO2", far), peripheral));
volatile __far Gpio CT_GPIO3 __attribute__((cregister("GPIO3", far), peripheral));
#endif


/* Monitor GPIO from System / Linux:
    sudo su
    cd /sys/class/gpio
    echo 81 > export
    cd gpio81
    echo in > direction
    cat value
 */


static inline void check_gpio_test()
{
    _GPIO_OFF(BIT_SHIFT(P8_11));
    const uint32_t gpio_reg = CT_GPIO2.GPIO_DATAIN;
    // test for shepherd sense-button, P8_34, gpio2[17], 81
    if (gpio_reg & (1U << 17U)) _GPIO_ON(BIT_SHIFT(P8_11));
    else                        _GPIO_OFF(BIT_SHIFT(P8_11));
}


#endif //PRU1_SYS_GPIO_H
