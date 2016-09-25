#include <bcm2835.h>
#include <linux/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#include "io.h"

#define GPIO_R1		11
#define GPIO_R2		8
#define GPIO_G1		27
#define GPIO_G2		9
#define GPIO_B1		7
#define GPIO_B2		10
#define GPIO_A		22
#define GPIO_B		23
#define GPIO_C		24
#define GPIO_D		25
#define GPIO_E		15
#define GPIO_OE		18
#define GPIO_STR	4
#define GPIO_CLK	17

#define ROWS 16
#define COLUMNS 32
#define PWM_BITS 8

#define GPIO_ADDR_MASK 0b0011110000000000000000000000

#define GPIO_DATA_MASK 0b1000000000000000111110000000

struct {
	uint32_t GPIO0	: 1;
	uint32_t GPIO1	: 1;
	uint32_t GPIO2	: 1;
	uint32_t GPIO3	: 1;
	uint32_t STR	: 1;
	uint32_t GPIO5	: 1;
	uint32_t GPIO6	: 1;
	uint32_t B1		: 1;
	uint32_t R2		: 1;
	uint32_t G2		: 1;
	uint32_t B2		: 1;
	uint32_t R1		: 1;
	uint32_t GPIO12 : 1;
	uint32_t GPIO13 : 1;
	uint32_t GPIO14 : 1;
	uint32_t E		: 1;
	uint32_t GPIO16 : 1;
	uint32_t CLK	: 1;
	uint32_t OE		: 1;
	uint32_t GPIO19 : 1;
	uint32_t GPIO20 : 1;
	uint32_t GPIO21 : 1;
	uint32_t A		: 1;
	uint32_t B		: 1;
	uint32_t C		: 1;
	uint32_t D		: 1;
	uint32_t GPIO26 : 1;
	uint32_t G1		: 1;
} gpio;

uint32_t* gpio_ptr = (uint32_t*)&gpio;

int run = 1;
int numchld = 0;
int address = 0;

long updates = 0;

#define LL_IO

void llgpio_setup()
{
	gpio_set_outputs((1 << GPIO_R1) | (1 << GPIO_R2) | (1 << GPIO_G1) | (1 << GPIO_G2) | (1 << GPIO_B1) | (1 << GPIO_B2) | (1 << GPIO_A) | (1 << GPIO_B) | (1 << GPIO_C) | (1 << GPIO_D) | (1 << GPIO_E) | (1 << GPIO_OE) | (1 << GPIO_STR) | (1 << GPIO_CLK));	
}

void gpio_setup()
{
	bcm2835_gpio_fsel(GPIO_R1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_R2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_G1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_G2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_B1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_B2, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_A, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_B, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_C, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_D, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_E, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_OE, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_STR, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(GPIO_CLK, BCM2835_GPIO_FSEL_OUTP);
}

#ifdef LL_IO
	#define GPIO_HI(gpio) gpio_set_bits((1 << gpio))
	#define GPIO_LO(gpio) gpio_clr_bits((1 << gpio))
#else
	#define GPIO_HI(gpio) bcm2835_gpio_set(gpio)
	#define GPIO_LO(gpio) bcm2835_gpio_clr(gpio)
#endif
#define GPIO_SET(gpio, state) (state ? GPIO_HI(gpio) : GPIO_LO(gpio))

void clock_out(unsigned char* data, int length)
{
	GPIO_LO(GPIO_STR);
	int i;
	for(i = 0; i < length; i++)
	{
		GPIO_LO(GPIO_CLK);
		GPIO_SET(GPIO_R1, (data[i] >> 0) & 0b1);
		GPIO_SET(GPIO_R2, (data[i] >> 1) & 0b1);
		GPIO_SET(GPIO_G1, (data[i] >> 2) & 0b1);
		GPIO_SET(GPIO_G2, (data[i] >> 3) & 0b1);
		GPIO_SET(GPIO_B1, (data[i] >> 4) & 0b1);
		GPIO_SET(GPIO_B2, (data[i] >> 5) & 0b1);
		GPIO_HI(GPIO_CLK);
	}
	GPIO_HI(GPIO_STR);
//	GPIO_LO(GPIO_STR);
}

#ifdef LL_IO
void set_address(uint i)
{
	*gpio_ptr &= ~GPIO_ADDR_MASK;
	*gpio_ptr |= (i << 22) & GPIO_ADDR_MASK;
	gpio.E = i >> 4;
	gpio_write_bits(*gpio_ptr);
}
#else
void set_address(uint i)
{
	GPIO_SET(GPIO_A, (i >> 0) & 0b1);
	GPIO_SET(GPIO_B, (i >> 1) & 0b1);
	GPIO_SET(GPIO_C, (i >> 2) & 0b1);
	GPIO_SET(GPIO_D, (i >> 3) & 0b1);
	GPIO_SET(GPIO_E, (i >> 4) & 0b1);
}
#endif

int fork_child()
{
	int ret;
	ret = fork();
	if(ret == 0)
	{
		sleep(1);
		exit(0);
	}
	if(ret < 0)
	{
		run = 0;
		perror("Failed to fork");
	}
	else
		numchld++;
	return ret;
}

void signalhandler(int sig)
{
	printf("Signal: %d\n", sig);
	if(sig == SIGINT)
		run = 0;
	else if(sig == SIGCHLD)
	{
		numchld--;
		address++;
		address %= ROWS;
		printf("%ld updates per second\n", updates);
		updates = 0;
		if(run)
		{
			fork_child();
		}
	}
}

#define UCP unsigned char*

void pwm_2lines(UCP red, UCP green, UCP blue, int bits, int addr, int rows, int columns)
{
	int i;
	int j;
	int offset;
	unsigned char line[columns];
	set_address(addr);
	int row1 = addr * columns;
	int row2 = (rows / 2 + addr) * columns;
	for(i = 0; i < (1 << bits); i++)
	{
		memset(line, 0, columns);
		for(j = 0; j < columns; j++)
		{
			if(red[row1 + j] >= i)
			{
				line[j] |= 0b1;
			}
			if(green[row1 + j] >= i)
			{
				line[j] |= 0b100;
			}
			if(blue[row1 + j] >= i)
			{
				line[j] |= 0b10000;
			}
			if(red[row2 + j] >= i)
			{
				line[j] |= 0b10;
			}
			if(green[row2 + j] >= i)
			{
				line[j] |= 0b1000;
			}
			if(blue[row2 + j] >= i)
			{
				line[j] |= 0b100000;
			}
		}
		clock_out(line, columns);
	}
}

void prerender_frame(UCP rowdata, UCP red, UCP green, UCP blue, int bits, int rows, int columns)
{
	int i;
	int j;
	int k;
	int pwm_steps = 1 << bits;
	unsigned char row[columns];
	for(i = 0; i < rows / 2; i++)
	{
		int row1_base = i * columns;
    	int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns);
			for(k = 0; k < columns; k++)
			{
				if(red[row1_base + k] >= j)
				{
					row[k] |= 0b1;
				}
				if(green[row1_base + k] >= j)
				{
					row[k] |= 0b100;
				}
				if(blue[row1_base + k] >= j)
				{
					row[k] |= 0b10000;
				}
				if(red[row2_base + k] >= j)
				{
					row[k] |= 0b10;
				}
				if(green[row2_base + k] >= j)
				{
					row[k] |= 0b1000;
				}
				if(blue[row2_base + k] >= j)
				{
					row[k] |= 0b100000;
				}			
			}
			memcpy(rowdata + i * pwm_steps * columns + j * columns, row, columns);
		}
	}
}

void show_frame(UCP frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int pwm_steps = (1 << bits);
	for(i = 0; i < rows / 2; i++)
	{		
		set_address(i);
		for(j = 0; j < pwm_steps; j++)
		{
			clock_out(frame + i * pwm_steps * columns + j * columns, columns);
		}
	}
}

int main(int argc, char** argv)
{
#ifdef LL_IO
	if(llgpio_init() != 0)
		return -EPERM;
	llgpio_setup();
#else
	if(!bcm2835_init())
		return -EPERM;
	gpio_setup();
#endif

	GPIO_LO(GPIO_OE);
	
	int len = ROWS * COLUMNS;

	unsigned char data_red[len];
	unsigned char data_green[len];
	unsigned char data_blue[len];


	float max_distance = sqrt(pow(ROWS, 2) + pow(COLUMNS, 2));
	int step = 255 / COLUMNS;
	int i;
	int j;
	for(i = 0; i < ROWS; i++)
	{
		for(j = 0; j < COLUMNS; j++)
		{
			float distance = sqrt(pow(i, 2) + pow(j, 2));
			data_red[i * COLUMNS + j] = (unsigned char)(255 * (distance / max_distance));
			data_green[i * COLUMNS + j] = (unsigned char)(255 * (distance / max_distance));
			data_blue[i * COLUMNS + j] = (unsigned char)(255 * (distance / max_distance));
		}
	}

	int rowdata_len = ROWS / 2 * COLUMNS * (1 << PWM_BITS);

	unsigned char rowdata[rowdata_len];

	prerender_frame(rowdata, data_red, data_green, data_blue, PWM_BITS, ROWS, COLUMNS);

	UCP row = rowdata + 3 * (1 << PWM_BITS) * COLUMNS + 10 * COLUMNS;
	for(i = 0; i < COLUMNS; i++)
	{
		printf("0x%x, ", row[i]);
	}
	printf("\n");

	signal(SIGINT, signalhandler);
	signal(SIGCHLD, signalhandler);

	fork_child();

	while(run)
	{
		show_frame(rowdata, PWM_BITS, ROWS, COLUMNS);
//		pwm_2lines(data_red, data_green, data_blue, PWM_BITS, address, ROWS, COLUMNS);
/*		set_address(address);
		clock_out(data_red, len);
		usleep(500000);
		address++;
		address %= ROWS / 2;*/
		updates++;
	}

	while(numchld > 0)
		sleep(1);

	unsigned char data[COLUMNS];
	memset(data, 0, COLUMNS);
	for(i = 0; i < ROWS / 2; i++)
	{
		set_address(i);
		clock_out(data, COLUMNS);
	}
#ifndef LL_IO
	bcm2835_close();
#endif
	return 0;
}
