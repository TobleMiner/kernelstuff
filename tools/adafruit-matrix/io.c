#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "io.h"

uint32_t* gpio_set = NULL;
uint32_t* gpio_clr = NULL;

uint32_t* gpio_map = NULL;

const uint32_t valid_gpio_bits = ((1 <<  0) | (1 <<  1) | // RPi 1 - Revision 1 accessible
   (1 <<  2) | (1 <<  3) | // RPi 1 - Revision 2 accessible
   (1 <<  4) | (1 <<  7) | (1 << 8) | (1 <<  9) |
   (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15)| (1 <<17) | (1 << 18) |
   (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25)| (1 << 27) |
   // support for A+/B+ and RPi2 with additional GPIO pins.
   (1 <<  5) | (1 <<  6) | (1 << 12) | (1 << 13) | (1 << 16) |
   (1 << 19) | (1 << 20) | (1 << 21) | (1 << 26));

int llgpio_init()
{
	int mem = open("/dev/mem", O_RDWR | O_SYNC);
	if(mem < 0)
	{
		perror("Failed to open /dev/mem");
		return mem;
	}
	uint32_t* map = (uint32_t*) mmap(NULL, REGISTER_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem, PERIPHERAL_BASE + GPIO_OFFSET);
	close(mem);
	if(mem == MAP_FAILED)
	{
		perror("Failed to map gpio");
		return -EFAULT;
	}
	gpio_map = map;
	gpio_set = map + GPIO_SET_OFFSET;
	gpio_clr = map + GPIO_CLR_OFFSET;
	return 0;
}

void gpio_set_outputs(uint32_t outputs)
{
	outputs &= valid_gpio_bits;
	for(uint32_t b = 0; b <= 27; ++b)
	{
		if(outputs & (1 << b))
		{
			INP_GPIO(b);
			OUT_GPIO(b);
		}
	}
}

void gpio_set_bits(uint32_t value)
{
    *gpio_set = value & valid_gpio_bits;
	#ifdef DEBUG_SLOW
	for(int i = 0; i < DEBUG_SLOW; i++)
	{
		asm("nop");
	}
	#endif
}

void gpio_clr_bits(uint32_t value)
{
    *gpio_clr = value & valid_gpio_bits;
	#ifdef DEBUG_SLOW
	for(int i = 0; i < DEBUG_SLOW; i++)
	{
		asm("nop");
	}
	#endif
}

void gpio_write_bits(uint32_t value)
{
	gpio_clr_bits(~value);
	gpio_set_bits(value);
}

void gpio_write_masked_bits(uint32_t value, uint32_t mask)
{
    gpio_clr_bits(~value & mask);
    gpio_set_bits(value & mask);
}

