#ifndef _ADAMTX_H
#define _ADAMTX_H

#define ADAMTX_NAME "adafruit-matrix"

// GPIO setup
#define ADAMTX_NUM_GPIOS 14

#define ADAMTX_GPIO_R1	11
#define ADAMTX_GPIO_R2	8
#define ADAMTX_GPIO_G1	27
#define ADAMTX_GPIO_G2	9
#define ADAMTX_GPIO_B1	7
#define ADAMTX_GPIO_B2	10
#define ADAMTX_GPIO_A	22
#define ADAMTX_GPIO_B	23
#define ADAMTX_GPIO_C	24
#define ADAMTX_GPIO_D	25
#define ADAMTX_GPIO_E	15
#define ADAMTX_GPIO_OE	18
#define ADAMTX_GPIO_STR	4
#define ADAMTX_GPIO_CLK	17


#define ADAMTX_GPIO_OFFSET_ADDRESS 22

#define ADAMTX_GPIO_MASK_CLOCK		(1 << ADAMTX_GPIO_CLK)
#define ADAMTX_GPIO_MASK_ADDR		0b0011110000001000000000000000
#define ADAMTX_GPIO_MASK_ADDR_HI	0b0011110000000000000000000000
#define ADAMTX_GPIO_MASK_DATA		0b1000000000000000111110000000


// Matrix parameters
#define ADAMTX_ROWS			32
#define ADAMTX_COLUMNS		128
#define ADAMTX_PWM_BITS		4
#define ADAMTX_REAL_WIDTH	64
#define ADAMTX_REAL_HEIGHT	64
#define ADAMTX_RATE			240UL

typedef struct adamtx_frame
{
	int width;
	int height;
	int vertical_offset;
	int rows;
	int pwm_bits;
	uint32_t* paneldata;
	off_t paneloffset;
	uint32_t* frame;
	off_t frameoffset;
};

typedef struct adamtx_processable_frame
{
	int width;
	int height;
	int columns;
	int rows;
	int pwm_bits;
	uint32_t* frame;
	uint32_t* iodata;
	struct matrix_ledpanel** panels;
};

#endif
