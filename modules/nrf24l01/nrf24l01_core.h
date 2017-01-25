#ifndef _NRF24L01_CORE_H_
#define _NRF24L01_CORE_H_

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/types.h>

#include "partregmap.h"
#include "nrf24l01_chrdev.h"
#include "nrf24l01_worker.h"

typedef struct nrf24l01_t {
	struct spi_device*		spi;
	struct regmap*			regmap_short;
	struct nrf24l01_chrdev	chrdev;
	struct partreg_table*	reg_table;
	unsigned 				gpio_ce;
	struct nrf24l01_worker	worker;
	struct mutex			m_rx_path;
	struct mutex			m_tx_path;
	struct mutex			m_rxtx;
	wait_queue_head_t		rx_queue;
	wait_queue_head_t		tx_queue;
	unsigned int			num_readers;
	unsigned int			mode_flags;
} nrf24l01_t;

#define NRF24L01_MODE_LOW_PWR	0b1

#endif
