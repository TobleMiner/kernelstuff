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

#define NRF24L01_NUM_PIPES			6
#define NRF24L01_PACKET_MAX_LENGTH	32

typedef struct nrf24l01_t {
	struct spi_device*		spi;
	struct regmap*			regmap_short;
	struct nrf24l01_chrdev	chrdev;
	struct partreg_table*	reg_table;
	unsigned				gpio_ce;
	struct nrf24l01_worker	worker;
	struct mutex			m_rx_path;
	struct mutex			m_tx_path;
	struct mutex			m_state;
	wait_queue_head_t		rx_queue;
	wait_queue_head_t		tx_queue;
	unsigned int			num_readers;
	unsigned int			mode_flags;
	struct list_head		list;
	unsigned int			id;
	struct {
		unsigned int addr_be : 1;
		unsigned int auto_ack : 1;
	} flags;
} nrf24l01_t;

// Don't idle in RX mode rxing all the time but do so only if there are readers active
#define NRF24L01_MODE_LOW_PWR				0b00000001
// Use a state transition not defined in the state diagram in NRF24L01+ spec
#define NRF24L01_MODE_NON_CANON_STANDBY		0b00000010
// Forces the module into lower power power down mode instead of standby
#define NRF24L01_MODE_PWR_DOWN_NOT_STANDBY	0b00000100

bool nrf24l01_nrf_registered(void);

#endif
