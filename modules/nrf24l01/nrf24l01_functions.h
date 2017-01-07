#ifndef _NRF24L01_FUNCTIONS_H_
#define _NRF24L01_FUNCTIONS_H_

#include "nrf24l01_core.h"

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch);
int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch);
int nrf24l01_set_tx_power(struct nrf24l01_t* nrf, unsigned int ch);
int nrf24l01_get_tx_power(struct nrf24l01_t* nrf, unsigned int* ch);
int nrf24l01_set_address_width(struct nrf24l01_t* nrf, unsigned int width);
int nrf24l01_get_address_width(struct nrf24l01_t* nrf, unsigned int* width);

#endif
