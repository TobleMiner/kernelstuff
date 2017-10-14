#ifndef _ADAMTX_MATRIX_H
#define _ADAMTX_MATRIX_H

#include <linux/types.h>

#define MAX_NUM_CHAINS 3

enum {
	ADAMTX_CHAIN_0,
	ADAMTX_CHAIN_1,
	ADAMTX_CHAIN_2
};

struct matrix_ledpanel {
	char* name;
	int	xres;
	int	yres;
	int	virtual_x;
	int	virtual_y;
	int	realx;
	int	realy;
	int	flip_x : 1;
	int	flip_y : 1;
	int chain;
};

struct matrix_pixel {
	uint32_t chains[MAX_NUM_CHAINS];
};

struct matrix_pos {
	int x;
	int y;
};

struct matrix_size {
	int width;
	int height;
};

int matrix_panel_contains_real(struct matrix_ledpanel* panel, int x, int y);

int matrix_panel_contains(struct matrix_ledpanel* panel, int x, int y);

struct matrix_ledpanel* matrix_get_panel_at(struct matrix_ledpanel** panels, int numpanels, int x, int y);

struct matrix_ledpanel* matrix_get_panel_at_real(struct matrix_ledpanel** panels, int numpanels, int x, int y);

void matrix_panel_get_local_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y);

void matrix_panel_get_position(struct matrix_pos* pos, struct matrix_ledpanel* panel, int x, int y);

void matrix_panel_get_chain_size_virtual(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels, int chain);

void matrix_panel_get_chain_size_real(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels, int chain);

void matrix_panel_get_size_virtual(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels);

void matrix_panel_get_size_real(struct matrix_size* size, struct matrix_ledpanel** panels, int num_panels);

#endif
