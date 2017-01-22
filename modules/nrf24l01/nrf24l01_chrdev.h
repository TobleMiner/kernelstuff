#ifndef _NRF24L01_CHRDEV_H_
#define _NRF24L01_CHRDEV_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

typedef struct nrf24l01_chrdev_session {
	struct nrf24l01_chrdev* chrdev;
	unsigned int 			read_offset;
} nrf24l01_chrdev_session;

typedef struct nrf24l01_chrdev {
	struct nrf24l01_t*	nrf;
	struct device*		dev;
	struct cdev			cdev;
	struct class*		class;
	dev_t				devt;	
	struct mutex		lock;
} nrf24l01_chrdev;

int chrdev_alloc(struct nrf24l01_t* nrf);
void chrdev_free(struct nrf24l01_t* nrf);

#endif
