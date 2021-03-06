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
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include "matrix.h"
#include "adafruit_matrix.h"
#include "io.h"
#include "bcm2835.h"
#include "color.h"
#include "gamma.h"

#include "../dummyfb/dummyfb.h"

#define ADAMTX_GPIO_HI(gpio) adamtx_gpio_set_bits((1 << gpio))
#define ADAMTX_GPIO_LO(gpio) adamtx_gpio_clr_bits((1 << gpio))
#define ADAMTX_GPIO_SET(gpio, state) (state ? ADAMTX_GPIO_HI(gpio) : ADAMTX_GPIO_LO(gpio))

#define ADAMTX_DMA_STEPS_PER_PIXEL 1
#define ADAMTX_DMA_ADDRESS_STEPS 3

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

void adamtx_clock_out_row(struct adamtx_panel_io* data, int length)
{
	while(--length >= 0)
	{
		adamtx_gpio_write_bits(((uint32_t*)data)[length]);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
		ADAMTX_GPIO_HI(ADAMTX_GPIO_CLK);
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

void remap_frame(struct adamtx_remap_frame* frame)
{
	int line, column;
	off_t offset;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	struct matrix_pixel* pixel;
	struct matrix_size* real_size = frame->real_size;
	struct matrix_size* virtual_size = frame->virtual_size;
	struct adamtx_gamma_table* gamma_table = frame->gamma_table;
	for(line = frame->offset; line < frame->offset + frame->rows; line++) {
		for(column = 0; column < real_size->width; column++) {
			panel = matrix_get_panel_at_real(frame->panels, column, line);
			if(!panel)
				continue;

			matrix_panel_get_position(&pos, panel, column, line);
			offset = line * real_size->width * 3 + column * 3;
			pixel = &frame->dst[pos.y * virtual_size->width + pos.x];
			pixel->chains[panel->chain] = adamtx_gamma_apply_gbr24(gamma_table, frame->src[offset], frame->src[offset + 1], frame->src[offset + 2]);
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
	int pwm_steps = adamtx->pwm_bits;
	int rows = adamtx->virtual_size.height;
	int columns = adamtx->virtual_size.width;
	struct timespec last, now;
	unsigned long rem_delay, clock_out_delay, last_delay, bcd_time = adamtx->current_bcd_base_time, est_remap_render_time;

	struct adamtx_remap_frame adamtx_remap_frame = {
		.real_size = &adamtx->real_size,
		.virtual_size = &adamtx->virtual_size,
		.offset = 0,
		.rows = 1,
		.pwm_bits = pwm_steps,
		.panels = &adamtx->panels,
		.src = adamtx->framedata,
		.dst = adamtx->intermediate_frame,
		.gamma_table = &adamtx->gamma_table
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

	est_remap_render_time = adamtx->update_remap_ns_per_line * adamtx->real_size.height +
							adamtx->update_prerender_ns_per_line * adamtx->virtual_size.height / 2;

	if(bcd_time * (BIT(pwm_steps) - 1) < est_remap_render_time * 3 / 2)
		adamtx->remap_and_render_in_update_thread = true;
	else if(bcd_time * (BIT(pwm_steps) - 1) > est_remap_render_time * 2)
		adamtx->remap_and_render_in_update_thread = false;


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

				if(!adamtx->remap_and_render_in_update_thread){
					while(remap_line < adamtx_remap_frame.real_size->height && rem_delay > adamtx->update_remap_ns_per_line * 2) {
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
						while(prerender_line < adamtx_prerender_frame.virtual_size->height && rem_delay > adamtx->update_prerender_ns_per_line * 2) {
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
	int line, line_base, line_dma_base, pwm_step, pwm_base, pwm_dma_base, column, column_base, column_dma_base;
	struct adamtx_panel_io io_address;
	struct timespec before, after;
	struct adamtx_update_param* param = (struct adamtx_update_param*)arg;
	struct adamtx* adamtx = container_of(param, struct adamtx, update_param);

	struct adamtx_remap_frame adamtx_remap_frame = {
		.real_size = &adamtx->real_size,
		.virtual_size = &adamtx->virtual_size,
		.offset = 0,
		.rows = adamtx->real_size.height,
		.pwm_bits = adamtx->pwm_bits,
		.panels = &adamtx->panels,
		.src = adamtx->framedata,
		.dst = adamtx->intermediate_frame,
		.gamma_table = &adamtx->gamma_table
	};

	struct adamtx_prerender_frame adamtx_prerender_frame = {
		.virtual_size = &adamtx->virtual_size,
		.offset = 0,
		.rows = adamtx->virtual_size.height,
		.pwm_bits = adamtx->pwm_bits,
		.iodata = adamtx->paneldata,
		.enabled_chains = &adamtx->enabled_chains,
		.intermediate_frame = adamtx->intermediate_frame
	};

	memset(&io_address, 0, sizeof(io_address));

	printk(KERN_INFO ADAMTX_NAME ": Update spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop()) {
		while(!adamtx->do_update && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx->do_update = false;

		getnstimeofday(&before);


		dummyfb_copy_as_bgr24(adamtx->framedata, adamtx->dummyfb);

		if(adamtx->enable_dma || adamtx->remap_and_render_in_update_thread) {
			remap_frame(&adamtx_remap_frame);
			prerender_frame(&adamtx_prerender_frame);
		}

		if(adamtx->enable_dma) {

			for(line = adamtx->virtual_size.height / 2 - 1; line >= 0; line--) {
				line_base = line * adamtx->pwm_bits * adamtx->virtual_size.width;
				line_dma_base = ADAMTX_DMA_STEPS_PER_PIXEL * line * BIT(adamtx->pwm_bits) * adamtx->virtual_size.width + line * ADAMTX_DMA_ADDRESS_STEPS;

				for(pwm_step = 0; pwm_step < adamtx->pwm_bits; pwm_step++) {
					pwm_base = line_base + pwm_step * adamtx->virtual_size.width;
					pwm_dma_base = line_dma_base + ADAMTX_DMA_STEPS_PER_PIXEL * (BIT(pwm_step) - 1) * adamtx->virtual_size.width;

					if(pwm_step == 1) {
						adamtx->dma_iodata[pwm_dma_base].set = BIT(ADAMTX_GPIO_OE);
						adamtx->dma_iodata[pwm_dma_base++].clear = ADAMTX_GPIO_MASK_ADDRESS;
						*((uint32_t*)&io_address) = (line << ADAMTX_GPIO_OFFSET_ADDRESS) & ADAMTX_GPIO_MASK_ADDRESS_HI;
						io_address.E = line >> 4;
						adamtx->dma_iodata[pwm_dma_base].set = *((uint32_t*)&io_address);
						adamtx->dma_iodata[pwm_dma_base++].clear = 0;
						adamtx->dma_iodata[pwm_dma_base].set = BIT(ADAMTX_GPIO_STR);
						adamtx->dma_iodata[pwm_dma_base++].clear = BIT(ADAMTX_GPIO_OE) | BIT(ADAMTX_GPIO_STR);
						line_dma_base += ADAMTX_DMA_ADDRESS_STEPS;
					}

					for(column = 0; column < adamtx->virtual_size.width; column++) {
						column_base = pwm_base + column;
						column_dma_base = pwm_dma_base + ADAMTX_DMA_STEPS_PER_PIXEL * column;

						adamtx->dma_iodata[column_dma_base].set = (((uint32_t*)adamtx->paneldata)[column_base] & ADAMTX_VALID_GPIO_BITS & ~ADAMTX_GPIO_MASK_ADDRESS) | BIT(ADAMTX_GPIO_CLK);
						if(column == (adamtx->virtual_size.width - 1) && pwm_step)
							adamtx->dma_iodata[column_dma_base].set |= BIT(ADAMTX_GPIO_STR);
						adamtx->dma_iodata[column_dma_base].clear = BIT(ADAMTX_GPIO_STR) | adamtx->dma_iodata[column_dma_base].set;
					}
				}
			}
		}

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
		printk(KERN_INFO ADAMTX_NAME ": Remapping and rendering offloaded to thread: %d\n", adamtx->remap_and_render_in_update_thread);

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

	if((err = adamtx_of_get_int(&adamtx->peripheral_base, dev_of_node, "adamtx-peripheral-base"))) {
		dev_err(dev, "Missing peripheral base address\n");
		goto exit_err;
	}

	adamtx->draw_thread_bind = !adamtx_of_get_int(&adamtx->draw_thread_cpu, dev_of_node, "adamtx-bind-draw");
	adamtx->update_thread_bind = !adamtx_of_get_int(&adamtx->update_thread_cpu, dev_of_node, "adamtx-bind-update");

	adamtx->enable_dma = !!adamtx_of_get_int_default(dev_of_node, "adamtx-dma", 0);

	adamtx->color_model.bitdepth = adamtx_of_get_int_default(dev_of_node, "adamtx-color-depth", 24);
	adamtx->color_model.grayscale = !!adamtx_of_get_int_default(dev_of_node, "adamtx-grayscale", 0);

	dev_info(dev, "Refresh rate: %d Hz, FB poll rate %d Hz, DMA: %d, Peripheral base address: 0x%x\n", adamtx->rate, adamtx->fb_rate, adamtx->enable_dma, adamtx->peripheral_base);
	dev_info(dev, "Color depth: %d bit, grayscale: %d\n", adamtx->color_model.bitdepth, adamtx->color_model.grayscale);

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
	int ret, i;
	size_t fbsize, iosize, remap_size;
	struct dummyfb_param dummyfb_param;
	struct adamtx* adamtx;
	struct dma_async_tx_descriptor* dma_desc;
	struct bcm2835_desc* desc;
	struct bcm2835_chan* bcm_channel;
	size_t len;

	if(!(adamtx = vzalloc(sizeof(struct adamtx)))) {
		ret = -ENOMEM;
		goto none_alloced;
	}

	adamtx->panels = (struct list_head) LIST_HEAD_INIT(adamtx->panels);
	spin_lock_init(&adamtx->lock_draw);

	platform_set_drvdata(device, adamtx);

	if((ret = adamtx_parse_device_tree(&device->dev, adamtx)))
		goto adamtx_alloced;

	if((ret = adamtx_gpio_alloc(adamtx)))
	{
		printk(KERN_WARNING ADAMTX_NAME ": failed to allocate gpios (%d)\n", ret);
		goto panels_alloced;
	}

	if(adamtx->enable_dma) {
		adamtx->dma_channel = dma_request_chan(&device->dev, "gpio");
		if(IS_ERR(adamtx->dma_channel)) {
			ret = PTR_ERR(adamtx->dma_channel);
			dev_err(&device->dev, "Failed to allocate DMA channel\n");
			goto gpio_alloced;
		}
		dev_info(&device->dev, "Got DMA channel %d\n", adamtx->dma_channel->chan_id);

		// More pi-specific hackery
		bcm_channel = BCM2835_CHAN_FROM_DMA_CHAN(adamtx->dma_channel);
		dev_info(&device->dev, "DMA channel %d maps to BCM2835 DMA channel %d\n", adamtx->dma_channel->chan_id, bcm_channel->ch);
		if(bcm_channel->is_lite_channel) {
			dev_err(&device->dev, "BCM2835 DMA channel %d is a DMA lite channel. We need a fully featured channel for 2D DMA mode\n", bcm_channel->ch);
			ret = -ENODEV;
			goto dma_alloced;
		}
		dev_info(&device->dev, "BCM2835 DMA channel %d is fully featured. Continuing\n", bcm_channel->ch);
	}

	adamtx_init_gpio(adamtx);

	// Calculate real and virtual display size (smallest rectangle around all displays)
	matrix_get_size_real(&adamtx->real_size, &adamtx->panels);
	matrix_get_size_virtual(&adamtx->virtual_size, &adamtx->panels);

	dev_info(&device->dev, "Calculated real (user) size: (%d, %d)\n", adamtx->real_size.width, adamtx->real_size.height);
	dev_info(&device->dev, "Calculated virtual (technical) size: (%d, %d)\n", adamtx->virtual_size.width, adamtx->virtual_size.height);

	dummyfb_param = (struct dummyfb_param) {
		.mode = {
			.width = adamtx->real_size.width,
			.height = adamtx->real_size.height,
			.rate = adamtx->fb_rate,
			.depth = adamtx->color_model.bitdepth,
			.grayscale = adamtx->color_model.grayscale
		},

		.priv = adamtx,
		.remove = NULL
	};

	if((ret = dummyfb_create(&adamtx->dummyfb, dummyfb_param))) {
		dev_err(&device->dev, "Failed to create framebuffer device\n");
		goto dma_alloced;
	}

	adamtx->pwm_bits = dummyfb_get_max_color_depth(adamtx->dummyfb);

	adamtx->color_model.depth_red = adamtx->dummyfb->color_format.red.length;
	adamtx->color_model.depth_green = adamtx->dummyfb->color_format.green.length;
	adamtx->color_model.depth_blue = adamtx->dummyfb->color_format.blue.length;
	adamtx_gamma_setup_table_fix_max(&adamtx->gamma_table, &adamtx->color_model, BIT(adamtx->pwm_bits) - 1);

	dev_info(&device->dev, "Using %d pwm bits\n", adamtx->pwm_bits);

	fbsize = adamtx->real_size.height * adamtx->real_size.width * 3;
	if(!(adamtx->framedata = vzalloc(fbsize))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate frame buffer buffer memory\n");
		goto dummyfb_alloced;
	}

	iosize = adamtx->pwm_bits * adamtx->virtual_size.height / 2 * adamtx->virtual_size.width * sizeof(struct adamtx_panel_io);
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

	if(adamtx->enable_dma) {
		adamtx->dma_len =
			// Actual image data
			adamtx->virtual_size.height / 2 * adamtx->virtual_size.width * BIT(adamtx->pwm_bits) * ADAMTX_DMA_STEPS_PER_PIXEL +
			// Extra DMA transfers for address switching
			adamtx->virtual_size.height / 2 * ADAMTX_DMA_ADDRESS_STEPS;
		dev_info(&device->dev, "Allocating %lu kB DMA buffers\n", adamtx->dma_len * sizeof(struct adamtx_dma_block) / 1024UL);
		if(!(adamtx->dma_iodata = dma_alloc_coherent(&device->dev, adamtx->dma_len * sizeof(struct adamtx_dma_block), &adamtx->dma_mapping_iodata, GFP_KERNEL | GFP_DMA32))) {
			ret = -ENOMEM;
			dev_err(&device->dev, "Failed to allocate dma buffer\n");
			goto paneldata_out_alloced;
		}

		memset(adamtx->dma_iodata, 0x00, adamtx->dma_len * sizeof(struct adamtx_dma_block));
	}

	remap_size = adamtx->virtual_size.height * adamtx->virtual_size.width * sizeof(struct matrix_pixel);
	if(!(adamtx->intermediate_frame = vzalloc(remap_size))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate intermediate frame memory\n");
		goto dma_data_out_alloced;
	}

	if(adamtx->enable_dma) {
		adamtx->dma_mapping_gpio = ADAMTX_PERIPHERAL_BUS_BASE + ADAMTX_GPIO_OFFSET + ADAMTX_GPIO_SET_OFFSET;

		memset(&adamtx->dma_config, 0, sizeof(adamtx->dma_config));
		adamtx->dma_config = (struct dma_slave_config) {
			.direction = DMA_MEM_TO_DEV,
			.dst_addr = adamtx->dma_mapping_gpio,
			.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
		};

		if((ret = dmaengine_slave_config(adamtx->dma_channel, &adamtx->dma_config))) {
			dev_err(&device->dev, "Failed to configure dma slave device\n");
			goto resource_mapped;
		}

		len = adamtx->dma_len * sizeof(struct adamtx_dma_block);
		while(len > (0x3fff * sizeof(struct adamtx_dma_block)) || (adamtx->dma_len * sizeof(struct adamtx_dma_block)) % len)
			len--;

		dma_desc = dmaengine_prep_dma_cyclic(adamtx->dma_channel,
			adamtx->dma_mapping_iodata, adamtx->dma_len * sizeof(struct adamtx_dma_block),
			len, DMA_MEM_TO_DEV, 0);

		if(!dma_desc) {
			ret = -EINVAL;
			dev_err(&device->dev, "Failed to prepare cyclic DMA\n");
			goto iodata_out_mapped;
		}

		desc = BCM2835_DESC_FROM_DMA_DESC(dma_desc);

		printk(KERN_INFO "Got %u frames in chain\n", desc->frames);
		for(i = 0; i < desc->frames; i++) {
			struct bcm2835_dma_cb* control_block = desc->cb_list[i].cb;
			printk(KERN_INFO "Got control block with transfer length %zu @0x%x\n", control_block->length, __pa(control_block));
			control_block->info = BCM2835_DMA_TDMODE | BCM2835_DMA_NO_WIDE_BURSTS | BCM2835_DMA_D_INC | BCM2835_DMA_S_INC;
			control_block->length = DMA_CB_TXFR_LEN_YLENGTH(control_block->length / sizeof(struct adamtx_dma_block)) | DMA_CB_TXFR_LEN_XLENGTH((uint16_t)sizeof(struct adamtx_dma_block));
			control_block->stride = DMA_CB_STRIDE_D_STRIDE(-((int16_t)sizeof(struct adamtx_dma_block))) | DMA_CB_STRIDE_S_STRIDE(0);

			printk("DMA flags: 0x%x\n", control_block->info);

			printk(KERN_INFO "DMA X LENGTH: %u\n", (((uint32_t*)control_block)[3]) & 0xFFFF);
			printk(KERN_INFO "DMA Y LENGTH: %u\n", (((uint32_t*)control_block)[3]) >> 16 & 0x3FFF);

			printk(KERN_INFO "DMA S STRIDE: %d\n", (int16_t)(((uint32_t*)control_block)[4] & 0xFFFF));
			printk(KERN_INFO "DMA D STRIDE: %d\n", (int16_t)(((uint32_t*)control_block)[4] >> 16 & 0xFFFF));

			control_block->dst = adamtx->dma_mapping_gpio;
			printk(KERN_INFO "DMA DST: 0x%x\n", ((uint32_t*)control_block)[2]);
			printk(KERN_INFO "DMA SRC: 0x%x\n", ((uint32_t*)control_block)[1]);
			printk(KERN_INFO "DMA SRC end: 0x%x\n", adamtx->dma_mapping_iodata + adamtx->dma_len * sizeof(struct adamtx_dma_block));
			printk(KERN_INFO "DMA NEXT: 0x%x\n", control_block->next);
		}

		dmaengine_submit(dma_desc);
		dma_async_issue_pending(adamtx->dma_channel);
	}

	adamtx->update_param.rate = adamtx->fb_rate;
	adamtx->update_thread = kthread_create(update_frame, &adamtx->update_param, "adamtx_update");

	if(IS_ERR(adamtx->update_thread))
	{
		ret = PTR_ERR(adamtx->update_thread);
		dev_err(&device->dev, "Failed to create update thread\n");
		goto dma_initialized;
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
		goto update_thread_started;
	}
	if(adamtx->draw_thread_bind)
		kthread_bind(adamtx->draw_thread, adamtx->draw_thread_cpu);
	wake_up_process(adamtx->draw_thread);

	adamtx->perf_thread = kthread_create(show_perf, adamtx, "adamtx_perf");
	if(IS_ERR(adamtx->perf_thread))
	{
		ret = PTR_ERR(adamtx->perf_thread);
		dev_err(&device->dev, "Failed to create perf thread\n");
		goto draw_thread_started;
	}
	wake_up_process(adamtx->perf_thread);

	if(!adamtx->enable_dma) {
		adamtx->frameperiod = ktime_set(0, 1000000000UL / adamtx->rate);
		hrtimer_init(&adamtx->frametimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		adamtx->frametimer.function = draw_callback;
		hrtimer_start(&adamtx->frametimer, adamtx->frameperiod, HRTIMER_MODE_REL);
		adamtx->frametimer_enabled = true;
	}

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

draw_thread_started:
	kthread_stop(adamtx->draw_thread);
update_thread_started:
	kthread_stop(adamtx->update_thread);
dma_initialized:
	if(adamtx->enable_dma)
		dmaengine_terminate_sync(adamtx->dma_channel);
iodata_out_mapped:
resource_mapped:
iodata_mapped:
interframe_alloced:
	vfree(adamtx->intermediate_frame);
paneldata_out_alloced:
	vfree(adamtx->paneldata_out);
dma_data_out_alloced:
dma_data_alloced:
	if(adamtx->enable_dma)
		dma_free_coherent(&device->dev, adamtx->dma_len * sizeof(struct adamtx_dma_block), adamtx->dma_iodata, adamtx->dma_mapping_iodata);
paneldata_alloced:
	vfree(adamtx->paneldata);
framedata_alloced:
	vfree(adamtx->framedata);
dummyfb_alloced:
	dummyfb_destroy(adamtx->dummyfb);
dma_alloced:
	if(adamtx->enable_dma)
		dma_release_channel(adamtx->dma_channel);
gpio_alloced:
	adamtx_gpio_free(adamtx);
panels_alloced:
	free_panels(adamtx);
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
	if(adamtx->enable_dma) {
		dmaengine_terminate_sync(adamtx->dma_channel);
		dma_release_channel(adamtx->dma_channel);
		dma_free_coherent(&device->dev, adamtx->dma_len * sizeof(struct adamtx_dma_block), adamtx->dma_iodata, adamtx->dma_mapping_iodata);
	}
	vfree(adamtx->intermediate_frame);
	vfree(adamtx->paneldata);
	vfree(adamtx->framedata);
	while(dummyfb_destroy(adamtx->dummyfb)) {
		usleep_range(100000UL, 500000UL);
	}
	free_panels(adamtx);
	adamtx_gpio_free(adamtx);
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
