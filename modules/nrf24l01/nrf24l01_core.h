typedef struct nrf24l01_t {
	struct spi_device*	dev;
	struct regmap*		regmap_short;
	struct device*		chrdev;
	int					chrdev_major;
	struct class*		chrclass;
} nrf24l01_t;
