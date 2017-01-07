#include <linux/regmap.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "partregmap.h"

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

int nrf24l01_get_address_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_SETUP_AW_AW, width, 1);
}
