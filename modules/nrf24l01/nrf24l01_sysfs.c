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

static ssize_t nrf24l01_sysfs_show_address(struct device* dev, char* buf, unsigned int pipe)
{
	int err;
	unsigned int addr_width;
	u64 addr = 0;
	nrf24l01_t* nrf = ((nrf24l01_chrdev*)dev_get_drvdata(dev))->nrf;
	if((err = nrf24l01_get_address_u64(nrf, &addr, pipe)))
		return err;
	if(pipe <= 1)
	{
		if((err = nrf24l01_get_address_width(nrf, &addr_width)))
			return err;
		char fmt[10];
		sprintf(fmt, "%%0%uXu", addr_width);
		return sprintf(buf, fmt, addr);
	}
	else
		return sprintf(buf, "%02Xu", addr);
}

ssize_t nrf24l01_sysfs_show_address_pipe0(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 0);
}

ssize_t nrf24l01_sysfs_show_address_pipe1(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 1);
}

ssize_t nrf24l01_sysfs_show_address_pipe2(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 2);
}

ssize_t nrf24l01_sysfs_show_address_pipe3(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 3);
}

ssize_t nrf24l01_sysfs_show_address_pipe4(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 4);
}

ssize_t nrf24l01_sysfs_show_address_pipe5(struct device* dev, struct device_attribute* attr, char* buf)
{
	return nrf24l01_sysfs_show_address(dev, buf, 5);
}
