#include <stdint.h>
#include "spi_transfer_sys.h"
#include "gpio.h"
#include "hw_config.h"


void sys_spi_init()
{
    const uint8_t spi_mode = 1u; // 1: default, clock active high, sampling on falling edge
    const uint8_t word_length = 24; // bit, min is 4, max is 14, (can be higher, but must be split up into two registers)
    const uint8_t clk_div = 3; // Base is 48 MHz

    // trigger soft reset
    //SYS_SPI.SYSCONFIG_bit.SOFTRESET = 1;

    //SYSSTATUS, check if reset is done
    //while (SYS_SPI.SYSSTATUS_bit.RESETDONE == 0) {__delay_cycles(1u);}
    // TODO: before RESETDone-Bit is set, CLK and CLKSPIREF must be provided

    // disable idle mode
    SYS_SPI.SYSCONFIG_bit.CLOCKACTIVITY = 3; // 3: ocp and clocks are maintained
    SYS_SPI.SYSCONFIG_bit.SIDLEMODE = 1; // 1: stay active, no idle
    SYS_SPI.SYSCONFIG_bit.AUTOIDLE = 0; // 0: no idle, 1:

    // [IRQSTATUS, IRQENABLE]
    SYS_SPI.SYST_bit.SPIENDIR = 0u; // 0: Output, Master-Mode
    SYS_SPI.SYST_bit.SPIDATDIR0 = 0u; // 0: D0 -> output
    SYS_SPI.SYST_bit.SPIDATDIR1 = 1u; // 1: D1 -> input
    // DEBUG ?!?
    SYS_SPI.SYST_bit.SPICLK = 1u; // clock signal level
    SYS_SPI.SYST_bit.SPIDAT_0 = 1u; // data0 signal level
    SYS_SPI.SYST_bit.SPIEN_0 = 1;   // should set CSs to neutral high
    SYS_SPI.SYST_bit.SPIEN_1 = 1;
    SYS_SPI.SYST_bit.SPIEN_2 = 1;
    SYS_SPI.SYST_bit.SPIEN_3 = 1;

    SYS_SPI.MODULCTRL_bit.SYSTEM_TEST = 0; // 0: functional mode, 1: test
    SYS_SPI.MODULCTRL_bit.MS = 0; // 0: master mode, 1: slave
    SYS_SPI.MODULCTRL_bit.PIN34 = 0; // 0: SPIEN is CS (4Pin-Mode), 1: SPIEN not used
    SYS_SPI.MODULCTRL_bit.SINGLE = 1; // 1: One Channel Mode

    // allow modifying CTRL-Reg by disabling channel
    SYS_SPI.CH0CTRL_bit.EN = 0; // 1: enables the channel
    // config, TODO: TRM, FORCE, DMAW/DMAR
    SYS_SPI.CH0CONF_bit.CLKG = 0; // clock granularity, 0: power of 2, 1: integer divider
    SYS_SPI.CH0CONF_bit.FFER = 0; // 1: use RX-FiFo for this channel
    SYS_SPI.CH0CONF_bit.FFEW = 0; // 1: use TX-FiFo for this channel
    SYS_SPI.CH0CONF_bit.TCS  = 2; // Chip select time control. 0: 0.5 cycles before, 1: 1.5 cycles, 2: 2.5 cycles
    SYS_SPI.CH0CONF_bit.TURBO = 0; // multi word transfer, for buffered mode, 1: active
    SYS_SPI.CH0CONF_bit.IS = 1; // input select, 1: DAT1 receives, 0: DAT0 receives
    SYS_SPI.CH0CONF_bit.DPE1 = 1; // transmission enable for data1, 0: active
    SYS_SPI.CH0CONF_bit.DPE0 = 0; // transmission enable for data0, 0: active
    SYS_SPI.CH0CONF_bit.DMAR = 0; // DMA read request, 0: disabled
    SYS_SPI.CH0CONF_bit.DMAW = 0; // DMA write request, 0: disabled
    SYS_SPI.CH0CONF_bit.TRM = 0; // transmit/receive-mode, 0: trx, 1: rx, 2: tx,
    // DMA line assertions
    SYS_SPI.CH0CONF_bit.WL = word_length - 1u;
    SYS_SPI.CH0CONF_bit.EPOL = 1; // SPIEN polarity, 1: low active
    SYS_SPI.CH0CONF_bit.CLKD = clk_div; // clockdivider, power of 2, are bit-granular
    SYS_SPI.CH0CONF_bit.POL = (spi_mode >> 1u) & 0x01u; // Clk POLarity, 1=active low
    SYS_SPI.CH0CONF_bit.PHA = (spi_mode >> 0u) & 0x01u; // Clk PHAse, 1: latch on even numbered edges

    // SYS_SPI.CH0STAT_bit.

    SYS_SPI.CH0CTRL_bit.EXTCLK = 0; // clock ratio extension

    // [CH1-3], NOTE: CS2 & CS3 are not pinned out
    /*
    SYS_SPI.XFERLEVEL // fifo transfer levels
    SYS_SPI.DAFTX // DMA FIFO TX Register
    SYS_SPI.DAFRX // DMA FIFO RX Register
    */


    // TODO: it would be possible to repeat data-aquisition automatically, adc also supports auto-increment mode, you just have to collect data
    // TODO: dma-mode only when in transmit-only or receive-only or slave mode (chapter 24.3.5)
}

uint32_t sys_adc_readwrite(const uint32_t cs_pin, const uint32_t val)
{
    // TODO: follow chapter 24.3.11.3
    // just a proof of concept
    SYS_SPI.CH0CTRL_bit.EN = 0; // enables the channel
    GPIO_ON(cs_pin);
    SYS_SPI.TX0 = val; // page4944, if empty CH0STAT.TXS is set
    const uint32_t rx_value = SYS_SPI.RX0; // if full CH0STAT.RXS is set
    SYS_SPI.CH0CTRL_bit.EN = 1;
    GPIO_OFF(cs_pin);
    return rx_value;
}

extern void sys_dac_write(uint32_t cs_pin, uint32_t val)
{
    // just a proof of concept
    GPIO_TOGGLE(cs_pin);
    GPIO_TOGGLE(cs_pin);
    SYS_SPI.SYST_bit.SPIEN_0 = 0;
    SYS_SPI.TX0 = val;
    SYS_SPI.SYST_bit.SPIEN_0 = 1;

}
