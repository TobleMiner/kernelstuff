#ifndef _DUMMYFB_H
#define _DUMMYFB_H

#include <linux/types.h>
#include <linux/fb.h>

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
	struct dummyfb_modedb modedb;

	char* fbmem;
	size_t fbmem_size;

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
void dummyfb_copy_part(void* buffer, off_t offset, size_t len, struct dummyfb* dummyfb);

#endif
