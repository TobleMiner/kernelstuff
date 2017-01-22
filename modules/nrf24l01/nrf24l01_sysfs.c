#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/device.h>

#include "nrf24l01_core.h"
#include "nrf24l01_functions.h"
#include "nrf24l01_sysfs.h"

ssize_t nrf24l01_sysfs_show_channel(struct device* dev, struct device_attribute* attr, char* buf)
{
	unsigned int channel;
	int err;
	if((err = nrf24l01_get_channel(((nrf24l01_chrdev*)dev_get_drvdata(dev))->nrf, &channel)))
		return err;
	printk(KERN_INFO "Got channel %d\n", channel);
	return sprintf(buf, "%d\n", channel);
}

ssize_t nrf24l01_sysfs_store_channel(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	unsigned int channel;
	ssize_t err;
	char* buff = vzalloc(count + 1);
	if(!buff)
		return -ENOMEM;
	memcpy(buff, buf, count);
	if((err = kstrtouint(buff, 10, &channel)))
		goto exit_buffalloc;;	
	err = nrf24l01_set_channel(((nrf24l01_chrdev*)dev_get_drvdata(dev))->nrf, channel);	
	if(err)
		goto exit_buffalloc;
	return count;
exit_buffalloc:
	vfree(buff);
	return err;
}

