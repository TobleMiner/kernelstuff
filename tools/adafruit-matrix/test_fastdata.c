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
#include <assert.h>

#include "io.h"
#include "matrix.h"

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
#define COLUMNS 128
#define PWM_BITS 4

#define REAL_X 64
#define REAL_Y 64

#define NUM_PROCESSING_THREADS 1

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

#define NUM_PANELS 2

struct matrix_ledpanel matrix_up = {
	.name = "Upper",
	.xres = 64,
	.yres = 32,
	.virtual_x = 64,
	.virtual_y = 0,
	.realx = 0,
	.realy = 0,
	.flip_y = 1
};

struct matrix_ledpanel matrix_low = {
	.name = "Lower",
	.xres = 64,
	.yres = 32,
	.virtual_x = 0,
	.virtual_y = 0,
	.realx = 0,
	.realy = 32,
	.flip_x = 1,
	.flip_y = 0
};

int run = 1;
int numchld = 0;
int address = 0;

int pixpos = 0;

long updates = 0;

#define LL_IO

void llgpio_setup()
{
	gpio_set_outputs((1 << GPIO_R1) | (1 << GPIO_R2) | (1 << GPIO_G1) | (1 << GPIO_G2) | (1 << GPIO_B1) | (1 << GPIO_B2) | (1 << GPIO_A) | (1 << GPIO_B) | (1 << GPIO_C) | (1 << GPIO_D) | (1 << GPIO_E) | (1 << GPIO_OE) | (1 << GPIO_STR) | (1 << GPIO_CLK));
}

#define GPIO_HI(gpio) gpio_set_bits((1 << gpio))
#define GPIO_LO(gpio) gpio_clr_bits((1 << gpio))
#define GPIO_SET(gpio, state) (state ? GPIO_HI(gpio) : GPIO_LO(gpio))

void display_row(struct panel_io* data, int length)
{
	uint32_t clock = 1;
	while(--length >= 0)
	{
		gpio_write_bits(((uint32_t*)data)[length]);
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

void remap_frame(struct matrix_ledpanel** panels, uint32_t* from, int width_from, int height_from, uint32_t* to, int width_to, int height_to)
{
	int i, j;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	for(i = 0; i < height_from; i++)
	{
		for(j = 0; j < width_from; j++)
		{
			panel = matrix_get_panel_at_real(panels, NUM_PANELS, j, i);
			assert(panel != NULL);
			matrix_panel_get_position(&pos, panel, j, i);
			//printf("REMAP: [%d,%d] -> [%d,%d]@%s\n", j, i, pos.x, pos.y, panel->name);
			to[pos.y * width_to + pos.x] = from[i * width_from + j];
		}
	}
}

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
				printf("%d: Offset: %d/%d of %d\n ", j, row1_base + k, row2_base + k, rows * columns);
				if(frame[row1_base + k] & 0xFF > j)
				{
					row[k].B1 = 1;
				}
				if(frame[row1_base + k] >> 8 & 0xFF > j)
				{
					row[k].G1 = 1;
				}
				if(frame[row1_base + k] >> 16 & 0xFF > j)
				{
					row[k].R1 = 1;
				}
				if(frame[row2_base + k] & 0xFF > j)
				{
					row[k].B2 = 1;
				}
				if(frame[row2_base + k] >> 8 & 0xFF > j)
				{
					row[k].G2 = 1;
				}
				if(frame[row2_base + k] >> 16 & 0xFF > j)
				{
					row[k].R2 = 1;
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
			display_row(frame + i * pwm_steps * columns + j * columns, columns);
			//gpio_clr_bits(GPIO_CLOCK_MASK | GPIO_DATA_MASK);
			GPIO_HI(GPIO_STR);
			GPIO_LO(GPIO_STR);
		}
	}
}

void* render_part(void* part)
{
	struct frame* framepart = (struct frame*)part;
	printf("Thread: Segment: 0x%x Width: %d Height: %d\n", framepart->paneldata + framepart->paneloffset, framepart->width, framepart->rows);
	prerender_frame_part(framepart);
}

void process_frame(struct panel_io* rowdata, int rowdata_len, struct matrix_ledpanel** panels, uint32_t* rawdata, int raw_width, int raw_height, int columns, int rows, int num_processing_threads, int pwm_bits)
{
	int i;

	int len = rows * columns;

	printf("Allocation size: %d (%d bytes)", len, len * sizeof(uint32_t));

	uint32_t* data = malloc(len * sizeof(uint32_t));

	memset(data, 0, len * sizeof(uint32_t));

	remap_frame(panels, rawdata, raw_width, raw_height, data, columns, rows);

	memset(rowdata, 0, rowdata_len * sizeof(struct panel_io));

	int rows_per_thread = rows / num_processing_threads;

	printf("Rows per thread: %d\n", rows_per_thread);

	struct frame threadframes[num_processing_threads];

	pthread_t threadids[num_processing_threads];

	for(i = 0; i < num_processing_threads; i++)
	{
		struct frame* threadframe = &threadframes[i];
		threadframe->width = columns;
		threadframe->height = rows;
		threadframe->vertical_offset = i * rows_per_thread;
		threadframe->rows = rows_per_thread;
		threadframe->paneldata = rowdata;
		threadframe->paneloffset = i * rows_per_thread * columns;
		threadframe->frame = data;
		threadframe->frameoffset = i * rows_per_thread * columns;
		threadframe->pwm_bits = pwm_bits;
		printf("Thread %d: Start @0x%x Threadframe: 0x%x\n", i, rowdata + i * rows_per_thread * columns, &threadframe);
		if(pthread_create(&threadids[i], NULL, &render_part, threadframe))
			perror("Failed to create processing thread: ");
	}
	for(i = 0; i < num_processing_threads; i++)
	{
		pthread_join(threadids[i], NULL);
	}
}

void set_pixmum(int* num)
{
	pixpos = *num;
}

int main(int argc, char** argv)
{
	if(llgpio_init() != 0)
		return -EPERM;
	llgpio_setup();

	struct matrix_ledpanel* ledpanels[NUM_PANELS] = {
		&matrix_low,
		&matrix_up
	};

	GPIO_LO(GPIO_OE);

	int rawlen = REAL_X * REAL_Y;

	uint32_t rawdata[rawlen];

	memset(rawdata, 0, rawlen * sizeof(uint32_t));

	float max_distance = sqrt(pow(ROWS, 2) + pow(COLUMNS, 2));
	float max_x = REAL_X;
	float max_y = REAL_Y;
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
	for(i = 0; i < REAL_Y; i++)
	{
		for(j = 0; j < REAL_X; j++)
		{
			if(i == j)
				rawdata[i * REAL_X + j] = 255;
/*			if(j < REAL_X / 2 - 6 || i < REAL_Y / 2 - 6 || j > REAL_X / 2 + 6 || i > REAL_Y / 2 + 6)
				continue;
			if(j < REAL_X / 2)
			{
				rawdata[i * REAL_X + j] = cnt == 0 ? pwm_max : 0;
				rawdata[i * REAL_X + j] |= cnt == 1 ? pwm_max << 8 : 0;
				rawdata[i * REAL_X + j] |= cnt == 2 ? pwm_max << 16: 0;
				rawdata[i * REAL_X + j] = 255;
			}
			else
			{
				rawdata[i * REAL_X + j] = cnt == 2 ? pwm_max : 0;
                rawdata[i * REAL_X + j] |= cnt == 1 ? pwm_max << 8 : 0;
                rawdata[i * REAL_X + j] |= cnt == 0 ? pwm_max << 16: 0;
				rawdata[i * REAL_X + j] = 255 << 16;
			}
*/		}
		cnt++;
		cnt %= 3;
	}

	int rowdata_len = ROWS / 2 * COLUMNS * (1 << PWM_BITS);
	struct panel_io rowdata[rowdata_len];

	process_frame(rowdata, rowdata_len, ledpanels, rawdata, REAL_X, REAL_Y, COLUMNS, ROWS, NUM_PROCESSING_THREADS, PWM_BITS);

/*	for(i = 0; i < rowdata_len; i++)
	{
		struct panel_io cio = rowdata[i];
		if(cio.R1 || cio.G1 || cio.B1)
			printf("%d: Row select 1 [R:%d G:%d B:%d]\n", i, cio.R1, cio.G1, cio.B1);
		if(cio.R2 || cio.G2 || cio.B2)
			printf("%d: Row select 2 [R:%d G:%d B:%d]\n", i, cio.R1, cio.G1, cio.B1);
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
		display_row(rowdata, COLUMNS);
	}
	return 0;
}
