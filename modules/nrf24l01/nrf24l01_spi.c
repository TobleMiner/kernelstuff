#include <linux/spi/spi.h>
#include <linux/vmalloc.h>

#include "nrf24l01_core.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_cmd.h"

static char nrf24l01_reg_masks[] = {
	NRF24L01_REG_MASK_CONFIG,
	NRF24L01_REG_MASK_EN_AA,
	NRF24L01_REG_MASK_EN_RXADDR,
	NRF24L01_REG_MASK_SETUP_AW,
	NRF24L01_REG_MASK_SETUP_RETR,
	NRF24L01_REG_MASK_RF_CH,
	NRF24L01_REG_MASK_RF_SETUP,
	NRF24L01_REG_MASK_STATUS,
	NRF24L01_REG_MASK_OBSERVE_TX,
	NRF24L01_REG_MASK_CD,
	NRF24L01_REG_MASK_RX_ADDR_PO,
	NRF24L01_REG_MASK_RX_ADDR_P1,
	NRF24L01_REG_MASK_RX_ADDR_P2,
	NRF24L01_REG_MASK_RX_ADDR_P3,
	NRF24L01_REG_MASK_RX_ADDR_P4,
	NRF24L01_REG_MASK_RX_ADDR_P5,
	NRF24L01_REG_MASK_TX_ADDR,
	NRF24L01_REG_MASK_RX_PW_P0,
	NRF24L01_REG_MASK_RX_PW_P1,
	NRF24L01_REG_MASK_RX_PW_P2,
	NRF24L01_REG_MASK_RX_PW_P3,
	NRF24L01_REG_MASK_RX_PW_P4,
	NRF24L01_REG_MASK_RX_PW_P5,
	NRF24L01_REG_MASK_FIFO_STATUS
};

#define NRF24L01_NUM_REGS 24


int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val)
{
	char data[] = { NRF24L01_CMD_W_REGISTER | (u8)reg, (u8)val };
	return spi_write(((struct nrf24l01_t*)ctx)->spi, (char*)&data, 2);
}

int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val)
{
	if(reg < 0 || reg >= NRF24L01_NUM_REGS)
		return -EINVAL;
	val &= nrf24l01_reg_masks[reg];
	return nrf24l01_write_short_reg(ctx, reg, val);
}

int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val)
{
	ssize_t err;
	printk(KERN_INFO "Reading register %d with cmd %d\n", reg, NRF24L01_CMD_R_REGISTER | (u8)reg);
	err = spi_w8r8(((struct nrf24l01_t*)ctx)->spi, NRF24L01_CMD_R_REGISTER | (u8)reg);
	if(err < 0)
		return err;
	*val = err;
	return 0;
}

int nrf24l01_spi_write(struct spi_device* spi, unsigned char* data, unsigned int len)
{
	return spi_write(spi, data, len);
}

int nrf24l01_write_reg(void* ctx, unsigned int reg, unsigned char* data, unsigned int len)
{
	int err;
	unsigned char* regwrite = vmalloc(len + 1);
	if(IS_ERR(regwrite))
		return PTR_ERR(regwrite);
	*regwrite = NRF24L01_CMD_W_REGISTER | (u8)reg;
	memcpy(regwrite + 1, data, len);
	err = nrf24l01_spi_write(((nrf24l01_t*)ctx)->spi, regwrite, len); 
	vfree(regwrite);
	return err;
}
