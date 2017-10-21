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
#include <linux/list.h>
#include <linux/of.h>
#include <linux/atomic.h>
#include <linux/dmaengine.h>

#include "matrix.h"
#include "adafruit_matrix.h"
#include "io.h"

#include "../dummyfb/dummyfb.h"

#define ADAMTX_GPIO_HI(gpio) adamtx_gpio_set_bits((1 << gpio))
#define ADAMTX_GPIO_LO(gpio) adamtx_gpio_clr_bits((1 << gpio))
#define ADAMTX_GPIO_SET(gpio, state) (state ? ADAMTX_GPIO_HI(gpio) : ADAMTX_GPIO_LO(gpio))

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

static uint8_t gamma_table[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };


void adamtx_clock_out_row(struct adamtx_panel_io* data, int length)
{
	while(--length >= 0)
	{
		adamtx_gpio_write_bits(((uint32_t*)data)[length]);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
//		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
	}
}

void adamtx_set_address(int i)
{
	struct adamtx_panel_io io;
	*((uint32_t*)(&io)) = (i << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
	io.E = i >> 4;
	adamtx_gpio_write_bits(*((uint32_t*)&io));
}

void remap_frame(struct adamtx_remap_frame* frame)
{
	int line, column;
	off_t offset;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	struct matrix_pixel* pixel;
	struct matrix_size* real_size = frame->real_size;
	struct matrix_size* virtual_size = frame->virtual_size;
	for(line = frame->offset; line < frame->offset + frame->rows; line++) {
		for(column = 0; column < real_size->width; column++) {
			panel = matrix_get_panel_at_real(frame->panels, column, line);
			if(!panel)
				continue;

			matrix_panel_get_position(&pos, panel, column, line);
			offset = line * real_size->width * ADAMTX_PIX_LEN + column * ADAMTX_PIX_LEN;
			pixel = &frame->dst[pos.y * virtual_size->width + pos.x];
			pixel->chains[panel->chain] = gamma_table[(unsigned char)frame->src[offset]] |
				gamma_table[(unsigned char)frame->src[offset + 1]] << 8 |
				gamma_table[(unsigned char)frame->src[offset + 2]] << 16;
		}
	}
}

void prerender_frame(struct adamtx_prerender_frame* frame)
{
	int line, j, col, addr;
	struct matrix_pixel* render_frame = frame->intermediate_frame;
	int rows = frame->virtual_size->height;
	int columns = frame->virtual_size->width;
	int pwm_steps = frame->pwm_bits;
	struct adamtx_panel_io* row;
	int vertical_offset = frame->offset / 2;
	line = vertical_offset;
	for(line = vertical_offset; line < vertical_offset + frame->rows / 2; line++) {
		int row1_base = line * columns;
		int row2_base = (rows / 2 + line) * columns;
		for(j = 0; j < pwm_steps; j++) {
			row = frame->iodata + line * pwm_steps * columns + j * columns;
			for(col = 0; col < columns; col++) {
				if(frame->enabled_chains->chain0) {
					row[col].C0_B1 = (render_frame[row1_base + col].chains[0] & (1 << j)) > 0;
					row[col].C0_G1 = ((render_frame[row1_base + col].chains[0] >> 8) & (1 << j)) > 0;
					row[col].C0_R1 = ((render_frame[row1_base + col].chains[0] >> 16) & (1 << j)) > 0;
					row[col].C0_B2 = (render_frame[row2_base + col].chains[0] & (1 << j)) > 0;
					row[col].C0_G2 = ((render_frame[row2_base + col].chains[0] >> 8) & (1 << j)) > 0;
					row[col].C0_R2 = ((render_frame[row2_base + col].chains[0] >> 16) & (1 << j)) > 0;
				}
				if(frame->enabled_chains->chain1) {
					row[col].C1_B1 = (render_frame[row1_base + col].chains[1] & (1 << j)) > 0;
					row[col].C1_G1 = ((render_frame[row1_base + col].chains[1] >> 8) & (1 << j)) > 0;
					row[col].C1_R1 = ((render_frame[row1_base + col].chains[1] >> 16) & (1 << j)) > 0;
					row[col].C1_B2 = (render_frame[row2_base + col].chains[1] & (1 << j)) > 0;
					row[col].C1_G2 = ((render_frame[row2_base + col].chains[1] >> 8) & (1 << j)) > 0;
					row[col].C1_R2 = ((render_frame[row2_base + col].chains[1] >> 16) & (1 << j)) > 0;
				}
				if(frame->enabled_chains->chain2) {
					row[col].C2_B1 = (render_frame[row1_base + col].chains[2] & (1 << j)) > 0;
					row[col].C2_G1 = ((render_frame[row1_base + col].chains[2] >> 8) & (1 << j)) > 0;
					row[col].C2_R1 = ((render_frame[row1_base + col].chains[2] >> 16) & (1 << j)) > 0;
					row[col].C2_B2 = (render_frame[row2_base + col].chains[2] & (1 << j)) > 0;
					row[col].C2_G2 = ((render_frame[row2_base + col].chains[2] >> 8) & (1 << j)) > 0;
					row[col].C2_R2 = ((render_frame[row2_base + col].chains[2] >> 16) & (1 << j)) > 0;
				}
				if(j == 0)
					addr = (line + 1) % (rows / 2);
				else
					addr = line;
				*((uint32_t*)(&row[col])) |= (addr << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
				row[col].E = addr >> 4;
			}
		}
	}
}


void show_frame(struct adamtx* adamtx)
{
	int i, j, remap_line = 0, prerender_line = 0;
	struct adamtx_panel_io* paneldata = adamtx->paneldata_out;
	int pwm_steps = ADAMTX_PWM_BITS;
	int rows = adamtx->virtual_size.height;
	int columns = adamtx->virtual_size.width;
	struct timespec last, now;
	unsigned long rem_delay, clock_out_delay, last_delay, bcd_time = adamtx->current_bcd_base_time;

	struct adamtx_remap_frame adamtx_remap_frame = {
		.real_size = &adamtx->real_size,
		.virtual_size = &adamtx->virtual_size,
		.offset = 0,
		.rows = 1,
		.pwm_bits = pwm_steps,
		.panels = &adamtx->panels,
		.src = adamtx->framedata,
		.dst = adamtx->intermediate_frame
	};

	struct adamtx_prerender_frame adamtx_prerender_frame = {
		.virtual_size = &adamtx->virtual_size,
		.offset = 0,
		.rows = 2,
		.pwm_bits = pwm_steps,
		.iodata = adamtx->paneldata,
		.enabled_chains = &adamtx->enabled_chains,
		.intermediate_frame = adamtx->intermediate_frame
	};

	ADAMTX_GPIO_LO(ADAMTX_GPIO_OE);
	for(i = rows / 2 - 1; i >= 0; i--)
	{
		for(j = 0; j < pwm_steps; j++)
		{
			ADAMTX_GPIO_HI(ADAMTX_GPIO_OE);
			getnstimeofday(&last);
			adamtx_clock_out_row(paneldata + i * pwm_steps * columns + j * columns, columns);
			if(!j) {
				adamtx_set_address(i);
			}
			ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);
			ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);
			ADAMTX_GPIO_HI(ADAMTX_GPIO_STR);

			ADAMTX_GPIO_LO(ADAMTX_GPIO_STR);
			ADAMTX_GPIO_LO(ADAMTX_GPIO_OE);

			getnstimeofday(&now);
			rem_delay = (1 << j) * bcd_time;
			clock_out_delay = ADAMTX_TIMESPEC_DIFF(now, last);

			if(clock_out_delay < rem_delay) {
				if((clock_out_delay < rem_delay) && !j)
					adamtx->current_bcd_base_time = bcd_time - 50;

				rem_delay -= clock_out_delay;

				while(remap_line < adamtx_remap_frame.real_size->height && rem_delay > adamtx->update_remap_ns_per_line * 3 / 2) {
					adamtx_remap_frame.offset = remap_line;
					remap_frame(&adamtx_remap_frame);
					remap_line++;
					getnstimeofday(&now);
					last_delay = ADAMTX_TIMESPEC_DIFF(now, last);
					if(rem_delay >= last_delay)
						rem_delay -= last_delay;
					adamtx->update_remap_ns_per_line = adamtx->update_remap_ns_per_line / 2 + last_delay / 2;
					getnstimeofday(&last);
				}
				if(remap_line >= adamtx_remap_frame.real_size->height) {
					while(prerender_line < adamtx_prerender_frame.virtual_size->height && rem_delay > adamtx->update_prerender_ns_per_line * 3 / 2) {
						adamtx_prerender_frame.offset = prerender_line;
						prerender_frame(&adamtx_prerender_frame);
						prerender_line += 2;
						getnstimeofday(&now);
						last_delay = ADAMTX_TIMESPEC_DIFF(now, last);
						if(rem_delay >= last_delay)
							rem_delay -= last_delay;
						adamtx->update_prerender_ns_per_line = adamtx->update_prerender_ns_per_line / 2 + last_delay / 2;
						getnstimeofday(&last);
					}
				}
				ndelay(rem_delay);


			} else if(clock_out_delay > rem_delay) {
					adamtx->current_bcd_base_time = bcd_time + 50;
			}
		}
	}
	adamtx->paneldata = xchg(&adamtx->paneldata_out, adamtx->paneldata);
	ADAMTX_GPIO_HI(ADAMTX_GPIO_OE);
}

static int draw_frame(void* arg)
{
	unsigned long irqflags;
	struct timespec before, after;
	struct adamtx_draw_param* param = (struct adamtx_draw_param*)arg;
	struct adamtx* adamtx = container_of(param, struct adamtx, draw_param);

	printk(KERN_INFO ADAMTX_NAME ": Draw spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop()) {
		while(!adamtx->do_draw && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx->do_draw = false;
		spin_lock_irqsave(&adamtx->lock_draw, irqflags);
		getnstimeofday(&before);
		show_frame(adamtx);
		getnstimeofday(&after);
		adamtx->draw_time += ADAMTX_TIMESPEC_DIFF(after, before);
		adamtx->draws++;
		spin_unlock_irqrestore(&adamtx->lock_draw, irqflags);
		yield();
	}
	do_exit(0);
}

static int update_frame(void* arg)
{
	struct timespec before;
	struct timespec after;
	struct adamtx_update_param* param = (struct adamtx_update_param*)arg;
	struct adamtx* adamtx = container_of(param, struct adamtx, update_param);

	printk(KERN_INFO ADAMTX_NAME ": Update spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop()) {
		while(!adamtx->do_update && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx->do_update = false;

		getnstimeofday(&before);
		dummyfb_copy(adamtx->framedata, adamtx->dummyfb);
		getnstimeofday(&after);
		adamtx->update_time += ADAMTX_TIMESPEC_DIFF(after, before);
		adamtx->updates++;

		yield();
	}

	do_exit(0);
}

static int show_perf(void* arg)
{
	unsigned long irqflags;
	long perf_adamtx_updates, perf_adamtx_update_irqs, perf_adamtx_update_time;
	long perf_adamtx_draws, perf_adamtx_draw_irqs, perf_adamtx_draw_time;
	struct adamtx* adamtx = arg;

	while(!kthread_should_stop())
	{
		while(!adamtx->do_perf && !kthread_should_stop())
		usleep_range(5000, 10000);
		if(kthread_should_stop())
			break;
		adamtx->do_perf = false;

		spin_lock_irqsave(&adamtx->lock_draw, irqflags);

		perf_adamtx_updates = adamtx->updates;
		adamtx->updates = 0;
		perf_adamtx_update_irqs = adamtx->update_irqs;
		adamtx->update_irqs = 0;
		perf_adamtx_update_time = adamtx->update_time;
		adamtx->update_time = 0;

		perf_adamtx_draws = adamtx->draws;
		adamtx->draws = 0;
		perf_adamtx_draw_irqs = adamtx->draw_irqs;
		adamtx->draw_irqs = 0;
		perf_adamtx_draw_time = adamtx->draw_time;
		adamtx->draw_time = 0;

		spin_unlock_irqrestore(&adamtx->lock_draw, irqflags);
		printk(KERN_INFO ADAMTX_NAME ": %ld updates/s\t%ld irqs/s\t%lu ns/update\n", perf_adamtx_updates, perf_adamtx_update_irqs, perf_adamtx_updates != 0 ? perf_adamtx_update_time / perf_adamtx_updates : 0);
		printk(KERN_INFO ADAMTX_NAME ": %ld draws/s\t%ld irqs/s\t%lu ns/draw\n", perf_adamtx_draws, perf_adamtx_draw_irqs, perf_adamtx_draws != 0 ? perf_adamtx_draw_time / perf_adamtx_draws : 0);
		printk(KERN_INFO ADAMTX_NAME ": update_remap_ns_per_line: %ld ns\tupdate_prerender_ns_per_line: %ld ns\n", adamtx->update_remap_ns_per_line, adamtx->update_prerender_ns_per_line);
		yield();
	}

	do_exit(0);
}

static enum hrtimer_restart update_callback(struct hrtimer* timer)
{
	struct adamtx* adamtx = container_of(timer, struct adamtx, updatetimer);

	hrtimer_forward_now(timer, adamtx->updateperiod);
	adamtx->do_update = true;
	adamtx->update_irqs++;
	return HRTIMER_RESTART;
}

static enum hrtimer_restart draw_callback(struct hrtimer* timer)
{
	struct adamtx* adamtx = container_of(timer, struct adamtx, frametimer);

	hrtimer_forward_now(timer, adamtx->frameperiod);
	adamtx->do_draw = true;
	adamtx->draw_irqs++;
	return HRTIMER_RESTART;
}

static enum hrtimer_restart perf_callback(struct hrtimer* timer)
{
	struct adamtx* adamtx = container_of(timer, struct adamtx, perftimer);

	hrtimer_forward_now(timer, adamtx->perfperiod);
	adamtx->do_perf = true;

	return HRTIMER_RESTART;
}

static int adamtx_of_get_int(int* dest, struct device_node* of_node, const char* name)
{
	int err = -ENOENT;
	const void* of_prop;
	if((of_prop = of_get_property(of_node, name, NULL))) {
		err = 0;
		*dest = be32_to_cpup(of_prop);
	}
	return err;
}

static int adamtx_of_get_int_default(struct device_node* of_node, const char* name, int def) {
	adamtx_of_get_int(&def, of_node, name);
	return def;
}

static int adamtx_of_get_int_range(int* dest, struct device_node* of_node, const char* name, int def, int min, int max) {
	int value = adamtx_of_get_int_default(of_node, name, def);
	if(value > max || value < min)
		return -EINVAL;
	*dest = value;
	return 0;
}

static void free_panels(struct adamtx* adamtx) {
	struct matrix_ledpanel *panel, *panel_next;

	list_for_each_entry_safe(panel, panel_next, &adamtx->panels, list) {
		list_del(&panel->list);
		vfree(panel);
	}
}

static int adamtx_parse_device_tree(struct device* dev, struct adamtx* adamtx) {
	int err = 0, adamtx_num_panels = 0;
	struct device_node *panel_node, *dev_of_node = dev->of_node;
	struct matrix_ledpanel *panel;

	if(!dev_of_node)
		goto exit_err;

	if((err = adamtx_of_get_int_range(&adamtx->rate, dev_of_node, "adamtx-rate", 60, 0, INT_MAX))) {
		dev_err(dev, "Invalid refresh rate. Must be in range from 0 to INT_MAX\n");
		goto exit_err;
	}

	if((err = adamtx_of_get_int_range(&adamtx->fb_rate, dev_of_node, "adamtx-fb-rate", 60, 0, INT_MAX))) {
		dev_err(dev, "Invalid framebuffer poll rate. Must be in range from 0 to INT_MAX\n");
		goto exit_err;
	}

	adamtx->draw_thread_bind = !adamtx_of_get_int(&adamtx->draw_thread_cpu, dev_of_node, "adamtx-bind-draw");
	adamtx->update_thread_bind = !adamtx_of_get_int(&adamtx->update_thread_cpu, dev_of_node, "adamtx-bind-update");

	adamtx->enable_dma = !!adamtx_of_get_int_default(dev_of_node, "adamtx-dma", 1);

	dev_info(dev, "Refresh rate: %d Hz, FB poll rate %d Hz, DMA: %d\n", adamtx->rate, adamtx->fb_rate, adamtx->enable_dma);

	for(panel_node = of_get_next_child(dev_of_node, NULL); panel_node; panel_node = of_get_next_child(dev_of_node, panel_node)) {
		panel = vzalloc(sizeof(struct matrix_ledpanel));
		if(!panel) {
			dev_err(dev, "Failed to allocate panel struct\n");
			err = -ENOMEM;
			goto exit_panel_alloc;
		}

		list_add(&panel->list, &adamtx->panels);

		if((err = adamtx_of_get_int_range(&panel->xres, panel_node, "adamtx-xres", 64, 1, INT_MAX))) {
			dev_err(dev, "Invalid horizontal panel resolution. Must be in range from 1 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->yres, panel_node, "adamtx-yres", 32, 1, INT_MAX))) {
			dev_err(dev, "Invalid vertical panel resolution. Must be in range from 1 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->virtual_x, panel_node, "adamtx-virtual-x", 0, 0, INT_MAX))) {
			dev_err(dev, "Invalid virtual horizontal panel offset. Must be in range from 0 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->virtual_y, panel_node, "adamtx-virtual-y", 0, 0, INT_MAX))) {
			dev_err(dev, "Invalid virtual vertical panel offset. Must be in range from 0 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->realx, panel_node, "adamtx-real-x", 0, 0, INT_MAX))) {
			dev_err(dev, "Invalid horizontal panel offset. Must be in range from 0 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->realy, panel_node, "adamtx-real-y", 0, 0, INT_MAX))) {
			dev_err(dev, "Invalid vertical panel offset. Must be in range from 0 to INT_MAX\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->flip_x, panel_node, "adamtx-flip-x", 0, 0, 1))) {
			dev_err(dev, "Invalid horizontal mirror flag. Must be either 0 or 1\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->flip_y, panel_node, "adamtx-flip-y", 0, 0, 1))) {
			dev_err(dev, "Invalid vertical mirror flag. Must be either 0 or 1\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->rotate, panel_node, "adamtx-rotate", 0, 0, 1))) {
			dev_err(dev, "Invalid rotation flag. Must be either 0 or 1\n");
			goto exit_panel_alloc;
		}

		if((err = adamtx_of_get_int_range(&panel->chain, panel_node, "adamtx-chain", 0, 0, 2))) {
			dev_err(dev, "Invalid chain. Must be in range from 0 to 2\n");
			goto exit_panel_alloc;
		}

		switch(panel->chain) {
			case(0):
				adamtx->enabled_chains.chain0 = 1;
				break;
			case(1):
				adamtx->enabled_chains.chain1 = 1;
				break;
			case(2):
				adamtx->enabled_chains.chain2 = 1;
		}

		dev_info(dev, "Panel %d: Resolution: %d px x %d px, Physical position: (%d, %d), Virtual position: (%d, %d), Flip x: %d, Flip y: %d, Rotate: %d, Chain %d",
				adamtx_num_panels, panel->xres, panel->yres, panel->realx, panel->realy, panel->virtual_x, panel->virtual_y, panel->flip_x, panel->flip_y, panel->rotate, panel->chain);

		adamtx_num_panels++;

	}

	return 0;

exit_panel_alloc:
	free_panels(adamtx);

exit_err:
	return err;
}

static void adamtx_init_gpio(struct adamtx* adamtx)
{
	uint32_t gpios = (1 << ADAMTX_GPIO_A) | (1 << ADAMTX_GPIO_B) | (1 << ADAMTX_GPIO_C) | (1 << ADAMTX_GPIO_D) | (1 << ADAMTX_GPIO_E) | (1 << ADAMTX_GPIO_OE) | (1 << ADAMTX_GPIO_STR) | (1 << ADAMTX_GPIO_CLK);
	if(adamtx->enabled_chains.chain0)
		gpios |= (1 << ADAMTX_GPIO_C0_R1) | (1 << ADAMTX_GPIO_C0_R2) | (1 << ADAMTX_GPIO_C0_G1) | (1 << ADAMTX_GPIO_C0_G2) | (1 << ADAMTX_GPIO_C0_B1) | (1 << ADAMTX_GPIO_C0_B2);
	if(adamtx->enabled_chains.chain1)
		gpios |= (1 << ADAMTX_GPIO_C1_R1) | (1 << ADAMTX_GPIO_C1_R2) | (1 << ADAMTX_GPIO_C1_G1) | (1 << ADAMTX_GPIO_C1_G2) | (1 << ADAMTX_GPIO_C1_B1) | (1 << ADAMTX_GPIO_C1_B2);
	if(adamtx->enabled_chains.chain2)
		gpios |= (1 << ADAMTX_GPIO_C2_R1) | (1 << ADAMTX_GPIO_C2_R2) | (1 << ADAMTX_GPIO_C2_G1) | (1 << ADAMTX_GPIO_C2_G2) | (1 << ADAMTX_GPIO_C2_B1) | (1 << ADAMTX_GPIO_C2_B2);
	adamtx_gpio_set_outputs(gpios);
}

static int adamtx_probe(struct platform_device* device)
{
	int ret;
	size_t fbsize, iosize, remap_size;
	struct dummyfb_param dummyfb_param;
	struct adamtx* adamtx;

	if(!(adamtx = vzalloc(sizeof(struct adamtx)))) {
		ret = -ENOMEM;
		goto none_alloced;
	}

	adamtx->panels = (struct list_head) LIST_HEAD_INIT(adamtx->panels);
	spin_lock_init(&adamtx->lock_draw);

	platform_set_drvdata(device, adamtx);

	if((ret = adamtx_gpio_alloc()))
	{
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate gpios (%d)\n", ret);
		goto adamtx_alloced;
	}

	if((ret = adamtx_parse_device_tree(&device->dev, adamtx)))
		goto gpio_alloced;

	if(adamtx->enable_dma) {
		//TODO: Setup dma channel
		adamtx->dma_channel = dma_request_chan(&device->dev, "gpio");
		if(IS_ERR(adamtx->dma_channel)) {
			ret = PTR_ERR(adamtx->dma_channel);
			dev_err(&device->dev, "Failed to allocate DMA channel\n");
			goto panels_alloced;
		}
	}

	adamtx_init_gpio(adamtx);

	// Calculate real and virtual display size (smallest rectangle around all displays)
	matrix_get_size_real(&adamtx->real_size, &adamtx->panels);
	matrix_get_size_virtual(&adamtx->virtual_size, &adamtx->panels);

	dev_info(&device->dev, "Calculated real (user) size: (%d, %d)\n", adamtx->real_size.width, adamtx->real_size.height);
	dev_info(&device->dev, "Calculated virtual (technical) size: (%d, %d)\n", adamtx->virtual_size.width, adamtx->virtual_size.height);

	dummyfb_param = (struct dummyfb_param) {
		.width = adamtx->real_size.width,
		.height = adamtx->real_size.height,
		.rate = adamtx->fb_rate,
		.depth = ADAMTX_DEPTH,

		.priv = adamtx,
		.remove = NULL
	};

	if((ret = dummyfb_create(&adamtx->dummyfb, dummyfb_param))) {
		dev_err(&device->dev, "Failed to create framebuffer device\n");
		goto dma_alloced;
	}

	fbsize = adamtx->real_size.height * adamtx->real_size.width * ADAMTX_PIX_LEN;
	if(!(adamtx->framedata = vzalloc(fbsize))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate frame buffer buffer memory\n");
		goto dummyfb_alloced;
	}

	iosize = ADAMTX_PWM_BITS * adamtx->virtual_size.height / 2 * adamtx->virtual_size.width * sizeof(struct adamtx_panel_io);
	if(!(adamtx->paneldata = vzalloc(iosize))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate io memory\n");
		goto framedata_alloced;
	}

	if(!(adamtx->paneldata_out = vzalloc(iosize))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate io clock out memory\n");
		goto paneldata_alloced;
	}

	remap_size = adamtx->virtual_size.height * adamtx->virtual_size.width * sizeof(struct matrix_pixel);
	if(!(adamtx->intermediate_frame = vzalloc(remap_size))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate intermediate frame memory\n");
		goto paneldata_out_alloced;
	}

	adamtx->update_param.rate = adamtx->fb_rate;
	adamtx->update_thread = kthread_create(update_frame, &adamtx->update_param, "adamtx_update");

	if(IS_ERR(adamtx->update_thread))
	{
		ret = PTR_ERR(adamtx->update_thread);
		dev_err(&device->dev, "Failed to create update thread\n");
		goto interframe_alloced;
	}
	if(adamtx->update_thread_bind)
		kthread_bind(adamtx->update_thread, adamtx->update_thread_cpu);

	wake_up_process(adamtx->update_thread);

	adamtx->draw_param.rate = adamtx->rate;
	adamtx->draw_thread = kthread_create(draw_frame, &adamtx->draw_param, "adamtx_draw");

	if(IS_ERR(adamtx->draw_thread))
	{
		ret = PTR_ERR(adamtx->draw_thread);
		dev_err(&device->dev, "Failed to create draw thread\n");
		goto interframe_alloced;
	}
	if(adamtx->draw_thread_bind)
		kthread_bind(adamtx->draw_thread, adamtx->draw_thread_cpu);
	wake_up_process(adamtx->draw_thread);

	adamtx->perf_thread = kthread_create(show_perf, adamtx, "adamtx_perf");
	if(IS_ERR(adamtx->perf_thread))
	{
		ret = PTR_ERR(adamtx->perf_thread);
		dev_err(&device->dev, "Failed to create perf thread\n");
		goto interframe_alloced;
	}
	wake_up_process(adamtx->perf_thread);

	adamtx->frameperiod = ktime_set(0, 1000000000UL / adamtx->rate);
	hrtimer_init(&adamtx->frametimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx->frametimer.function = draw_callback;
	hrtimer_start(&adamtx->frametimer, adamtx->frameperiod, HRTIMER_MODE_REL);
	adamtx->frametimer_enabled = true;

	adamtx->updateperiod = ktime_set(0, 1000000000UL / adamtx->fb_rate);
	hrtimer_init(&adamtx->updatetimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx->updatetimer.function = update_callback;
	hrtimer_start(&adamtx->updatetimer, adamtx->updateperiod, HRTIMER_MODE_REL);
	adamtx->updatetimer_enabled = true;

	adamtx->perfperiod = ktime_set(0, 1000000000UL);
	hrtimer_init(&adamtx->perftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adamtx->perftimer.function = perf_callback;
	hrtimer_start(&adamtx->perftimer, adamtx->perfperiod, HRTIMER_MODE_REL);
	adamtx->perftimer_enabled = true;

	dev_info(&device->dev, "Initialized\n");
	return 0;

interframe_alloced:
	vfree(adamtx->intermediate_frame);
paneldata_out_alloced:
	vfree(adamtx->paneldata_out);
paneldata_alloced:
	vfree(adamtx->paneldata);
framedata_alloced:
	vfree(adamtx->framedata);
dummyfb_alloced:
	dummyfb_destroy(adamtx->dummyfb);
dma_alloced:
	if(adamtx->enable_dma)
		dma_release_channel(adamtx->dma_channel);
panels_alloced:
	free_panels(adamtx);
gpio_alloced:
	adamtx_gpio_free();
adamtx_alloced:
	vfree(adamtx);
none_alloced:
	return ret;
}

static int adamtx_remove(struct platform_device* device)
{
	struct adamtx* adamtx = platform_get_drvdata(device);

	if(adamtx->updatetimer_enabled)
		hrtimer_cancel(&adamtx->updatetimer);
	if(adamtx->frametimer_enabled)
		hrtimer_cancel(&adamtx->frametimer);
	if(adamtx->perftimer_enabled)
		hrtimer_cancel(&adamtx->perftimer);
	kthread_stop(adamtx->draw_thread);
	kthread_stop(adamtx->update_thread);
	kthread_stop(adamtx->perf_thread);
	if(adamtx->enable_dma)
		dma_release_channel(adamtx->dma_channel);
	vfree(adamtx->intermediate_frame);
	vfree(adamtx->paneldata);
	vfree(adamtx->framedata);
	while(dummyfb_destroy(adamtx->dummyfb)) {
		usleep_range(100000UL, 500000UL);
	}
	free_panels(adamtx);
	adamtx_gpio_free();
	vfree(adamtx);

	dev_info(&device->dev, "Shutting down\n");
	return 0;
}

static const struct of_device_id adamtx_of_match[] = {
	{ .compatible = "adafruit,matrix" },
	{ .compatible = "adafruit,adamtx" },
	{}
};


static struct platform_driver adamtx_driver = {
	.probe = adamtx_probe,
	.remove = adamtx_remove,
	.driver = {
		.name = ADAMTX_NAME,
		.of_match_table = adamtx_of_match
	}
};

MODULE_DEVICE_TABLE(of, adamtx_of_match);

module_platform_driver(adamtx_driver);
