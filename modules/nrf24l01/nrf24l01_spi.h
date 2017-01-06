#ifndef _NRF24L01_SPI_H_
#define _NRF24L01_SPI_H_

int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val);

#endif
