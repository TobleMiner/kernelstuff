#include <linux/regmap.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch)
{
	return regmap_write(nrf->regmap_short, NRF24L01_REG_RF_CH, ch);
}

int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch)
{
	return regmap_read(nrf->regmap_short, NRF24L01_REG_RF_CH, ch);
}

/*int nrf24l01_set_tx_power(struct nrf24l01_t* nrf, unsigned int ch)
{
	return regmap_write_bits(nrf->regmap_short, NRF24L01_REG_RF_SETUP, NRF24L01_MASK_TXPWR);
}*/

