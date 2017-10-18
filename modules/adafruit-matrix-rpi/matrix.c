#include <stddef.h>

#include <linux/list.h>

#include "matrix.h"

static void matrix_panel_get_size_real(struct matrix_size* size, struct matrix_ledpanel* panel)
{
	if(panel->rotate) {
		size->width = panel->yres;
		size->height = panel->xres;
	} else {
		size->width = panel->xres;
		size->height = panel->yres;
	}
}

int matrix_panel_contains_real(struct matrix_ledpanel* panel, int x, int y)
{
	struct matrix_size size_real;
	matrix_panel_get_size_real(&size_real, panel);
	return x >= panel->realx && x < panel->realx + size_real.width && y >= panel->realy && y < panel->realy + size_real.height;
}

int matrix_panel_contains(struct matrix_ledpanel* panel, int x, int y)
{
	return x >= panel->virtual_x && x <= panel->virtual_x + panel->xres && y >= panel->virtual_y && y <= panel->virtual_y + panel->yres;
}

struct matrix_ledpanel* matrix_get_panel_at(struct list_head* panels, int x, int y)
{
	struct matrix_ledpanel* panel;
	list_for_each_entry(panel, panels, list) {
		if(matrix_panel_contains(panel, x, y))
			return panel;
	}
	return NULL;
}

struct matrix_ledpanel* matrix_get_panel_at_real(struct list_head* panels, int x, int y)
{
	struct matrix_ledpanel* panel;
	list_for_each_entry(panel, panels, list) {
		if(matrix_panel_contains_real(panel, x, y))
			return panel;
	}
	return NULL;
}

void matrix_panel_get_local_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y)
{
	int tmp;
	struct matrix_size panel_size;

	pos->x = x - panel->realx;
	pos->y = y - panel->realy;

	matrix_panel_get_size_real(&panel_size, panel);
	if(panel->flip_x) {
		pos->x = panel_size.width - pos->x - 1;
	}
	if(panel->flip_y) {
		pos->y = panel_size.height - pos->y - 1;
	}

	if(panel->rotate) {
		tmp = pos->x;
		pos->x = pos->y;
		pos->y = tmp;
	}
}

void matrix_panel_get_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y)
{
	matrix_panel_get_local_position(pos, panel, x, y);
	pos->x += panel->virtual_x;
	pos->y += panel->virtual_y;
}

static void matrix_chain_get_size_virtual(struct matrix_size* size, struct list_head* panels, int chain) {
	struct matrix_ledpanel* panel;
	int current_width;

	size->width = 0;
	size->height = 0;

	list_for_each_entry(panel, panels, list) {
		if(panel->chain != chain)
			continue;

		current_width = panel->xres + panel->virtual_x;
		if(current_width > size->width)
			size->width = current_width;
		size->height = panel->yres;
	}
}

void matrix_get_size_virtual(struct matrix_size* size, struct list_head* panels) {
	struct matrix_size chain_size;
	int i;

	size->width = 0;
	size->height = 0;
	for(i = 0; i < MAX_NUM_CHAINS; i++) {
		matrix_chain_get_size_virtual(&chain_size, panels, i);
		if(chain_size.width > size->width)
			size->width = chain_size.width;

		if(chain_size.height > size->height)
			size->height = chain_size.height;
	}
}

void matrix_get_size_real(struct matrix_size* size, struct list_head* panels) {
	struct matrix_ledpanel* panel;
	int current_width, current_height;
	struct matrix_size size_real;

	size->width = 0;
	size->height = 0;
	list_for_each_entry(panel, panels, list) {

		matrix_panel_get_size_real(&size_real, panel);

		current_width = size_real.width + panel->realx;
		if(current_width > size->width)
			size->width = current_width;

		current_height = size_real.height + panel->realy;
		if(current_height > size->height)
			size->height = current_height;
	}
}
