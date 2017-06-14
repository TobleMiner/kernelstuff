#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_cmd.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_chrdev.h"
#include "nrf24l01_functions.h"
#include "nrf24l01_worker.h"
#include "nrf24l01_quirks.h"
#include "partregmap.h"

static LIST_HEAD(nrf_list);

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
static const struct regmap_range regmap_volatile_table_short_no[] = { regmap_reg_range(0x01, 0x06), regmap_reg_range(0x0A, 0x16),
	regmap_reg_range(0x1C, 0x1D)};

static const struct regmap_access_table regmap_volatile_table_short = {
	.yes_ranges = regmap_volatile_table_short_yes,
	.n_yes_ranges = 2,
	.no_ranges = regmap_volatile_table_short_no,
	.n_no_ranges = 3,
};

static const struct regmap_range regmap_precious_table_short_yes[] = { regmap_reg_range(0x17, 0x17) };
static const struct regmap_range regmap_precious_table_short_no[] = { regmap_reg_range(0x00, 0x16), regmap_reg_range(0x1C, 0x1D)};

static const struct regmap_access_table regmap_precious_table_short = {
	.yes_ranges = regmap_precious_table_short_yes,
	.n_yes_ranges = 1,
	.no_ranges = regmap_precious_table_short_no,
	.n_no_ranges = 2
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
	.use_single_rw = 1,
	.cache_type = REGCACHE_RBTREE
};

static irqreturn_t nrf24l01_irq(int irq, void* data)
{
	struct nrf24l01_t* nrf = data;
	up(&nrf->worker.sema);
	return IRQ_HANDLED;
}

bool nrf24l01_nrf_registered()
{
	return !list_empty(&nrf_list);
}

static int nrf24l01_get_free_id(struct list_head* nrf_list, unsigned int* id)
{
	struct list_head* cursor;
	unsigned int id_test;
	bool id_free;
	nrf24l01_t* nrf;
	for(id_test = 0; id_test <= UINT_MAX; id_test++)
	{
		id_free = true;
		list_for_each(cursor, nrf_list)
		{
			nrf = list_entry(cursor, nrf24l01_t, list);
			if(nrf->id == id_test)
			{
				dev_dbg(&nrf->spi->dev, "Id %u is in use\n", id_test);
				id_free = false;
				break;
			}
		}
		if(id_free)
		{
			*id = id_test;
			return 0;
		}
	}
	return -ENOMEM;
}

static int nrf24l01_probe(struct spi_device* spi)
{
	int err = 0;
	unsigned int irq_trigger;
	const void *of_gpio_ce, *of_nrf_mode, *of_nrf_addr_be;
	struct nrf24l01_t* nrf;
	dev_info(&spi->dev, "Initializing nrf driver\n");
	nrf = vzalloc(sizeof(nrf24l01_t));
	if(!nrf)
	{
		err = -ENOMEM;
		goto exit_noalloc;
	}
	dev_set_drvdata(&spi->dev, nrf);
	nrf->spi = spi;
	if((err = nrf24l01_get_free_id(&nrf_list, &nrf->id)))
	{
		dev_err(&spi->dev, "No free nrf id found\n");
		goto exit_nrfalloc;
	}
	list_add(&nrf->list, &nrf_list);
	dev_info(&spi->dev, "Got id %u\n", nrf->id);
	mutex_init(&nrf->m_rx_path);
	mutex_init(&nrf->m_tx_path);
	mutex_init(&nrf->m_state);
	nrf->regmap_short = regmap_init(&spi->dev, NULL, nrf, &nrf24l01_regmap_short);
	if(IS_ERR(nrf->regmap_short))
	{
		err = PTR_ERR(nrf->regmap_short);
		goto exit_nrfalloc;
	}
	if((err = nrf24l01_create_partregs(nrf)) < 0)
	{
		goto exit_regmapalloc;
	}
	if((err = chrdev_alloc(nrf)) < 0)
	{
		goto exit_partregalloc;
	}
	of_nrf_mode = of_get_property(spi->dev.of_node, "nrf-mode", NULL);
	if(!of_nrf_mode)
	{
		dev_warn(&spi->dev, "Mode not specified, defaulting to 0\n");
	}
	else
	{
		nrf->mode_flags = be32_to_cpup(of_nrf_mode);
	}
	dev_info(&spi->dev, "nrf mode: %u\n", nrf->mode_flags);
	of_nrf_addr_be = of_get_property(spi->dev.of_node, "nrf-addr-be", NULL);
	if(!of_nrf_addr_be)
	{
		dev_warn(&spi->dev, "Address endianess not specified, defaulting to LE\n");
	}
	else
	{
		nrf->addr_be = !!be32_to_cpup(of_nrf_addr_be);
	}
	dev_info(&spi->dev, "Address endianess: %s\n", nrf->addr_be ? "BE" : "LE");
	init_waitqueue_head(&nrf->rx_queue);
	init_waitqueue_head(&nrf->tx_queue);
	if((err = nrf24l01_create_worker(nrf)))
	{
		dev_err(&spi->dev, "Failed to create worker thread\n");
		goto exit_chrdevalloc;
	}
	of_gpio_ce = of_get_property(spi->dev.of_node, "nrf-ce", NULL);
	if(!of_gpio_ce)
	{
		dev_err(&spi->dev, "Chip Enable not specified\n");
		err = -EINVAL;
		goto exit_workeralloc;
	}
	nrf->gpio_ce = be32_to_cpup(of_gpio_ce);
	dev_info(&nrf->spi->dev, "CE GPIO: %u\n", nrf->gpio_ce);
	if((err = gpio_request(nrf->gpio_ce, "ce")))
	{
		dev_err(&spi->dev, "Allocation of GPIO%u failed\n", nrf->gpio_ce);
		goto exit_workeralloc;
	}
	gpio_direction_output(nrf->gpio_ce, 0);
	irq_trigger = irq_get_trigger_type(spi->irq);
	if(!irq_trigger)
	{
		dev_err(&spi->dev, "IRQ trigger type not set\n");
		err = -EINVAL;
		goto exit_gpioalloc;
	}
	if((err = devm_request_irq(&spi->dev, spi->irq, nrf24l01_irq, irq_trigger, dev_name(&spi->dev), nrf)))
	{
		dev_err(&spi->dev, "Failed to allocate interrupt\n");
		goto exit_gpioalloc;
	}
	NRF24L01_CE_LO(nrf);
	nrf24l01_pwr_down(nrf);
	nrf24l01_flush(nrf);
	nrf24l01_set_status_max_rt(nrf, 1);	
	nrf24l01_set_status_rx_dr(nrf, 1);
	nrf24l01_set_status_tx_ds(nrf, 1);
	if(!nrf24l01_get_mode_low_pwr(nrf))
		nrf24l01_set_rx(nrf);

	err = nrf24l01_test_unflushable_fifo(nrf);
	if(err < 0)
		goto exit_gpioalloc;
	if(err)
		dev_err(&spi->dev, "Faulty nrf module detected! TX FIFO stuck full\n");

	return 0;
exit_gpioalloc:
	gpio_free(nrf->gpio_ce);
exit_workeralloc:
	nrf24l01_destroy_worker(nrf);
exit_chrdevalloc:
	chrdev_free(nrf);
exit_partregalloc:
	nrf24l01_free_partregs(nrf);
exit_regmapalloc:
	regmap_exit(nrf->regmap_short);
exit_nrfalloc:
	vfree(nrf);
exit_noalloc:
	return err;
}

static int nrf24l01_remove(struct spi_device* spi)
{
	struct nrf24l01_t* nrf;
	nrf = dev_get_drvdata(&spi->dev);
	dev_info(&nrf->spi->dev, "Removing nrf %u\n", nrf->id);
	list_del(&nrf->list);
	chrdev_free(nrf);
	nrf24l01_destroy_worker(nrf);
	gpio_free(nrf->gpio_ce);
	nrf24l01_free_partregs(nrf);
	regmap_exit(nrf->regmap_short);
	vfree(nrf);
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
