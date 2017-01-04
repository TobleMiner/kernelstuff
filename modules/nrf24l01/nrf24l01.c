#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>

#include "nrf24l01.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_cmd.h"

enum nrf24l01_modules {nRF24L01, nRF24L01p};

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

static const struct regmap_range regmap_wr_table_short_yes[] = { regmap_reg_range(0x00, 0x07), regmap_reg_range(0x0C, 0x0F), regmap_reg_range(0x11, 0x16) };
static const struct regmap_range regmap_wr_table_short_no[] = { regmap_reg_range(0x08, 0x08), regmap_reg_range(0x17, 0x17) };

static const struct regmap_access_table regmap_wr_table_short = {
	.yes_ranges = regmap_wr_table_short_yes,
	.n_yes_ranges = 3,
	.no_ranges = regmap_wr_table_short_no,
	.n_no_ranges = 2,
};

static const struct regmap_range regmap_rd_table_short_yes[] = { regmap_reg_range(0x00, 0x09), regmap_reg_range(0x0C, 0x0F), regmap_reg_range(0x11, 0x17) };
static const struct regmap_range regmap_rd_table_short_no[] = { regmap_reg_range(0x0A, 0x0B), regmap_reg_range(0x10, 0x10) };

static const struct regmap_access_table regmap_rd_table_short = {
	.yes_ranges = regmap_rd_table_short_yes,
	.n_yes_ranges = 3,
	.no_ranges = regmap_rd_table_short_no,
	.n_no_ranges = 2
};

static const struct regmap_range regmap_volatile_table_short_yes[] = { regmap_reg_range(0x07, 0x09), regmap_reg_range(0x17, 0x17) };
static const struct regmap_range regmap_volatile_table_short_no[] = { regmap_reg_range(0x00, 0x06), regmap_reg_range(0x0A, 0x16) };

static const struct regmap_access_table regmap_volatile_table_short = {
	.yes_ranges = regmap_volatile_table_short_yes,
	.n_yes_ranges = 2,
	.no_ranges = regmap_volatile_table_short_no,
	.n_no_ranges = 2,
};

static const struct regmap_range regmap_precious_table_short_yes[] = { regmap_reg_range(0x17, 0x17) };
static const struct regmap_range regmap_precious_table_short_no[] = { regmap_reg_range(0x00, 0x16) };

static const struct regmap_access_table regmap_precious_table_short = {
	.yes_ranges = regmap_precious_table_short_yes,
	.n_yes_ranges = 1,
	.no_ranges = regmap_precious_table_short_no,
	.n_no_ranges = 1
};

static const struct regmap_config nrf24l01_regmap_short = {
	.name = "NRF24L01 short",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_read = nrf24l01_read_short_reg,
	.reg_write = nrf24l01_write_short_reg_masked,
	.wr_table = &regmap_wr_table_short,
	.rd_table = &regmap_rd_table_short,
	.volatile_table = &regmap_volatile_table_short,
	.precious_table = &regmap_precious_table_short
};

static int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val)
{
	char data[] = { NRF24L01_CMD_R_REGISTER | (u8)reg, (u8)val };
	return spi_write((struct spi_device*)ctx, (char*)&data, 2);
}

static int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val)
{
	if(reg < 0 || reg >= NRF24L01_NUM_REGS)
		return -EINVAL;
	val &= nrf24l01_reg_masks[reg];
	return nrf24l01_write_short_reg(ctx, reg, val);
}

static int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val)
{
	ssize_t err = spi_w8r8((struct spi_device*)ctx, NRF24L01_CMD_W_REGISTER | (u8)reg);
	if(err < 0)
		return err;
	*val = err;
	return 0;
}

static struct regmap* regmap_short;

static int nrf24l01_probe(struct spi_device* spi)
{
	printk(KERN_WARNING "nrf24l01_probe\n");
	printk(KERN_INFO "Adding regmap...\n");
	regmap_short = regmap_init_spi(spi, &nrf24l01_regmap_short);
	if(IS_ERR(regmap_short))
		return PTR_ERR(regmap_short);
/*	char data_init[] = {0b00100000 | 0x00, 0b00001011};
	size_t len = 2;
	int err = spi_write(spi, data_init, len);
	if(err)
		return err;
	char data_addr[] = {0b00100000 | 0x0A, 0xDE, 0xAD, 0xBE, 0xEF, 0x42};
	len = 6;
	err = spi_write(spi, data_addr, len);
	if(err)
		return err;
	char data_read[] = {0x0A, 0, 0, 0, 0, 0};
	len = 6;
	err = spi_write(spi, data_read, len);
	if(err)
		return err;*/
	return 0;
}

static int nrf24l01_remove(struct spi_device* spi)
{
	printk(KERN_WARNING "nrf24l01_remove\n");
	regmap_exit(regmap_short);
	return 0;
}

static struct spi_driver nrf24l01_driver = {
	.probe = nrf24l01_probe,
	.remove = nrf24l01_remove,
	.driver = {
		.name = "nrf24l01"
	}
};

static const struct of_device_id nrf24l01_of_match[] = {
	{ .compatible = "nordicsemi,nrf24l01", .data = (void*)nRF24L01 },
	{ .compatible = "nordicsemi,nrf24l01p", .data = (void*)nRF24L01p },
	{}
};

MODULE_DEVICE_TABLE(of, nrf24l01_of_match);

static const struct spi_device_id nrf24l01_ids[] = {
	{"nrf24l01", nRF24L01},
	{"nrf24l01p", nRF24L01p},
	{}
};

MODULE_DEVICE_TABLE(spi, nrf24l01_ids);

module_spi_driver(nrf24l01_driver);

MODULE_DESCRIPTION("NRF24L01 SPI driver");
MODULE_AUTHOR("Tobias Schramm");
MODULE_LICENSE("GPL");
