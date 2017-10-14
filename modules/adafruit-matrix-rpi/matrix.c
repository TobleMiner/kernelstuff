#include <stddef.h>

#include "matrix.h"

int matrix_panel_contains_real(struct matrix_ledpanel* panel, int x, int y)
{
	return x >= panel->realx && x < panel->realx + panel->xres && y >= panel->realy && y < panel->realy + panel->yres;
}

int matrix_panel_contains(struct matrix_ledpanel* panel, int x, int y)
{
	return x >= panel->virtual_x && x <= panel->virtual_x + panel->xres && y >= panel->virtual_y && y <= panel->virtual_y + panel->yres;
}

struct matrix_ledpanel* matrix_get_panel_at(struct matrix_ledpanel** panels, int numpanels, int x, int y)
{
	while(--numpanels >= 0)
	{
		if(matrix_panel_contains(panels[numpanels], x, y))
			return panels[numpanels];
	}
	return NULL;
}

struct matrix_ledpanel* matrix_get_panel_at_real(struct matrix_ledpanel** panels, int numpanels, int x, int y)
{
	while(--numpanels >= 0)
	{
		if(matrix_panel_contains_real(panels[numpanels], x, y))
			return panels[numpanels];
	}
	return NULL;
}

void matrix_panel_get_local_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y)
{
	pos->x = x - panel->realx;
	pos->y = y - panel->realy;
	if(panel->flip_x)
	{
		pos->x = panel->xres - pos->x - 1;
	}
	if(panel->flip_y)
	{
		pos->y = panel->yres - pos->y - 1;
	}
}

void matrix_panel_get_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y)
{
	matrix_panel_get_local_position(pos, panel, x, y);
	pos->x += panel->virtual_x;
	pos->y += panel->virtual_y;
}

void matrix_panel_get_size_virtual(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels) {
	struct matrix_ledpanel* panel;

	size->width = 0;
	size->height = 0;
	while(num_panels-- > 0) {
		panel = panels[num_panels];

		size->width += panel->xres;
		size->height = panel->yres;
	}
}

void matrix_panel_get_size_real(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels) {
	struct matrix_ledpanel* panel;
	int current_width, current_height;

	size->width = 0;
	size->height = 0;
	while(num_panels-- > 0) {
		panel = panels[num_panels];

		current_width = panel->xres + panel->realx;
		if(current_width > size->width)
			size->width = current_width;

		current_height = panel->yres + panel->realy;
		if(current_height > size->height)
			size->height = current_height;
	}
}
