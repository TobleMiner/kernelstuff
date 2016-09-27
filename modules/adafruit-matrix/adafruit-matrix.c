#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include "matrix.h"
#include "adafruit-matrix.h"

#define ADAMTX_GPIO_HI(gpio) ADAMTX_GPIO_SET(gpio, 1)
#define ADAMTX_GPIO_LO(gpio) ADAMTX_GPIO_SET(gpio, 0)
//#define ADAMTX_GPIO_SET(gpio, state) gpio_set_value(gpio, state)
#define ADAMTX_GPIO_SET(gpio, state) asm("nop")

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

static DEFINE_MUTEX(adamtx_draw_mutex);

static unsigned adamtx_gpio_ids[ADAMTX_NUM_GPIOS] = {ADAMTX_GPIO_R1, ADAMTX_GPIO_R2, ADAMTX_GPIO_G1, ADAMTX_GPIO_G2, ADAMTX_GPIO_B1, ADAMTX_GPIO_B2, ADAMTX_GPIO_A, ADAMTX_GPIO_B, ADAMTX_GPIO_C, ADAMTX_GPIO_D, ADAMTX_GPIO_E, ADAMTX_GPIO_OE, ADAMTX_GPIO_STR, ADAMTX_GPIO_CLK};

static struct gpio* adamtx_gpios;

static struct matrix_ledpanel** adamtx_panels;

static uint32_t* framedata;
static uint32_t* paneldata;

#define ADAMTX_NUM_PANELS 2

static struct matrix_ledpanel adamtx_matrix_up = {
	.name = "Upper",
	.xres = 64,
	.yres = 32,
	.virtual_x = 64,
	.virtual_y = 0,
	.realx = 0,
	.realy = 0,
	.flip_y = 1,
	.flip_y = 0
};

static struct matrix_ledpanel adamtx_matrix_low = {
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

static struct hrtimer adamtx_frametimer;
static ktime_t adamtx_frameperiod;
static int adamtx_frametimer_enabled = 0;

static int __init adamtx_alloc_gpio(void)
{
	return 0;
	int i;
	adamtx_gpios = vmalloc(ADAMTX_NUM_GPIOS * sizeof(struct gpio));
	if(adamtx_gpios == NULL)
		return -ENOMEM;
	for(i = 0; i < ADAMTX_NUM_GPIOS; i++)
	{
		adamtx_gpios[i].gpio = adamtx_gpio_ids[i];
		adamtx_gpios[i].flags = GPIOF_OUT_INIT_LOW;
		adamtx_gpios[i].label = ADAMTX_NAME;
	}
	return gpio_request_array(adamtx_gpios, ADAMTX_NUM_GPIOS);
}

static int __exit adamtx_free_gpio(void)
{
	return 0;
	gpio_free_array(adamtx_gpios, ADAMTX_NUM_GPIOS);
	vfree(adamtx_gpios);
	return 0;
}

void adamtx_clock_out_row(uint32_t* data, int length)
{
	while(--length >= 0)
	{
		ADAMTX_GPIO_LO(ADAMTX_GPIO_CLK);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_R1, (data[length] >> 0) & 0b1);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_R2, (data[length] >> 1) & 0b1);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_G1, (data[length] >> 2) & 0b1);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_G2, (data[length] >> 3) & 0b1);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_B1, (data[length] >> 4) & 0b1);
		ADAMTX_GPIO_SET(ADAMTX_GPIO_B2, (data[length] >> 5) & 0b1);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
	}
	ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);
	ADAMTX_GPIO_LO(ADAMTX_GPIO_STR);
}

void adamtx_set_address(int address)
{
	ADAMTX_GPIO_SET(ADAMTX_GPIO_A, (address >> 0) & 0b1);
	ADAMTX_GPIO_SET(ADAMTX_GPIO_B, (address >> 1) & 0b1);
	ADAMTX_GPIO_SET(ADAMTX_GPIO_C, (address >> 2) & 0b1);
	ADAMTX_GPIO_SET(ADAMTX_GPIO_D, (address >> 3) & 0b1);
	ADAMTX_GPIO_SET(ADAMTX_GPIO_E, (address >> 4) & 0b1);
}

void remap_frame(struct matrix_ledpanel** panels, uint32_t* from, int width_from, int height_from, uint32_t* to, int width_to, int height_to)
{
	int i, j;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	for(i = 0; i < height_from; i++)
	{
		for(j = 0; j < width_from; j++)
		{
			panel = matrix_get_panel_at_real(panels, ADAMTX_NUM_PANELS, j, i);
			matrix_panel_get_position(&pos, panel, j, i);
			to[pos.y * width_to + pos.x] = from[i * width_from + j];
		}
	}
}

void prerender_frame_part(struct adamtx_frame* framepart)
{
	int i;
	int j;
	int k;
	uint32_t* frame = framepart->frame;
	int rows = framepart->height;
	int columns = framepart->width;
	int pwm_steps = 1 << framepart->pwm_bits;
	int vertical_offset = framepart->vertical_offset / 2;
	printk(KERN_INFO ADAMTX_NAME ": dimensions: [%d;%d]\n", columns, rows);
	printk(KERN_INFO ADAMTX_NAME ": vertical offset: %d\n", vertical_offset);
	uint32_t row[columns];
	for(i = vertical_offset; i < vertical_offset + framepart->rows / 2; i++)
	{
		int row1_base = i * columns;
		int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns * sizeof(uint32_t));
			for(k = 0; k < columns; k++)
			{
				printk(KERN_INFO ADAMTX_NAME ": [%d;%d]@%d offset1: %d offset2: %d", k, i, j, row1_base + k, row2_base + k);
				if(frame[row1_base + k] & 0xFF > j)
				{
					row[k] |= 0b1;
				}
				if(frame[row1_base + k] >> 8 & 0xFF > j)
				{
					row[k] |= 0b100;
				}
				if(frame[row1_base + k] >> 16 & 0xFF > j)
				{
					row[k] |= 0b10000;
				}
				if(frame[row2_base + k] & 0xFF > j)
				{
					row[k] |= 0b10;
				}
				if(frame[row2_base + k] >> 8 & 0xFF > j)
				{
					row[k] |= 0b1000;
				}
				if(frame[row2_base + k] >> 16 & 0xFF > j)
				{
					row[k] |= 0b100000;
				}
			}
			printk(KERN_INFO ADAMTX_NAME ": row: %d pwm:%d offset: %d address: 0x%x", i, j, i * pwm_steps * columns + j * columns, framepart->paneldata + i * pwm_steps * columns + j * columns);
			memcpy(framepart->paneldata + i * pwm_steps * columns + j * columns, row, columns * sizeof(uint32_t));
		}
	}
}

void show_frame(uint32_t* frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int pwm_steps = (1 << bits);
	for(i = 0; i < rows / 2; i++)
	{
		adamtx_set_address(i);
		for(j = 0; j < pwm_steps; j++)
		{
			adamtx_clock_out_row(frame + i * pwm_steps * columns + j * columns, columns);
		}
	}
}

void render_part(struct adamtx_frame* part)
{
	struct adamtx_frame* framepart = (struct adamtx_frame*)part;
	printk(KERN_INFO ADAMTX_NAME ": Segment: 0x%x Width: %d Height: %d\n", framepart->paneldata + framepart->paneloffset, framepart->width, framepart->rows);
	prerender_frame_part(framepart);
}

void process_frame(struct adamtx_processable_frame* frame)
{
	int i;

	int datalen = frame->rows * frame->columns * sizeof(uint32_t);

	printk(KERN_INFO ADAMTX_NAME ": allocation size: %d bytes\n", datalen);

	uint32_t* data = vmalloc(datalen);

	memset(data, 0, datalen);

	remap_frame(frame->panels, frame->frame, frame->width, frame->height, data, frame->columns, frame->rows);

	memset(frame->iodata, 0, (1 << frame->pwm_bits) * frame->columns * frame->rows * sizeof(uint32_t));

	struct adamtx_frame threadframe = {
		.width = frame->columns,
		.height = frame->rows,
		.vertical_offset = 0,
		.rows = frame->rows,
		.paneldata = frame->iodata,
		.paneloffset = 0,
		.frame = data,
		.frameoffset = 0,
		.pwm_bits = frame->pwm_bits
	};

	render_part(&threadframe);


/*	int rows_per_thread = rows / num_processing_threads;

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
*/
	vfree(data);
}

static enum hrtimer_restart draw_frame(struct hrtimer* timer)
{
	printk(KERN_INFO ADAMTX_NAME ": Draw frame\n");
	hrtimer_forward_now(timer, adamtx_frameperiod);
	if(!mutex_trylock(&adamtx_draw_mutex))
	{
		printk(KERN_WARNING ADAMTX_NAME ": Can't keep up. Frame not finished\n");
		return HRTIMER_RESTART;
	}
	show_frame(paneldata, ADAMTX_PWM_BITS, ADAMTX_ROWS, ADAMTX_COLUMNS);
	mutex_unlock(&adamtx_draw_mutex);
	return HRTIMER_RESTART;
}

static int __init adamtx_init(void)
{
	int i, j, ret;
	
	if((ret = adamtx_alloc_gpio()))
	{
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate gpios (%d)\n", ret);
		goto none_alloced;
	}

	adamtx_panels = vmalloc(ADAMTX_NUM_PANELS * sizeof(struct matrix_ledpanel*));
	if(adamtx_panels == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate panels (%d)\n", ret);
		goto gpio_alloced;
	}
	adamtx_panels[0] = &adamtx_matrix_up;
	adamtx_panels[1] = &adamtx_matrix_low;

	framedata = vmalloc(ADAMTX_REAL_HEIGHT * ADAMTX_REAL_WIDTH * sizeof(uint32_t));
	if(framedata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate frame memory (%d)\n", ret);
		goto panels_alloced;
	}
	paneldata = vmalloc((1 << ADAMTX_PWM_BITS) * ADAMTX_ROWS * ADAMTX_COLUMNS * sizeof(uint32_t));
	if(paneldata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate panel memory (%d)\n", ret);
		goto framedata_alloced;
	}

	for(i = 0; i < ADAMTX_REAL_HEIGHT; i++)
	{
		for(j = 0; j < ADAMTX_REAL_WIDTH; j++)
		{
			if(i == j)
				framedata[i * ADAMTX_REAL_WIDTH + j] = 255;
		}
	}

	struct adamtx_processable_frame frame = {
		.width = ADAMTX_REAL_WIDTH,
		.height = ADAMTX_REAL_HEIGHT,
		.columns = ADAMTX_COLUMNS,
		.rows = ADAMTX_ROWS,
		.pwm_bits = ADAMTX_PWM_BITS,
		.iodata = paneldata,
		.frame = framedata,
		.panels = adamtx_panels
	};
	

	process_frame(&frame);

//	adamtx_frameperiod = ktime_set(0, 1000000000UL / ADAMTX_RATE);
	adamtx_frameperiod = ktime_set(1, 0);
	hrtimer_init(&adamtx_frametimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	adamtx_frametimer.function = draw_frame;
	hrtimer_start(&adamtx_frametimer, adamtx_frameperiod, HRTIMER_MODE_REL);
	adamtx_frametimer_enabled = 1;

	printk(KERN_INFO ADAMTX_NAME ": initialized\n");
	return 0;

framedata_alloced:
	vfree(framedata);
panels_alloced:
	vfree(adamtx_panels);
gpio_alloced:
	adamtx_free_gpio();
none_alloced:
	return ret;
}

static void __exit adamtx_exit(void)
{
	if(adamtx_frametimer_enabled)
		hrtimer_cancel(&adamtx_frametimer);
	vfree(paneldata);
	vfree(framedata);
	vfree(adamtx_panels);
	adamtx_free_gpio();
	printk(KERN_INFO ADAMTX_NAME ": shutting down\n");
}

module_init(adamtx_init);
module_exit(adamtx_exit);
