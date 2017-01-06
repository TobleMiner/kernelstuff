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

static struct partreg_range range_en_aa_enaa_p0 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p0 = 0b1;

static struct partreg_template reg_en_aa_enaa_p0 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 0,
	.mask = &mask_en_aa_enaa_p0,
	.len = 1,
	.value_range = &range_en_aa_enaa_p0
};

static struct partreg_range range_en_aa_enaa_p1 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p1 = 0b10;

static struct partreg_template reg_en_aa_enaa_p1 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 1,
	.mask = &mask_en_aa_enaa_p1,
	.len = 1,
	.value_range = &range_en_aa_enaa_p1
};

static struct partreg_range range_en_aa_enaa_p2 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p2 = 0b100;

static struct partreg_template reg_en_aa_enaa_p2 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 2,
	.mask = &mask_en_aa_enaa_p2,
	.len = 1,
	.value_range = &range_en_aa_enaa_p2
};

static struct partreg_range range_en_aa_enaa_p3 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p3 = 0b1000;

static struct partreg_template reg_en_aa_enaa_p3 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 3,
	.mask = &mask_en_aa_enaa_p3,
	.len = 1,
	.value_range = &range_en_aa_enaa_p3
};

static struct partreg_range range_en_aa_enaa_p4 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p4 = 0b10000;

static struct partreg_template reg_en_aa_enaa_p4 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 4,
	.mask = &mask_en_aa_enaa_p4,
	.len = 1,
	.value_range = &range_en_aa_enaa_p4
};

static struct partreg_range range_en_aa_enaa_p5 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p5 = 0b100000;

static struct partreg_template reg_en_aa_enaa_p5 = {
	.reg = NRF24L01_REG_EN_AA,
	.offset = 5,
	.mask = &mask_en_aa_enaa_p5,
	.len = 1,
	.value_range = &range_en_aa_enaa_p5
};

static struct partreg_range range_en_rxaddr_erx_p0 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p0 = 0b1;

static struct partreg_template reg_en_rxaddr_erx_p0 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 0,
	.mask = &mask_en_rxaddr_erx_p0,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p0
};

static struct partreg_range range_en_rxaddr_erx_p1 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p1 = 0b10;

static struct partreg_template reg_en_rxaddr_erx_p1 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 1,
	.mask = &mask_en_rxaddr_erx_p1,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p1
};

static struct partreg_range range_en_rxaddr_erx_p2 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p2 = 0b100;

static struct partreg_template reg_en_rxaddr_erx_p2 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 2,
	.mask = &mask_en_rxaddr_erx_p2,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p2
};

static struct partreg_range range_en_rxaddr_erx_p3 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p3 = 0b1000;

static struct partreg_template reg_en_rxaddr_erx_p3 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 3,
	.mask = &mask_en_rxaddr_erx_p3,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p3
};

static struct partreg_range range_en_rxaddr_erx_p4 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p4 = 0b10000;

static struct partreg_template reg_en_rxaddr_erx_p4 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 4,
	.mask = &mask_en_rxaddr_erx_p4,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p4
};

static struct partreg_range range_en_rxaddr_erx_p5 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p5 = 0b100000;

static struct partreg_template reg_en_rxaddr_erx_p5 = {
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 5,
	.mask = &mask_en_rxaddr_erx_p5,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p5
};

static struct partreg_range range_setup_aw_aw = partreg_reg_range(1, 3);
static unsigned int mask_setup_aw_aw = 0b11;

static struct partreg_template reg_setup_aw_aw = {
	.reg = NRF24L01_REG_SETUP_AW,
	.offset = 0,
	.mask = &mask_setup_aw_aw,
	.len = 1,
	.value_range = &range_setup_aw_aw
};

static struct partreg_range range_setup_retr_arc = partreg_reg_range(0, 0b1111);
static unsigned int mask_setup_retr_arc = 0b1111;

static struct partreg_template reg_setup_retr_arc = {
	.reg = NRF24L01_REG_SETUP_RETR,
	.offset = 0,
	.mask = &mask_setup_retr_arc,
	.len = 1,
	.value_range = &range_setup_retr_arc
};

static struct partreg_range range_setup_retr_ard = partreg_reg_range(0, 0b1111);
static unsigned int mask_setup_retr_ard = 0b11110000;

static struct partreg_template reg_setup_retr_ard = {
	.reg = NRF24L01_REG_SETUP_RETR,
	.offset = 3,
	.mask = &mask_setup_retr_ard,
	.len = 1,
	.value_range = &range_setup_retr_ard
};

static struct partreg_range range_rf_ch_rf_ch = partreg_reg_range(0, 0b01111111);
static unsigned int mask_rf_ch_rf_ch = 0b01111111;

static struct partreg_template reg_rf_ch_rf_ch = {
    .reg = NRF24L01_REG_RF_CH,
    .offset = 0,
    .mask = &mask_rf_ch_rf_ch,
    .len = 1,
    .value_range = &range_rf_ch_rf_ch
};


static struct partreg_template* nrf24l01_regs[] = {
	&reg_config_prim_rx,
	&reg_config_pwr_up,
	&reg_config_crco,
	&reg_config_en_crc,
	&reg_config_mask_max_rt,
	&reg_config_mask_tx_ds,
	&reg_config_mask_rx_dr,
	&reg_en_aa_enaa_p0,
	&reg_en_aa_enaa_p1,
	&reg_en_aa_enaa_p2,
	&reg_en_aa_enaa_p3,
	&reg_en_aa_enaa_p4,
	&reg_en_aa_enaa_p5,
	&reg_en_rxaddr_erx_p0,
	&reg_en_rxaddr_erx_p1,
	&reg_en_rxaddr_erx_p2,
	&reg_en_rxaddr_erx_p3,
	&reg_en_rxaddr_erx_p4,
	&reg_en_rxaddr_erx_p5,
	&reg_setup_aw_aw,
	&reg_setup_retr_arc,
	&reg_setup_retr_ard,
	&reg_rf_ch_rf_ch,
};

static struct partreg_layout nrf24l01_reg_layout = {
	.regs = nrf24l01_regs,
	.n_regs = 23,
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
