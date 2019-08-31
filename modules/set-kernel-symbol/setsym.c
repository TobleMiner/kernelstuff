#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Set integer kernel symbol to a specific value");
MODULE_VERSION("1.0");

static char* name = "";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Name of the symbol to set");

static int type = -1;
module_param(type, int, S_IRUGO);
MODULE_PARM_DESC(type, "Type of the symbol, byte = 0, short = 1, int = 2, long = 3, must be specified");

static unsigned long val = 0;
module_param(val, ulong, S_IRUGO);
MODULE_PARM_DESC(val, "Value to set the symbol to");

enum {
  SYMBOL_TYPE_BYTE = 0,
  SYMBOL_TYPE_SHORT,
  SYMBOL_TYPE_INT,
  SYMBOL_TYPE_LONG,
  SYMBOL_TYPE_MAX
};

static int __init setsym_init(void) {
  unsigned long symaddr;
	printk(KERN_INFO "Setting value of symbol %s to 0x%lx\n", name, val);

  symaddr = kallsyms_lookup_name(name);
  if(symaddr == 0) {
  	printk(KERN_WARNING "Symbol %s not found :(\n", name);
    return -2;
  }

  switch(type) {
    case SYMBOL_TYPE_BYTE:
      *((unsigned char*)(symaddr)) = (unsigned char)val;
      break;
    case SYMBOL_TYPE_SHORT:
      *((unsigned short*)(symaddr)) = (unsigned short)val;
      break;
    case SYMBOL_TYPE_INT:
      *((unsigned int*)(symaddr)) = (unsigned int)val;
      break;
    case SYMBOL_TYPE_LONG:
      *((unsigned long*)(symaddr)) = (unsigned long)val;
      break;
    case -1:
    	printk(KERN_WARNING "Symbol data type not specified\n");
      return -3;
    default:
    	printk(KERN_WARNING "Invalid symbol data type\n");
      return -4;
  }

	printk(KERN_INFO "Set value of symbol %s to 0x%lx sucessfully\n", name, val);

	return -1;
}

static void __exit setsym_exit(void) {
}

module_init(setsym_init);
module_exit(setsym_exit);
