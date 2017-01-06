typedef struct nrf24l01_t {
	struct spi_device*		spi;
	struct regmap*			regmap_short;
	struct device*			chrdev;
	int						chrdev_major;
	struct class*			chrclass;
	struct partreg_table*	reg_table;
} nrf24l01_t;
