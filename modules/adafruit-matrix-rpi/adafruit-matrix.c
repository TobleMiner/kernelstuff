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

#include "matrix.h"
#include "adafruit-matrix.h"
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

void remap_frame(struct list_head* panels, char* from, int width_from, int height_from, struct matrix_pixel* to, int width_to, int height_to)
{
	int i, j;
	off_t offset;
	struct matrix_ledpanel* panel;
	struct matrix_pos pos;
	struct matrix_pixel* pixel;
	for(i = 0; i < height_from; i++) {
		for(j = 0; j < width_from; j++) {
			panel = matrix_get_panel_at_real(panels, j, i);
			matrix_panel_get_position(&pos, panel, j, i);
			offset = i * width_from * ADAMTX_PIX_LEN + j * ADAMTX_PIX_LEN;
			pixel = &to[pos.y * width_to + pos.x];
			pixel->chains[panel->chain] = gamma_table[(unsigned char)from[offset]] | gamma_table[(unsigned char)from[offset + 1]] << 8 | gamma_table[(unsigned char)from[offset + 2]] << 16;
		}
	}
}

void prerender_frame_part(struct adamtx_frame* framepart)
{
	int i, j, k, addr;
	struct matrix_pixel* frame = framepart->frame;
	struct adamtx_enabled_chains* enabled_chains = framepart->enabled_chains;
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
				if(enabled_chains->chain0) {
					row[k].C0_B1 = (frame[row1_base + k].chains[0] & (1 << j)) > 0;
					row[k].C0_G1 = ((frame[row1_base + k].chains[0] >> 8) & (1 << j)) > 0;
					row[k].C0_R1 = ((frame[row1_base + k].chains[0] >> 16) & (1 << j)) > 0;
					row[k].C0_B2 = (frame[row2_base + k].chains[0] & (1 << j)) > 0;
					row[k].C0_G2 = ((frame[row2_base + k].chains[0] >> 8) & (1 << j)) > 0;
					row[k].C0_R2 = ((frame[row2_base + k].chains[0] >> 16) & (1 << j)) > 0;
				}
				if(enabled_chains->chain1) {
					row[k].C1_B1 = (frame[row1_base + k].chains[1] & (1 << j)) > 0;
					row[k].C1_G1 = ((frame[row1_base + k].chains[1] >> 8) & (1 << j)) > 0;
					row[k].C1_R1 = ((frame[row1_base + k].chains[1] >> 16) & (1 << j)) > 0;
					row[k].C1_B2 = (frame[row2_base + k].chains[1] & (1 << j)) > 0;
					row[k].C1_G2 = ((frame[row2_base + k].chains[1] >> 8) & (1 << j)) > 0;
					row[k].C1_R2 = ((frame[row2_base + k].chains[1] >> 16) & (1 << j)) > 0;
				}
				if(enabled_chains->chain2) {
					row[k].C2_B1 = (frame[row1_base + k].chains[2] & (1 << j)) > 0;
					row[k].C2_G1 = ((frame[row1_base + k].chains[2] >> 8) & (1 << j)) > 0;
					row[k].C2_R1 = ((frame[row1_base + k].chains[2] >> 16) & (1 << j)) > 0;
					row[k].C2_B2 = (frame[row2_base + k].chains[2] & (1 << j)) > 0;
					row[k].C2_G2 = ((frame[row2_base + k].chains[2] >> 8) & (1 << j)) > 0;
					row[k].C2_R2 = ((frame[row2_base + k].chains[2] >> 16) & (1 << j)) > 0;
				}
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

void show_frame(struct adamtx_panel_io* frame, int bits, int rows, int columns, unsigned long* current_bcd_base_time)
{
	int i, j;
	int pwm_steps = bits;
	struct timespec last, now;
	unsigned long rem_delay, clock_out_delay, bcd_time = *current_bcd_base_time;
	ADAMTX_GPIO_LO(ADAMTX_GPIO_OE);
	for(i = rows / 2 - 1; i >= 0; i--)
	{
		for(j = 0; j < pwm_steps; j++)
		{
			ADAMTX_GPIO_HI(ADAMTX_GPIO_OE);
			getnstimeofday(&last);
			adamtx_clock_out_row(frame + i * pwm_steps * columns + j * columns, columns);
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
			clock_out_delay = (now.tv_sec - last.tv_sec) * 1000000000UL + (now.tv_nsec - last.tv_nsec);
			if(clock_out_delay < rem_delay) {
				if((clock_out_delay < rem_delay) && !j)
					*current_bcd_base_time = bcd_time - 50;
				ndelay(rem_delay - clock_out_delay);
			} else if(clock_out_delay > rem_delay) {
					*current_bcd_base_time = bcd_time + 50;
			}
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
	struct adamtx_frame threadframe = {
		.width = frame->columns,
		.height = frame->rows,
		.vertical_offset = 0,
		.rows = frame->rows,
		.paneldata = frame->iodata,
		.paneloffset = 0,
		.frame = frame->intermediate_frame,
		.frameoffset = 0,
		.pwm_bits = frame->pwm_bits,
		.enabled_chains = frame->enabled_chains
	};

	remap_frame(frame->panels, frame->frame, frame->width, frame->height, frame->intermediate_frame, frame->columns, frame->rows);

	render_part(&threadframe);

	return 0;
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
		show_frame(adamtx->paneldata, ADAMTX_PWM_BITS, adamtx->virtual_size.height, adamtx->virtual_size.width, &adamtx->current_bcd_base_time);
		getnstimeofday(&after);
		adamtx->draw_time += (after.tv_sec - before.tv_sec) * 1000000000UL + (after.tv_nsec - before.tv_nsec);
		adamtx->draws++;
		spin_unlock_irqrestore(&adamtx->lock_draw, irqflags);
		yield();
	}
	do_exit(0);
}

static int update_frame(void* arg)
{
	int err;
	unsigned long irqflags;
	struct timespec before;
	struct timespec after;
	struct adamtx_processable_frame frame;
	struct adamtx_update_param* param = (struct adamtx_update_param*)arg;
	struct adamtx* adamtx = container_of(param, struct adamtx, update_param);

	printk(KERN_INFO ADAMTX_NAME ": Update spacing: %lu us", 1000000UL / param->rate);
	while(!kthread_should_stop()) {
		while(!adamtx->do_update && !kthread_should_stop())
			usleep_range(50, 500);
		if(kthread_should_stop())
			break;
		adamtx->do_update = false;

		dummyfb_copy(adamtx->framedata, adamtx->dummyfb);

		frame = (struct adamtx_processable_frame){
			.width = adamtx->real_size.width,
			.height = adamtx->real_size.height,
			.columns = adamtx->virtual_size.width,
			.rows = adamtx->virtual_size.height,
			.pwm_bits = ADAMTX_PWM_BITS,
			.iodata = adamtx->paneldata,
			.frame = adamtx->framedata,
			.panels = &adamtx->panels,
			.enabled_chains = &adamtx->enabled_chains,
			.intermediate_frame = adamtx->intermediate_frame
		};

		spin_lock_irqsave(&adamtx->lock_draw, irqflags);
		getnstimeofday(&before);
		err = process_frame(&frame);
		getnstimeofday(&after);
		adamtx->update_time += (after.tv_sec - before.tv_sec) * 1000000000UL + (after.tv_nsec - before.tv_nsec);
		adamtx->updates++;
		spin_unlock_irqrestore(&adamtx->lock_draw, irqflags);
		if(err)
			do_exit(err);
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
		printk(KERN_INFO ADAMTX_NAME ": %ld updates/s\t%ld irqs/s\t%lu ns/update", perf_adamtx_updates, perf_adamtx_update_irqs, perf_adamtx_updates != 0 ? perf_adamtx_update_time / perf_adamtx_updates : 0);
		printk(KERN_INFO ADAMTX_NAME ": %ld draws/s\t%ld irqs/s\t%lu ns/draw", perf_adamtx_draws, perf_adamtx_draw_irqs, perf_adamtx_draws != 0 ? perf_adamtx_draw_time / perf_adamtx_draws : 0);	
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

static int adamtx_of_get_int(struct device_node* of_node, const char* name, int def) {
	const void* of_prop;
	if((of_prop = of_get_property(of_node, name, NULL)))
		return be32_to_cpup(of_prop);
	return def;
}

static int adamtx_of_get_int_range(int* dest, struct device_node* of_node, const char* name, int def, int min, int max) {
	int value = adamtx_of_get_int(of_node, name, def);
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

	dev_info(dev, "Refresh rate: %d Hz, FB poll rate %d Hz", adamtx->rate, adamtx->fb_rate);

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

		dev_info(dev, "Panel %d: Resolution: %d px x %d px, Physical position: (%d, %d), Virtual position: (%d, %d), Flip x: %d, Flip y: %d, Chain %d",
				adamtx_num_panels, panel->xres, panel->yres, panel->realx, panel->realy, panel->virtual_x, panel->virtual_y, panel->flip_x, panel->flip_y, panel->chain);

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

	adamtx_init_gpio(adamtx);

	// Calculate real and virtual display size (smallest rectangle around all displays)
	matrix_panel_get_size_real(&adamtx->real_size, &adamtx->panels);
	matrix_panel_get_size_virtual(&adamtx->virtual_size, &adamtx->panels);

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
		goto panels_alloced;
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

	remap_size = adamtx->virtual_size.height * adamtx->virtual_size.width * sizeof(struct matrix_pixel);
	if(!(adamtx->intermediate_frame = vmalloc(remap_size))) {
		ret = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate intermediate frame memory\n");
		goto paneldata_alloced;
	}

	adamtx->update_param.rate = adamtx->fb_rate;
	adamtx->update_thread = kthread_create(update_frame, &adamtx->update_param, "adamtx_update");
	kthread_bind(adamtx->update_thread, 2);
	if(IS_ERR(adamtx->update_thread))
	{
		ret = PTR_ERR(adamtx->update_thread);
		dev_err(&device->dev, "Failed to create update thread\n");
		goto interframe_alloced;
	}
	wake_up_process(adamtx->update_thread);

	adamtx->draw_param.rate = adamtx->rate;
	adamtx->draw_thread = kthread_create(draw_frame, &adamtx->draw_param, "adamtx_draw");
	kthread_bind(adamtx->draw_thread, 3);
	if(IS_ERR(adamtx->draw_thread))
	{
		ret = PTR_ERR(adamtx->draw_thread);
		dev_err(&device->dev, "Failed to create draw thread\n");
		goto interframe_alloced;
	}
	wake_up_process(adamtx->draw_thread);

	adamtx->perf_thread = kthread_create(show_perf, adamtx, "adamtx_perf");
	kthread_bind(adamtx->perf_thread, 1);
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
paneldata_alloced:
	vfree(adamtx->paneldata);
framedata_alloced:
	vfree(adamtx->framedata);
dummyfb_alloced:
	dummyfb_destroy(adamtx->dummyfb);
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
