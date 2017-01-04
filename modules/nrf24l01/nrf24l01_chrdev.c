#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/llist.h>

#include "nrf24l01_core.h"
#include "nrf24l01_functions.h"

#define NRF24L01_CHRDEV_NAME "nrf24l01"
#define NRF24L01_CHRDEV_CLASS "nrf24"

static int dev_open(struct inode* inodep, struct file *filep)
{
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	return 0;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	return 0;
}

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release	
};

static ssize_t show_channel(struct device* dev, struct device_attribute* attr, char* buf)
{
	int channel = 0;
	nrf24l01_get_channel((nrf24l01_t*)dev_get_drvdata(dev), &channel);
	printk(KERN_INFO "Got channel %d\n", channel);
	return sprintf(buf, "%d\n", channel);
}

static ssize_t store_channel(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;	
}

static DEVICE_ATTR(txpower, 0644, NULL, NULL);
static DEVICE_ATTR(channel, 0644, show_channel, store_channel);

static struct attribute* attr_rf[] = {
	&dev_attr_txpower.attr,
	&dev_attr_channel.attr,
	NULL
};

static struct attribute_group group_rf = {
	.attrs = attr_rf,
	.name = "rf"
};

static struct attribute_group* attribute_groups[] = {
	&group_rf,
	NULL
};


int chrdev_alloc(nrf24l01_t* nrf)
{
	int err = 0;
	nrf->chrdev_major = register_chrdev(0, NRF24L01_CHRDEV_NAME, &fops);
	if(nrf->chrdev_major < 0)
	{
		err = nrf->chrdev_major;
		goto exit_noalloc;
	}
	nrf->chrclass = class_create(THIS_MODULE, NRF24L01_CHRDEV_CLASS);
	if(IS_ERR(nrf->chrclass))
    {
        err = PTR_ERR(nrf->chrclass);
        goto exit_unregchrdev;
    }
	nrf->chrdev = device_create_with_groups(nrf->chrclass, NULL, MKDEV(nrf->chrdev_major, 0), nrf, attribute_groups, NRF24L01_CHRDEV_NAME);
	if(IS_ERR(nrf->chrdev))
	{
		err = PTR_ERR(nrf->chrdev);
		goto exit_unregclass;
	}
	return 0;
exit_unregclass:	
	class_unregister(nrf->chrclass);
	class_destroy(nrf->chrclass);
exit_unregchrdev:
	unregister_chrdev(nrf->chrdev_major, NRF24L01_CHRDEV_NAME);
exit_noalloc:
	return err;
}

void chrdev_free(nrf24l01_t* nrf)
{
	device_destroy(nrf->chrclass, MKDEV(nrf->chrdev_major, 0));
	class_unregister(nrf->chrclass);
	class_destroy(nrf->chrclass);
	unregister_chrdev(nrf->chrdev_major, NRF24L01_CHRDEV_NAME);
}
