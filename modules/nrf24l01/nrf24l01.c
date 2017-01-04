#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

enum nrf24l01_modules {nRF24L01, nRF24L01p};

static int nrf24l01_probe(struct spi_device* spi)
{
	printk(KERN_WARNING "nrf24l01_probe\n");
	char data_init[] = {0b00100000 | 0x00, 0b00001011};
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
		return err;
	return 0;
}

static int nrf24l01_remove(struct spi_device* spi)
{
	printk(KERN_WARNING "nrf24l01_remove\n");
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
