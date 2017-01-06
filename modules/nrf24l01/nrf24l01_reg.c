#include <linux/errno.h>
#include <linux/err.h>
#include <linux/vmalloc.h>

#include "nrf24l01_reg.h"
#include "nrf24l01_core.h"
#include "partregmap.h"

static struct partreg_range range_config_prim_rx = partreg_reg_range(0, 1);
static unsigned int mask_config_prim_rx = 0b1;

static struct partreg_template reg_config_prim_rx = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 0,
	.mask = &mask_config_prim_rx,
	.len = 1,
	.value_range = &range_config_prim_rx
};

static struct partreg_range range_config_pwr_up = partreg_reg_range(0, 1);
static unsigned int mask_config_pwr_up = 0b10;

static struct partreg_template reg_config_pwr_up = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 1,
	.mask = &mask_config_pwr_up,
	.len = 1,
	.value_range = &range_config_pwr_up
};

static struct partreg_range range_config_crco = partreg_reg_range(0, 1);
static unsigned int mask_config_crco = 0b100;

static struct partreg_template reg_config_crco = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 2,
	.mask = &mask_config_crco,
	.len = 1,
	.value_range = &range_config_crco
};

static struct partreg_range range_config_en_crc = partreg_reg_range(0, 1);
static unsigned int mask_config_en_crc = 0b1000;

static struct partreg_template reg_config_en_crc = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 3,
	.mask = &mask_config_en_crc,
	.len = 1,
	.value_range = &range_config_en_crc
};

static struct partreg_range range_config_mask_max_rt = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_max_rt = 0b10000;

static struct partreg_template reg_config_mask_max_rt = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 4,
	.mask = &mask_config_mask_max_rt,
	.len = 1,
	.value_range = &range_config_mask_max_rt
};

static struct partreg_range range_config_mask_tx_ds = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_tx_ds = 0b100000;

static struct partreg_template reg_config_mask_tx_ds = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 5,
	.mask = &mask_config_mask_tx_ds,
	.len = 1,
	.value_range = &range_config_mask_tx_ds
};

static struct partreg_range range_config_mask_rx_dr = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_rx_dr = 0b1000000;

static struct partreg_template reg_config_mask_rx_dr = {
	.reg = NRF24L01_REG_CONFIG,
	.offset = 6,
	.mask = &mask_config_mask_rx_dr,
	.len = 1,
	.value_range = &range_config_mask_rx_dr
};

static struct partreg_template* nrf24l01_regs[] = {
	&reg_config_prim_rx,
	&reg_config_pwr_up,
	&reg_config_crco,
	&reg_config_en_crc,
	&reg_config_mask_max_rt,
	&reg_config_mask_tx_ds,
	&reg_config_mask_rx_dr,
};

static struct partreg_layout nrf24l01_reg_layout = {
	.regs = nrf24l01_regs,
	.n_regs = 2,
};

int nrf24l01_create_partregs(struct nrf24l01_t* nrf)
{
	nrf->reg_table = partreg_create_table(&nrf24l01_reg_layout, nrf->regmap_short, nrf);	
	if(IS_ERR(nrf->reg_table))
		return PTR_ERR(nrf->reg_table);
	return 0;
}

void nrf24l01_free_partregs(nrf24l01_t* nrf)
{
	partreg_free_table(nrf->reg_table);
}
