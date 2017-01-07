#include <linux/regmap.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "partregmap.h"

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, &ch, 1);
}

int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, ch, 1);
}

int nrf24l01_set_tx_power(struct nrf24l01_t* nrf, int tx_pwr)
{
	unsigned int pwr;
	switch(tx_pwr)
	{
		case 0:
			pwr = 0b11;
			break;
		case -6:
			pwr = 0b10;
			break;
		case -12:
			pwr = 0b01;
			break;
		case -18:
			pwr = 0b00;
			break;
		default:
			return -EINVAL;
	}
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_PWR, &pwr, 1);
}

int nrf24l01_get_tx_power(struct nrf24l01_t* nrf, int* tx_pwr)
{
	unsigned int pwr;
	int err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_PWR, &pwr, 1);
	if(err < 0)
		return err;
	if(pwr > 0b11)
		return -EINVAL;
	return -18 + pwr * 6;
}

int nrf24l01_set_address_width(struct nrf24l01_t* nrf, unsigned int width)
{
	if(width < 3 || width > 5)
		return -EINVAL;
	width -= 2;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_SETUP_AW_AW, &width, 1);
}

int nrf24l01_get_address_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	int err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_SETUP_AW_AW, width, 1);
	if(err < 0)
		return err;
	if(*width < 1 || *width > 3)
		return -EINVAL; 
	*width += 2;
	return 0;
}
