#include <stdint.h>

#include "gpio.h"
#include "hw_config.h"
#include "sampling.h"
#include "virtcap.h"

#define CMD_ID_ADC      0U
#define CMD_ID_DAC      1U

#define ADC_CH_V_IN     0U
#define ADC_CH_V_OUT    1U
#define ADC_CH_A_IN     2U
#define ADC_CH_A_OUT    3U

#define MAN_CH_SLCT     ((1U << 15U) | (1U << 14U))

#define DAC_CH_V_ADDR   (1U << 16U)
#define DAC_CH_A_ADDR   (0U << 16U)

#define DAC_CMD_OFFSET  19U
#define DAC_ADDR_OFFSET 16U

extern uint32_t adc_readwrite(const uint32_t cs_pin, const uint32_t val);
extern void dac_write(const uint32_t cs_pin, const uint32_t val);

static inline void sample_harvesting(struct SampleBuffer *const buffer, const uint32_t sample_idx)
{
	/* Read current  and select channel 0 (voltage) for the read after! */
	buffer->values_current[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_IN << 10U));
	/* Read voltage and select channel 2 (current) for the read after! */
	buffer->values_voltage[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_IN << 10U));
}

static inline void sample_load(struct SampleBuffer *const buffer, const uint32_t sample_idx)
{
	/* Read load current and select load voltage for next reading */
	buffer->values_current[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
	/* Read load voltage and select load current for next reading */
	buffer->values_voltage[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
}

static inline void sample_emulation(struct SampleBuffer *const buffer, const uint32_t sample_idx)
{
	/* write the emulation voltage value from buffer to DAC */
	dac_write(SPI_CS_DAC, buffer->values_current[sample_idx] | DAC_CH_A_ADDR);
	/* write the emulation current value from buffer to DAC */
	dac_write(SPI_CS_DAC, buffer->values_voltage[sample_idx] | DAC_CH_V_ADDR);

	/* read the load current value from ADC to buffer and select load voltage for
   * next reading */
	buffer->values_current[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
	/* read the load voltage value from ADC to buffer and select load current for
   * next reading */
	buffer->values_voltage[sample_idx] = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
}

static inline void sample_virtcap(struct SampleBuffer *const buffer, const uint32_t sample_idx)
{
	struct ADCReading read;
	static int32_t under_sample_voltage_cntr = 0;
	static int32_t last_voltage_measurement = 0;
	static int32_t last_current_measurement = 0;

	int32_t input_current;
	int32_t input_voltage;

	/* Get input current/voltage from shared memory buffer */
	input_current = buffer->values_current[sample_idx] - ((1 << 17) - 1);
	input_voltage = buffer->values_voltage[sample_idx];

	if (under_sample_voltage_cntr++ == 7) {
		under_sample_voltage_cntr = 0;

		read.current = last_current_measurement;
		buffer->values_current[sample_idx] = last_current_measurement;

		/* Read load voltage and select load current for next reading */
		read.voltage = adc_readwrite(
			SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
		buffer->values_voltage[sample_idx] = read.voltage;
		last_voltage_measurement = buffer->values_voltage[sample_idx];

	} else if (under_sample_voltage_cntr == 6) {
		/* Read load current and select load voltage for next reading */
		read.current = adc_readwrite(
			SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_V_OUT << 10));
		buffer->values_current[sample_idx] = read.current;
		last_current_measurement = buffer->values_current[sample_idx];

		read.voltage = last_voltage_measurement;
		buffer->values_voltage[sample_idx] = last_voltage_measurement;

	} else {
		/* Read load current and select load current for next reading */
		read.current = adc_readwrite(
			SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10));
		buffer->values_current[sample_idx] = read.current;

		read.voltage = last_voltage_measurement;
		buffer->values_voltage[sample_idx] = last_voltage_measurement;
	}

	/* Execute virtcap algorithm */
	virtcap_update(buffer->values_current[sample_idx] - ((1 << 17) - 1),
		       buffer->values_voltage[sample_idx], input_current,
		       input_voltage);

	/*
	 * If output is off, force buffer voltage to zero.
	 * Else it will go to max 2.6V, because voltage sense is put before
	 * output switch.
	 */
	if (!virtcap_get_output_state())
		buffer->values_voltage[sample_idx] = 0;
}

void sample(struct SampleBuffer *const current_buffer, const uint32_t sample_idx,
	    const enum ShepherdMode mode)
{
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

uint32_t sample_dbg_adc(const uint32_t channel_no)
{
	adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (channel_no << 10U));
    const uint32_t result = adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (channel_no << 10U));
	return result;
}

void sample_dbg_dac(const uint32_t value)
{
	dac_write(SPI_CS_DAC, value);
}

void sampling_init(const enum ShepherdMode mode, const uint32_t harvesting_voltage)
{
	/* Chip-Select signals are active low */
	_GPIO_ON(SPI_CS_ADC);
	_GPIO_ON(SPI_CS_DAC);

	_GPIO_OFF(SPI_SCLK);
	_GPIO_OFF(SPI_MOSI);

	/* Reset all registers (see DAC8562T datasheet Table 17) */
	dac_write(SPI_CS_DAC, (0x5 << DAC_CMD_OFFSET) | (1U << 0U));
	__delay_cycles(12);

	/* Enable internal 2.5V reference (see DAC8562T datasheet Table 17) */
	dac_write(SPI_CS_DAC, (0x7 << DAC_CMD_OFFSET) | (1U << 0U));
	__delay_cycles(12);

	/* GAIN=2 for DAC-B and GAIN=1 for DAC-A (see DAC8562T datasheet Table 17) */
	dac_write(SPI_CS_DAC, (0x2 << DAC_ADDR_OFFSET) | (1U << 0U));
	__delay_cycles(12);

	/* LDAC pin inactive for DAC-B and DAC-A (see DAC8562T datasheet Table 17) */
	dac_write(SPI_CS_DAC, (0x6 << DAC_CMD_OFFSET) | (1U << 1U) | (1U << 0U));
	__delay_cycles(12);

	/* Range 1.25*VREF: B9-15 select CH, B8 enables write, B0-4 select range*/
	adc_readwrite(SPI_CS_ADC, ((ADC_CH_V_IN + 5U) << 9U) | (1U << 8U) | (1U << 2U) | (1U << 1U));
	adc_readwrite(SPI_CS_ADC, ((ADC_CH_V_OUT + 5U) << 9U) | (1U << 8U) | (1U << 2U) | (1U << 1U));

	/* Range +/-0.625*VREF */
	adc_readwrite(SPI_CS_ADC, ((ADC_CH_A_IN + 5U) << 9U) | (1U << 8U) | (1U << 1U));
	adc_readwrite(SPI_CS_ADC, ((ADC_CH_A_OUT + 5U) << 9U) | (1U << 8U) | (1U << 1U));

	/* Initialize ADC for correct channel */
	if (mode == MODE_HARVESTING) {
		/* Select harvesting current for first reading */
		adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_IN << 10U));
		dac_write(SPI_CS_DAC, harvesting_voltage | DAC_CH_V_ADDR);
	} else
		/* Select load current for first reading */
		adc_readwrite(SPI_CS_ADC, MAN_CH_SLCT | (ADC_CH_A_OUT << 10U));
}
