#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test kernel module");
MODULE_VERSION("0.1");

static char* name = "Bernd"; 
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Name the module indentifies itself with");

static int __init helloworld_init(void)
{
	printk(KERN_INFO "%s: Hello World!", name);
	return 0;
}

static void __exit helloworld_exit(void)
{
	printk(KERN_INFO "%s: Bye", name);
}

module_init(helloworld_init);
module_exit(helloworld_exit);
