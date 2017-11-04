#include <linux/types.h>

#include "gamma.h"
#include "color.h"

static const uint8_t gamma_table_template[ADAMTX_GAMMA_TABLE_SIZE] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

void adamtx_gamma_setup_table(struct adamtx_gamma_table* table, struct adamtx_color_model* model)
{
	int i;
	table->color_model = model;

	for(i = 0; i < ADAMTX_GAMMA_TABLE_SIZE; i++) {
		table->red[i] = gamma_table_template[i] * adamtx_color_get_max_value_red(model) / ADAMTX_GAMMA_TABLE_SCALE;
		table->green[i] = gamma_table_template[i] * adamtx_color_get_max_value_green(model) / ADAMTX_GAMMA_TABLE_SCALE;
		table->blue[i] = gamma_table_template[i] * adamtx_color_get_max_value_blue(model) / ADAMTX_GAMMA_TABLE_SCALE;
	}
}

void adamtx_gamma_setup_table_fix_max(struct adamtx_gamma_table* table, struct adamtx_color_model* model, uint32_t max_val)
{
	int i;
	table->color_model = model;

	for(i = 0; i < ADAMTX_GAMMA_TABLE_SIZE; i++) {
		table->red[i] = gamma_table_template[i] * max_val / ADAMTX_GAMMA_TABLE_SCALE;
		table->green[i] = gamma_table_template[i] * max_val / ADAMTX_GAMMA_TABLE_SCALE;
		table->blue[i] = gamma_table_template[i] * max_val / ADAMTX_GAMMA_TABLE_SCALE;
	}
}

uint32_t adamtx_gamma_apply_red(struct adamtx_gamma_table* table, uint32_t val)
{
	return table->red[val * ADAMTX_GAMMA_TABLE_SCALE / adamtx_color_get_max_value_red(table->color_model)];
}

uint32_t adamtx_gamma_apply_green(struct adamtx_gamma_table* table, uint32_t val)
{
	return table->green[val * ADAMTX_GAMMA_TABLE_SCALE / adamtx_color_get_max_value_green(table->color_model)];
}

uint32_t adamtx_gamma_apply_blue(struct adamtx_gamma_table* table, uint32_t val)
{
	return table->blue[val * ADAMTX_GAMMA_TABLE_SCALE / adamtx_color_get_max_value_blue(table->color_model)];
}

uint32_t adamtx_gamma_apply_gbr24(struct adamtx_gamma_table* table, uint32_t blue, uint32_t green, uint32_t red)
{
	return adamtx_gamma_apply_blue(table, blue) |
		(adamtx_gamma_apply_green(table, green) << 8) |
		(adamtx_gamma_apply_red(table, red) << 16);
}
