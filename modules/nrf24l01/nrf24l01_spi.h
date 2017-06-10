#ifndef _NRF24L01_SPI_H_
#define _NRF24L01_SPI_H_

#include "nrf24l01_core.h"

int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val);
int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val);
int nrf24l01_spi_write_reg(struct nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len);
int nrf24l01_spi_read_reg(struct nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len);
int nrf24l01_spi_read_rx_pld(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len);
int nrf24l01_spi_write_tx_pld(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len);
int nrf24l01_spi_flush_tx(struct nrf24l01_t* nrf);
int nrf24l01_spi_flush_rx(struct nrf24l01_t* nrf);
int nrf24l01_spi_reuse_tx_pl(struct nrf24l01_t* nrf);
int nrf24l01_spi_read_rx_pl_width(struct nrf24l01_t* nrf, unsigned int* width);
int nrf24l01_spi_write_ack_pld(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* data, unsigned int len);
int nrf24l01_spi_write_tx_pld_no_ack(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len);
int nrf24l01_spi_nop(struct nrf24l01_t* nrf);
int nrf24l01_spi_read_status(struct nrf24l01_t* nrf, unsigned char* status);

#endif
