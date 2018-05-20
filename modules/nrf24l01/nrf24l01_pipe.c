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

#define NRF24L01_PIPE_DEV_FORMAT "nrf24l0%lup%u"
#define NRF24L01_CHRDEV_CLASS "nrf24"

extern struct class* chrdev_class;

static int dev_pipe_open(struct inode* inodep, struct file *filep)
{
	int err = 0;
	struct nrf24l01_chrdev_pipe_session* session;
	struct nrf24l01_chrdev_pipe* pipechr = (struct nrf24l01_chrdev_pipe*)container_of(inodep->i_cdev, struct nrf24l01_chrdev_pipe, cdev);
	mutex_lock(&pipechr->m_session);
	if(pipechr->flags.shutdown)
	{
		err = -ESHUTDOWN;
		goto exit_err;
	}
	pipechr->num_sessions++;
	session = vzalloc(sizeof(struct nrf24l01_chrdev_pipe_session));
	if(!session)
	{
		err = -ENOMEM;
		goto exit_err;
	}
	session->pipedev = pipechr;
	filep->private_data = session;
exit_err:
	mutex_unlock(&pipechr->m_session);
	return err;
}

static ssize_t dev_pipe_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	unsigned long lenoffset;
	ssize_t err, readlen;
	char* data;
	struct nrf24l01_chrdev_pipe_session* session = (struct nrf24l01_chrdev_pipe_session*)filep->private_data;
	struct nrf24l01_t* nrf = session->pipedev->nrf;

	data = vmalloc(len);
	if(!data)
	{
		err = -ENOMEM;
		goto exit_err;
	}
	if((readlen = nrf24l01_read_fifo(nrf, session->pipedev->pipe_no, !!(filep->f_flags & O_NONBLOCK), data, (size_t)len)) < 0)
	{
		err = readlen;
		goto exit_dataalloc;
	}
	if((lenoffset = copy_to_user(buffer, data, readlen)))
		dev_warn(nrf->chrdev.dev, "%lu of %zu bytes could not be copied to userspace\n", lenoffset, len);
	session->read_offset += readlen;
	err = readlen;
exit_dataalloc:
	vfree(data);
exit_err:
	return err;
}

static int dev_pipe_release(struct inode *inodep, struct file *filep)
{
	struct nrf24l01_chrdev_pipe_session* session = (struct nrf24l01_chrdev_pipe_session*)filep->private_data;
	struct nrf24l01_chrdev_pipe* pipechr = session->pipedev;
	vfree(session);
	mutex_lock(&pipechr->m_session);
	pipechr->num_sessions--;
	mutex_unlock(&pipechr->m_session);
	wake_up(&pipechr->exit_queue);
	return 0;
}

static struct file_operations fops =
{
	.open = dev_pipe_open,
	.read = dev_pipe_read,
	// May be useful for ack payloads later
	.write = NULL,
	.release = dev_pipe_release
};

int chrdev_pipe_alloc(struct nrf24l01_t* nrf, unsigned int pipe_no) {
	int err = 0;
	struct nrf24l01_chrdev* nrfchr = &nrf->chrdev;
	struct nrf24l01_chrdev_pipe* pipechr = &nrf->pipes[pipe_no].chrdev;
	pipechr->nrf = nrf;
	pipechr->pipe_no = pipe_no;
	init_waitqueue_head(&pipechr->exit_queue);
	mutex_init(&pipechr->m_session);
	mutex_lock(&pipechr->m_session);
	cdev_init(&pipechr->cdev, &fops);
	pipechr->devt = MKDEV(MAJOR(nrfchr->devt), MINOR(nrfchr->devt) + 1 + pipe_no);
	pipechr->dev = device_create(chrdev_class, nrfchr->dev, pipechr->devt, pipechr, NRF24L01_PIPE_DEV_FORMAT, ((unsigned long)nrf->id) + 1, pipe_no);
	if(IS_ERR(pipechr->dev))
	{
		err = PTR_ERR(pipechr->dev);
		goto exit_noalloc;
	}
	if((err = cdev_add(&pipechr->cdev, pipechr->devt, 1))) {
		goto exit_destroydev;
	}
	goto exit_noalloc;

exit_destroydev:
	device_destroy(chrdev_class, pipechr->devt);
exit_noalloc:
	mutex_unlock(&pipechr->m_session);
	return err;
}

void chrdev_pipe_free(struct nrf24l01_chrdev_pipe* pipechr)
{
	mutex_lock(&pipechr->m_session);
	// Prevent any state changes
	pipechr->flags.shutdown = 1;
	mutex_unlock(&pipechr->m_session);
	if(pipechr->num_sessions > 0) {
		dev_warn(pipechr->dev, "Waiting for %u active readers/writers to finish\n", pipechr->num_sessions);
		wait_event(pipechr->exit_queue, pipechr->num_sessions == 0);
	}
	cdev_del(&pipechr->cdev);
	device_destroy(chrdev_class, pipechr->devt);
}
