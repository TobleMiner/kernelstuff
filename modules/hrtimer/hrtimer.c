#include "hrtimer.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test kernel hrtimer module");
MODULE_VERSION("0.1");

static struct hrtimer htimer;
static ktime_t kt_periode;

static char* name = "hrtest";

static int __init hrtest_init(void)
{
	kt_periode = ktime_set(3, 0);
	hrtimer_init (&htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	htimer.function = hrtest_callback;
	hrtimer_start(&htimer, kt_periode, HRTIMER_MODE_REL);
	printk(KERN_INFO "%s: hrtimer initialized\n", name);
	return 0;
}

static void __exit hrtest_exit(void)
{
	hrtimer_cancel(&htimer);
	printk(KERN_INFO "%s: Shutting down\n", name);
}

static enum hrtimer_restart hrtest_callback(struct hrtimer * timer)
{
	printk(KERN_INFO "%s: hrtimer tick", name);
	hrtimer_forward_now(timer, kt_periode);
	return HRTIMER_RESTART;
}

module_init(hrtest_init);
module_exit(hrtest_exit);
