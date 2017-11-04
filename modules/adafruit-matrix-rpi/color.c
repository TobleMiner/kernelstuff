#include <linux/bitops.h>

#include "color.h"


uint32_t adamtx_color_get_max_value_red(struct adamtx_color_model* model)
{
	return BIT(model->depth_red) - 1;
}

uint32_t adamtx_color_get_max_value_green(struct adamtx_color_model* model)
{
	return BIT(model->depth_green) - 1;
}

uint32_t adamtx_color_get_max_value_blue(struct adamtx_color_model* model)
{
	return BIT(model->depth_blue) - 1;
}
