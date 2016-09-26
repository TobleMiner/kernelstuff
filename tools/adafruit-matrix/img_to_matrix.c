#include "matrix.h"

#include "stdio.h"

#define NUM_PANELS 2

int main(int argc, char** argv)
{
	struct matrix_ledpanel matrix_up = {
		.name = "Upper",
		.xres = 64,
		.yres = 32,
		.virtual_x = 64,
		.virtual_y = 0,
		.realx = 0,
		.realy = 0,
		.flip_y = 1
	};

	struct matrix_ledpanel matrix_low = {
		.name = "Lower",
		.xres = 64,
		.yres = 32,
		.virtual_x = 0,
		.virtual_y = 0,
		.realx = 0,
		.realy = 32,
		.flip_x = 1,
		.flip_y = 0
	};

	int x,y;
	x = 0;
	y = 0;

	struct matrix_ledpanel* panels[NUM_PANELS] = {&matrix_low, &matrix_up};

	struct matrix_ledpanel* panel = matrix_get_panel_at_real(panels, NUM_PANELS, x, y);

	printf("Panel: %s\n", panel->name);

	struct matrix_pos pos;

	matrix_panel_get_local_position(&pos, panel, x, y);

	printf("Local pos: %d,%d\n", pos.x, pos.y);

	matrix_panel_get_position(&pos, panel, x, y);

	printf("Virtual pos: %d,%d\n", pos.x, pos.y);
}
