#ifndef _NRF24L01_UTIL_H_
#define _NRF24L01_UTIL_H_

u64 nrf24l01_addr_host_to_addr_nrf(struct nrf24l01_t* nrf, u64 addr, unsigned int addr_width);
u64 nrf24l01_addr_nrf_to_addr_host(struct nrf24l01_t* nrf, u64 addr, unsigned int addr_width);

#endif
