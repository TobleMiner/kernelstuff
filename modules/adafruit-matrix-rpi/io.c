#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "io.h"

uint32_t* adamtx_gpio_set = NULL;
uint32_t* adamtx_gpio_clr = NULL;

uint32_t* adamtx_gpio_map = NULL;

const uint32_t adamtx_valid_gpio_bits = ((1 <<  0) | (1 <<  1) | // RPi 1 - Revision 1 accessible
   (1 <<  2) | (1 <<  3) | // RPi 1 - Revision 2 accessible
   (1 <<  4) | (1 <<  7) | (1 << 8) | (1 <<  9) |
   (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15)| (1 <<17) | (1 << 18) |
   (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25)| (1 << 27) |
   // support for A+/B+ and RPi2 with additional GPIO pins.
   (1 <<  5) | (1 <<  6) | (1 << 12) | (1 << 13) | (1 << 16) |
   (1 << 19) | (1 << 20) | (1 << 21) | (1 << 26));

int adamtx_gpio_alloc()
{
	#ifdef REQUEST_EXCLUSIVE_GPIO
	if(request_mem_region(ADAMTX_PERIPHERAL_BASE + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE, "ADAMTX_GPIO") == NULL)
	{
		return -EIO;
	}
	#endif
	adamtx_gpio_map = ioremap_nocache(ADAMTX_PERIPHERAL_BASE + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE);
	if(adamtx_gpio_map == NULL)
	{
		return -EIO;
	}
	adamtx_gpio_set = adamtx_gpio_map + ADAMTX_GPIO_SET_OFFSET;
	adamtx_gpio_clr = adamtx_gpio_map + ADAMTX_GPIO_CLR_OFFSET;	
	return 0;
}

void adamtx_gpio_free()
{
    #ifdef REQUEST_EXCLUSIVE_GPIO
	iounmap(adamtx_gpio_map);
	#endif
	release_mem_region(ADAMTX_PERIPHERAL_BASE + ADAMTX_GPIO_OFFSET, ADAMTX_REGISTER_BLOCK_SIZE);
}

void adamtx_gpio_set_outputs(uint32_t outputs)
{
	outputs &= adamtx_valid_gpio_bits;
	uint32_t b;
	for(b = 0; b <= 27; ++b)
	{
		if(outputs & (1 << b))
		{
			INP_GPIO(b);
			OUT_GPIO(b);
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
	for(i = 0; i < DEBUG_SLOW; i++)
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
