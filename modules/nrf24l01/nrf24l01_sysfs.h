#ifndef _NRF24L01_SYSFS_H_
#define _NRF24L01_SYSFS_H_

#include <linux/device.h>

ssize_t nrf24l01_sysfs_show_channel(struct device* dev, struct device_attribute* attr, char* buf);
ssize_t nrf24l01_sysfs_store_channel(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

#endif
