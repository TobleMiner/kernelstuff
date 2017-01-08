#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/of.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_cmd.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_chrdev.h"
#include "nrf24l01_functions.h"
#include "partregmap.h"

enum nrf24l01_modules {nRF24L01, nRF24L01p};

static const struct regmap_range regmap_wr_table_short_yes[] = { regmap_reg_range(0x00, 0x07), regmap_reg_range(0x0C, 0x0F),
	regmap_reg_range(0x11, 0x16), regmap_reg_range(0x1C, 0x1D) };
static const struct regmap_range regmap_wr_table_short_no[] = { regmap_reg_range(0x08, 0x08), regmap_reg_range(0x17, 0x17) };

static const struct regmap_access_table regmap_wr_table_short = {
	.yes_ranges = regmap_wr_table_short_yes,
	.n_yes_ranges = 4,
	.no_ranges = regmap_wr_table_short_no,
	.n_no_ranges = 2,
};

static const struct regmap_range regmap_rd_table_short_yes[] = { regmap_reg_range(0x00, 0x09), regmap_reg_range(0x0C, 0x0F), 
	regmap_reg_range(0x11, 0x17), regmap_reg_range(0x1C, 0x1D) };
static const struct regmap_range regmap_rd_table_short_no[] = { regmap_reg_range(0x0A, 0x0B), regmap_reg_range(0x10, 0x10) };

static const struct regmap_access_table regmap_rd_table_short = {
	.yes_ranges = regmap_rd_table_short_yes,
	.n_yes_ranges = 4,
	.no_ranges = regmap_rd_table_short_no,
	.n_no_ranges = 2
};

static const struct regmap_range regmap_volatile_table_short_yes[] = { regmap_reg_range(0x07, 0x09), regmap_reg_range(0x17, 0x17) };
static const struct regmap_range regmap_volatile_table_short_no[] = { regmap_reg_range(0x01, 0x06), regmap_reg_range(0x0A, 0x16) };

static const struct regmap_access_table regmap_volatile_table_short = {
	.yes_ranges = regmap_volatile_table_short_yes,
	.n_yes_ranges = 3,
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
	.precious_table = &regmap_precious_table_short,
	.use_single_rw = 1
};

static irqreturn_t nrf24l01_irq(int irq, void* data)
{
	struct nrf24l01_t* nrf = data;
	return IRQ_HANDLED;
}

static struct nrf24l01_t* nrf24l01_dev;

static int nrf24l01_probe(struct spi_device* spi)
{
	int err = 0;
	unsigned int irq_trigger;
	void* of_gpio_ce;
	printk(KERN_WARNING "nrf24l01_probe\n");
	nrf24l01_dev = vzalloc(sizeof(nrf24l01_t));
	if(IS_ERR(nrf24l01_dev))
	{
		err = PTR_ERR(nrf24l01_dev);
		goto exit_noalloc;
	}
	nrf24l01_dev->spi = spi;
	mutex_init(&nrf24l01_dev->m_rx_path);
	mutex_init(&nrf24l01_dev->m_tx_path);
	printk(KERN_INFO "Adding regmap...\n");
	nrf24l01_dev->regmap_short = regmap_init(&spi->dev, NULL, nrf24l01_dev, &nrf24l01_regmap_short);
	if(IS_ERR(nrf24l01_dev->regmap_short))
	{
		err = PTR_ERR(nrf24l01_dev->regmap_short);
		goto exit_nrfalloc;
	}
	if((err = nrf24l01_create_partregs(nrf24l01_dev)) < 0)
	{
		goto exit_regmapalloc;
	}
	if((err = chrdev_alloc(nrf24l01_dev)) < 0)
	{
		goto exit_partregalloc;
	}
	irq_trigger = irq_get_trigger_type(spi->irq);
	if(!irq_trigger)
	{
		dev_err(&spi->dev, "IRQ trigger type not set\n");
		err = -EINVAL;
		goto exit_chrdevalloc;
	}
	if((err = devm_request_irq(&spi->dev, spi->irq, nrf24l01_irq, irq_trigger, dev_name(&spi->dev), nrf24l01_dev)))
	{
		dev_err(&spi->dev, "Failed to allocate interrupt\n");
		goto exit_chrdevalloc;
	}
	of_gpio_ce = of_get_property(spi->dev.of_node, "nrf-ce", NULL);
	if(!of_gpio_ce)
	{
        dev_err(&spi->dev, "Chip Enable not specified\n");
		err = -EINVAL;
        goto exit_chrdevalloc;
	}
	nrf24l01_dev->gpio_ce = be32_to_cpup(of_gpio_ce);
	printk(KERN_INFO "CE GPIO: %u\n", nrf24l01_dev->gpio_ce);
	if((err = gpio_request(nrf24l01_dev->gpio_ce, "ce")))
	{
		dev_err(&spi->dev, "Allocation of GPIO%u failed\n", nrf24l01_dev->gpio_ce);
		goto exit_chrdevalloc;
	}
	gpio_direction_output(nrf24l01_dev->gpio_ce, 0);
	unsigned int val = 0;
	int ret = regmap_read(nrf24l01_dev->regmap_short, NRF24L01_REG_STATUS, &val);
	printk(KERN_INFO "Read NRF24L01_REG_STATUS as %d with result %d\n", val, ret);
	printk(KERN_INFO "Testing partregmap\n");
	val = 1;
	err = partreg_table_write(nrf24l01_dev->reg_table, 1, &val, 1);
	printk(KERN_INFO "Wrote to partreg: %d\n", err);
	val = 0;
	err = partreg_table_read(nrf24l01_dev->reg_table, 1, &val, 1);
	printk(KERN_INFO "Read back from partreg: %d, err: %d", val, err);
	printk(KERN_INFO "Setting address width to 4\n");
	val = 4;
	err = nrf24l01_set_address_width(nrf24l01_dev, val);
	printk(KERN_INFO "Wrote address width, err: %d\n", err);
	err = nrf24l01_get_address_width(nrf24l01_dev, &val);
	printk(KERN_INFO "Read back address width: %u, err: %d\n", val, err);
	unsigned char address[] = {0x42, 0x42, 0x42, 0x42, 0x55};
	printk(KERN_INFO "Writing address\n");
	err = partreg_table_write(nrf24l01_dev->reg_table, NRF24L01_VREG_RX_ADDR_P0, (unsigned int*) address, 5);
	printk(KERN_INFO "Wrote address, err: %d\n", err);
	printk(KERN_INFO "Reading back address\n");
	err = partreg_table_read(nrf24l01_dev->reg_table, NRF24L01_VREG_RX_ADDR_P0, (unsigned int*) address, 5);
	printk(KERN_INFO "Read back address: err=%d, address=", err);
	for(val = 0; val < 5; val++)
	{
		printk(KERN_INFO "%x", address[val]);
	}
	printk(KERN_INFO "\n");
	return 0;
exit_chrdevalloc:
	chrdev_free(nrf24l01_dev);
exit_partregalloc:
	nrf24l01_free_partregs(nrf24l01_dev);
exit_regmapalloc:
	regmap_exit(nrf24l01_dev->regmap_short);
exit_nrfalloc:
	vfree(nrf24l01_dev);
exit_noalloc:
	return err;
}

static int nrf24l01_remove(struct spi_device* spi)
{
	printk(KERN_WARNING "nrf24l01_remove\n");
	gpio_free(nrf24l01_dev->gpio_ce);
	chrdev_free(nrf24l01_dev);
	nrf24l01_free_partregs(nrf24l01_dev);
	regmap_exit(nrf24l01_dev->regmap_short);
	vfree(nrf24l01_dev);
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
