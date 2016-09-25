#define PERIPHERAL_BASE		0x3F000000
#define GPIO_OFFSET  		0x200000
#define GPIO_SET_OFFSET 	(0x1C / sizeof(uint32_t))
#define GPIO_CLR_OFFSET 	(0x28 / sizeof(uint32_t))
#define REGISTER_BLOCK_SIZE	4096

#define INP_GPIO(g) *(gpio_map+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio_map+((g)/10)) |=  (1<<(((g)%10)*3))

uint32_t* gpio_set;
uint32_t* gpio_clr;

const uint32_t valid_gpio_bits;

int llgpio_init();

void gpio_set_outputs(uint32_t outputs);

void gpio_set_bits(uint32_t value);

void gpio_clr_bits(uint32_t value);

void gpio_write_bits(uint32_t value);

void gpio_write_masked_bits(uint32_t value, uint32_t mask);

