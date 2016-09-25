#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/gpio.h>

static enum hrtimer_restart blink_callback(struct hrtimer * timer);
