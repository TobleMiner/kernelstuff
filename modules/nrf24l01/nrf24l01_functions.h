#ifndef _NRF24L01_FUNCTIONS_H_
#define _NRF24L01_FUNCTIONS_H_

#include "nrf24l01_core.h"

#define NRF24L01_VREG_TRIES 10

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch);
int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch);
int nrf24l01_set_tx_power(struct nrf24l01_t* nrf, int tx_pwr);
int nrf24l01_get_tx_power(struct nrf24l01_t* nrf, int* tx_pwr);
int nrf24l01_set_address_width(struct nrf24l01_t* nrf, unsigned int width);
int nrf24l01_get_address_width(struct nrf24l01_t* nrf, unsigned int* width);
int nrf24l01_flush_rx(struct nrf24l01_t* nrf);
int nrf24l01_flush_tx(struct nrf24l01_t* nrf);
int nrf24l01_flush(struct nrf24l01_t* nrf);
int nrf24l01_set_pwr_up(struct nrf24l01_t* nrf, unsigned int state);
int nrf24l01_pwr_up(struct nrf24l01_t* nrf);
int nrf24l01_pwr_down(struct nrf24l01_t* nrf);
int nrf24l01_get_pwr_up(struct nrf24l01_t* nrf, unsigned int* state);
int nrf24l01_set_prim_rx(struct nrf24l01_t* nrf, unsigned int state);
int nrf24l01_get_prim_rx(struct nrf24l01_t* nrf, unsigned int* state);
int nrf24l01_set_pipe_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int width);
int nrf24l01_get_pipe_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* width);
int nrf24l01_get_dyn_pld_width(struct nrf24l01_t* nrf, unsigned int* width);
int nrf24l01_set_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len);
int nrf24l01_get_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len);
int nrf24l01_set_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len);
int nrf24l01_get_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len);
void nrf24l01_set_ce(struct nrf24l01_t* nrf, unsigned int state);
int nrf24l01_get_status_rx_dr(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_set_status_rx_dr(struct nrf24l01_t* nrf, unsigned int status);
int nrf24l01_get_status_tx_ds(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_set_status_tx_ds(struct nrf24l01_t* nrf, unsigned int status);
int nrf24l01_get_status_max_rt(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_set_status_max_rt(struct nrf24l01_t* nrf, unsigned int status);
int nrf24l01_get_status_rx_p_no(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_get_status_tx_full(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_get_vreg_or_fail_short(struct nrf24l01_t* nrf, int vreg, int ntries);
int nrf24l01_get_rx_p_no_or_fail(struct nrf24l01_t* nrf);
int nrf24l01_get_tx_full_or_fail(struct nrf24l01_t* nrf);
int nrf24l01_set_rxtx(struct nrf24l01_t* nrf, int state);
int nrf24l01_set_rx(struct nrf24l01_t* nrf);
int nrf24l01_set_tx(struct nrf24l01_t* nrf);
int nrf24l01_send_packet(struct nrf24l01_t* nrf, bool noblock, unsigned char* data, unsigned int len);
ssize_t nrf24l01_read_packet(struct nrf24l01_t* nrf, bool noblock, unsigned char* data, unsigned int len);
int nrf24l01_set_address_u64(struct nrf24l01_t* nrf, u64 addr, unsigned int pipe);
int nrf24l01_get_address_u64(struct nrf24l01_t* nrf, u64* addr, unsigned int pipe);
int nrf24l01_set_en_rxaddr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int state);
int nrf24l01_get_en_rxaddr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* state);
int nrf24l01_set_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int state);
int nrf24l01_get_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* state);
int nrf24l01_set_enaa(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int autoack);
int nrf24l01_get_enaa(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* autoack);
int nrf24l01_set_dr(struct nrf24l01_t* nrf, int dr);
int nrf24l01_get_dr(struct nrf24l01_t* nrf, unsigned int* dr);
int nrf24l01_set_crc(struct nrf24l01_t* nrf, unsigned int crc);
int nrf24l01_get_crc(struct nrf24l01_t* nrf, unsigned int* crc);
int nrf24l01_set_tx_address_u64(struct nrf24l01_t* nrf, u64 addr);
int nrf24l01_get_tx_address_u64(struct nrf24l01_t* nrf, u64* addr);
int nrf24l01_get_fifo_tx_full(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_get_fifo_tx_empty(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_get_mode(struct nrf24l01_t* nrf, unsigned int mode);
int nrf24l01_get_mode_low_pwr(struct nrf24l01_t* nrf);
int nrf24l01_set_pwr_up_(struct nrf24l01_t* nrf, unsigned int state);
int nrf24l01_pwr_up_(struct nrf24l01_t* nrf);
int nrf24l01_pwr_down_(struct nrf24l01_t* nrf);
int nrf24l01_get_pwr_up_(struct nrf24l01_t* nrf, unsigned int* state);
int nrf24l01_get_prim_rx_(struct nrf24l01_t* nrf, unsigned int* state);
int nrf24l01_get_rxtx_(struct nrf24l01_t* nrf, int* state);
void nrf24l01_set_ce_(struct nrf24l01_t* nrf, unsigned int state);
int nrf24l01_shutdown(struct nrf24l01_t* nrf);
int nrf24l01_get_ce_(struct nrf24l01_t* nrf);
int nrf24l01_get_ce(struct nrf24l01_t* nrf);
int nrf24l01_set_retr_ard(struct nrf24l01_t* nrf, unsigned int ard);
int nrf24l01_get_retr_ard(struct nrf24l01_t* nrf, unsigned int* ard);
int nrf24l01_set_retr_arc(struct nrf24l01_t* nrf, unsigned int arc);
int nrf24l01_get_retr_arc(struct nrf24l01_t* nrf, unsigned int* arc);
int nrf24l01_get_fifo_rx_full(struct nrf24l01_t* nrf, unsigned int* status);
int nrf24l01_get_fifo_rx_empty(struct nrf24l01_t* nrf, unsigned int* status);
void nrf24l01_store_ce_(struct nrf24l01_t* nrf, unsigned int *state);
void nrf24l01_load_ce_(struct nrf24l01_t* nrf, unsigned int state);


#define NRF24L01_CE_HI_(nrf) nrf24l01_set_ce_(nrf, 1)
#define NRF24L01_CE_LO_(nrf) nrf24l01_set_ce_(nrf, 0)
	
#define NRF24L01_CE_HI(nrf) nrf24l01_set_ce(nrf, 1)
#define NRF24L01_CE_LO(nrf) nrf24l01_set_ce(nrf, 0)

#endif
