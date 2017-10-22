#ifndef _BCM2835_H_
#define _BCM2835_H_

#include <linux/dmaengine.h>

#define BCM2835_DMA_MAX_DMA_CHAN_SUPPORTED 14
#define BCM2835_DMA_CHAN_NAME_SIZE 8

// ========= VIRT DMA =========

struct virt_dma_desc {
	struct dma_async_tx_descriptor tx;
	/* protected by vc.lock */
	struct list_head node;
};

struct virt_dma_chan {
	struct dma_chan	chan;
	struct tasklet_struct task;
	void (*desc_free)(struct virt_dma_desc *);

	spinlock_t lock;

	/* protected by vc.lock */
	struct list_head desc_allocated;
	struct list_head desc_submitted;
	struct list_head desc_issued;
	struct list_head desc_completed;

	struct virt_dma_desc *cyclic;
};


// ========= BCM2835 ==========
struct bcm2835_dmadev {
	struct dma_device ddev;
	spinlock_t lock;
	void __iomem *base;
	struct device_dma_parameters dma_parms;
};

struct bcm2835_dma_cb {
	uint32_t info;
	uint32_t src;
	uint32_t dst;
	uint32_t length;
	uint32_t stride;
	uint32_t next;
	uint32_t pad[2];
};

struct bcm2835_cb_entry {
	struct bcm2835_dma_cb *cb;
	dma_addr_t paddr;
};

struct bcm2835_chan {
	struct virt_dma_chan vc;
	struct list_head node;

	struct dma_slave_config	cfg;
	unsigned int dreq;

	int ch;
	struct bcm2835_desc *desc;
	struct dma_pool *cb_pool;

	void __iomem *chan_base;
	int irq_number;
	unsigned int irq_flags;

	bool is_lite_channel;
};

struct bcm2835_desc {
	struct bcm2835_chan *c;
	struct virt_dma_desc vd;
	enum dma_transfer_direction dir;

	unsigned int frames;
	size_t size;

	bool cyclic;

	struct bcm2835_cb_entry cb_list[];
};

#define BCM2835_DMA_CS		0x00
#define BCM2835_DMA_ADDR	0x04
#define BCM2835_DMA_TI		0x08
#define BCM2835_DMA_SOURCE_AD	0x0c
#define BCM2835_DMA_DEST_AD	0x10
#define BCM2835_DMA_LEN		0x14
#define BCM2835_DMA_STRIDE	0x18
#define BCM2835_DMA_NEXTCB	0x1c
#define BCM2835_DMA_DEBUG	0x20

/* DMA CS Control and Status bits */
#define BCM2835_DMA_ACTIVE	BIT(0)  /* activate the DMA */
#define BCM2835_DMA_END		BIT(1)  /* current CB has ended */
#define BCM2835_DMA_INT		BIT(2)  /* interrupt status */
#define BCM2835_DMA_DREQ	BIT(3)  /* DREQ state */
#define BCM2835_DMA_ISPAUSED	BIT(4)  /* Pause requested or not active */
#define BCM2835_DMA_ISHELD	BIT(5)  /* Is held by DREQ flow control */
#define BCM2835_DMA_WAITING_FOR_WRITES BIT(6) /* waiting for last
					       * AXI-write to ack
					       */
#define BCM2835_DMA_ERR		BIT(8)
#define BCM2835_DMA_PRIORITY(x) ((x & 15) << 16) /* AXI priority */
#define BCM2835_DMA_PANIC_PRIORITY(x) ((x & 15) << 20) /* panic priority */
/* current value of TI.BCM2835_DMA_WAIT_RESP */
#define BCM2835_DMA_WAIT_FOR_WRITES BIT(28)
#define BCM2835_DMA_DIS_DEBUG	BIT(29) /* disable debug pause signal */
#define BCM2835_DMA_ABORT	BIT(30) /* Stop current CB, go to next, WO */
#define BCM2835_DMA_RESET	BIT(31) /* WO, self clearing */

/* Transfer information bits - also bcm2835_cb.info field */
#define BCM2835_DMA_INT_EN	BIT(0)
#define BCM2835_DMA_TDMODE	BIT(1) /* 2D-Mode */
#define BCM2835_DMA_WAIT_RESP	BIT(3) /* wait for AXI-write to be acked */
#define BCM2835_DMA_D_INC	BIT(4)
#define BCM2835_DMA_D_WIDTH	BIT(5) /* 128bit writes if set */
#define BCM2835_DMA_D_DREQ	BIT(6) /* enable DREQ for destination */
#define BCM2835_DMA_D_IGNORE	BIT(7) /* ignore destination writes */
#define BCM2835_DMA_S_INC	BIT(8)
#define BCM2835_DMA_S_WIDTH	BIT(9) /* 128bit writes if set */
#define BCM2835_DMA_S_DREQ	BIT(10) /* enable SREQ for source */
#define BCM2835_DMA_S_IGNORE	BIT(11) /* ignore source reads - read 0 */
#define BCM2835_DMA_BURST_LENGTH(x) ((x & 15) << 12)
#define BCM2835_DMA_PER_MAP(x)	((x & 31) << 16) /* REQ source */
#define BCM2835_DMA_WAIT(x)	((x & 31) << 21) /* add DMA-wait cycles */
#define BCM2835_DMA_NO_WIDE_BURSTS BIT(26) /* no 2 beat write bursts */

/* debug register bits */
#define BCM2835_DMA_DEBUG_LAST_NOT_SET_ERR	BIT(0)
#define BCM2835_DMA_DEBUG_FIFO_ERR		BIT(1)
#define BCM2835_DMA_DEBUG_READ_ERR		BIT(2)
#define BCM2835_DMA_DEBUG_OUTSTANDING_WRITES_SHIFT 4
#define BCM2835_DMA_DEBUG_OUTSTANDING_WRITES_BITS 4
#define BCM2835_DMA_DEBUG_ID_SHIFT		16
#define BCM2835_DMA_DEBUG_ID_BITS		9
#define BCM2835_DMA_DEBUG_STATE_SHIFT		16
#define BCM2835_DMA_DEBUG_STATE_BITS		9
#define BCM2835_DMA_DEBUG_VERSION_SHIFT		25
#define BCM2835_DMA_DEBUG_VERSION_BITS		3
#define BCM2835_DMA_DEBUG_LITE			BIT(28)

/* shared registers for all dma channels */
#define BCM2835_DMA_INT_STATUS         0xfe0
#define BCM2835_DMA_ENABLE             0xff0

#define BCM2835_DMA_DATA_TYPE_S8	1
#define BCM2835_DMA_DATA_TYPE_S16	2
#define BCM2835_DMA_DATA_TYPE_S32	4
#define BCM2835_DMA_DATA_TYPE_S128	16

/* Valid only for channels 0 - 14, 15 has its own base address */
#define BCM2835_DMA_CHAN(n)	((n) << 8) /* Base address */
#define BCM2835_DMA_CHANIO(base, n) ((base) + BCM2835_DMA_CHAN(n))

/* the max dma length for different channels */
#define MAX_DMA_LEN SZ_1G
#define MAX_LITE_DMA_LEN (SZ_64K - 4)

#define BCM2835_FROM_DMA(dev) container_of((dev), struct bcm2835_dmadev, ddev)
#define BCM2835_FROM_CHAN(chan) container_of((chan), struct bcm2835_chan, vc.chan)
#define BCM2835_DESC_FROM_TX(tx) container_of((tx), struct bcm2835_desc, vd.tx)

#define DMA_CB_TXFR_LEN_YLENGTH(y) (((y - 1) & 0x3fff) << 16)
#define DMA_CB_TXFR_LEN_XLENGTH(x) ((x)&0xffff)

#define DMA_CB_STRIDE_D_STRIDE(x) (((x)&0xffff) << 16)
#define DMA_CB_STRIDE_S_STRIDE(x) ((x)&0xffff)

#endif
