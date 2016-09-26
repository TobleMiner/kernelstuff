#include <bcm2835.h>
#include <linux/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

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

#define ROWS 32
#define COLUMNS 64
#define PWM_BITS 8

#define NUM_PROCESSING_THREADS 2

#define GPIO_CLOCK_MASK (1 << 17)

#define GPIO_ADDR_OFFSET 22
#define GPIO_HI_ADDR_MASK 0b0011110000000000000000000000
#define GPIO_ADDR_MASK 0b0011110000001000000000000000

#define GPIO_DATA_OFFSET 0
#define GPIO_DATA_MASK 0b1000000000000000111110000000

typedef struct panel_io {
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
};

typedef struct frame
{
	int width;
	int height;
	int vertical_offset;
	int rows;
	int pwm_bits;
	struct panel_io* paneldata;
	off_t paneloffset;
	uint32_t* frame;
	off_t frameoffset;
};

int run = 1;
int numchld = 0;
int address = 0;

long updates = 0;

#define LL_IO

void llgpio_setup()
{
	gpio_set_outputs((1 << GPIO_R1) | (1 << GPIO_R2) | (1 << GPIO_G1) | (1 << GPIO_G2) | (1 << GPIO_B1) | (1 << GPIO_B2) | (1 << GPIO_A) | (1 << GPIO_B) | (1 << GPIO_C) | (1 << GPIO_D) | (1 << GPIO_E) | (1 << GPIO_OE) | (1 << GPIO_STR) | (1 << GPIO_CLK));	
}

#define GPIO_HI(gpio) gpio_set_bits((1 << gpio))
#define GPIO_LO(gpio) gpio_clr_bits((1 << gpio))
#define GPIO_SET(gpio, state) (state ? GPIO_HI(gpio) : GPIO_LO(gpio))

void clock_out(struct panel_io* data, int length)
{
	int i;
	uint32_t clock = 1;
	for(i = 0; i < length; i++)
	{
		gpio_write_bits(((uint32_t*)data)[i]);
		clock ^= 1;
		GPIO_HI(GPIO_CLK);
	}
}

void set_address(int addr)
{
	struct panel_io data;
	*((uint32_t*)(&data)) = (addr << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
	data.E = addr >> 4;
	gpio_write_masked_bits(*((uint32_t*)&data), GPIO_ADDR_MASK);	
}

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
		printf("%ld updates per second\n", updates);
		updates = 0;
		if(run)
		{
			fork_child();
		}
	}
}

#define UCP unsigned char*

#define U16P uint16_t*

void prerender_frame_part(struct frame* framepart)
{
	int i;
	int j;
	int k;
	uint32_t* frame = framepart->frame;
	int rows = framepart->height;
	int columns = framepart->width;
	int pwm_steps = 1 << framepart->pwm_bits;
	int vertical_offset = framepart->vertical_offset / 2;
	printf("vertical offset: %d\n", vertical_offset);
	struct panel_io row[columns];
	for(i = vertical_offset; i < vertical_offset + framepart->rows / 2; i++)
	{
		int row1_base = i * columns;
		int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns * sizeof(struct panel_io));
			for(k = 0; k < columns; k++)
			{
				if(frame[row1_base + k] & 0xFF > j)
				{
					row[k].R1 = 1;
				}
				if(frame[row1_base + k] >> 8 & 0xFF > j)
				{
					row[k].G1 = 1;
				}
				if(frame[row1_base + k] >> 16 & 0xFF > j)
				{
					row[k].B1 = 1;
				}
				if(frame[row2_base + k] & 0xFF > j)
				{
					row[k].R2 = 1;
				}
				if(frame[row2_base + k] >> 8 & 0xFF > j)
				{
					row[k].G2 = 1;
				}
				if(frame[row2_base + k] >> 16 & 0xFF > j)
				{
					row[k].B2 = 1;
				}			
				*((uint32_t*)(&row[k])) |= (i << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
				row[k].E = i >> 4;
			}
			memcpy(framepart->paneldata + i * pwm_steps * columns + j * columns, row, columns * sizeof(struct panel_io));
		}
	}
}

void prerender_frame(struct panel_io* rowdata, uint32_t* frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int k;
	int pwm_steps = 1 << bits;
	struct panel_io row[columns];
	for(i = 0; i < rows / 2; i++)
	{
		int row1_base = i * columns;
		int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns * sizeof(struct panel_io));
			for(k = 0; k < columns; k++)
			{
/*				if(i == 0 &&)
					printf("%d: %d:%d:%d Offset: %d\n ", j, red[row1_base + k], green[row1_base + k], blue[row1_base + k], row1_base + k);
*/				if(frame[row1_base + k] & 0xFF > j)
				{
					row[k].R1 = 1;
				}
				if(frame[row1_base + k] >> 8 & 0xFF > j)
				{
					row[k].G1 = 1;
				}
				if(frame[row1_base + k] >> 16 & 0xFF > j)
				{
					row[k].B1 = 1;
				}
				if(frame[row2_base + k] & 0xFF > j)
				{
					row[k].R2 = 1;
				}
				if(frame[row2_base + k] >> 8 & 0xFF > j)
				{
					row[k].G2 = 1;
				}
				if(frame[row2_base + k] >> 16 & 0xFF > j)
				{
					row[k].B2 = 1;
				}			
				*((uint32_t*)(&row[k])) |= (i << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
				row[k].E = i >> 4;
			}
			memcpy(rowdata + i * pwm_steps * columns + j * columns, row, columns * sizeof(struct panel_io));
		}
	}
}

void show_frame(struct panel_io* frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int pwm_steps = (1 << bits);
	for(i = 0; i < rows / 2; i++)
	{
		//set_address(i);
		for(j = 0; j < pwm_steps; j++)
		{
			clock_out(frame + i * pwm_steps * columns + j * columns, columns);
			//gpio_clr_bits(GPIO_CLOCK_MASK | GPIO_DATA_MASK);
			GPIO_HI(GPIO_STR);
			GPIO_LO(GPIO_STR);
		}
	}
}

void* render_part(void* part)
{
	struct frame* framepart = (struct frame*)part;
	printf("Thread: Segment: 0x%x Width: %d Height: %d\n", framepart->paneldata, framepart->width, framepart->height);
	prerender_frame_part(framepart);
}

int main(int argc, char** argv)
{
	if(llgpio_init() != 0)
		return -EPERM;
	llgpio_setup();

	GPIO_LO(GPIO_OE);
	
	int len = ROWS * COLUMNS;

	uint32_t data[len];
	
	memset(data, 0, len * sizeof(uint32_t));

	float max_distance = sqrt(pow(ROWS, 2) + pow(COLUMNS, 2));
	float max_x = COLUMNS;
	float max_y = ROWS;
	int i;
	int j;
	int pwm_max = (1 << PWM_BITS) - 1;
/*	for(i = 0; i < ROWS; i++)
	{
		for(j = 0; j < COLUMNS; j++)
		{
			float distance = (i / max_y) * (j / max_x);
			printf("%f\n", distance);
			data[i * COLUMNS + j] = (uint8_t)(pwm_max * (1 - distance));
			data[i * COLUMNS + j] |= (uint8_t)(pwm_max * (1 - distance)) << 8;
			data[i * COLUMNS + j] |= (uint8_t)(pwm_max * (1 - distance)) << 16;
		}
	}

	data[16] = 255 | 255 < 8 | 255 < 16;
*/

	int cnt = 0;
	for(i = 0; i < ROWS; i++)
	{
		for(j = 0; j < COLUMNS; j++)
		{
			data[i * COLUMNS + j] = cnt == 0 ? pwm_max : 0; 
			data[i * COLUMNS + j] |= cnt == 1 ? pwm_max << 8 : 0; 
			data[i * COLUMNS + j] |= cnt == 2 ? pwm_max << 16: 0;
		}
		cnt++;
		cnt %= 3;
	}

	int rowdata_len = ROWS / 2 * COLUMNS * (1 << PWM_BITS);

	struct panel_io rowdata[rowdata_len];
	memset(rowdata, 0, rowdata_len * sizeof(struct panel_io));

	int rows_per_thread = ROWS / NUM_PROCESSING_THREADS;

	printf("Rows per thread: %d\n", rows_per_thread);

	struct frame threadframes[NUM_PROCESSING_THREADS];
	
	for(i = 0; i < NUM_PROCESSING_THREADS; i++)
	{
		struct frame* threadframe = &threadframes[i];
		threadframe->width = COLUMNS;
		threadframe->height = ROWS;
		threadframe->vertical_offset = i * rows_per_thread;
		threadframe->rows = rows_per_thread;
		threadframe->paneldata = rowdata;
		threadframe->paneloffset = i * rows_per_thread * COLUMNS;
		threadframe->frame = data;
		threadframe->frameoffset = i * rows_per_thread * COLUMNS;
		threadframe->pwm_bits = PWM_BITS;
		pthread_t addr;
		printf("Thread %d: Start @0x%x Threadframe: 0x%x\n", i, rowdata + i * rows_per_thread * COLUMNS, &threadframe);
		if(pthread_create(&addr, NULL, &render_part, threadframe))
			perror("Failed to create processing thread: ");
	}

	//prerender_frame(rowdata, data, PWM_BITS, ROWS, COLUMNS);

/*	for(i = 0; i < rowdata_len; i++)
	{
		struct panel_io cio = rowdata[i];
		if(cio.R1 || cio.G1 || cio.B1)
			printf("%d: Row select 1\n", i);
		if(cio.R2 || cio.G2 || cio.B2)
			printf("%d: Row select 2\n", i);
	}
*/
	signal(SIGINT, signalhandler);
	signal(SIGCHLD, signalhandler);

	fork_child();

	while(run)
	{
		show_frame(rowdata, PWM_BITS, ROWS, COLUMNS);
		updates++;
	}

	while(numchld > 0)
		sleep(1);

	memset(rowdata, 0, COLUMNS * sizeof(struct panel_io));
	for(i = 0; i < ROWS / 2; i++)
	{
		*((uint32_t*)(&rowdata[i])) |= (i << GPIO_ADDR_OFFSET) & GPIO_HI_ADDR_MASK;
		rowdata[i].E = i >> 4;
		clock_out(rowdata, COLUMNS);
	}
	return 0;
}
