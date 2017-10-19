#ifndef _ADAMTX_H
#define _ADAMTX_H

#include <linux/ktime.h>

#include "matrix.h"
#include "../dummyfb/dummyfb.h"

#define ADAMTX_NAME "adafruit-matrix"

// chain 0
#define ADAMTX_GPIO_C0_R1	11
#define ADAMTX_GPIO_C0_R2	8
#define ADAMTX_GPIO_C0_G1	27
#define ADAMTX_GPIO_C0_G2	9
#define ADAMTX_GPIO_C0_B1	7
#define ADAMTX_GPIO_C0_B2	10

// chain 1
#define ADAMTX_GPIO_C1_R1	12
#define ADAMTX_GPIO_C1_R2	19
#define ADAMTX_GPIO_C1_G1	5
#define ADAMTX_GPIO_C1_G2	13
#define ADAMTX_GPIO_C1_B1	6
#define ADAMTX_GPIO_C1_B2	20

// chain 2
#define ADAMTX_GPIO_C2_R1	14
#define ADAMTX_GPIO_C2_R2	26
#define ADAMTX_GPIO_C2_G1	2
#define ADAMTX_GPIO_C2_G2	16
#define ADAMTX_GPIO_C2_B1	3
#define ADAMTX_GPIO_C2_B2	21

// address lines
#define ADAMTX_GPIO_A	22
#define ADAMTX_GPIO_B	23
#define ADAMTX_GPIO_C	24
#define ADAMTX_GPIO_D	25
#define ADAMTX_GPIO_E	15

// misc
#define ADAMTX_GPIO_OE	18
#define ADAMTX_GPIO_STR	4
#define ADAMTX_GPIO_CLK	17


#define ADAMTX_GPIO_OFFSET_ADDRESS 22

#define ADAMTX_GPIO_MASK_CLOCK		(1 << ADAMTX_GPIO_CLK)
#define ADAMTX_GPIO_MASK_ADDRESS	0b0011110000001000000000000000
#define ADAMTX_GPIO_MASK_ADDRESS_HI	0b0011110000000000000000000000
#define ADAMTX_GPIO_MASK_DATA		0b1000000000000000111110000000


// Matrix parameters
#define ADAMTX_PWM_BITS		8
#define ADAMTX_DEPTH		ADAMTX_PWM_BITS * 3
#define ADAMTX_BCD_TIME_NS	2000UL

// Macros
#define ADAMTX_BITS_TO_BYTES(bits) (bits >> 3)
#define ADAMTX_PIX_LEN ADAMTX_BITS_TO_BYTES(ADAMTX_DEPTH)
#define ADAMTX_KTIME_DIFF(now, last) ((now).tv_sec - (last).tv_sec) * 1000000000UL + ((now).tv_nsec - (last).tv_nsec)

// Typdedefs
struct adamtx_panel_io {
	uint32_t GPIO0	: 1;
	uint32_t GPIO1	: 1;
	uint32_t C2_G1	: 1;
	uint32_t C2_B1	: 1;
	uint32_t STR	: 1;
	uint32_t C1_G1	: 1;
	uint32_t C1_B1	: 1;
	uint32_t C0_B1	: 1;
	uint32_t C0_R2	: 1;
	uint32_t C0_G2	: 1;
	uint32_t C0_B2	: 1;
	uint32_t C0_R1	: 1;
	uint32_t C1_R1	: 1;
	uint32_t C1_G2	: 1;
	uint32_t C2_R1	: 1;
	uint32_t E		: 1;
	uint32_t C2_G2	: 1;
	uint32_t CLK	: 1;
	uint32_t OE		: 1;
	uint32_t C1_R2	: 1;
	uint32_t C1_B2	: 1;
	uint32_t C2_B2	: 1;
	uint32_t A		: 1;
	uint32_t B		: 1;
	uint32_t C		: 1;
	uint32_t D		: 1;
	uint32_t C2_R2	: 1;
	uint32_t C0_G1	: 1;
};

struct adamtx_update_param {
	long rate;
};

struct adamtx_draw_param {
	long rate;
};

struct adamtx_enabled_chains {
	uint8_t chain0: 1;
	uint8_t chain1: 1;
	uint8_t chain2: 1;
};

struct adamtx {
	struct list_head panels;
	struct adamtx_panel_io* paneldata;
	struct adamtx_panel_io* paneldata_out;

	char* framedata;
	struct dummyfb* dummyfb;

	struct adamtx_update_param update_param;
	struct task_struct* update_thread;
	bool update_thread_bind;
	unsigned int update_thread_cpu;

	unsigned long update_remap_ns_per_line;
	unsigned long update_prerender_ns_per_line;

	struct adamtx_draw_param draw_param;
	struct task_struct* draw_thread;
	bool draw_thread_bind;
	unsigned int draw_thread_cpu;

	struct task_struct* perf_thread;
	int do_perf;

	struct matrix_pixel* intermediate_frame;

	unsigned long current_bcd_base_time;

	struct matrix_size real_size;
	struct matrix_size virtual_size;

	struct adamtx_enabled_chains enabled_chains;

	int rate;
	int fb_rate;


	struct hrtimer frametimer;
	ktime_t frameperiod;
	bool frametimer_enabled;

	struct hrtimer updatetimer;
	ktime_t updateperiod;
	bool updatetimer_enabled;

	spinlock_t lock_draw;
	bool do_draw;
	bool do_update;

	struct hrtimer perftimer;
	ktime_t perfperiod;
	bool perftimer_enabled;

	unsigned long draws;
	unsigned long draw_irqs;
	unsigned long draw_time;

	unsigned long updates;
	unsigned long update_irqs;
	unsigned long update_time;
};

struct adamtx_remap_frame {
	struct matrix_size* real_size;
	struct matrix_size* virtual_size;
	int offset;
	int rows;
	int pwm_bits;
	struct list_head* panels;
	char* src;
	struct matrix_pixel* dst;
};

struct adamtx_prerender_frame {
	struct matrix_size* real_size;
	int offset;
	int rows;
	int pwm_bits;
	struct adamtx_panel_io* iodata;
	struct adamtx_enabled_chains* enabled_chains;
	struct matrix_pixel* intermediate_frame;
};

extern void dummyfb_copy(void* buffer, struct dummyfb* dummyfb);

#endif
