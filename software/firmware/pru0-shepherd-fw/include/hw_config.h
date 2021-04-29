#ifndef _HW_CONFIG_H_
#define _HW_CONFIG_H_

#include "gpio.h"

// asm-routine needs unshifted bit positions (_PIN)
#define SPI_CS_ADC_PIN     	(P9_25)
#define SPI_CS_ADC_MASK		BIT_SHIFT(SPI_CS_ADC_PIN)
#define SPI_CS_DAC_PIN      	(P9_31)
#define SPI_CS_DAC_MASK 	BIT_SHIFT(SPI_CS_DAC_PIN)

#define SPI_SCLK_MASK 		BIT_SHIFT(P9_28)
#define SPI_MOSI_MASK 		BIT_SHIFT(P9_30)
#define SPI_MISO_MASK 		BIT_SHIFT(P9_27)

#define DEBUG_PIN0_MASK 	BIT_SHIFT(P8_11)
#define DEBUG_PIN1_MASK 	BIT_SHIFT(P8_12)

#endif /* _HW_CONFIG_H_ */
