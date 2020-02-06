#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/consumer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Set pin drive strength on rockchip platforms");
MODULE_VERSION("1.0");

static int gpio = -1;
module_param(gpio, int, S_IRUGO);
MODULE_PARM_DESC(gpio, "GPIO to set");

static int drive_strength = -1;
module_param(drive_strength, int, S_IRUGO);
MODULE_PARM_DESC(drive_strength, "Drive strength to set");
	
static int __init setdrv_init(void) {
  int err = 0;
  unsigned long packed;
  printk(KERN_INFO "Setting drive strength of gpio %d to %d\n", gpio, drive_strength);

  if(gpio < 0) {
    printk(KERN_ERR "gpio must be >= 0\n");
    return -EINVAL;
  }

  if(drive_strength < 0) {
    printk(KERN_ERR "drive_strength must be >= 0\n");
    return -EINVAL;
  }

  packed = pinconf_to_config_packed(PIN_CONFIG_DRIVE_STRENGTH, drive_strength);
  err = pinctrl_gpio_set_config(gpio, packed);
  if(err) {
    printk(KERN_ERR "Failed to set drive strength: %d\n", err);
    return err;
  }

  return -1;
}

static void __exit setdrv_exit(void) {
}

module_init(setdrv_init);
module_exit(setdrv_exit);
