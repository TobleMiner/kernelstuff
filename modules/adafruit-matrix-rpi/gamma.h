#ifndef _ADAMTX_GAMMA_H_
#define _ADAMTX_GAMMA_H_

#include "color.h"

// Gamma correction
#define ADAMTX_GAMMA_TABLE_SIZE 256
#define ADAMTX_GAMMA_TABLE_SCALE 255

struct adamtx_gamma_table {
	struct adamtx_color_model* color_model;

	uint8_t red[ADAMTX_GAMMA_TABLE_SIZE];
	uint8_t green[ADAMTX_GAMMA_TABLE_SIZE];
	uint8_t blue[ADAMTX_GAMMA_TABLE_SIZE];
};

void adamtx_gamma_setup_table(struct adamtx_gamma_table* table, struct adamtx_color_model* model);
void adamtx_gamma_setup_table_fix_max(struct adamtx_gamma_table* table, struct adamtx_color_model* model, uint32_t max_val);

uint32_t adamtx_gamma_apply_red(struct adamtx_gamma_table* table, uint32_t val);
uint32_t adamtx_gamma_apply_green(struct adamtx_gamma_table* table, uint32_t val);
uint32_t adamtx_gamma_apply_blue(struct adamtx_gamma_table* table, uint32_t val);
uint32_t adamtx_gamma_apply_gbr24(struct adamtx_gamma_table* table, uint32_t blue, uint32_t green, uint32_t red);

#endif
