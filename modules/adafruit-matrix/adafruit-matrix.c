#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>

#include "matrix.h"
#include "adafruit-matrix.h"

#define ADAMTX_GPIO_HI(gpio) GPIO_SET(gpio, 1)
#define ADAMTX_GPIO_LO(gpio) GPIO_SET(gpio, 0)
#define ADAMTX_GPIO_SET(gpio, state) gpio_set_value(gpio, state)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Adafruit LED matrix driver");
MODULE_VERSION("0.1");

static unsigned adamtx_gpios[ADAMTX_NUM_GPIOS] = {ADAMTX_GPIO_R1, ADAMTX_GPIO_R2, ADAMTX_GPIO_G1, ADAMTX_GPIO_G2, ADAMTX_GPIO_B1, ADAMTX_GPIO_B2, ADAMTX_GPIO_A, ADAMTX_GPIO_B, ADAMTX_GPIO_C, ADAMTX_GPIO_D, ADAMTX_GPIO_E, ADAMTX_GPIO_OE, ADAMTX_GPIO_STR, ADAMTX_GPIO_CLK};

struct gpio* adamtx_gpios;

struct matrix_ledpanel* adamtx_panels;

#define ADAMTX_NUM_PANELS 2

struct matrix_ledpanel adamtx_matrix_up = {
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

struct matrix_ledpanel adamtx_matrix_low = {
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

static int __init adamtx_alloc_gpio()
{
	int i;
	adamtx_gpios = vmalloc(ADAMTX_NUM_GPIOS * sizeof(struct gpio));
	if(adamtx_gpios == NULL)
		return -ENOMEM;
	for(i = 0; i < ADAMTX_NUM_GPIOS; i++)
	{
		adamtx_gpios[i].gpio = adamtx_gpios[i];
		adamtx_gpios[i].flags = GPIOF_OUT_INIT_LOW;
		adamtx_gpios[i].label = ADAMTX_NAME;
	}
	return gpio_request_array(adamtx_gpios, ADAMTX_NUM_GPIOS);
}

static int __exit adamtx_free_gpio()
{
	int ret;
	ret = gpio_free_array(adamtx_gpios, ADAMTX_NUM_GPIOS);
	vfree(adamtx_gpios);
	return ret;
}

void adamtx_clock_out_row(unsigned char* data, int length)
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

static int __init adamtx_init(void)
{
	int ret;
	if(ret = adamtx_alloc_gpio())
	{
		printk(KERN_WARN ADAMTX_NAME ": failed to allocate gpios (%d)", ret);
		goto none_alloced;
	}
	adamtx_panels = malloc(ADAMTX_NUM_PANELS * sizeof(struct matrix_ledpanel));
	if(adamtx_panels == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_WARN ADAMTX_NAME ": failed to allocate panels (%d)", ret);
		goto gpio_alloced;
	}
	printk(KERN_INFO ADAMTX_NAME ": initialized");
	return 0;

gpio_alloced:
	adamtx_free_gpio();
none_alloced:
	return ret;
}

static void __exit adamtx_exit(void)
{
	adamtx_free_gpio();
	printk(KERN_INFO ADAMTX_NAME ": Shutting down\n");
}

module_init(adamtx_init);
module_exit(adamtx_exit);
