#ifndef _NRF24L01_CORE_H_
#define _NRF24L01_CORE_H_

typedef struct nrf24l01_t {
	struct spi_device*		spi;
	struct regmap*			regmap_short;
	struct device*			chrdev;
	int						chrdev_major;
	struct class*			chrclass;
	struct partreg_table*	reg_table;
	unsigned 				gpio_ce;
} nrf24l01_t;

#endif
