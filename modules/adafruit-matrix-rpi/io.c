#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "adafruit_matrix.h"
#include "io.h"

uint32_t* adamtx_gpio_set = NULL;
uint32_t* adamtx_gpio_clr = NULL;

uint32_t* adamtx_gpio_map = NULL;

const uint32_t adamtx_valid_gpio_bits = ADAMTX_VALID_GPIO_BITS;

int adamtx_gpio_alloc(struct adamtx* adamtx)
{
	#ifdef ADAMTX_REQUEST_EXCLUSIVE_GPIO
	if(request_mem_region(adamtx->peripheral_base + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE, "ADAMTX_GPIO") == NULL)
	{
		return -EIO;
	}
	#endif
	adamtx_gpio_map = ioremap_nocache(adamtx->peripheral_base + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE);
	if(adamtx_gpio_map == NULL)
	{
		return -EIO;
	}
	adamtx_gpio_set = adamtx_gpio_map + ADAMTX_GPIO_SET_OFFSET / sizeof(uint32_t);
	adamtx_gpio_clr = adamtx_gpio_map + ADAMTX_GPIO_CLR_OFFSET / sizeof(uint32_t);
	return 0;
}

void adamtx_gpio_free(struct adamtx* adamtx)
{
	iounmap(adamtx_gpio_map);
	#ifdef ADAMTX_REQUEST_EXCLUSIVE_GPIO
	release_mem_region(adamtx->peripheral_base + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE);
	#endif
}

void adamtx_gpio_set_outputs(uint32_t outputs)
{
	uint32_t b;
	outputs &= adamtx_valid_gpio_bits;
	for(b = 0; b <= 27; ++b)
	{
		if(outputs & (1 << b))
		{
			ADAMTX_INP_GPIO(b);
			ADAMTX_OUT_GPIO(b);
		}
	}
}

void adamtx_gpio_set_bits(uint32_t value)
{
	*adamtx_gpio_set = value & adamtx_valid_gpio_bits;
	#ifdef ADAMTX_DEBUG_SLOW
	int i;
	for(i = 0; i < ADAMTX_DEBUG_SLOW; i++)
	{
		asm("nop");
	}
	#endif
}

void adamtx_gpio_clr_bits(uint32_t value)
{
	*adamtx_gpio_clr = value & adamtx_valid_gpio_bits;
	#ifdef ADAMTX_DEBUG_SLOW
	int i;
	for(i = 0; i < ADAMTX_DEBUG_SLOW; i++)
	{
		asm("nop");
	}
	#endif
}

void adamtx_gpio_write_bits(uint32_t value)
{
	adamtx_gpio_clr_bits(~value);
	adamtx_gpio_set_bits(value);
}

void adamtx_gpio_write_masked_bits(uint32_t value, uint32_t mask)
{
	adamtx_gpio_clr_bits(~value & mask);
	adamtx_gpio_set_bits(value & mask);
}
