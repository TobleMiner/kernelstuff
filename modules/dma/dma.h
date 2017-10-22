#ifndef _DMA_H_
#define _DMA_H_

#define BCM2835_PERIPHERAL_BUS_BASE	0x7E000000
#define BCM2835_PERIPHERAL_BASE		0x3F000000
#define BCM2835_GPIO_OFFSET			0x00200000
#define BCM2835_GPIO_SET_OFFSET		0x1C
#define BCM2835_GPIO_CLR_OFFSET		0x28
#define BCM2835_REGISTER_BLOCK_SIZE	4096

struct dma_block {
	uint32_t set;
	uint32_t set1;
	uint32_t reserved;
	uint32_t clear;
};

#endif
