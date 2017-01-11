#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/err.h>

#include "nrf24l01_chrdev.h"
#include "nrf24l01_core.h"
#include "nrf24l01_functions.h"

#define NRF24L01_CHRDEV_NAME "nrf24l01"
#define NRF24L01_CHRDEV_CLASS "nrf24"

static int dev_open(struct inode* inodep, struct file *filep)
{
	struct nrf24l01_t* nrf = ((struct nrf24l01_chrdev*)container_of(inodep->i_cdev, struct nrf24l01_chrdev, cdev))->nrf;
	if(!mutex_trylock(&nrf->chrdev.lock))
		return -EBUSY;
	filep->private_data = nrf;
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	struct nrf24l01_t* nrf = (struct nrf24l01_t*)filep->private_data;
	printk(KERN_INFO "CS is on gpio %u\n", nrf->gpio_ce);
	return 0;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	struct nrf24l01_t* nrf = (struct nrf24l01_t*)filep->private_data;
	mutex_unlock(&nrf->chrdev.lock);
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
	nrf24l01_get_channel(((nrf24l01_chrdev*)dev_get_drvdata(dev))->nrf, &channel);
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


int chrdev_alloc(struct nrf24l01_t* nrf)
{
	int err;
	struct nrf24l01_chrdev* nrfchr = &nrf->chrdev;
	nrfchr->nrf = nrf;
	mutex_init(&nrfchr->lock);
	if((err = alloc_chrdev_region(&nrfchr->devt, 0, 1, NRF24L01_CHRDEV_NAME)))
		goto exit_noalloc;
	nrfchr->class = class_create(THIS_MODULE, NRF24L01_CHRDEV_CLASS);
	if(IS_ERR(nrfchr->class))
    {
        err = PTR_ERR(nrfchr->class);
        goto exit_unregchrdev;
    }
	cdev_init(&nrfchr->cdev, &fops);
	dev_t devnum = MKDEV(MAJOR(nrfchr->devt), MINOR(nrfchr->devt));
	nrfchr->dev = device_create_with_groups(nrfchr->class, NULL, devnum, nrfchr, attribute_groups, NRF24L01_CHRDEV_NAME);
	if(IS_ERR(nrfchr->dev))
	{
		err = PTR_ERR(nrfchr->dev);
		goto exit_unregclass;
	}
	if((err = cdev_add(&nrfchr->cdev, devnum, 1)))
		goto exit_destroydev;
	return 0;
exit_destroydev:
	device_destroy(nrfchr->class, devnum);
exit_unregclass:	
	class_unregister(nrfchr->class);
	class_destroy(nrfchr->class);
exit_unregchrdev:
	unregister_chrdev_region(MAJOR(nrfchr->devt), 1);
exit_noalloc:
	return err;
}

void chrdev_free(struct nrf24l01_t* nrf)
{
	cdev_del(&nrf->chrdev.cdev);
	device_destroy(nrf->chrdev.class, nrf->chrdev.devt);
	class_unregister(nrf->chrdev.class);
	class_destroy(nrf->chrdev.class);
	unregister_chrdev_region(MAJOR(nrf->chrdev.devt), 1);
}
