#ifndef _NRF24L01_WORKER_H_
#define _NRF24L01_WORKER_H_

#include <linux/kthread.h>

typedef struct nrf24l01_worker {
	wait_queue_head_t	queue;
	struct task_struct*	thread;
} nrf24l01_worker;

static int nrf24l01_worker_do_work(void* data);

#endif
