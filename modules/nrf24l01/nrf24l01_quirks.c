#include "nrf24l01_quirks.h"
#include "nrf24l01_core.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_functions.h"

int nrf24l01_test_unflushable_fifo(struct nrf24l01_t* nrf)
{
	int err;
	unsigned int val;
	unsigned char data[32];
	for(val = 0; val < 3; val++)
	{
		if((err = nrf24l01_spi_write_tx_pld(nrf, data, 32)))
			return err;
	}
	if((err = nrf24l01_flush_tx(nrf)))
		return err;
	if((err = nrf24l01_get_status_tx_full(nrf, &val)))
		return err;
	return val;
}
