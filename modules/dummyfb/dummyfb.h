#ifndef _DUMMYFB_H
#define _DUMMYFB_H

#include <linux/types.h>
#include <linux/fb.h>

#define DUMMYFB_MAX(x, y) ((x) > (y) ? (x) : (y))

struct dummyfb;

struct dummyfb_mode {
	unsigned int	width;
	unsigned int	height;
	unsigned int	depth;
	bool			grayscale;
	unsigned int	rate;
};

struct dummyfb_color_format {
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
};

struct dummyfb_param {
	struct dummyfb_mode mode;

	void* priv;
	void (*remove)(struct dummyfb*);
};

struct dummyfb_modedb {
	struct fb_videomode* modes;
	unsigned int num_modes;
};

struct dummyfb {
	struct dummyfb_param param;
	struct dummyfb_modedb modedb;

	struct dummyfb_color_format color_format;

	char* fbmem;
	size_t fbmem_size;
	size_t fbmem_line_length;

	struct fb_info* fbinfo;
	struct fb_monspecs monspecs;
	struct fb_ops fbops;

	struct list_head list;

	atomic_t refcount;
	bool exiting;
};

int dummyfb_create(struct dummyfb** dummyfb_ptr, struct dummyfb_param param);
int dummyfb_destroy(struct dummyfb* dummyfb);

size_t dummyfb_get_fbsize(struct dummyfb* dummyfb);
char* dummyfb_get_fbmem(struct dummyfb* dummyfb);
void dummyfb_copy(void* buffer, struct dummyfb* dummyfb);
void dummyfb_copy_chunk(void* buffer, size_t len, struct dummyfb* dummyfb);

unsigned int dummyfb_get_max_color_depth(struct dummyfb* dummyfb);
uint32_t dummyfb_color_get_max_red(struct dummyfb* dummyfb);
uint32_t dummyfb_color_get_max_green(struct dummyfb* dummyfb);
uint32_t dummyfb_color_get_max_blue(struct dummyfb* dummyfb);

#endif
