#ifndef _NRF24L01_SPI_H_
#define _NRF24L01_SPI_H_

int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val);
int nrf24l01_spi_write_reg(nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len);
int nrf24l01_spi_read_reg(nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len);

#endif
