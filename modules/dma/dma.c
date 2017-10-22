#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/atomic.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/gpio.h>

#include "dma.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Example DMA driver");
MODULE_VERSION("0.1");

unsigned int dma_gpio;

dma_addr_t dma_mapping_gpio;
dma_addr_t dma_mapping_data;

struct dma_block* dma_iodata;

struct dma_slave_config dma_config;
struct dma_chan* dma_channel;

struct dma_async_tx_descriptor* dma_desc;
dma_cookie_t dma_cookie;

#define DMA_NUM_BLOCKS 1

void dma_complete(void* param) {
	printk(KERN_INFO "DMA finished\n");
}

static int dma_of_get_int(int* dest, struct device_node* of_node, const char* name)
{
	int err = -ENOENT;
	const void* of_prop;
	if((of_prop = of_get_property(of_node, name, NULL))) {
		err = 0;
		*dest = be32_to_cpup(of_prop);
	}
	return err;
}

static int dma_of_get_int_default(struct device_node* of_node, const char* name, int def) {
	dma_of_get_int(&def, of_node, name);
	return def;
}


static int dma_probe(struct platform_device* device)
{
	int err;
	dma_gpio = dma_of_get_int_default(device->dev.of_node, "dma-gpio", 14);
	if((err = gpio_request_one(dma_gpio, GPIOF_DIR_OUT, "dma-gpio"))) {
		dev_err(&device->dev, "Failed to allocate gpio %u\n", dma_gpio);
		goto exit_err;
	}

	dma_channel = dma_request_chan(&device->dev, "gpio");
	if(IS_ERR(dma_channel)) {
		err = PTR_ERR(dma_channel);
		dev_err(&device->dev, "Failed to allocate dma channel\n");
		goto gpio_alloced;
	}

	dev_info(&device->dev, "Got dma channel %d\n", dma_channel->chan_id);

	if(!(dma_iodata = dma_alloc_coherent(&device->dev, DMA_NUM_BLOCKS * sizeof(struct dma_block), &dma_mapping_data, GFP_KERNEL | GFP_DMA32))) {
		err = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate dma buffer\n");
		goto chan_alloced;
	}

	dma_mapping_gpio = BCM2835_PERIPHERAL_BUS_BASE + BCM2835_GPIO_OFFSET + BCM2835_GPIO_SET_OFFSET;

	dma_config = (struct dma_slave_config) {
			.direction = DMA_MEM_TO_DEV,
			.dst_addr = dma_mapping_gpio,
			.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,

	};

	if((err = dmaengine_slave_config(dma_channel, &dma_config))) {
		dev_err(&device->dev, "Failed to configure dma slave device\n");
		goto dma_alloced;
	}

	dma_desc = dma_channel->device->device_prep_dma_memcpy(dma_channel,
		dma_mapping_gpio,
		dma_mapping_data, sizeof(struct dma_block), DMA_PREP_INTERRUPT);

	dma_desc->callback = dma_complete;

	dma_cookie = dmaengine_submit(dma_desc);
	dma_async_issue_pending(dma_channel);

	return 0;

dma_configured:
dma_alloced:
	dma_unmap_single(&device->dev, dma_mapping_data, DMA_NUM_BLOCKS * sizeof(struct dma_block), DMA_BIDIRECTIONAL);
chan_alloced:
	dmaengine_terminate_sync(dma_channel);
	dma_release_channel(dma_channel);
gpio_alloced:
	gpio_free(dma_gpio);
exit_err:
	return err;
}

static int dma_remove(struct platform_device* device)
{
	dma_unmap_single(&device->dev, dma_mapping_data, DMA_NUM_BLOCKS * sizeof(struct dma_block), DMA_BIDIRECTIONAL);
	dmaengine_terminate_sync(dma_channel);
	dma_release_channel(dma_channel);
	gpio_free(dma_gpio);
	return 0;
}

static const struct of_device_id dma_of_match[] = {
	{ .compatible = "testing,dma" },
	{}
};


static struct platform_driver dma_driver = {
	.probe = dma_probe,
	.remove = dma_remove,
	.driver = {
		.name = "dmatest",
		.of_match_table = dma_of_match
	}
};

MODULE_DEVICE_TABLE(of, dma_of_match);

module_platform_driver(dma_driver);
