#ifndef _ADAMTX_COLOR_H_
#define _ADAMTX_COLOR_H_

#include <linux/types.h>

struct adamtx_color_model {
	unsigned int bitdepth;
	bool grayscale;
	unsigned int depth_red;
	unsigned int depth_green;
	unsigned int depth_blue;
};

uint32_t adamtx_color_get_max_value_red(struct adamtx_color_model* model);
uint32_t adamtx_color_get_max_value_green(struct adamtx_color_model* model);
uint32_t adamtx_color_get_max_value_blue(struct adamtx_color_model* model);

#endif
