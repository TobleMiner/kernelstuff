#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/list.h>

#include "dummyfb.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Memory only dummy fb module");
MODULE_VERSION("0.1");

#define DUMMY_FB_NAME "dummyfb"

static LIST_HEAD(dummyfbs);

static int dummyfb_check_var(struct fb_var_screeninfo* var, struct fb_info* info);
static int dummyfb_set_par(struct fb_info* info);
static int dummyfb_mmap(struct fb_info *info, struct vm_area_struct *vma);

static int dummyfb_open(struct fb_info* fbinfo, int user);
static int dummyfb_release(struct fb_info* fbinfo, int user);

static struct fb_ops dummyfb_fbops =
{
	.owner =		THIS_MODULE,
	.fb_check_var =	dummyfb_check_var,
	.fb_set_par =	dummyfb_set_par,
	.fb_mmap =		dummyfb_mmap,

	.fb_open =		dummyfb_open,
	.fb_release =	dummyfb_release
};

static void dummyfb_color_format_grayscale(struct dummfb_color_format* fmt, unsigned int depth) {
	fmt->red.offset = 0;
	fmt->red.length = depth;
	fmt->green.offset = 0;
	fmt->green.length = depth;
	fmt->blue.offset = 0;
	fmt->blue.length = depth;
}

static void dummyfb_color_format_truecolor(struct dummfb_color_format* fmt, unsigned int depth) {
	uint32_t bits_rb = DUMMYFB_DIV_ROUND_DOWN(depth, 3);
	uint32_t bits_g = depth - 2 * bits_rb;
	fmt->red.offset = bits_rb + bits_g;
	fmt->red.length = bits_rb;
	fmt->green.offset = bits_rb;
	fmt->green.length = bits_g;
	fmt->blue.offset = 0;
	fmt->blue.length = bits_rb;
}

static int dummyfb_open(struct fb_info* fbinfo, int user) {
	struct dummyfb* dummyfb = fbinfo->par;

	if(dummyfb->exiting)
		return -EAGAIN;

	atomic_inc(&dummyfb->refcount);

	return 0;
}

static int dummyfb_release(struct fb_info* fbinfo, int user) {
	struct dummyfb* dummyfb = fbinfo->par;

	atomic_dec(&dummyfb->refcount);

	return 0;
}

static int dummyfb_check_var(struct fb_var_screeninfo* var, struct fb_info* info)
{
	struct dummyfb* dummyfb = info->par;
	printk(KERN_INFO "dummyfb: check_var");
	printk(KERN_INFO "dummyfb: size: %dx%d virtualsize:%dx%d bpp: %d", var->xres, var->yres, var->xres_virtual, var->yres_virtual, var->bits_per_pixel);
	if(var->bits_per_pixel != dummyfb->param.mode.depth) // Accept only fixed color depth
		return -EINVAL;
	if(var->xres != dummyfb->param.mode.width || var->yres != dummyfb->param.mode.height)
		return -EINVAL;
	if(var->xres_virtual != dummyfb->param.mode.width || var->yres_virtual != dummyfb->param.mode.height)
		return -EINVAL;
	return 0;
}

static int dummyfb_set_par(struct fb_info* info)
{
	printk(KERN_INFO "dummyfb: set_par");
	return 0;
}

static int dummyfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct dummyfb* dummyfb = info->par;
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)dummyfb->fbmem) >> PAGE_SHIFT, dummyfb->fbmem_size, vma->vm_page_prot) < 0) {
		return -EIO;
	}
	return 0;
}

static void dummyfb_init_color_format(struct dummyfb* dummyfb) {
	if(dummyfb->param.mode.grayscale)
		dummyfb_color_format_grayscale(&dummyfb->color_format, dummyfb->param.mode.depth);
	else
		dummyfb_color_format_truecolor(&dummyfb->color_format, dummyfb->param.mode.depth);
}

static void dummyfb_init_fb_info(struct dummyfb* dummyfb)
{
	struct fb_info* fbinfo = dummyfb->fbinfo;
	fbinfo->par = dummyfb;

	fbinfo->fbops = &dummyfb_fbops;
	fbinfo->flags = FBINFO_DEFAULT;
	strcpy(fbinfo->fix.id, "Dummy");

	fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
	if(dummyfb->param.mode.depth > 1) {
		fbinfo->fix.visual = FB_VISUAL_TRUECOLOR;
	} else
		fbinfo->fix.visual = FB_VISUAL_MONO01;
	fbinfo->fix.grayscale = dummyfb->param.mode.grayscale;

	fbinfo->fix.line_length = DIV_ROUND_UP(dummyfb->param.mode.width * dummyfb->param.mode.depth, 8); // Line length (in bytes!)
	fbinfo->fix.accel = FB_ACCEL_NONE;
	memset(&fbinfo->var, 0, sizeof(fbinfo->var));
	fbinfo->var.xres = dummyfb->param.mode.width;
	fbinfo->var.yres = dummyfb->param.mode.height;
	fbinfo->var.xres_virtual = dummyfb->param.mode.width;
	fbinfo->var.yres_virtual = dummyfb->param.mode.height;
	fbinfo->var.pixclock = KHZ2PICOS(dummyfb->param.mode.height * dummyfb->param.mode.width * dummyfb->param.mode.rate + 999UL) * 1000;

	memcpy(&fbinfo->var.red, &dummyfb->color_format.red, sizeof(fbinfo->var.red));
	memcpy(&fbinfo->var.green, &dummyfb->color_format.green, sizeof(fbinfo->var.green));
	memcpy(&fbinfo->var.blue, &dummyfb->color_format.blue, sizeof(fbinfo->var.blue));

	fbinfo->var.transp.length = 0;
	fbinfo->var.bits_per_pixel = dummyfb->param.mode.depth;
	fbinfo->var.activate = FB_ACTIVATE_NOW;
	fbinfo->var.vmode = FB_VMODE_NONINTERLACED;
	fbinfo->monspecs = dummyfb->monspecs;
	fbinfo->mode = &dummyfb->modedb.modes[0];

	fbinfo->fix.smem_start = virt_to_phys((void *)dummyfb->fbmem);
	fbinfo->fix.smem_len = dummyfb->fbmem_size;
	fbinfo->screen_base = (char __iomem *)dummyfb->fbmem;
}

void dummyfb_init_monspecs(struct dummyfb* dummyfb) {
	struct fb_monspecs* monspecs = &dummyfb->monspecs;

	*monspecs = (struct fb_monspecs) {
		.input =	dummyfb->param.mode.grayscale ? FB_DISP_MONO : FB_DISP_RGB,
		.signal =	FB_SIGNAL_NONE,
		.vfmin =	dummyfb->param.mode.rate,
		.vfmax =	dummyfb->param.mode.rate,
		.hfmin =	dummyfb->param.mode.rate * dummyfb->param.mode.width,
		.hfmax =	dummyfb->param.mode.rate * dummyfb->param.mode.width
	};
}

int dummyfb_init_modedb(struct dummyfb* dummyfb) {
	struct dummyfb_modedb* modedb = &dummyfb->modedb;
	modedb->num_modes = 1;

	modedb->modes = vmalloc(sizeof(struct fb_videomode) * modedb->num_modes);
	if(!modedb->modes) {
		return -EINVAL;
	}

	modedb->modes[0] = (struct fb_videomode){
		.name =		"DEFAULT",
		.sync =		0,
		.vmode =	FB_VMODE_NONINTERLACED,
		.refresh =	dummyfb->param.mode.rate,
		.xres =		dummyfb->param.mode.width,
		.yres =		dummyfb->param.mode.height
	};

	return 0;
}

void dummyfb_free_modedb(struct dummyfb* dummyfb) {
	vfree(dummyfb->modedb.modes);
}

static int dummyfb_validate_param(struct dummyfb_param param) {
	if(param.mode.grayscale && param.mode.depth > 8)
		return -EINVAL;

	if(!param.mode.grayscale && param.mode.depth > 24)
		return -EINVAL;

	return 0;
}

int dummyfb_create(struct dummyfb** dummyfb_ptr, struct dummyfb_param param) {
	int err;
	struct dummyfb* dummyfb;

	if((err = dummyfb_validate_param(param)))
		return err;

	dummyfb = vzalloc(sizeof(struct dummyfb));
	if(!dummyfb) {
		err = -ENOMEM;
		goto fail;
	}

	atomic_set(&dummyfb->refcount, 0);
	dummyfb->param = param;

	dummyfb->fbinfo = framebuffer_alloc(sizeof(struct dummyfb*), NULL);
	if(!dummyfb->fbinfo) {
		err = -ENOMEM;
		goto dummyfb_alloced;
	}

	dummyfb->fbmem_size = param.mode.height * DIV_ROUND_UP(param.mode.width * dummyfb->param.mode.depth, 8);
	dummyfb->fbmem = kzalloc(dummyfb->fbmem_size, GFP_KERNEL);
	if(!dummyfb->fbmem) {
		err = -ENOMEM;
		goto fb_alloced;
	}

	if((err = dummyfb_init_modedb(dummyfb))) {
		goto fb_mem_alloced;
	}

	dummyfb_init_monspecs(dummyfb);

	dummyfb_init_color_format(dummyfb);

	dummyfb_init_fb_info(dummyfb);

	if((err = register_framebuffer(dummyfb->fbinfo))) {
		goto fb_modedb_alloced;
	}

	list_add(&dummyfb->list, &dummyfbs);

	*dummyfb_ptr = dummyfb;
	return 0;

fb_modedb_alloced:
	dummyfb_free_modedb(dummyfb);
fb_mem_alloced:
	kfree(dummyfb->fbmem);
fb_alloced:
	framebuffer_release(dummyfb->fbinfo);
dummyfb_alloced:
	vfree(dummyfb);
fail:
	return err;
}

EXPORT_SYMBOL(dummyfb_create);

int dummyfb_destroy(struct dummyfb* dummyfb) {
		dummyfb->exiting = true;

		lock_fb_info(dummyfb->fbinfo);

		if(atomic_read(&dummyfb->refcount)) {
			unlock_fb_info(dummyfb->fbinfo);
			dummyfb->exiting = false;
			return -EBUSY;
		}

		unlock_fb_info(dummyfb->fbinfo);

		if(dummyfb->param.remove)
			dummyfb->param.remove(dummyfb);

		list_del(&dummyfb->list);

		unregister_framebuffer(dummyfb->fbinfo);
		dummyfb_free_modedb(dummyfb);
		kfree(dummyfb->fbmem);
		framebuffer_release(dummyfb->fbinfo);

		vfree(dummyfb);

		return 0;
}

EXPORT_SYMBOL(dummyfb_destroy);

static int __init dummyfb_init(void)
{
	printk(KERN_INFO "dummyfb: INSERT");

	return 0;
}

static void __exit dummyfb_exit(void)
{
	struct dummyfb *dummyfb, *dummyfb_next;
	printk(KERN_INFO "dummyfb: REMOVE");

	list_for_each_entry_safe(dummyfb, dummyfb_next, &dummyfbs, list) {
		dummyfb_destroy(dummyfb);
	}
}


module_init(dummyfb_init);
module_exit(dummyfb_exit);


size_t dummyfb_get_fbsize(struct dummyfb* dummyfb)
{
	return dummyfb->fbmem_size;
}

char* dummyfb_get_fbmem(struct dummyfb* dummyfb)
{
	return dummyfb->fbmem;
}

void dummyfb_copy(void* buffer, struct dummyfb* dummyfb)
{
	memcpy(buffer, dummyfb->fbmem, dummyfb->fbmem_size);
}

void dummyfb_copy_chunk(void* buffer, size_t len, struct dummyfb* dummyfb)
{
	memcpy(buffer, dummyfb->fbmem, len);
}

EXPORT_SYMBOL(dummyfb_get_fbsize);
EXPORT_SYMBOL(dummyfb_get_fbmem);
EXPORT_SYMBOL(dummyfb_copy);
EXPORT_SYMBOL(dummyfb_copy_chunk);

