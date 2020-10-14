#ifndef PRU1_SPI_TRANSFER_SYS_H
#define PRU1_SPI_TRANSFER_SYS_H
#include <sys_mcspi.h>


#define SYS_SPI     CT_MCSPI0

void sys_spi_init();
uint32_t sys_adc_readwrite(uint32_t cs_pin, uint32_t val);
void sys_dac_write(uint32_t cs_pin, uint32_t val);

#endif //PRU1_SPI_TRANSFER_SYS_H
