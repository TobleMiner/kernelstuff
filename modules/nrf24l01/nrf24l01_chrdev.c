#include <linux/fs.h>
#include <linux/device.h>

#include "nrf24l01_core.h"

#define NRF24L01_CHRDEV_NAME "nrf24l01"
#define NRF24L01_CHRDEV_CLASS "nrf24"

static struct file_operations fops =
{
	
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
	nrf->chrdev = device_create(nrf->chrclass, NULL, MKDEV(nrf->chrdev_major, 0), NULL, NRF24L01_CHRDEV_NAME);
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
