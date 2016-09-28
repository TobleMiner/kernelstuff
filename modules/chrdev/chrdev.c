#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "chrdev.h"

#define DEVICE_NAME "testchar"
#define CLASS_NAME "test"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test kernel chardev module");
MODULE_VERSION("0.1");

static DEFINE_MUTEX(test_mutex);

static int majorNumber;
static char* message;
static int message_len = 0;
static int dev_open_count;
static struct class* charClass = NULL;
static struct device* charDev = NULL;
static char* read_ptr;

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

static int __init chardev_init(void)
{
	printk(KERN_INFO "Testchar: Init\n");
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber < 0)
	{
		printk(KERN_ALERT "Testchar: Failed to allocate major number\n");
		return majorNumber;
	}
	printk(KERN_INFO "Testchar: Allocated majorNumber %d\n", majorNumber);

	charClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(charClass))
	{
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Testchar: Failed to create device class\n");
		return PTR_ERR(charClass);
	}
	
	charDev = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(charDev))
	{
		class_destroy(charClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Testchar: Failed to create device\n");
		return PTR_ERR(charDev);
	}
	printk(KERN_INFO "Testchar: Device created\n");
	mutex_init(&test_mutex);
	return 0;
}

static void __exit chardev_exit(void)
{
	device_destroy(charClass, MKDEV(majorNumber, 0));
	class_unregister(charClass);
	class_destroy(charClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	if(message_len > 0)
		kfree(message);
	mutex_unlock(&test_mutex);
	printk(KERN_INFO "Testchar: shutting down\n");
}

static int dev_open(struct inode* inodep, struct file *filep)
{
	if(!mutex_trylock(&test_mutex))
	{
		printk(KERN_ALERT "Testchar: Device busy");
		return -EBUSY;
	}
	dev_open_count++;
	read_ptr = message;
	printk(KERN_INFO "Testchar: Device has been opened %d times", dev_open_count);
	return 0;
}

ssize_t testchr_set_message(char* buffer, size_t len)
{
	printk(KERN_INFO "Testchar: Got foreign pointer 0x%x, size: %d\n", buffer, len);
	msg_buff_alloc(len);
    message_len = 0;
	if(message == NULL)
	{
		printk(KERN_INFO "Testchar: failed to allocate memory");
		return -ENOMEM;
	}
    message_len = len;
	memcpy(message, buffer, len);
	printk(KERN_INFO "Testchar: Stored %zu bytes\n", len);
	return len;
}

EXPORT_SYMBOL(testchr_set_message);

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	if(message_len == 0)
		return 0;
	if(read_ptr - message == message_len)
		return 0;
	size_t bytes_read = message_len - (read_ptr - message);
	if(len < bytes_read)
		bytes_read = len;
	int error_count;
	error_count = copy_to_user(buffer, read_ptr, len);
	read_ptr += bytes_read;
	if(error_count > 0)
	{
		printk(KERN_INFO "Testchar: Failed to copy %zu bytes to userspace", bytes_read);
		return -EFAULT;
	}
	printk(KERN_INFO "Testchar: Copied %zu bytes to userspace\n", bytes_read);
	return message_len;
}

static void msg_buff_alloc(size_t len)
{
	if(message_len > 0)
		message = krealloc(message, len, GFP_KERNEL);
	else
		message = kmalloc(len, GFP_KERNEL);
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	msg_buff_alloc(len);
	message_len = 0;
	if(message == NULL)
	{
		printk(KERN_INFO "Testchar: failed to allocate memory");
		return -ENOMEM;
	}
	message_len = len;
	read_ptr = message;
	printk(KERN_INFO "Testchar: Allocated %zu bytes\n", len);
	ulong errors = copy_from_user(message, buffer, len);
	if(errors > 0)
	{
		printk(KERN_INFO "Testchar: Failed to copy %lu bytes to memory", errors);
		return -EFAULT;
	}
	printk(KERN_INFO "Testchar: Stored %zu bytes\n", len);
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "Testchar: Device closed");
	mutex_unlock(&test_mutex);
	return 0;
}

module_init(chardev_init);
module_exit(chardev_exit);
