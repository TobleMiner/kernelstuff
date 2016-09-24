#include "blink.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test kernel gpio/hrtimer module");
MODULE_VERSION("0.1");

#define BLINK_NAME "blink"

static struct hrtimer htimer;
static ktime_t kt_periode;

static int gpioid = 16;
static int state = 0;

static int __init blink_init(void)
{
	if(gpio_request(gpioid, "sysfs"))
		return -EINVAL;
	gpio_direction_output(gpioid, state);
	kt_periode = ktime_set(1, 0);
	hrtimer_init (&htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	htimer.function = blink_callback;
	hrtimer_start(&htimer, kt_periode, HRTIMER_MODE_REL);
	printk(KERN_INFO BLINK_NAME ": initialized");
	return 0;
}

static void __exit blink_exit(void)
{
	hrtimer_cancel(&htimer);
	gpio_free(gpioid);
	printk(KERN_INFO BLINK_NAME ": Shutting down\n");
}

static enum hrtimer_restart blink_callback(struct hrtimer * timer)
{
	printk(KERN_INFO BLINK_NAME ": blink tick");
	state ^= 1;
	gpio_set_value(gpioid, state);
	hrtimer_forward_now(timer, kt_periode);
	return HRTIMER_RESTART;
}

module_init(blink_init);
module_exit(blink_exit);
