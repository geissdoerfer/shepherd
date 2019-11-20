#include <stdint.h>

#include "gpio.h"
#include "hw_config.h"
#include "sampling.h"

#define CMD_ID_ADC 0
#define CMD_ID_DAC 1

#define ADC_CH_V_IN 0
#define ADC_CH_V_OUT 1
#define ADC_CH_A_IN 2
#define ADC_CH_A_OUT 3

#define MAN_CH_SLCT (1 << 15) | (1 << 14)

#define DAC_CH_V_ADDR (1 << 16)
#define DAC_CH_A_ADDR (0 << 16)

#define DAC_CMD_OFFSET 19
#define DAC_ADDR_OFFSET 16

extern unsigned int adc_readwrite(unsigned int cs_pin, unsigned int val);
extern void dac_write(unsigned int cs_pin, unsigned int val);

static inline void sample_harvesting(struct SampleBuffer *buffer,
                                     unsigned int sample_idx) {
  /* Read current  and select channel 0 (voltage) for the read after! */
  buffer->values_current[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_IN << 10));
  /* Read voltage and select channel 2 (current) for the read after! */
  buffer->values_voltage[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_IN << 10));
}

static inline void sample_load(struct SampleBuffer *buffer,
                               unsigned int sample_idx) {
  /* Read load current and select load voltage for next reading */
  buffer->values_current[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
  /* Read load voltage and select load current for next reading */
  buffer->values_voltage[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
}

static inline void sample_emulation(struct SampleBuffer *buffer,
                                    unsigned int sample_idx) {
  /* write the emulation voltage value from buffer to DAC */
  dac_write(SPI_CS_DAC, buffer->values_current[sample_idx] | DAC_CH_A_ADDR);
  /* write the emulation current value from buffer to DAC */
  dac_write(SPI_CS_DAC, buffer->values_voltage[sample_idx] | DAC_CH_V_ADDR);

  /* read the load current value from ADC to buffer and select load voltage for
   * next reading */
  buffer->values_current[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
  /* read the load voltage value from ADC to buffer and select load current for
   * next reading */
  buffer->values_voltage[sample_idx] =
      adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
}

struct ADCReading sample_virtcap(struct SampleBuffer *buffer,
                                  unsigned int sample_idx) {
	struct ADCReading read;
  static int under_sample_voltage_cntr = 0;
  static int last_voltage_measurement = 0;
  static int last_current_measurement = 0;

  if (under_sample_voltage_cntr == 7) {
    under_sample_voltage_cntr = 0;

		read.current = last_current_measurement;
    buffer->values_current[sample_idx] = last_current_measurement;

    /* Read load voltage and select load current for next reading */
		read.voltage =
        adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
    buffer->values_voltage[sample_idx] = read.voltage;
    last_voltage_measurement = buffer->values_voltage[sample_idx];

  } else if (under_sample_voltage_cntr == 6) {
    /* Read load current and select load voltage for next reading */
		read.current = 
        adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
    buffer->values_current[sample_idx] = read.current;
    last_current_measurement = buffer->values_current[sample_idx];

		read.voltage = last_voltage_measurement;
    buffer->values_voltage[sample_idx] = last_voltage_measurement;


  } else {
    /* Read load current and select load current for next reading */
		read.current = 
        adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
    buffer->values_current[sample_idx] = read.current;

		read.voltage = last_voltage_measurement;
    buffer->values_voltage[sample_idx] = last_voltage_measurement;
  }

  under_sample_voltage_cntr++;

	return read;
}

void sample(struct SampleBuffer *current_buffer, unsigned int sample_idx,
            enum ShepherdMode mode) {
  switch (mode) {
    case MODE_HARVESTING:
      sample_harvesting(current_buffer, sample_idx);
      break;
    case MODE_LOAD:
      sample_load(current_buffer, sample_idx);
      break;
    case MODE_EMULATION:
      sample_emulation(current_buffer, sample_idx);
      break;
    case MODE_VIRTCAP:
      sample_virtcap(current_buffer, sample_idx);
      break;
  }
}

unsigned int sample_dbg_adc(unsigned int channel_no) {
  unsigned int result;
  adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (channel_no << 10));
  result = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (channel_no << 10));
  return result;
}

void sample_dbg_dac(unsigned int value) { dac_write(SPI_CS_DAC, value); }

void sampling_init(enum ShepherdMode mode, unsigned int harvesting_voltage) {
  /* Chip-Select signals are active low */
  _GPIO_ON(SPI_CS_ADC);
  _GPIO_ON(SPI_CS_DAC);

  _GPIO_OFF(SPI_SCLK);
  _GPIO_OFF(SPI_MOSI);

  /* Reset all registers (see DAC8562T datasheet Table 17) */
  dac_write(SPI_CS_DAC, (0x5 << DAC_CMD_OFFSET) | (1 << 0));
  __delay_cycles(12);

  /* Enable internal 2.5V reference (see DAC8562T datasheet Table 17) */
  dac_write(SPI_CS_DAC, (0x7 << DAC_CMD_OFFSET) | (1 << 0));
  __delay_cycles(12);

  /* GAIN=2 for DAC-B and GAIN=1 for DAC-A (see DAC8562T datasheet Table 17) */
  dac_write(SPI_CS_DAC, (0x2 << DAC_ADDR_OFFSET) | (1 << 0));
  __delay_cycles(12);

  /* LDAC pin inactive for DAC-B and DAC-A (see DAC8562T datasheet Table 17) */
  dac_write(SPI_CS_DAC, (0x6 << DAC_CMD_OFFSET) | (1 << 1) | (1 << 0));
  __delay_cycles(12);

  /* Range 1.25*VREF: B9-15 select CH, B8 enables write, B0-4 select range*/
  adc_readwrite(SPI_CS_ADC,
                ((ADC_CH_V_IN + 5) << 9) | (1 << 8) | (1 << 2) | (1 << 1));
  adc_readwrite(SPI_CS_ADC,
                ((ADC_CH_V_OUT + 5) << 9) | (1 << 8) | (1 << 2) | (1 << 1));

  /* Range +/-0.625*VREF */
  adc_readwrite(SPI_CS_ADC, ((ADC_CH_A_IN + 5) << 9) | (1 << 8) | (1 << 1));
  adc_readwrite(SPI_CS_ADC, ((ADC_CH_A_OUT + 5) << 9) | (1 << 8) | (1 << 1));

  /* Initialize ADC for correct channel */
  if (mode == MODE_HARVESTING) {
    /* Select harvesting current for first reading */
    adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_IN << 10));
    dac_write(SPI_CS_DAC, harvesting_voltage | DAC_CH_V_ADDR);
  } else
    /* Select load current for first reading */
    adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
}
