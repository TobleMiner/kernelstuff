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
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include "matrix.h"
#include "adafruit-matrix.h"
#include "io.h"

#define ADAMTX_GPIO_HI(gpio) adamtx_gpio_set_bits((1 << gpio))
#define ADAMTX_GPIO_LO(gpio) adamtx_gpio_clr_bits((1 << gpio))
#define ADAMTX_GPIO_SET(gpio, state) (state ? ADAMTX_GPIO_HI(gpio) : ADAMTX_GPIO_LO(gpio))

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

static struct matrix_ledpanel** adamtx_panels;

static char* framedata;
static struct adamtx_panel_io* paneldata;

static struct adamtx_update_param adamtx_update_param;
struct task_struct* adamtx_update_thread;

static struct adamtx_draw_param adamtx_draw_param;
struct task_struct* adamtx_draw_thread;

struct task_struct* adamtx_perf_thread;
static int adamtx_do_perf = 0;

static uint32_t* adamtx_intermediate_frame;

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

static struct matrix_ledpanel adamtx_matrix_32x16 = {
	.name = "small",
	.xres = 32,
	.yres = 16,
	.virtual_x = 0,
	.virtual_y = 0,
	.realx = 0,
	.realy = 0,
	.flip_x = 1,
	.flip_y = 0
};

static struct hrtimer adamtx_frametimer;
static ktime_t adamtx_frameperiod;
static int adamtx_frametimer_enabled = 0;

static struct hrtimer adamtx_updatetimer;
static ktime_t adamtx_updateperiod;
static int adamtx_updatetimer_enabled = 0;

static DEFINE_SPINLOCK(adamtx_lock_draw);
static int adamtx_do_draw = 0;
static int adamtx_do_update = 0;

static struct hrtimer adamtx_perftimer;
static ktime_t adamtx_perfperiod;
static int adamtx_perftimer_enabled = 0;

void adamtx_clock_out_row(struct adamtx_panel_io* data, int length)
{
	while(--length >= 0)
	{
		adamtx_gpio_write_bits(((uint32_t*)data)[length]);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
	}
}

void adamtx_set_address(int i)
{
	struct adamtx_panel_io io;
	*((uint32_t*)(&io)) = (i << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
	io.E = i >> 4;
	adamtx_gpio_write_bits(*((uint32_t*)&io));
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
			panel = matrix_get_panel_at_real(panels, ADAMTX_NUM_PANELS, j, i);
			matrix_panel_get_position(&pos, panel, j, i);
			offset = i * width_from * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN;
			to[pos.y * width_to + pos.x] = from[offset] | from[offset + 1] << 8 | from[offset + 2] << 16;
		}
	}
}

void prerender_frame_part(struct adamtx_frame* framepart)
{
	int i, j, k, addr;
	uint32_t* frame = framepart->frame;
	int rows = framepart->height;
	int columns = framepart->width;
	int pwm_steps = framepart->pwm_bits;
	int vertical_offset = framepart->vertical_offset / 2;
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
				row[k].B1 = (frame[row1_base + k] & (1 << j)) > 0;
				row[k].G1 = ((frame[row1_base + k] >> 8) & (1 << j)) > 0;
				row[k].R1 = ((frame[row1_base + k] >> 16) & (1 << j)) > 0;
				row[k].B2 = (frame[row2_base + k] & (1 << j)) > 0;
				row[k].G2 = ((frame[row2_base + k] >> 8) & (1 << j)) > 0;
				row[k].R2 = ((frame[row2_base + k] >> 16) & (1 << j)) > 0;
				if(j == 0)
					addr = (i + 1) % (framepart->rows / 2);
				else
					addr = i;
				*((uint32_t*)(&row[k])) |= (addr << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
				row[k].E = addr >> 4;
			}
			memcpy(framepart->paneldata + i * pwm_steps * columns + j * columns, row, columns * sizeof(struct adamtx_panel_io));
		}
	}
}

void show_frame(struct adamtx_panel_io* frame, int bits, int rows, int columns)
{
	ADAMTX_GPIO_LO(ADAMTX_GPIO_OE);
	int i, j, k;
	int pwm_steps = bits;
	for(i = rows / 2 - 1; i >= 0; i--)
	{
		for(j = 0; j < pwm_steps; j++)
		{
			adamtx_clock_out_row(frame + i * pwm_steps * columns + j * columns, columns);
			ADAMTX_GPIO_HI(ADAMTX_GPIO_OE);
			ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);
			ADAMTX_GPIO_LO(ADAMTX_GPIO_STR);
			ADAMTX_GPIO_LO(ADAMTX_GPIO_OE);
			ndelay((1 << j) * ADAMTX_BCD_TIME_NS);
		}
	}
	ADAMTX_GPIO_HI(ADAMTX_GPIO_OE);
}

void render_part(struct adamtx_frame* part)
{
	struct adamtx_frame* framepart = (struct adamtx_frame*)part;
	prerender_frame_part(framepart);
}

int process_frame(struct adamtx_processable_frame* frame)
{
	int i;

/*	int datalen = frame->rows * frame->columns * sizeof(uint32_t);

//	printk(KERN_INFO ADAMTX_NAME ": allocation size: %d bytes\n", datalen);

	uint32_t* data = vmalloc(datalen);
	if(data == NULL)
		return -ENOMEM;
*/
	remap_frame(frame->panels, frame->frame, frame->width, frame->height, adamtx_intermediate_frame, frame->columns, frame->rows);

//	memset(frame->iodata, 0, frame->pwm_bits * frame->columns * frame->rows / 2 * sizeof(struct adamtx_panel_io));

	struct adamtx_frame threadframe = {
		.width = frame->columns,
		.height = frame->rows,
		.vertical_offset = 0,
		.rows = frame->rows,
		.paneldata = frame->iodata,
		.paneloffset = 0,
		.frame = adamtx_intermediate_frame,
		.frameoffset = 0,
		.pwm_bits = frame->pwm_bits
	};

	render_part(&threadframe);

//	vfree(data);
	return 0;
}

static unsigned long adamtx_draws = 0;
static unsigned long adamtx_draw_irqs = 0;
static unsigned long adamtx_draw_time = 0;

static int draw_frame(void* arg)
{
	unsigned long irqflags;
	struct timespec before;
	struct timespec after;
	struct adamtx_draw_param* param = (struct adamtx_draw_param*)arg;
	printk(KERN_INFO ADAMTX_NAME ": Draw spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop())
	{
		while(!adamtx_do_draw && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx_do_draw = 0;
		spin_lock_irqsave(&adamtx_lock_draw, irqflags);
		getnstimeofday(&before);
		show_frame(paneldata, ADAMTX_PWM_BITS, ADAMTX_ROWS, ADAMTX_COLUMNS);
		getnstimeofday(&after);
		adamtx_draw_time += (after.tv_sec - before.tv_sec) * 1000000000UL + (after.tv_nsec - before.tv_nsec);
		adamtx_draws++;
		spin_unlock_irqrestore(&adamtx_lock_draw, irqflags);
	}
	do_exit(0);	
}

static unsigned long adamtx_updates = 0;
static unsigned long adamtx_update_irqs = 0;
static unsigned long adamtx_update_time = 0;

static int update_frame(void* arg)
{
	int err;
	unsigned long irqflags;
	struct timespec before;
	struct timespec after;
	struct adamtx_update_param* param = (struct adamtx_update_param*)arg;
	printk(KERN_INFO ADAMTX_NAME ": Update spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop())
	{
		while(!adamtx_do_update && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx_do_update = 0;

		dummyfb_copy(framedata);

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

		spin_lock_irqsave(&adamtx_lock_draw, irqflags);
		getnstimeofday(&before);
		err = process_frame(&frame);
		getnstimeofday(&after);
		adamtx_update_time += (after.tv_sec - before.tv_sec) * 1000000000UL + (after.tv_nsec - before.tv_nsec);
		adamtx_updates++;
		spin_unlock_irqrestore(&adamtx_lock_draw, irqflags);
		if(err)
			do_exit(err);
	}
	do_exit(0);
}

static int show_perf(void* arg)
{
	long irqflags, perf_adamtx_updates, perf_adamtx_update_irqs, perf_adamtx_update_time;
	long perf_adamtx_draws, perf_adamtx_draw_irqs, perf_adamtx_draw_time;
	while(!kthread_should_stop())
    {
        while(!adamtx_do_perf && !kthread_should_stop())
            usleep_range(5000, 10000);
        if(kthread_should_stop())
            break;
		adamtx_do_perf = 0;

		spin_lock_irqsave(&adamtx_lock_draw, irqflags);

		perf_adamtx_updates = adamtx_updates;
		adamtx_updates = 0;
		perf_adamtx_update_irqs = adamtx_update_irqs;
		adamtx_update_irqs = 0;
		perf_adamtx_update_time = adamtx_update_time;
		adamtx_update_time = 0;

		perf_adamtx_draws = adamtx_draws;
		adamtx_draws = 0;
		perf_adamtx_draw_irqs = adamtx_draw_irqs;
		adamtx_draw_irqs = 0;
		perf_adamtx_draw_time = adamtx_draw_time;
		adamtx_draw_time = 0;

		spin_unlock_irqrestore(&adamtx_lock_draw, irqflags);
		printk(KERN_INFO ADAMTX_NAME ": %ld updates/s\t%ld irqs/s\t%lu ns/update", perf_adamtx_updates, perf_adamtx_update_irqs, perf_adamtx_updates != 0 ? perf_adamtx_update_time / perf_adamtx_updates : 0);
		printk(KERN_INFO ADAMTX_NAME ": %ld draws/s\t%ld irqs/s\t%lu ns/draw", perf_adamtx_draws, perf_adamtx_draw_irqs, perf_adamtx_draws != 0 ? perf_adamtx_draw_time / perf_adamtx_draws : 0);	
	}
}

static enum hrtimer_restart update_callback(struct hrtimer* timer)
{
	hrtimer_forward_now(timer, adamtx_frameperiod);
	adamtx_do_update = 1;
	adamtx_update_irqs++;
	return HRTIMER_RESTART;
}

static enum hrtimer_restart draw_callback(struct hrtimer* timer)
{
	hrtimer_forward_now(timer, adamtx_frameperiod);
	adamtx_do_draw = 1;
	adamtx_draw_irqs++;
	return HRTIMER_RESTART;
}

static enum hrtimer_restart perf_callback(struct hrtimer* timer)
{
	hrtimer_forward_now(timer, adamtx_perfperiod);
	adamtx_do_perf = 1;

	return HRTIMER_RESTART;
}

static void __init adamtx_init_gpio(void)
{
	adamtx_gpio_set_outputs((1 << ADAMTX_GPIO_R1) | (1 << ADAMTX_GPIO_R2) | (1 << ADAMTX_GPIO_G1) | (1 << ADAMTX_GPIO_G2) | (1 << ADAMTX_GPIO_B1) | (1 << ADAMTX_GPIO_B2) | (1 << ADAMTX_GPIO_A) | (1 << ADAMTX_GPIO_B) | (1 << ADAMTX_GPIO_C) | (1 << ADAMTX_GPIO_D) | (1 << ADAMTX_GPIO_E) | (1 << ADAMTX_GPIO_OE) | (1 << ADAMTX_GPIO_STR) | (1 << ADAMTX_GPIO_CLK));
}

static int adamtx_probe(struct platform_device *device)
{
	int i, j, ret, framesize;
	
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
//	adamtx_panels[0] = &adamtx_matrix_32x16;

	framesize = ADAMTX_REAL_HEIGHT * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN;
	if(dummyfb_get_fbsize() != framesize)
	{
        ret = -EINVAL;
        printk(KERN_WARNING ADAMTX_NAME ": size of framebuffer != framesize\n");
        goto framedata_alloced;
	}
	framedata = vzalloc(framesize);
	if(framedata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate frame memory (%d)\n", ret);
		goto panels_alloced;
	}
	paneldata = vmalloc(ADAMTX_PWM_BITS * ADAMTX_ROWS / 2 * ADAMTX_COLUMNS * sizeof(struct adamtx_panel_io));
	if(paneldata == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate panel memory (%d)\n", ret);
		goto framedata_alloced;
	}
	adamtx_intermediate_frame = vmalloc(ADAMTX_ROWS * ADAMTX_COLUMNS * sizeof(uint32_t));
	if(adamtx_intermediate_frame == NULL)
	{
        ret = -ENOMEM;
        printk(KERN_WARNING ADAMTX_NAME ": failed to allocate intermediate frame memory (%d)\n", ret);
        goto paneldata_alloced;
	}

	for(i = 0; i < ADAMTX_REAL_HEIGHT; i++)
	{
		for(j = 0; j < ADAMTX_REAL_WIDTH; j++)
		{
			if(i == j || i == ADAMTX_REAL_WIDTH - j - 1)
			{
				framedata[i * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN + 0] = 4;
				framedata[i * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN + 1] = 8;
				framedata[i * ADAMTX_REAL_WIDTH * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN + 2] = 2;
			}
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

	adamtx_update_param.rate = ADAMTX_FBRATE;
	adamtx_update_thread = kthread_create(update_frame, &adamtx_update_param, "adamtx_update");
	kthread_bind(adamtx_update_thread, 2);
	if(IS_ERR(adamtx_update_thread))
	{
		ret = PTR_ERR(adamtx_update_thread);
		printk(KERN_WARNING ADAMTX_NAME ": failed to create update thread (%d)\n", ret);
		goto interframe_alloced;
	}
	wake_up_process(adamtx_update_thread);

	adamtx_draw_param.rate = ADAMTX_RATE;
	adamtx_draw_thread = kthread_create(draw_frame, &adamtx_draw_param, "adamtx_draw@");
	kthread_bind(adamtx_draw_thread, 3);
	if(IS_ERR(adamtx_draw_thread))
	{
		ret = PTR_ERR(adamtx_draw_thread);
		printk(KERN_WARNING ADAMTX_NAME ": failed to create draw thread (%d)\n", ret);
		goto interframe_alloced;
	}
	wake_up_process(adamtx_draw_thread);

	adamtx_perf_thread = kthread_create(show_perf, NULL, "adamtx_perf");
	kthread_bind(adamtx_perf_thread, 1);
	if(IS_ERR(adamtx_perf_thread))
	{
		ret = PTR_ERR(adamtx_perf_thread);
		printk(KERN_WARNING ADAMTX_NAME ": failed to create perf thread (%d)\n", ret);
		goto interframe_alloced;
	}
	wake_up_process(adamtx_perf_thread);

	adamtx_frameperiod = ktime_set(0, 1000000000UL / ADAMTX_RATE);
	hrtimer_init(&adamtx_frametimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx_frametimer.function = draw_callback;
	hrtimer_start(&adamtx_frametimer, adamtx_frameperiod, HRTIMER_MODE_REL);
	adamtx_frametimer_enabled = 1;

	adamtx_updateperiod = ktime_set(0, 1000000000UL / ADAMTX_FBRATE);
	hrtimer_init(&adamtx_updatetimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx_updatetimer.function = update_callback;
	hrtimer_start(&adamtx_updatetimer, adamtx_updateperiod, HRTIMER_MODE_REL);
	adamtx_updatetimer_enabled = 1;

	adamtx_perfperiod = ktime_set(0, 1000000000UL);
	hrtimer_init(&adamtx_perftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx_perftimer.function = perf_callback;
	hrtimer_start(&adamtx_perftimer, adamtx_perfperiod, HRTIMER_MODE_REL);
	adamtx_perftimer_enabled = 1;

	printk(KERN_INFO ADAMTX_NAME ": initialized\n");
	return 0;

interframe_alloced:
	vfree(adamtx_intermediate_frame);
paneldata_alloced:
	vfree(paneldata);
framedata_alloced:
	vfree(framedata);
panels_alloced:
	vfree(adamtx_panels);
gpio_alloced:
	adamtx_gpio_free();
none_alloced:
	return ret;
}

static int adamtx_remove(struct platform_device *device)
{
	if(adamtx_updatetimer_enabled)
		hrtimer_cancel(&adamtx_updatetimer);
	if(adamtx_frametimer_enabled)
		hrtimer_cancel(&adamtx_frametimer);
	if(adamtx_perftimer_enabled)
		hrtimer_cancel(&adamtx_perftimer);
	kthread_stop(adamtx_draw_thread);
	kthread_stop(adamtx_update_thread);
	kthread_stop(adamtx_perf_thread);
	vfree(adamtx_intermediate_frame);
	vfree(paneldata);
	vfree(framedata);
	vfree(adamtx_panels);
	adamtx_gpio_free();
	printk(KERN_INFO ADAMTX_NAME ": shutting down\n");
	return 0;
}

static struct platform_driver adamtx_driver = {
	.probe = adamtx_probe,
	.remove = adamtx_remove,
	.driver = {
		.name = ADAMTX_NAME
	}
};

static struct platform_device* adamtx_dev;

static int __init adamtx_init(void)
{
	int ret;
	ret = platform_driver_register(&adamtx_driver);
	if(ret)
		goto none_allocated;
	adamtx_dev = platform_device_alloc(ADAMTX_NAME, 0);
	if(adamtx_dev == NULL)
	{
		ret = -ENOMEM;
		goto driver_registered;
	}
	ret = platform_device_add(adamtx_dev);
	if(ret)
		goto dev_allocated;
	return 0;

dev_allocated:
	platform_device_put(adamtx_dev);
driver_registered:
	platform_driver_unregister(&adamtx_driver);
none_allocated:
	return ret;
}

static void __exit adamtx_exit(void)
{
	platform_device_unregister(adamtx_dev);
	platform_driver_unregister(&adamtx_driver);
}

module_init(adamtx_init);
module_exit(adamtx_exit);

