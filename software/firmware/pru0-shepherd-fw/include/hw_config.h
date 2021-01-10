#ifndef _HW_CONFIG_H_
#define _HW_CONFIG_H_

#include "gpio.h"

#ifndef SHEPHERD_VER
#define SHEPHERD_VER 1
#endif

#if (SHEPHERD_VER == 1)

// asm-routine needs unshifted bit positions (_PIN)
#define SPI_CS_ADC_PIN     	(P9_25)
#define SPI_CS_ADC_MASK		BIT_SHIFT(SPI_CS_ADC_PIN)
#define SPI_CS_DAC_PIN      	(P9_31)
#define SPI_CS_DAC_MASK 	BIT_SHIFT(SPI_CS_DAC_PIN)

// make naming scheme compatible to v2
#define SPI_CS_REC_DAC_PIN      SPI_CS_DAC_PIN
#define SPI_CS_EMU_DAC_PIN      SPI_CS_DAC_PIN
#define SPI_CS_REC_DAC_MASK     SPI_CS_DAC_MASK
#define SPI_CS_EMU_DAC_MASK     SPI_CS_DAC_MASK
#define SPI_CS_REC_V_ADC_PIN    SPI_CS_ADC_PIN
#define SPI_CS_REC_C_ADC_PIN    SPI_CS_ADC_PIN
#define SPI_CS_EMU_ADC_PIN      SPI_CS_ADC_PIN
#define SPI_CS_REC_V_ADC_MASK   SPI_CS_ADC_MASK
#define SPI_CS_REC_C_ADC_MASK   SPI_CS_ADC_MASK
#define SPI_CS_EMU_ADC_MASK     SPI_CS_ADC_MASK

#define SPI_SCLK_MASK 		BIT_SHIFT(P9_28)
#define SPI_MOSI_MASK 		BIT_SHIFT(P9_30)
#define SPI_MISO_MASK 		BIT_SHIFT(P9_27)

#define DEBUG_PIN0_MASK 	BIT_SHIFT(P8_11)
#define DEBUG_PIN1_MASK 	BIT_SHIFT(P8_12)

#define VIRTCAP_OUT_PIN     	(P9_29)
#define VIRTCAP_OUT_MASK     	BIT_SHIFT(VIRTCAP_OUT_PIN)

#elif (SHEPHERD_VER == 2)

// TODO: remove when sampling.c is prepared for V2
#define SPI_CS_ADC_PIN     	(P9_25)
#define SPI_CS_ADC_MASK		BIT_SHIFT(SPI_CS_ADC_PIN)
#define SPI_CS_DAC_PIN      	(P9_31)
#define SPI_CS_DAC_MASK 	BIT_SHIFT(SPI_CS_DAC_PIN)

#define SPI_CS_REC_DAC_PIN      (P9_27)
#define SPI_CS_REC_DAC_MASK	BIT_SHIFT(SPI_CS_REC_DAC_PIN)
#define SPI_CS_REC_V_ADC_PIN    (P9_41B)
#define SPI_CS_REC_V_ADC_MASK 	BIT_SHIFT(SPI_CS_REC_V_ADC_PIN)
#define SPI_CS_REC_C_ADC_PIN    (P9_25)
#define SPI_CS_REC_C_ADC_MASK 	BIT_SHIFT(SPI_CS_REC_C_ADC_PIN)

#define SPI_CS_EMU_DAC_PIN      (P9_28)
#define SPI_CS_EMU_DAC_MASK 	BIT_SHIFT(SPI_CS_EMU_DAC_PIN)
#define SPI_CS_EMU_ADC_PIN      (P9_42B)
#define SPI_CS_EMU_ADC_MASK 	BIT_SHIFT(SPI_CS_EMU_ADC_PIN)

// Pins now share correct mapping with SPI1
#define SPI_SCLK_MASK           BIT_SHIFT(P9_31)
#define SPI_MOSI_MASK           BIT_SHIFT(P9_29)
#define SPI_MISO_MASK           BIT_SHIFT(P9_30)

// both pins have a LED
#define DEBUG_PIN0_MASK         BIT_SHIFT(P8_12)
#define DEBUG_PIN1_MASK         BIT_SHIFT(P8_11)

// TODO: has to stay as long as virtcap code is present (but will be replaced by virtual regulator)
#define VIRTCAP_OUT_MASK     	(DEBUG_PIN0_MASK)

#else
#error "shepherd-version not defined"
#endif

#endif /* _HW_CONFIG_H_ */
