#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "nrf24l01_chrdev.h"
#include "nrf24l01_core.h"
#include "nrf24l01_functions.h"
#include "nrf24l01_sysfs.h"
#include "nrf24l01_reg.h"

#define NRF24L01_CHRDEV_FORMAT "nrf24l0%lu"
#define NRF24L01_CHRDEV_CLASS "nrf24"

static struct class* chrdev_class = NULL;

static int dev_open(struct inode* inodep, struct file *filep)
{
	int err = 0;
	struct nrf24l01_chrdev_session* session;
	struct nrf24l01_chrdev* nrfchr = (struct nrf24l01_chrdev*)container_of(inodep->i_cdev, struct nrf24l01_chrdev, cdev);
	mutex_lock(&nrfchr->m_session);
	if(nrfchr->shutdown)
	{
		err = -ESHUTDOWN;
		goto exit_err;
	}
	nrfchr->num_sessions++;
	session = vzalloc(sizeof(struct nrf24l01_chrdev_session));
	if(!session)
	{
		err = -ENOMEM;
		goto exit_err;
	}
	session->chrdev = nrfchr;
	filep->private_data = session;
exit_err:
	mutex_unlock(&nrfchr->m_session);
	return err;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	unsigned long lenoffset;
	ssize_t err, readlen;
	char* data;
	struct nrf24l01_chrdev_session* session = (struct nrf24l01_chrdev_session*)filep->private_data;
	struct nrf24l01_t* nrf = session->chrdev->nrf;
	data = vmalloc(len);
	if(!data)
	{
		err = -ENOMEM;
		goto exit_err;
	}
	if((readlen = nrf24l01_read_packet(nrf, !!(filep->f_flags & O_NONBLOCK), data, (unsigned int) len)) < 0)
	{
		err = readlen;
		goto exit_dataalloc;
	}
	if((lenoffset = copy_to_user(buffer, data, len)))
		dev_warn(nrf->chrdev.dev, "%lu of %zu bytes could not be copied to userspace\n", lenoffset, len);
	session->read_offset += readlen;
	err = readlen;
exit_dataalloc:
	vfree(data);
exit_err:
	return err;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	unsigned long lenoffset;
	ssize_t err;
	size_t len_packet;
	char* data;
	loff_t readoffset = 0;
	struct nrf24l01_chrdev_session* session = (struct nrf24l01_chrdev_session*)filep->private_data;
	struct nrf24l01_t* nrf = session->chrdev->nrf;
	data = vmalloc(len);
	if(!data)
	{
		err = -ENOMEM;
		goto exit_err;
	}
	if((lenoffset = copy_from_user(data, buffer, len)))
		dev_warn(nrf->chrdev.dev, "%lu of %zu bytes could not be copied to kernelspace\n", lenoffset, len);
	while(len - readoffset > 0)
	{
		len_packet = min((size_t)NRF24L01_PACKET_MAX_LENGTH, (size_t)(len - readoffset));
		if((err = nrf24l01_send_packet(nrf, !!(filep->f_flags & O_NONBLOCK), data + readoffset, len_packet)))
			goto exit_dataalloc;
		readoffset += len_packet;
	}
	err = len;
exit_dataalloc:
	vfree(data);
exit_err:
	return err;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	struct nrf24l01_chrdev_session* session = (struct nrf24l01_chrdev_session*)filep->private_data;
	struct nrf24l01_chrdev* nrfchr = session->chrdev;
	vfree(session);
	mutex_lock(&nrfchr->m_session);
	nrfchr->num_sessions--;
	mutex_unlock(&nrfchr->m_session);
	wake_up(&nrfchr->exit_queue);
	return 0;
}

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release	
};

static DEVICE_ATTR(txpower, 0644, nrf24l01_sysfs_show_tx_pwr, nrf24l01_sysfs_store_tx_pwr);
static DEVICE_ATTR(channel, 0644, nrf24l01_sysfs_show_channel, nrf24l01_sysfs_store_channel);
static DEVICE_ATTR(datarate, 0644, nrf24l01_sysfs_show_dr, nrf24l01_sysfs_store_dr);

static struct attribute* attr_rf[] = {
	&dev_attr_txpower.attr,
	&dev_attr_channel.attr,
	&dev_attr_datarate.attr,
	NULL
};

static struct attribute_group group_rf = {
	.attrs = attr_rf,
	.name = "rf"
};

static DEVICE_ATTR(delay, 0644, nrf24l01_sysfs_show_retr_ard, nrf24l01_sysfs_store_retr_ard);
static DEVICE_ATTR(count, 0644, nrf24l01_sysfs_show_retr_arc, nrf24l01_sysfs_store_retr_arc);

static struct attribute* attr_retr[] = {
	&dev_attr_delay.attr,
	&dev_attr_count.attr,
	NULL
};

static struct attribute_group group_retr = {
	.attrs = attr_retr,
	.name = "retransmit"
};

static DEVICE_ATTR(address_width, 0644, nrf24l01_sysfs_show_addr_width, nrf24l01_sysfs_store_addr_width);
static DEVICE_ATTR(pwr_up, 0644, nrf24l01_sysfs_show_pwr_up, nrf24l01_sysfs_store_pwr_up);
static DEVICE_ATTR(gpio_ce, 0644, nrf24l01_sysfs_show_ce, nrf24l01_sysfs_store_ce);
static DEVICE_ATTR(crc, 0644, nrf24l01_sysfs_show_crc, nrf24l01_sysfs_store_crc);
static DEVICE_ATTR(tx_address, 0644, nrf24l01_sysfs_show_tx_address, nrf24l01_sysfs_store_tx_address);

static struct attribute* attr_general[] = {
	&dev_attr_address_width.attr,
	&dev_attr_pwr_up.attr,
	&dev_attr_gpio_ce.attr,
	&dev_attr_crc.attr,
	&dev_attr_tx_address.attr,
	NULL
};

static struct attribute_group group_general = {
	.attrs = attr_general,
	.name = NULL
};

static struct device_attribute attr_pipe0_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe0,
	.store = nrf24l01_sysfs_store_payload_width_pipe0
};

static struct device_attribute attr_pipe0_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe0,
	.store = nrf24l01_sysfs_store_address_pipe0
};

static struct device_attribute attr_pipe0_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe0,
	.store = nrf24l01_sysfs_store_enable_pipe0
};

static struct device_attribute attr_pipe0_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe0,
	.store = nrf24l01_sysfs_store_dynpd_pipe0
};

static struct device_attribute attr_pipe0_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe0,
	.store = nrf24l01_sysfs_store_enaa_pipe0
};

static struct attribute* attr_pipe0[] = {
	&attr_pipe0_pw.attr,
	&attr_pipe0_addr.attr,
	&attr_pipe0_state.attr,
	&attr_pipe0_dpl.attr,
	&attr_pipe0_enaa.attr,
	NULL
};

static struct attribute_group group_pipe0 = {
	.attrs = attr_pipe0,
	.name = "pipe0"
};

static struct device_attribute attr_pipe1_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe1,
	.store = nrf24l01_sysfs_store_payload_width_pipe1
};

static struct device_attribute attr_pipe1_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe1,
	.store = nrf24l01_sysfs_store_address_pipe1
};

static struct device_attribute attr_pipe1_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe1,
	.store = nrf24l01_sysfs_store_enable_pipe1
};

static struct device_attribute attr_pipe1_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe1,
	.store = nrf24l01_sysfs_store_dynpd_pipe1
};

static struct device_attribute attr_pipe1_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe1,
	.store = nrf24l01_sysfs_store_enaa_pipe1
};

static struct attribute* attr_pipe1[] = {
	&attr_pipe1_pw.attr,
	&attr_pipe1_addr.attr,
	&attr_pipe1_state.attr,
	&attr_pipe1_dpl.attr,
	&attr_pipe1_enaa.attr,
	NULL
};

static struct attribute_group group_pipe1 = {
	.attrs = attr_pipe1,
	.name = "pipe1"
};

static struct device_attribute attr_pipe2_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe2,
	.store = nrf24l01_sysfs_store_payload_width_pipe2
};

static struct device_attribute attr_pipe2_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe2,
	.store = nrf24l01_sysfs_store_address_pipe2
};

static struct device_attribute attr_pipe2_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe2,
	.store = nrf24l01_sysfs_store_enable_pipe2
};

static struct device_attribute attr_pipe2_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe2,
	.store = nrf24l01_sysfs_store_dynpd_pipe2
};

static struct device_attribute attr_pipe2_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe2,
	.store = nrf24l01_sysfs_store_enaa_pipe2
};

static struct attribute* attr_pipe2[] = {
	&attr_pipe2_pw.attr,
	&attr_pipe2_addr.attr,
	&attr_pipe2_state.attr,
	&attr_pipe2_dpl.attr,
	&attr_pipe2_enaa.attr,
	NULL
};

static struct attribute_group group_pipe2 = {
	.attrs = attr_pipe2,
	.name = "pipe2"
};

static struct device_attribute attr_pipe3_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe3,
	.store = nrf24l01_sysfs_store_payload_width_pipe3
};

static struct device_attribute attr_pipe3_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe3,
	.store = nrf24l01_sysfs_store_address_pipe3
};

static struct device_attribute attr_pipe3_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe3,
	.store = nrf24l01_sysfs_store_enable_pipe3
};

static struct device_attribute attr_pipe3_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe3,
	.store = nrf24l01_sysfs_store_dynpd_pipe3
};

static struct device_attribute attr_pipe3_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe3,
	.store = nrf24l01_sysfs_store_enaa_pipe3
};

static struct attribute* attr_pipe3[] = {
	&attr_pipe3_pw.attr,
	&attr_pipe3_addr.attr,
	&attr_pipe3_state.attr,
	&attr_pipe3_dpl.attr,
	&attr_pipe3_enaa.attr,
	NULL
};

static struct attribute_group group_pipe3 = {
	.attrs = attr_pipe3,
	.name = "pipe3"
};

static struct device_attribute attr_pipe4_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe4,
	.store = nrf24l01_sysfs_store_payload_width_pipe4
};

static struct device_attribute attr_pipe4_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe4,
	.store = nrf24l01_sysfs_store_address_pipe4
};

static struct device_attribute attr_pipe4_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe4,
	.store = nrf24l01_sysfs_store_enable_pipe4
};

static struct device_attribute attr_pipe4_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe4,
	.store = nrf24l01_sysfs_store_dynpd_pipe4
};

static struct device_attribute attr_pipe4_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe4,
	.store = nrf24l01_sysfs_store_enaa_pipe4
};

static struct attribute* attr_pipe4[] = {
	&attr_pipe4_pw.attr,
	&attr_pipe4_addr.attr,
	&attr_pipe4_state.attr,
	&attr_pipe4_dpl.attr,
	&attr_pipe4_enaa.attr,
	NULL
};

static struct attribute_group group_pipe4 = {
	.attrs = attr_pipe4,
	.name = "pipe4"
};

static struct device_attribute attr_pipe5_pw = {
	.attr = {
		.name = "payloadwidth",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_payload_width_pipe5,
	.store = nrf24l01_sysfs_store_payload_width_pipe5
};

static struct device_attribute attr_pipe5_addr = {
	.attr = {
		.name = "address",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_address_pipe5,
	.store = nrf24l01_sysfs_store_address_pipe5
};

static struct device_attribute attr_pipe5_state = {
	.attr = {
		.name = "enable",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enable_pipe5,
	.store = nrf24l01_sysfs_store_enable_pipe5
};

static struct device_attribute attr_pipe5_dpl = {
	.attr = {
		.name = "dynamicpayload",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_dynpd_pipe5,
	.store = nrf24l01_sysfs_store_dynpd_pipe5
};

static struct device_attribute attr_pipe5_enaa = {
	.attr = {
		.name = "autoack",
		.mode = 0644
	},
	.show = nrf24l01_sysfs_show_enaa_pipe5,
	.store = nrf24l01_sysfs_store_enaa_pipe5
};

static struct attribute* attr_pipe5[] = {
	&attr_pipe5_pw.attr,
	&attr_pipe5_addr.attr,
	&attr_pipe5_state.attr,
	&attr_pipe5_dpl.attr,
	&attr_pipe5_enaa.attr,
	NULL
};

static struct attribute_group group_pipe5 = {
	.attrs = attr_pipe5,
	.name = "pipe5"
};

static const struct attribute_group* attribute_groups[] = {
	&group_general,
	&group_rf,
	&group_retr,
	&group_pipe0,
	&group_pipe1,
	&group_pipe2,
	&group_pipe3,
	&group_pipe4,
	&group_pipe5,
	NULL
};

int chrdev_alloc(struct nrf24l01_t* nrf)
{
	int err;
	dev_t devnum;
	char dev_name[25];
	struct nrf24l01_chrdev* nrfchr = &nrf->chrdev;
	nrfchr->nrf = nrf;
	init_waitqueue_head(&nrfchr->exit_queue);
	mutex_init(&nrfchr->m_session);
	mutex_lock(&nrfchr->m_session);
	snprintf(dev_name, sizeof(dev_name), NRF24L01_CHRDEV_FORMAT, ((unsigned long) nrf->id) + 1);
	if((err = alloc_chrdev_region(&nrfchr->devt, 0, 1, dev_name)))
		goto exit_noalloc;
	if(!chrdev_class)
	{
		chrdev_class = class_create(THIS_MODULE, NRF24L01_CHRDEV_CLASS);
		if(IS_ERR(chrdev_class))
		{
			err = PTR_ERR(chrdev_class);
			goto exit_unregchrdev;
		}
	}
	cdev_init(&nrfchr->cdev, &fops);
	devnum = MKDEV(MAJOR(nrfchr->devt), MINOR(nrfchr->devt));
	nrfchr->dev = device_create_with_groups(chrdev_class, NULL, devnum, nrfchr, attribute_groups, dev_name);
	if(IS_ERR(nrfchr->dev))
	{
		err = PTR_ERR(nrfchr->dev);
		goto exit_unregclass;
	}
	if((err = cdev_add(&nrfchr->cdev, devnum, 1)))
		goto exit_destroydev;
	goto exit_noalloc;

exit_destroydev:
	device_destroy(chrdev_class, devnum);
exit_unregclass:	
	if(!nrf24l01_nrf_registered())
	{
		class_unregister(chrdev_class);
		class_destroy(chrdev_class);
	}
exit_unregchrdev:
	unregister_chrdev_region(MAJOR(nrfchr->devt), 1);
exit_noalloc:
	mutex_unlock(&nrfchr->m_session);
	return err;
}

void chrdev_free(struct nrf24l01_t* nrf)
{
	struct nrf24l01_chrdev* nrfchr = &nrf->chrdev;
	mutex_lock(&nrfchr->m_session);
	nrfchr->shutdown = true;
	mutex_unlock(&nrfchr->m_session);
	if(nrfchr->num_sessions > 0)
	{
		dev_warn(nrfchr->dev, "Waiting for %u active readers/writers to finish\n", nrfchr->num_sessions);
		wait_event(nrfchr->exit_queue, nrfchr->num_sessions == 0);
	}
	cdev_del(&nrfchr->cdev);
	device_destroy(chrdev_class, nrfchr->devt);
	if(!nrf24l01_nrf_registered())
	{
		class_unregister(chrdev_class);
		class_destroy(chrdev_class);
		chrdev_class = NULL;
	}
	unregister_chrdev_region(MAJOR(nrfchr->devt), 1);
}
