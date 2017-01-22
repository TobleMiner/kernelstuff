#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/kernel.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_functions.h"
#include "partregmap.h"

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch)
{
	printk(KERN_INFO "Calling table write\n");
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, &ch, 1);
}

int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, ch, 1);
}

int nrf24l01_set_dr(struct nrf24l01_t* nrf, int dr)
{
	switch(dr)
	{
		case 250:
			dr = 0b100;
			break;
		case 1000:
			dr = 0b000;
			break;
		case 2000:
			dr = 0b001;
			break;
		default:
			return -EINVAL;
	}
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_DR, &dr, 1);
}

int nrf24l01_get_dr(struct nrf24l01_t* nrf, unsigned int* dr)
{
	unsigned int dr_reg;
	int err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_DR, &dr_reg, 1);
	if(err)
		return err;
	if(dr_reg & 0b100)
		*dr = 250;
	else
		*dr = 1000 + dr_reg * 1000;
	return 0;
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
	int err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_PWR, &pwr, 1);
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

int nrf24l01_set_address_u64(struct nrf24l01_t* nrf, u64 addr, unsigned int pipe)
{
	int err;
	if(pipe <= 1)
		err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) &addr, 5);
	else if(pipe <= 5)
		err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) &addr, 1);
	else
		return -EINVAL;
	return err;
}

int nrf24l01_get_address_u64(struct nrf24l01_t* nrf, u64* addr, unsigned int pipe)
{
	int err;
	if(pipe <= 1)
		err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) addr, 5);
	else if(pipe <= 5)
		err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) addr, 1);
	else
		return -EINVAL;
	return err;
}


int nrf24l01_flush_rx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_rx_path);
	err = nrf24l01_spi_flush_rx(nrf);
	mutex_unlock(&nrf->m_rx_path);
	return err;
}

int nrf24l01_flush_tx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_tx_path);
	err = nrf24l01_spi_flush_tx(nrf);
	mutex_unlock(&nrf->m_tx_path);
	return err;
}

int nrf24l01_flush(struct nrf24l01_t* nrf)
{
	int err = nrf24l01_flush_rx(nrf);
	if(err < 0)
		return err;
	return nrf24l01_flush_tx(nrf);
}

int nrf24l01_set_pwr_up(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, &state, 1);
}

int nrf24l01_pwr_up(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 1);
}

int nrf24l01_pwr_down(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 0);
}

int nrf24l01_get_pwr_up(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, state, 1);
}

int nrf24l01_set_prim_rx(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, &state, 1);
}

int nrf24l01_get_prim_rx(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, state, 1);
}

int nrf24l01_set_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int width)
{
	if(pipe > 5)
		return -EINVAL;
	if(width > 32)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, &width, 1);
}

int nrf24l01_get_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* width)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, width, 1);	
}

int nrf24l01_set_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*)addr, len);
}

int nrf24l01_get_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*)addr, len);
}

int nrf24l01_set_en_rxaddr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int state)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_EN_RXADDR_ERX_P0 + pipe, &state, 1);
}

int nrf24l01_get_en_rxaddr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* state)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_EN_RXADDR_ERX_P0 + pipe, state, 1);
}

int nrf24l01_set_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int state)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_DYNPD_DPL_P0 + pipe, &state, 1);
}

int nrf24l01_get_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* state)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_DYNPD_DPL_P0 + pipe, state, 1);
}

int nrf24l01_set_enaa(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int autoack)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_EN_AA_ENAA_P0 + pipe, &autoack, 1);
}

int nrf24l01_get_enaa(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* autoack)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_EN_AA_ENAA_P0 + pipe, autoack, 1);
}

int nrf24l01_set_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*)addr, len);
}

int nrf24l01_get_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*)addr, len);
}

void nrf24l01_set_ce(struct nrf24l01_t* nrf, unsigned int state)
{
	gpio_set_value(nrf->gpio_ce, state);
}

int nrf24l01_get_status_rx_dr(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_STATUS_RX_DR, status, 1);
}

int nrf24l01_set_status_rx_dr(struct nrf24l01_t* nrf, unsigned int status)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_STATUS_RX_DR, &status, 1);
}

int nrf24l01_get_status_tx_ds(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_STATUS_TX_DS, status, 1);
}

int nrf24l01_set_status_tx_ds(struct nrf24l01_t* nrf, unsigned int status)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_STATUS_TX_DS, &status, 1);
}

int nrf24l01_get_status_max_rt(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_STATUS_MAX_RT, status, 1);
}

int nrf24l01_set_status_max_rt(struct nrf24l01_t* nrf, unsigned int status)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_STATUS_MAX_RT, &status, 1);
}

int nrf24l01_get_status_rx_p_no(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_STATUS_RX_P_NO, status, 1);
}

int nrf24l01_get_status_tx_full(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_STATUS_TX_FULL, status, 1);
}

/*int nrf24l01_get_pld_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	Get width via R_RX_PL_WID cmd	
}*/

int nrf24l01_get_vreg_or_fail_short(struct nrf24l01_t* nrf, int vreg, int ntries)
{
	int err, tries = 0;
	unsigned int data;
	while(++tries <= ntries)
	{
		err = partreg_table_read(nrf->reg_table, vreg, &data, 1);
		if(err)
			dev_warn(&nrf->spi->dev, "Critical register read failed. Try %d of %d\n", tries, ntries);
		else
			return data;
	}
	dev_err(&nrf->spi->dev, "Critical register read failed!\n");
	return err;
}

int nrf24l01_get_rx_p_no_or_fail(struct nrf24l01_t* nrf)
{
	return nrf24l01_get_vreg_or_fail_short(nrf, NRF24L01_VREG_STATUS_RX_P_NO, NRF24L01_VREG_TRIES);
}

int nrf24l01_get_tx_full_or_fail(struct nrf24l01_t* nrf)
{
	return nrf24l01_get_vreg_or_fail_short(nrf, NRF24L01_VREG_STATUS_TX_FULL, NRF24L01_VREG_TRIES);
}

int nrf24l01_set_rxtx(struct nrf24l01_t* nrf, int state)
{
	int err;
	NRF24L01_CE_LO(nrf);
	if((err = nrf24l01_pwr_down(nrf)))
		return err;
	if((err = nrf24l01_set_prim_rx(nrf, state)))
		return err;
	if((err = nrf24l01_pwr_up(nrf)))
		return err;
	NRF24L01_CE_HI(nrf);
	return 0;
}

int nrf24l01_set_rx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_rxtx(nrf, 1);
}

int nrf24l01_set_tx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_rxtx(nrf, 0);
}

// TODO: Implement non blocking version
ssize_t nrf24l01_read_packet(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	size_t err;
	unsigned int pipe_no, payload_width;
	// TODO: claim rxtx state mutex here and hold it until function exit
	nrf24l01_set_rx(nrf);
tryagain:
	if((err = wait_event_interruptible(nrf->rx_queue, nrf24l01_get_rx_p_no_or_fail(nrf) != NRF24L01_RX_P_NO_EMPTY)))
	{
		goto exit_err;
	}
	mutex_lock(&nrf->m_rx_path);
	if((err = nrf24l01_get_status_rx_p_no(nrf, &pipe_no)))
	{
		goto exit_err_mutex;
	}
	if(pipe_no == NRF24L01_RX_P_NO_EMPTY)
	{
		mutex_unlock(&nrf->m_rx_path);
		goto tryagain;
	}
	if((err = nrf24l01_get_pld_width(nrf, pipe_no, &payload_width)))
	{
		goto exit_err_mutex;
	}
	if(len < payload_width)
	{
		dev_err(&nrf->spi->dev, "Packet read length too short. Should be >= %u, is %u\n", payload_width, len);
		err = -EINVAL;
		goto exit_err_mutex;
	}
	if((err = nrf24l01_spi_read_rx_pld(nrf, data, payload_width)))
	{
		goto exit_err_mutex;
	}
	err = payload_width;
exit_err_mutex:
	mutex_unlock(&nrf->m_rx_path);
exit_err:
	return err;
}

// TODO: Basically same as read_packet
int nrf24l01_send_packet(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	int err;
	unsigned int tx_full;
	if(len > 32)
	{
		err = -EINVAL;
		goto exit_err;
	}	
	nrf24l01_set_tx(nrf);
tryagain:
	if((err = wait_event_interruptible(nrf->tx_queue, nrf24l01_get_tx_full_or_fail(nrf) != 1)))
	{
		goto exit_err;
	}
	mutex_lock(&nrf->m_tx_path);
	if((err = nrf24l01_get_status_tx_full(nrf, &tx_full)))
	{
		goto exit_err_mutex;
	}
	if(tx_full == 1)
	{
		mutex_unlock(&nrf->m_tx_path);
		goto tryagain;
	}
	if((err = nrf24l01_spi_write_tx_pld(nrf, data, len)))
	{
		goto exit_err_mutex;
	}
	err = 0;
exit_err_mutex:
	mutex_unlock(&nrf->m_tx_path);
exit_err:
	return err;
}
