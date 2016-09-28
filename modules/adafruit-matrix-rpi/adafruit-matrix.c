#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/timekeeping.h>

static struct timespec last_call_time;

#include "matrix.h"
#include "adafruit-matrix.h"
#include "io.h"

#define ADAMTX_GPIO_HI(gpio) adamtx_gpio_set_bits((1 << gpio))
#define ADAMTX_GPIO_LO(gpio) adamtx_gpio_clr_bits((1 << gpio))
#define ADAMTX_GPIO_SET(gpio, state) (state ? ADAMTX_GPIO_HI(gpio) : ADAMTX_GPIO_LO(gpio))

/*
#define ADAMTX_GPIO_HI(gpio) asm("nop")
#define ADAMTX_GPIO_LO(gpio) asm("nop")
#define ADAMTX_GPIO_SET(gpio, state) asm("nop")
*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

static DEFINE_MUTEX(adamtx_draw_mutex);

static struct matrix_ledpanel** adamtx_panels;

static char* framedata;
static struct adamtx_panel_io* paneldata;

#define ADAMTX_NUM_PANELS 2

static struct matrix_ledpanel adamtx_matrix_up = {
	.name = "Upper",
	.xres = 64,
	.yres = 32,
	.virtual_x = 64,
	.virtual_y = 0,
	.realx = 0,
	.realy = 0,
	.flip_x = 0,
	.flip_y = 1
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

void adamtx_clock_out_row(struct adamtx_panel_io* data, int length)
{
	while(--length >= 0)
	{
		adamtx_gpio_write_bits(((uint32_t*)data)[length]);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
	}
	ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);
	ADAMTX_GPIO_LO(ADAMTX_GPIO_STR);
}

void remap_frame(struct matrix_ledpanel** panels, char* from, int width_from, int height_from, uint32_t* to, int width_to, int height_to)
{
	int i, j;
	off_t offset;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	for(i = 0; i < height_from; i++)
	{
		for(j = 0; j < width_from; j++)
		{
//			printk(KERN_INFO ADAMTX_NAME ": [%d,%d] color: 0x%x", j, i, from[i * width_from + j]);
			panel = matrix_get_panel_at_real(panels, ADAMTX_NUM_PANELS, j, i);
			matrix_panel_get_position(&pos, panel, j, i);
			offset = i * width_from * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN;
			to[pos.y * width_to + pos.x] = from[offset] | from[offset + 1] << 8 | from[offset + 2] << 16;
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
	struct adamtx_panel_io row[columns];
	for(i = vertical_offset; i < vertical_offset + framepart->rows / 2; i++)
	{
		int row1_base = i * columns;
		int row2_base = (rows / 2 + i) * columns;
		for(j = 0; j < pwm_steps; j++)
		{
			memset(row, 0, columns * sizeof(struct adamtx_panel_io));
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
//				if(row[k].G1 || row[k].G2 || row[k].B1 || row[k].B2)
//					printk(KERN_INFO ADAMTX_NAME ": [%d;%d]@%d offset1: %d offset2: %d raw value1:0x%x raw value2:0x%x\n", k, i, j, row1_base + k, row2_base + k, frame[row1_base + k], frame[row2_base + k]);
				*((uint32_t*)(&row[k])) |= (i << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
				row[k].E = i >> 4;
			}
//			printk(KERN_INFO ADAMTX_NAME ": row: %d pwm:%d offset: %d address: 0x%x", i, j, i * pwm_steps * columns + j * columns, framepart->paneldata + i * pwm_steps * columns + j * columns);
			memcpy(framepart->paneldata + i * pwm_steps * columns + j * columns, row, columns * sizeof(struct adamtx_panel_io));
		}
	}
}

void show_frame(struct adamtx_panel_io* frame, int bits, int rows, int columns)
{
	int i;
	int j;
	int pwm_steps = (1 << bits);
	for(i = 0; i < rows / 2; i++)
	{
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

int process_frame(struct adamtx_processable_frame* frame)
{
	int i;

	int datalen = frame->rows * frame->columns * sizeof(uint32_t);

	printk(KERN_INFO ADAMTX_NAME ": allocation size: %d bytes\n", datalen);

	uint32_t* data = vmalloc(datalen);
	if(data == NULL)
		return -ENOMEM;

	memset(data, 0, datalen);

	remap_frame(frame->panels, frame->frame, frame->width, frame->height, data, frame->columns, frame->rows);

	memset(frame->iodata, 0, (1 << frame->pwm_bits) * frame->columns * frame->rows * sizeof(struct adamtx_panel_io));

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
	return 0;
}

static enum hrtimer_restart draw_frame(struct hrtimer* timer)
{
	//printk(KERN_INFO ADAMTX_NAME ": Draw frame\n");
/*	struct timespec now;
	getnstimeofday(&now);
	printk("%lu ns since last timer interrupt\n", (now.tv_sec - last_call_time.tv_sec) * 1000000000 + (now.tv_nsec - last_call_time.tv_nsec));
	getnstimeofday(&now);
*/	hrtimer_forward_now(timer, adamtx_frameperiod);
	if(!mutex_trylock(&adamtx_draw_mutex))
	{
		printk(KERN_WARNING ADAMTX_NAME ": Can't keep up. Frame not finished\n");
		return HRTIMER_RESTART;
	}
	show_frame(paneldata, ADAMTX_PWM_BITS, ADAMTX_ROWS, ADAMTX_COLUMNS);
/*	struct timespec now_after;
	getnstimeofday(&now_after);
	printk("Showing frame took %lu ns\n", (now_after.tv_sec - now.tv_sec) * 1000000000 + (now_after.tv_nsec - now.tv_nsec));
	getnstimeofday(&now);
	memcpy(&last_call_time, &now, sizeof(struct timespec));*/
	mutex_unlock(&adamtx_draw_mutex);
	return HRTIMER_RESTART;
}

static void __init adamtx_init_gpio(void)
{
	adamtx_gpio_set_outputs((1 << ADAMTX_GPIO_R1) | (1 << ADAMTX_GPIO_R2) | (1 << ADAMTX_GPIO_G1) | (1 << ADAMTX_GPIO_G2) | (1 << ADAMTX_GPIO_B1) | (1 << ADAMTX_GPIO_B2) | (1 << ADAMTX_GPIO_A) | (1 << ADAMTX_GPIO_B) | (1 << ADAMTX_GPIO_C) | (1 << ADAMTX_GPIO_D) | (1 << ADAMTX_GPIO_E) | (1 << ADAMTX_GPIO_OE) | (1 << ADAMTX_GPIO_STR) | (1 << ADAMTX_GPIO_CLK));
}

static int __init adamtx_init(void)
{
	int i, j, k, ret;
	
	if((ret = adamtx_gpio_alloc()))
	{
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate gpios (%d)\n", ret);
		goto none_alloced;
	}
	adamtx_init_gpio();

	adamtx_panels = vmalloc(ADAMTX_NUM_PANELS * sizeof(struct matrix_ledpanel*));
	if(adamtx_panels == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate panels (%d)\n", ret);
		goto gpio_alloced;
	}
	adamtx_panels[0] = &adamtx_matrix_up;
	adamtx_panels[1] = &adamtx_matrix_low;

	framedata = vmalloc(ADAMTX_REAL_HEIGHT * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN);
	if(framedata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate frame memory (%d)\n", ret);
		goto panels_alloced;
	}
	paneldata = vmalloc((1 << ADAMTX_PWM_BITS) * ADAMTX_ROWS * ADAMTX_COLUMNS * sizeof(struct adamtx_panel_io));
	if(paneldata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate panel memory (%d)\n", ret);
		goto framedata_alloced;
	}

	memset(framedata, 0, ADAMTX_REAL_HEIGHT * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN);
	for(i = 0; i < ADAMTX_REAL_HEIGHT; i++)
	{
		for(j = 0; j < ADAMTX_REAL_WIDTH; j++)
		{
			if(i == j)
				framedata[i * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN] = 255;
			if(i == ADAMTX_REAL_WIDTH - j - 1)
				framedata[i * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN + 1] = 255;
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

/*	for(i = 0; i < ADAMTX_ROWS / 2; i++)
	{
		for(j = 0; j < ADAMTX_COLUMNS; j++)
		{
			for(k = 0; k < (1 << ADAMTX_PWM_BITS); k++)
			{
				struct adamtx_panel_io cio = paneldata[i * (1 << ADAMTX_PWM_BITS) * ADAMTX_COLUMNS + j * (1 << ADAMTX_PWM_BITS) + k];
				if(cio.R1 || cio.G1 || cio.B1)
					printk(KERN_INFO ADAMTX_NAME " [%d;%d]@%d: Row select 1 [R:%d G:%d B:%d]\n", j, i, k, cio.R1, cio.G1, cio.B1);
				if(cio.R2 || cio.G2 || cio.B2)
					printk(KERN_INFO ADAMTX_NAME " [%d;%d]@%d: Row select 2 [R:%d G:%d B:%d]\n", j, i + ADAMTX_ROWS / 2, k, cio.R1, cio.G1, cio.B1);
			}
		}
	}
*/
	printk(KERN_INFO ADAMTX_NAME ": Update spacing %lu ns\n", 1000000000UL / ADAMTX_RATE);

	adamtx_frameperiod = ktime_set(0, 1000000000UL / ADAMTX_RATE);
	hrtimer_init(&adamtx_frametimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
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
	adamtx_gpio_free();
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
	adamtx_gpio_free();
	printk(KERN_INFO ADAMTX_NAME ": shutting down\n");
}

module_init(adamtx_init);
module_exit(adamtx_exit);
