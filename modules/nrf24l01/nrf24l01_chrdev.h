#ifndef _NRF24L01_CHRDEV_H_
#define _NRF24L01_CHRDEV_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#include "nrf24l01_pipe.h"
#include "nrf24l01_hw.h"

struct nrf24l01_chrdev_session {
	struct nrf24l01_chrdev* chrdev;
	unsigned int			read_offset;
};

struct nrf24l01_chrdev {
	struct device*		dev;
	struct cdev			cdev;
	dev_t				devt;
	wait_queue_head_t	exit_queue;
	unsigned int		num_sessions;
	struct mutex		m_session;
	bool				shutdown;
	struct nrf24l01_chrdev_pipe pipes[NRF24L01_NUM_PIPES];
};

#define NRF24L01_NRFCHR_TO_NRF(chr) (container_of((chr), struct nrf24l01_t, chrdev))

int chrdev_alloc(struct nrf24l01_t* nrf);
void chrdev_free(struct nrf24l01_t* nrf);

#endif
