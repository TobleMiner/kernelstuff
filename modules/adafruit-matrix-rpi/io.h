#ifndef _ADAMTX_IO_H
#define _ADAMTX_IO_H

//#define ADAMTX_DEBUG_SLOW 10
//#define ADMATX_REQUEST_EXCLUSIVE_GPIO

#define ADAMTX_PERIPHERAL_BASE		0x3F000000
#define ADAMTX_GPIO_OFFSET  		0x200000
#define ADAMTX_GPIO_SET_OFFSET 		(0x1C / sizeof(uint32_t))
#define ADAMTX_GPIO_CLR_OFFSET 		(0x28 / sizeof(uint32_t))
#define ADAMTX_REGISTER_BLOCK_SIZE	4096

#define ADAMTX_INP_GPIO(g) *(adamtx_gpio_map+((g)/10)) &= ~(7<<(((g)%10)*3))
#define ADAMTX_OUT_GPIO(g) *(adamtx_gpio_map+((g)/10)) |=  (1<<(((g)%10)*3))

int adamtx_gpio_alloc(void);

void adamtx_gpio_free(void);

void adamtx_gpio_set_outputs(uint32_t outputs);

void adamtx_gpio_set_bits(uint32_t value);

void adamtx_gpio_clr_bits(uint32_t value);

void adamtx_gpio_write_bits(uint32_t value);

void adamtx_gpio_write_masked_bits(uint32_t value, uint32_t mask);

#endif
