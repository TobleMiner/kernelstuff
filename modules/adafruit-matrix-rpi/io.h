#ifndef _ADAMTX_IO_H
#define _ADAMTX_IO_H

//#define ADAMTX_DEBUG_SLOW 20
//#define ADMATX_REQUEST_EXCLUSIVE_GPIO

#define ADAMTX_PERIPHERAL_BUS_BASE	0x7E000000
#define ADAMTX_PERIPHERAL_BASE		0x3F000000
#define ADAMTX_GPIO_OFFSET  		0x00200000
#define ADAMTX_GPIO_SET_OFFSET 		0x1C
#define ADAMTX_GPIO_CLR_OFFSET 		0x28
#define ADAMTX_REGISTER_BLOCK_SIZE	4096

#define ADAMTX_VALID_GPIO_BITS ((1 << 0) | (1 << 1) | \
	(1 << 2) | (1 << 3) | \
	(1 << 4) | (1 << 7) | (1 << 8) | (1 << 9) | \
	(1 << 10) | (1 << 11) | (1 << 14) | (1 << 15)| (1 <<17) | (1 << 18) | \
	(1 << 22) | (1 << 23) | (1 << 24) | (1 << 25)| (1 << 27) | \
	(1 << 5) | (1 << 6) | (1 << 12) | (1 << 13) | (1 << 16) | \
	(1 << 19) | (1 << 20) | (1 << 21) | (1 << 26))

#define ADAMTX_INP_GPIO(g) *(adamtx_gpio_map+((g)/10)) &= ~(7<<(((g)%10)*3))
#define ADAMTX_OUT_GPIO(g) *(adamtx_gpio_map+((g)/10)) |= (1<<(((g)%10)*3))

int adamtx_gpio_alloc(struct adamtx* adamtx);

void adamtx_gpio_free(struct adamtx* adamtx);

void adamtx_gpio_set_outputs(uint32_t outputs);

void adamtx_gpio_set_bits(uint32_t value);

void adamtx_gpio_clr_bits(uint32_t value);

void adamtx_gpio_write_bits(uint32_t value);

void adamtx_gpio_write_masked_bits(uint32_t value, uint32_t mask);

#endif
