#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/kernel.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_functions.h"
#include "nrf24l01_util.h"
#include "partregmap.h"

int nrf24l01_get_mode(struct nrf24l01_t* nrf, unsigned int mode)
{
	return nrf->mode_flags & mode;
}

int nrf24l01_get_mode_low_pwr(struct nrf24l01_t* nrf)
{
	return nrf24l01_get_mode(nrf, NRF24L01_MODE_LOW_PWR);
}

int nrf24l01_get_mode_non_canon_standby(struct nrf24l01_t* nrf)
{
	return nrf24l01_get_mode(nrf, NRF24L01_MODE_NON_CANON_STANDBY);
}

int nrf24l01_get_mode_pwr_down_not_standby(struct nrf24l01_t* nrf)
{
	return nrf24l01_get_mode(nrf, NRF24L01_MODE_PWR_DOWN_NOT_STANDBY);
}

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch)
{
	int err, ce_state;
	mutex_lock(&nrf->m_state);
	ce_state = nrf24l01_get_ce_(nrf);
	NRF24L01_CE_LO_(nrf);
	err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, &ch, 1);
	nrf24l01_set_ce_(nrf, ce_state);
	mutex_unlock(&nrf->m_state);
	return err;
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

int nrf24l01_set_retr_ard(struct nrf24l01_t* nrf, unsigned int ard)
{
	if(ard < 250 || ard > 4000)
		return -EINVAL;
	if(ard % 250 != 0)
		return -EINVAL;
	ard /= 250;
	ard--;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_SETUP_RETR_ARD, &ard, 1);
}

int nrf24l01_get_retr_ard(struct nrf24l01_t* nrf, unsigned int* ard)
{
	int err;
	if((err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_SETUP_RETR_ARD, ard, 1)))
	{
		return err;
	}
	*ard += 1;
	*ard *= 250;
	return err;
}

int nrf24l01_set_retr_arc(struct nrf24l01_t* nrf, unsigned int arc)
{
	if(arc > 15)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_SETUP_RETR_ARC, &arc, 1);
}

int nrf24l01_get_retr_arc(struct nrf24l01_t* nrf, unsigned int* arc)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_SETUP_RETR_ARC, arc, 1);
}

int nrf24l01_set_crc(struct nrf24l01_t* nrf, unsigned int crc)
{
	int err;
	unsigned int one = 1;
	if(crc == 0)
	{
		if((err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_EN_CRC, &crc, 1)))
			goto exit_err;
	}
	else if(crc < 3)
	{
		crc--;
		if((err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_EN_CRC, &one, 1)))
			goto exit_err;
		err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_CRCO, &crc, 1);
	}
	else
		err = -EINVAL;
exit_err:
	return err;
}

int nrf24l01_get_crc(struct nrf24l01_t* nrf, unsigned int* crc)
{
	int err;
	if((err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_EN_CRC, crc, 1)))
		goto exit_err;
	if(*crc == 0)
		goto exit_err;
	if((err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_CRCO, crc, 1)))
		goto exit_err;
	*crc += 1;
exit_err:
	return err;
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
	*tx_pwr = -18 + pwr * 6;
	return 0;
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

int nrf24l01_get_address_width_pipe(struct nrf24l01_t* nrf, unsigned int* width, unsigned int pipe)
{
	if(pipe > 1)
	{
		*width = 1;
		return 0;
	}
	return nrf24l01_get_address_width(nrf, width);
}

int nrf24l01_set_address_u64(struct nrf24l01_t* nrf, u64 addr, unsigned int pipe)
{
	int err;
	unsigned int addr_width = 1;
	if(pipe > 5)
		return -EINVAL;
	if((err = nrf24l01_get_address_width_pipe(nrf, &addr_width, pipe)))
	{
		goto exit_err;
	}
	addr = nrf24l01_addr_host_to_addr_nrf(nrf, addr, addr_width);
	err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) &addr, addr_width);
exit_err:
	return err;
}

int nrf24l01_get_address_u64(struct nrf24l01_t* nrf, u64* addr, unsigned int pipe)
{
	int err;
	unsigned int addr_width;
	if(pipe > 5 )
		return -EINVAL;
	if((err = nrf24l01_get_address_width_pipe(nrf, &addr_width, pipe)))
	{
		goto exit_err;
	}
	if((err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*) addr, addr_width)))
	{
		goto exit_err;
	}
	*addr = nrf24l01_addr_nrf_to_addr_host(nrf, *addr, addr_width);
exit_err:
	return err;
}

int nrf24l01_set_tx_address_u64(struct nrf24l01_t* nrf, u64 addr)
{
	int err;
	unsigned int addr_width;
	if((err = nrf24l01_get_address_width(nrf, &addr_width)))
	{
		return err;
	}
	addr = nrf24l01_addr_host_to_addr_nrf(nrf, addr, addr_width);
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*) &addr, 5);
}

int nrf24l01_get_tx_address_u64(struct nrf24l01_t* nrf, u64* addr)
{
	unsigned int addr_width;
	int err;
	if((err = nrf24l01_get_address_width(nrf, &addr_width)))
	{
		goto exit_err;
	}
	if((err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*) addr, 5)))
	{
		goto exit_err;
	}
	*addr = nrf24l01_addr_nrf_to_addr_host(nrf, *addr, addr_width);
exit_err:
	return err;
}

static int nrf24l01_flush_rx_(struct nrf24l01_t* nrf)
{
	return nrf24l01_spi_flush_rx(nrf);
}

int nrf24l01_flush_rx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_rx_path);
	err = nrf24l01_flush_rx_(nrf);
	mutex_unlock(&nrf->m_rx_path);
	return err;
}

static int nrf24l01_flush_tx_(struct nrf24l01_t* nrf)
{
	return nrf24l01_spi_flush_tx(nrf);
}

int nrf24l01_flush_tx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_tx_path);
	err = nrf24l01_flush_tx_(nrf);
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

int nrf24l01_set_pwr_up_(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, &state, 1);
}

int nrf24l01_set_pwr_up(struct nrf24l01_t* nrf, unsigned int state)
{
	int err;
	mutex_lock(&nrf->m_state);
	err = nrf24l01_set_pwr_up_(nrf, state);
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_pwr_up_(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up_(nrf, 1);
}

int nrf24l01_pwr_up(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 1);
}

int nrf24l01_pwr_down_(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up_(nrf, 0);
}

int nrf24l01_pwr_down(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 0);
}

int nrf24l01_shutdown(struct nrf24l01_t* nrf)
{
	int err;
	unsigned int is_rx;
	mutex_lock(&nrf->m_state);
	if((err = nrf24l01_get_prim_rx_(nrf, &is_rx)))
	{
		goto exit_err_mutex;
	}
	if((is_rx || (!is_rx && nrf24l01_get_mode_non_canon_standby(nrf))) && !nrf24l01_get_mode_pwr_down_not_standby(nrf))
	{
		NRF24L01_CE_LO_(nrf);
	}
	else
	{
		if((err = nrf24l01_pwr_down_(nrf)))
		{
			goto exit_err_mutex;
		}
	}
exit_err_mutex:
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_get_pwr_up_(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, state, 1);
}

int nrf24l01_get_pwr_up(struct nrf24l01_t* nrf, unsigned int* state)
{
	int err;
	mutex_lock(&nrf->m_state);
	err = nrf24l01_get_pwr_up_(nrf, state);
	mutex_unlock(&nrf->m_state);
	return err;	
}

int nrf24l01_set_prim_rx_(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, &state, 1);
}

int nrf24l01_set_prim_rx(struct nrf24l01_t* nrf, unsigned int state)
{
	int err;
	mutex_lock(&nrf->m_state);
	err = nrf24l01_set_prim_rx_(nrf, state);
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_get_prim_rx_(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, state, 1);
}

int nrf24l01_get_prim_rx(struct nrf24l01_t* nrf, unsigned int* state)
{
	int err;
	mutex_lock(&nrf->m_state);
	err = nrf24l01_get_prim_rx_(nrf, state);
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_set_pipe_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int width)
{
	if(pipe > 5)
		return -EINVAL;
	if(width > NRF24L01_PACKET_MAX_LENGTH)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, &width, 1);
}

int nrf24l01_get_pipe_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* width)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, width, 1);	
}

int nrf24l01_get_dyn_pld_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	return nrf24l01_spi_read_rx_pl_width(nrf, width);
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

static int nrf24l01_set_feature_dynpd(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_FEATURE_EN_DPL, &state, 1);
}

static int nrf24l01_get_feature_dynpd(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_FEATURE_EN_DPL, state, 1);
}

int nrf24l01_set_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int state)
{
	int err;
	if(pipe > 5)
		return -EINVAL;
	if(state != 0 && (err = nrf24l01_set_feature_dynpd(nrf, 1)))
		goto exit_err;
	if(state != 0 && (err = nrf24l01_set_enaa(nrf, pipe, 1)))
		goto exit_err;
	err = partreg_table_write(nrf->reg_table, NRF24L01_VREG_DYNPD_DPL_P0 + pipe, &state, 1);
exit_err:
	return err;
}

int nrf24l01_get_dynpd(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* state)
{
	int err;
	unsigned int en_dpl, en_aa;
	if(pipe > 5)
		return -EINVAL;
	if((err = nrf24l01_get_feature_dynpd(nrf, &en_dpl)))
		goto exit_err;
	if((err = nrf24l01_get_enaa(nrf, pipe, &en_aa)))
		goto exit_err;
	if(!en_dpl || !en_aa)
	{
		*state = 0;
		goto exit_err;		
	}
	err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_DYNPD_DPL_P0 + pipe, state, 1);
exit_err:
	return err;
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

void nrf24l01_set_ce_(struct nrf24l01_t* nrf, unsigned int state)
{
	gpio_set_value(nrf->gpio_ce, state);
}

void nrf24l01_set_ce(struct nrf24l01_t* nrf, unsigned int state)
{
	mutex_lock(&nrf->m_state);
	nrf24l01_set_ce_(nrf, state);
	mutex_unlock(&nrf->m_state);
}

int nrf24l01_get_ce_(struct nrf24l01_t* nrf)
{
	return gpio_get_value(nrf->gpio_ce);
}

int nrf24l01_get_ce(struct nrf24l01_t* nrf)
{
	int state;
	mutex_lock(&nrf->m_state);
	state = nrf24l01_get_ce_(nrf);
	mutex_unlock(&nrf->m_state);
	return state;
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

int nrf24l01_get_fifo_tx_full(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_FIFO_STATUS_TX_FULL, status, 1);
}

int nrf24l01_get_fifo_tx_empty(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_FIFO_STATUS_TX_EMPTY, status, 1);
}

int nrf24l01_get_fifo_rx_full(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_FIFO_STATUS_RX_FULL, status, 1);
}

int nrf24l01_get_fifo_rx_empty(struct nrf24l01_t* nrf, unsigned int* status)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_FIFO_STATUS_RX_EMPTY, status, 1);
}


int nrf24l01_get_vreg_or_fail_short(struct nrf24l01_t* nrf, int vreg, int ntries)
{
	int err = 0, tries = 0;
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

int nrf24l01_get_rxtx_(struct nrf24l01_t* nrf, int* state)
{
	return nrf24l01_get_prim_rx_(nrf, state);
}

int nrf24l01_get_rxtx(struct nrf24l01_t* nrf, int* state)
{
	int err;
	mutex_lock(&nrf->m_state);
	err = nrf24l01_get_prim_rx_(nrf, state);
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_set_rxtx(struct nrf24l01_t* nrf, int state)
{
	int err, cstate;
	mutex_lock(&nrf->m_state);
	if((err = nrf24l01_get_rxtx_(nrf, &cstate)))
		goto exit_err_mutex;
	if(state != cstate)
	{
		NRF24L01_CE_LO_(nrf);
		if((err = nrf24l01_pwr_down_(nrf)))
			goto exit_err_mutex;
		if((err = nrf24l01_set_prim_rx_(nrf, state)))
			goto exit_err_mutex;
	}
	if((err = nrf24l01_get_pwr_up_(nrf, &cstate)))
		goto exit_err_mutex;
	if(!cstate)
	{
		if((err = nrf24l01_pwr_up_(nrf)))
			goto exit_err_mutex;
	}
	NRF24L01_CE_HI_(nrf);
exit_err_mutex:
	mutex_unlock(&nrf->m_state);
	return err;
}

int nrf24l01_set_rx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_rxtx(nrf, 1);
}

int nrf24l01_set_tx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_rxtx(nrf, 0);
}

static void nrf24l01_reader_inc(struct nrf24l01_t* nrf)
{
	mutex_lock(&nrf->m_rx_path);
	nrf->num_readers++;
	mutex_unlock(&nrf->m_rx_path);
}

static int nrf24l01_reader_dec(struct nrf24l01_t* nrf)
{
	int err = 0;
	unsigned int fifo_status;
	mutex_lock(&nrf->m_rx_path);
	nrf->num_readers--;
	if(nrf->num_readers == 0)
	{
		mutex_lock(&nrf->m_tx_path);
		if((err = nrf24l01_get_fifo_tx_empty(nrf, &fifo_status)))
		{
			goto exit_mutex_tx;
		}
		if(fifo_status)
		{
			if((err = nrf24l01_shutdown(nrf)))
			{
				goto exit_mutex_tx;
			}
		}
exit_mutex_tx:
		mutex_unlock(&nrf->m_tx_path);			
	}
	mutex_unlock(&nrf->m_rx_path);
	return err;
}

ssize_t nrf24l01_read_packet(struct nrf24l01_t* nrf, bool noblock, unsigned char* data, unsigned int len)
{
	size_t err, cleanup_err;
	unsigned int pipe_no, payload_width, dyn_pld, fifo_status;
	// Keep track of active readers for power saving
	if(nrf24l01_get_mode_low_pwr(nrf))
	{
		nrf24l01_reader_inc(nrf);
		mutex_lock(&nrf->m_tx_path);
		if((err = nrf24l01_get_fifo_tx_empty(nrf, &fifo_status)))
		{
			mutex_unlock(&nrf->m_tx_path);
			goto exit_err;
		}
		if(fifo_status)
		{
			if((err = nrf24l01_set_rx(nrf)))
			{
				mutex_unlock(&nrf->m_tx_path);
				goto exit_err;
			}
		}
		mutex_unlock(&nrf->m_tx_path);
	}

	// This block does the actual receiving
tryagain:
	// We can't enter the event queue if we are not allowed to block
	if(noblock)
	{
		// Check if there is any data ...
		if((err = nrf24l01_get_status_rx_p_no(nrf, &pipe_no)))
		{
			goto exit_err;
		}
		// ... and exit if there is none
		if(pipe_no == NRF24L01_RX_P_NO_EMPTY)
		{
			err = -EAGAIN;
			goto exit_err;
		}
	}
	else
	{
		// Wait for the nrf worker to wake us up
		if((err = wait_event_interruptible(nrf->rx_queue, nrf24l01_get_rx_p_no_or_fail(nrf) != NRF24L01_RX_P_NO_EMPTY)))
		{
			goto exit_err;
		}
	}
	// Make sure we are the only one retreiving packets
	mutex_lock(&nrf->m_rx_path);
	// Check if another worker already fetched our packet
	if((err = nrf24l01_get_status_rx_p_no(nrf, &pipe_no)))
	{
		goto exit_err_mutex;
	}
	if(pipe_no == NRF24L01_RX_P_NO_EMPTY)
	{
		// Fail if there was no packet
		if(noblock)
		{
			err = -EAGAIN;
			goto exit_err_mutex;
		}
		// Try again if we are supposed to block
		mutex_unlock(&nrf->m_rx_path);
		goto tryagain;
	}
	dev_dbg(&nrf->spi->dev, "Got payload in pipe %u\n", pipe_no);
	// Check wether our payload has dynamic length
	if((err = nrf24l01_get_dynpd(nrf, pipe_no, &dyn_pld)))
	{
		dev_err(&nrf->spi->dev, "Failed to determine pipe payload type (dynamic/fixed size): %zd\n", err);
		goto exit_err_mutex;
	}
	if(dyn_pld)
	{
		// Get payload width
		if((err = nrf24l01_get_dyn_pld_width(nrf, &payload_width)))
		{
			dev_err(&nrf->spi->dev, "Payload size read failed: %zd\n", err);
			goto exit_err_mutex;
		}
		// Flush rx fifo if payload size is invalid
		if(payload_width > NRF24L01_PACKET_MAX_LENGTH)
		{
			dev_err(&nrf->spi->dev, "Payload size is > %d, flushing rx fifo\n", NRF24L01_PACKET_MAX_LENGTH);
			if((err = nrf24l01_flush_rx_(nrf)))
			{
				dev_err(&nrf->spi->dev, "Failed to flush rx fifo: %zu\n", err);
				goto exit_err_mutex;
			}
			err = -EIO;
			goto exit_err_mutex;
		}
	}
	else
	{
		// Get static payload width from pipe
		if((err = nrf24l01_get_pipe_pld_width(nrf, pipe_no, &payload_width)))
		{
			goto exit_err_mutex;
		}
	}
	// Fail if supplied buffer is too short
	if(len < payload_width)
	{
		dev_err(&nrf->spi->dev, "Packet read buffer too short. Should be >= %u, is %u\n", payload_width, len);
		err = -EINVAL;
		goto exit_err_mutex;
	}
	// And (finally!) read packet to buffer
	if((err = nrf24l01_spi_read_rx_pld(nrf, data, payload_width)))
	{
		goto exit_err_mutex;
	}
	err = payload_width;
exit_err_mutex:
	mutex_unlock(&nrf->m_rx_path);
exit_err:
	// Decrement reader count if in low power mode
	if(nrf24l01_get_mode_low_pwr(nrf))
	{
		if((cleanup_err = nrf24l01_reader_dec(nrf)))
		{
			err = cleanup_err;
		}
	}
	return err;
}

int nrf24l01_send_packet(struct nrf24l01_t* nrf, bool noblock, unsigned char* data, unsigned int len)
{
	int err;
	unsigned int tx_full;
	// Check wether packet is too long for nrf
	if(len > NRF24L01_PACKET_MAX_LENGTH)
	{
		err = -EINVAL;
		goto exit_err;
	}

	// This block handles the actual data transmission
tryagain:
	// We can't enter the tx queue if we are not allowed to block
	if(noblock)
	{
		// Check if tx buffer is full ...
		if((err = nrf24l01_get_status_tx_full(nrf, &tx_full)))
		{
			goto exit_err;
		}
		// ... and fail when it is
		if(tx_full)
		{
			err = -EAGAIN;
			goto exit_err;
		}
	}
	else
	{
		// Wait for nrf worker to wake us up
		if((err = wait_event_interruptible(nrf->tx_queue, nrf24l01_get_tx_full_or_fail(nrf) != 1)))
		{
			goto exit_err;
		}
	}
	// Make sure we are the only one doing stuff modifying the tx state
	mutex_lock(&nrf->m_tx_path);
	// Check if there is still space in the tx fifo
	if((err = nrf24l01_get_status_tx_full(nrf, &tx_full)))
	{
		goto exit_err_mutex;
	}
	// Abort current write attempt if tx fifo became full
	if(tx_full)
	{
		// Fail if we can't block
		if(noblock)
		{
			err = -EAGAIN;
			goto exit_err_mutex;
		}
		// Back to square one if we are allowd to block
		mutex_unlock(&nrf->m_tx_path);
		goto tryagain;
	}
	// Set nrf to tx mode
	if((err = nrf24l01_set_tx(nrf)))
	{
		goto exit_err_mutex;
	}
	// Write and send the payload
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
