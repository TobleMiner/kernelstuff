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
#include <linux/kallsyms.h>

#include "dma.h"
#include "bcm2835.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Example DMA driver");
MODULE_VERSION("0.1");

dma_addr_t dma_mapping_gpio;
dma_addr_t dma_mapping_data;

struct dma_block* dma_iodata;

struct dma_slave_config dma_config;
struct dma_chan* dma_channel;

struct dma_async_tx_descriptor* dma_desc;
dma_cookie_t dma_cookie;

#define DMA_BLOCK_SIZE (65536 / 2)
#define DMA_NUM_BLOCKS (DMA_BLOCK_SIZE / sizeof(struct dma_block))

#define DMA_NUM_GPIOS 14

unsigned int dma_gpios[DMA_NUM_GPIOS] = {
	// Address lines
	22, 23, 24, 25, 15,
	// Misc
	18, 4, 17,
	// Chain 0
	11, 8, 27, 9, 7, 10
};

void multi_dma_complete(void* param) {
	printk(KERN_INFO "Multi DMA finished\n");
}

void dma_complete(void* param) {
	printk(KERN_INFO "DMA finished\n");
}

static int dma_probe(struct platform_device* device)
{
	int err, i, gpio_count;
	struct list_head* dma_channels;
	uint32_t gpio_bitmask = 0;
	struct bcm2835_desc* desc;
	struct bcm2835_dma_cb* control_block;
	struct bcm2835_chan* bcm_channel;
	struct dma_device* dev_cursor;
	struct dma_chan* chan_cursor;

	for(gpio_count = 0; gpio_count < DMA_NUM_GPIOS; gpio_count++) {
		if((err = gpio_request_one(dma_gpios[gpio_count], GPIOF_DIR_OUT, NULL))) {
			dev_err(&device->dev, "Failed to allocate gpio %u\n", dma_gpios[gpio_count]);
			goto gpio_alloced;
		}
		gpio_bitmask |= (1 << dma_gpios[gpio_count]);
	}

	struct list_head* dma_chan_list = (struct list_head*)kallsyms_lookup_name("dma_device_list");
	if(dma_chan_list) {
		list_for_each_entry(dev_cursor, dma_chan_list, global_node) {
			dev_info(&device->dev, "Got DMA device with %d channels\n", dev_cursor->chancnt);
			list_for_each_entry(chan_cursor, &dev_cursor->channels, device_node) {
				dev_info(&device->dev, "Got channel %d with %d users\n", chan_cursor->chan_id, chan_cursor->client_count);
				bcm_channel = container_of(chan_cursor, struct bcm2835_chan, vc.chan);
				dev_info(&device->dev, "\tActual channel: %d is_lite: %d\n", bcm_channel->ch, bcm_channel->is_lite_channel);
			}
		}
	} else {
		dev_warn(&device->dev, "Failed to fetch dma device list\n");
	}

	dma_channel = dma_request_chan(&device->dev, "gpio");
	if(IS_ERR(dma_channel)) {
		err = PTR_ERR(dma_channel);
		dev_err(&device->dev, "Failed to allocate dma channel\n");
		goto gpio_alloced;
	}

	dev_info(&device->dev, "Got DMA channel %d\n", dma_channel->chan_id);

	dev_info(&device->dev, "Actual DMA channel: %d\n", container_of(dma_channel, struct bcm2835_chan, vc.chan)->ch);
	dev_info(&device->dev, "Is lite channel: %d\n", container_of(dma_channel, struct bcm2835_chan, vc.chan)->is_lite_channel);

	if(!(dma_iodata = dma_alloc_coherent(&device->dev, DMA_NUM_BLOCKS * sizeof(struct dma_block), &dma_mapping_data, GFP_KERNEL | GFP_DMA32))) {
		err = -ENOMEM;
		dev_err(&device->dev, "Failed to allocate dma buffer\n");
		goto chan_alloced;
	}

	memset(dma_iodata, 0x00, DMA_NUM_BLOCKS * sizeof(struct dma_block));

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

	dev_info(&device->dev, "Sending 5 conventional high pulse\n");
	for(i = 0; i < 5; i++) {
		for(gpio_count = 0; gpio_count < DMA_NUM_GPIOS; gpio_count++) {
			gpio_set_value(gpio_count, 0);
			gpio_set_value(gpio_count, 1);
			gpio_set_value(gpio_count, 0);
		}
	}
	dev_info(&device->dev, "Sent 5 conventional high pulse\n");

	usleep_range(10000, 50000);

	for(i = 0; i < DMA_NUM_BLOCKS; i++) {
		dma_iodata[i].set = BIT(ADAMTX_GPIO_CLK);
		dma_iodata[i].clear = BIT(ADAMTX_GPIO_CLK);
		if(i == 0/*((i * sizeof(struct dma_block)) % DMA_BLOCK_SIZE) && i == 0*/) {
			dma_iodata[i].set |= BIT(ADAMTX_GPIO_STR);
			dma_iodata[i].clear |= BIT(ADAMTX_GPIO_STR);
		}
		if(i % 2) {
			dma_iodata[i].set |= BIT(ADAMTX_GPIO_A);
			dma_iodata[i].clear |= BIT(ADAMTX_GPIO_A);
		}
		if(i % 3) {
			dma_iodata[i].set |= BIT(ADAMTX_GPIO_B);
			dma_iodata[i].clear |= BIT(ADAMTX_GPIO_B);
		}
		if(i % 4) {
			dma_iodata[i].set |= BIT(ADAMTX_GPIO_C);
			dma_iodata[i].clear |= BIT(ADAMTX_GPIO_C);
		}
		if(i % 5) {
			dma_iodata[i].set |= BIT(ADAMTX_GPIO_D);
			dma_iodata[i].clear |= BIT(ADAMTX_GPIO_D);
		}
	}

	dev_info(&device->dev, "Sending 5 DMA high pulses via single DMA\n");

/*	for(i = 0; i < DMA_NUM_BLOCKS; i++) {
		dma_desc = dma_channel->device->device_prep_dma_memcpy(dma_channel,
			dma_mapping_gpio,
			dma_mapping_data + i * sizeof(struct dma_block), sizeof(struct dma_block), DMA_PREP_INTERRUPT);

		dma_desc->callback = dma_complete;

		dma_cookie = dmaengine_submit(dma_desc);
	}
	dma_async_issue_pending(dma_channel);
*/
	dev_info(&device->dev, "Sent 5 DMA high pulses via single DMA\n");

	usleep_range(10000, 50000);

	dev_info(&device->dev, "Sending 5 DMA high pulses via multi DMA\n");
/*	dma_desc = dma_channel->device->device_prep_dma_memcpy(dma_channel,
		dma_mapping_gpio,
		dma_mapping_data, DMA_NUM_BLOCKS * sizeof(struct dma_block), DMA_PREP_INTERRUPT);
*/
	dma_desc = dmaengine_prep_dma_cyclic(dma_channel,
		dma_mapping_data, DMA_NUM_BLOCKS * sizeof(struct dma_block),
		DMA_BLOCK_SIZE, DMA_MEM_TO_DEV, 0);

	dma_desc->callback = multi_dma_complete;

	desc = container_of(dma_desc, struct bcm2835_desc, vd.tx);

	printk(KERN_INFO "Got %u frames in chain\n", desc->frames);
	for(i = 0; i < desc->frames; i++) {
		control_block = desc->cb_list[i].cb;
		printk(KERN_INFO "Got control block with transfer length %zu\n", control_block->length);
		control_block->info = BCM2835_DMA_TDMODE | BCM2835_DMA_NO_WIDE_BURSTS | BCM2835_DMA_D_INC | BCM2835_DMA_S_INC/* | BCM2835_DMA_INT_EN*/;
		control_block->length = DMA_CB_TXFR_LEN_YLENGTH(control_block->length / sizeof(struct dma_block)) | DMA_CB_TXFR_LEN_XLENGTH((uint16_t)sizeof(struct dma_block));
		control_block->stride = DMA_CB_STRIDE_D_STRIDE(-((int16_t)sizeof(struct dma_block))) | DMA_CB_STRIDE_S_STRIDE(0);

		printk("DMA flags: 0x%x\n", control_block->info);

		printk(KERN_INFO "DMA X LENGTH: %u\n", (((uint32_t*)control_block)[3]) & 0xFFFF);
		printk(KERN_INFO "DMA Y LENGTH: %u\n", (((uint32_t*)control_block)[3]) >> 16 & 0x3FFF);

		printk(KERN_INFO "DMA S STRIDE: %d\n", (int16_t)(((uint32_t*)control_block)[4] & 0xFFFF));
		printk(KERN_INFO "DMA D STRIDE: %d\n", (int16_t)(((uint32_t*)control_block)[4] >> 16 & 0xFFFF));

		//control_block->src = dma_mapping_data;
		control_block->dst = dma_mapping_gpio;

/*		struct dma_block* dma_block = phys_to_virt((phys_addr_t)control_block[1]);
		dma_block->set = BIT(ADAMTX_GPIO_STR);
		dma_block->clear = BIT(ADAMTX_GPIO_STR);
*/
		printk(KERN_INFO "DMA DST: 0x%x\n", ((uint32_t*)control_block)[2]);
		printk(KERN_INFO "DMA SRC: 0x%x\n", ((uint32_t*)control_block)[1]);
		printk(KERN_INFO "DMA SRC end: 0x%x\n", dma_mapping_data + DMA_NUM_BLOCKS * sizeof(struct dma_block));

//		control_block->next = __pa(control_block);
//		break;
	}

	dma_cookie = dmaengine_submit(dma_desc);
	dma_async_issue_pending(dma_channel);

	dev_info(&device->dev, "Sent 5 DMA high pulses via multi DMA\n");


	return 0;

dma_started:
	dmaengine_terminate_sync(dma_channel);
dma_configured:
dma_alloced:
	dma_free_coherent(&device->dev, DMA_NUM_BLOCKS * sizeof(struct dma_block), dma_iodata, dma_mapping_data);
chan_alloced:
	dma_release_channel(dma_channel);
gpio_alloced:
	for(i = 0; i <= gpio_count; i++) {
		gpio_free(dma_gpios[i]);
	}
exit_err:
	return err;
}

static int dma_remove(struct platform_device* device)
{
	int i;
	while(dmaengine_terminate_sync(dma_channel)) {
		dev_warn(&device->dev, "Failed to terminate DMA, retrying...");
		msleep(500);
	}
	dma_release_channel(dma_channel);
	dma_free_coherent(&device->dev, DMA_NUM_BLOCKS * sizeof(struct dma_block), dma_iodata, dma_mapping_data);
	for(i = 0; i < DMA_NUM_GPIOS; i++) {
		gpio_free(dma_gpios[i]);
	}
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
