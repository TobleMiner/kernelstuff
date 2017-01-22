#ifndef _NRF24L01_WORKER_H_
#define _NRF24L01_WORKER_H_

#include <linux/kthread.h>
#include <linux/semaphore.h>

typedef struct nrf24l01_worker {
	struct semaphore	sema;
	struct task_struct*	thread;
} nrf24l01_worker;

int nrf24l01_create_worker(struct nrf24l01_t* nrf);
int nrf24l01_destroy_worker(struct nrf24l01_t* nrf);

#endif
