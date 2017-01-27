#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/semaphore.h>

#include "nrf24l01_core.h"
#include "nrf24l01_worker.h"
#include "nrf24l01_functions.h"

static int nrf24l01_worker_do_work(void* ctx)
{
	int err;
	unsigned int data;
	struct nrf24l01_t* nrf = (nrf24l01_t*) ctx;
	while(!kthread_should_stop())
	{
		if((err = down_timeout(&nrf->worker.sema, HZ)))
		{
			if(err != -ETIME)
			{
				dev_err(&nrf->spi->dev, "Worker thread failed! err=%d\n", err);
//				do_exit(err);
			}
			if(kthread_should_stop())
				break;
			continue;
		}
		dev_dbg(&nrf->spi->dev, "Event!\n");
		if((err = nrf24l01_get_status_rx_dr(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get rx_dr flag: %d\n", err);
		}
		if(data)
		{
			wake_up_interruptible(&nrf->rx_queue);
			if((err = nrf24l01_set_status_rx_dr(nrf, 1)))
			{
				dev_err(&nrf->spi->dev, "Failed to clear rx_dr flag: %d\n", err);
			}
		}
		if((err = nrf24l01_get_status_tx_ds(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get tx_ds flag\n");
		}
		if(data)
		{
			wake_up_interruptible(&nrf->tx_queue);
			if((err = nrf24l01_set_status_tx_ds(nrf, 1)))
			{
				dev_err(&nrf->spi->dev, "Failed to clear tx_ds flag: %d\n", err);
			}
			mutex_lock(&nrf->m_tx_path);
			if((err = nrf24l01_get_fifo_tx_empty(nrf, &data)))
			{
				dev_err(&nrf->spi->dev, "Failed to check fifo status\n");
				goto tx_fifo_empty_mutex;
			}
			if(data)
			{
				if(nrf24l01_get_mode_low_pwr(nrf))
				{
					mutex_lock(&nrf->m_rx_path);
					if(nrf->num_readers > 0)
					{
						if((err = nrf24l01_set_rx(nrf)))
						{
							dev_err(&nrf->spi->dev, "Failed to set nrf to RX: %d\n", err);
						}
					}
					else
					{
						if((err = nrf24l01_shutdown(nrf)))
						{
							dev_err(&nrf->spi->dev, "Failed to power down nrf: %d\n", err);
						}
					}
					mutex_unlock(&nrf->m_rx_path);
				}
				else
				{
					if((err = nrf24l01_set_rx(nrf)))
					{
						dev_err(&nrf->spi->dev, "Failed to set nrf to RX: %d\n", err);
					}
				}
			}
tx_fifo_empty_mutex:
			mutex_unlock(&nrf->m_tx_path);
		}
		if((err = nrf24l01_get_status_max_rt(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get max_rt flag\n");
		}
		if(data)
		{
			dev_warn(&nrf->spi->dev, "Maximum number of retransmissions reached. Dropping tx fifo\n");
			if((err = nrf24l01_set_status_max_rt(nrf, 1)))
			{
				dev_err(&nrf->spi->dev, "Failed to reset max_rt flag\n");
			}
			// Flush tx fifo to get rid of packet stuck in there with max_rt set
			// TODO: Switch to pulling CE low until nex W_TX_PAYLAOD
			if((err = nrf24l01_flush_tx(nrf)))
			{
				dev_err(&nrf->spi->dev, "Failed to flush tx fifo");
			}
			mutex_lock(&nrf->m_tx_path);
			if(nrf24l01_get_mode_low_pwr(nrf))
			{
				mutex_lock(&nrf->m_rx_path);
				if(nrf->num_readers > 0)
				{
					if((err = nrf24l01_set_rx(nrf)))
					{
						dev_err(&nrf->spi->dev, "Failed to set nrf to RX: %d\n", err);
					}
				}
				else
				{
					if((err = nrf24l01_shutdown(nrf)))
					{
						dev_err(&nrf->spi->dev, "Failed to power down nrf: %d\n", err);
					}
				}
				mutex_unlock(&nrf->m_rx_path);
			}
			else
			{
				if((err = nrf24l01_set_rx(nrf)))
				{
					dev_err(&nrf->spi->dev, "Failed to set nrf to RX: %d\n", err);
				}
			}
			mutex_unlock(&nrf->m_tx_path);
			wake_up_interruptible(&nrf->tx_queue);
		}
	}
	return 0;
}

int nrf24l01_create_worker(struct nrf24l01_t* nrf)
{
	sema_init(&nrf->worker.sema, 0);
	nrf->worker.thread = kthread_run(nrf24l01_worker_do_work, nrf, "nrf_worker");
	if(IS_ERR(nrf->worker.thread))
		return PTR_ERR(nrf->worker.thread);
	return 0;
}

int nrf24l01_destroy_worker(struct nrf24l01_t* nrf)
{
	return kthread_stop(nrf->worker.thread);
}
