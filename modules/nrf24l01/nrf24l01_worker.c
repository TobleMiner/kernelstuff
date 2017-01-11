#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>

#include "nrf24l01_core.h"
#include "nrf24l01_worker.h"
#include "nrf24l01_functions.h"

int nrf24l01_create_worker(struct nrf24l01_t* nrf)
{
	int err;
	nrf->worker.thread = kthread_run(nrf24l01_worker_do_work, nrf, "nrf_worker");
	if(IS_ERR(nrf->worker.thread))
		return PTR_ERR(nrf->worker.thread);
	return 0;
}

int nrf24l01_destroy_worker(struct nrf24l01_t* nrf)
{
	return kthread_stop(nrf->worker.thread);
}

static int nrf24l01_worker_do_work(void* data)
{
	int err;
	struct nrf24l01_t* nrf = (nrf24l01_t*) data;
	while(!kthread_should_stop())
	{
		if((err = wait_event_interruptible_timeout(nrf->worker.queue, true, HZ)))
		{
			printk(KERN_WARNING "Wait returned err=%d\n", err);
		}
		
//		usleep_range(500000, 1000000);
	}
	return 0;
}
