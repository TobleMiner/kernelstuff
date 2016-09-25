#define DIRECTION_FORW 1
#define DIRECTION_BACK 1

struct ledmatrix;

struct ledmatrix {
	u8	xres;
	u8	yres;
	u32	offset;
	int direction_x;
	int direction_y;
}
