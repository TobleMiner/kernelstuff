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

static struct partreg_template* nrf24l01_regs[] = {
	&reg_config_prim_rx,
};

static struct partreg_layout nrf24l01_reg_layout = {
	.regs = nrf24l01_regs,
	.n_regs = 1,
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
