static int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val);


static int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val);


static int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val);


static int nrf24l01_probe(struct spi_device* spi);


static int nrf24l01_remove(struct spi_device* spi);
