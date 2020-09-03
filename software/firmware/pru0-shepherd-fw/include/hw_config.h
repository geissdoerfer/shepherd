#ifndef __HW_CONFIG_H_
#define __HW_CONFIG_H_

#include "gpio.h"

// asm-routine needs unshifted bit positions
#define SPI_CS_ADC_REG      PIN_SHIFT(P9_25)
#define SPI_CS_ADC_PIN      P9_25
#define SPI_CS_DAC_REG      PIN_SHIFT(P9_31)
#define SPI_CS_DAC_PIN      P9_31

#define SPI_SCLK            PIN_SHIFT(P9_28)
#define SPI_MOSI            PIN_SHIFT(P9_30)
#define SPI_MISO            PIN_SHIFT(P9_27)

#define DEBUG_P0            PIN_SHIFT(P8_11)

#define USR_LED1            PIN_SHIFT(P8_12)

#define VIRTCAP_OUT_PIN     P9_29

#endif /* __HW_CONFIG_H_ */
