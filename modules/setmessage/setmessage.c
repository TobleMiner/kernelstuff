#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "../chrdev/chrdev.h"
#include "setmessage.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test kernel module");
MODULE_VERSION("0.1");

#define SETMSG_NAME "setmessage"
#define SETMSG_TIMES 100000

static int __init helloworld_init(void)
{
	int i;
	char* str = "Hello World\n";
	int len = strlen(str) * SETMSG_TIMES;
	char* data = vmalloc(len * sizeof(char));
	for(i = 0; i < SETMSG_TIMES; i++)
	{
		memcpy(data + i * strlen(str), str, strlen(str));
	}
	testchr_set_message(data, len);
	vfree(data);
	printk(KERN_INFO SETMSG_NAME ": Hello World!");
	return 0;
}

static void __exit helloworld_exit(void)
{
	printk(KERN_INFO SETMSG_NAME ": Bye");
}

module_init(helloworld_init);
module_exit(helloworld_exit);
