#ifndef _NRF24L01_PIPE_H_
#define _NRF24L01_PIPE_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

struct nrf24l01_chrdev_pipe_session {
	struct nrf24l01_chrdev_pipe*	pipedev;
	unsigned int					read_offset;
};

struct nrf24l01_chrdev_pipe {
	struct nrf24l01_t*	nrf;
	struct device*		dev;
	struct cdev			cdev;
	dev_t				devt;
	wait_queue_head_t	exit_queue;
	unsigned int		num_sessions;
	struct mutex		m_session;
	struct {
		unsigned int shutdown : 1;
	} flags;
	unsigned int		pipe_no;
};

int chrdev_pipe_alloc(struct nrf24l01_t* nrf, unsigned int pipe_no);
void chrdev_pipe_free(struct nrf24l01_chrdev_pipe* pipechr);

#endif
