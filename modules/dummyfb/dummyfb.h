#ifndef _DUMMYFB_H
#define _DUMMYFB_H

#include <linux/types.h>

#define DUMMYFB_DEFAULT_WIDTH 64
#define DUMMYFB_DEFAULT_HEIGHT 64
#define DUMMYFB_DEFAULT_DEPTH 24
#define DUMMYFB_DEFAULT_REFRESH 60

#define DUMMYFB_MEMSIZE (dummyfb_width * dummyfb_height * (dummyfb_depth >> 3))

struct dummyfb;

struct dummyfb_param {
	unsigned int width;
	unsigned int height;
	unsigned int depth;
	unsigned int rate;

	void* priv;
	void (*remove)(struct dummyfb*);
};

struct dummyfb_modedb {
	struct fb_videomode* modes;
	unsigned int num_modes;
};

struct dummyfb {
	struct dummyfb_param param;

	char* fbmem;
	size_t fbmem_size;

	struct fb_info* fbinfo;
	
	struct dummyfb_modedb modedb;

	struct fb_monspecs monspecs;

	struct fb_ops fbops;

	struct list_head list;
};

static int dummyfb_check_var(struct fb_var_screeninfo* var, struct fb_info* info);
static int dummyfb_set_par(struct fb_info* info);
static int dummyfb_mmap(struct fb_info *info, struct vm_area_struct *vma);

size_t dummyfb_get_fbsize(void);
char* dummyfb_get_fbmem(void);
void dummyfb_copy(void* buffer);
void dummyfb_copy_part(void* buffer, size_t len);

#endif
