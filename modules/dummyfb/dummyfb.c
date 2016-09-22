#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include "dummyfb.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Memory only dummy fb module");
MODULE_VERSION("0.1");

#define DUMMY_FB_NAME "dummyfb"

static char* fbmem;

#define NUM_MODES 1
static struct fb_videomode dummy_modedb[NUM_MODES] =
{
	{
		.name =		"DEFAULT",
		.refresh =	DUMMYFB_REFRESH,
		.xres =		DUMMYFB_WIDTH,
		.yres =		DUMMYFB_HEIGHT,
		.sync =		0,
		.vmode =	FB_VMODE_NONINTERLACED
	}
};

static struct fb_monspecs dummy_monspecs =
{
	.modedb =	dummy_modedb,
	.modedb_len =	NUM_MODES,
	.input =	FB_DISP_RGB,
	.signal =	FB_SIGNAL_NONE
};

static struct fb_ops dummy_fbops = 
{
	.owner =	THIS_MODULE,
	.fb_read =	fb_sys_read,
	.fb_write =	fb_sys_write,
	.fb_fillrect =	sys_fillrect,
	.fb_copyarea =	sys_copyarea,
	.fb_imageblit =	sys_imageblit,
	.fb_check_var =	dummy_check_var,
	.fb_set_par =	dummy_set_par,
	.fb_mmap =	dummy_mmap
};

static int dummy_check_var(struct fb_var_screeninfo* var, struct fb_info* info)
{
	printk(KERN_INFO "dummyfb: check_var");
	printk(KERN_INFO "dummyfb: bpp: %d", var->bits_per_pixel);
	if(var->bits_per_pixel != DUMMYFB_DEPTH) // Accept only fixed color depth
		return -EINVAL;
	return 0;
}

static int dummy_set_par(struct fb_info* info)
{
	printk(KERN_INFO "dummyfb: set_par");
	return 0;
}

static int dummy_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)fbmem) >> PAGE_SHIFT, DUMMYFB_MEMSIZE, vma->vm_page_prot) < 0)
	{
		return -EIO;
	}
	return 0;
}

static void init_fb_info(struct fb_info* dummy_fb_info)
{
	dummy_fb_info->fbops = &dummy_fbops;
	dummy_fb_info->flags = FBINFO_DEFAULT;
	strcpy(dummy_fb_info->fix.id, "Dummy");
	dummy_fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	dummy_fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	dummy_fb_info->fix.line_length = DUMMYFB_WIDTH * (DUMMYFB_DEPTH >> 3); // Line length (in bytes!)
	dummy_fb_info->fix.accel = FB_ACCEL_NONE;
	memset(&dummy_fb_info->var, 0, sizeof(dummy_fb_info->var));
	dummy_fb_info->var.xres = DUMMYFB_WIDTH;
	dummy_fb_info->var.yres = DUMMYFB_HEIGHT;
	dummy_fb_info->var.xres_virtual = DUMMYFB_WIDTH;
	dummy_fb_info->var.yres_virtual = DUMMYFB_HEIGHT;
	dummy_fb_info->var.red.length = 8;
	dummy_fb_info->var.red.offset = 16;
	dummy_fb_info->var.green.length = 8;
	dummy_fb_info->var.green.offset = 8;
	dummy_fb_info->var.blue.length = 8;
	dummy_fb_info->var.blue.offset = 0;
	dummy_fb_info->var.bits_per_pixel = DUMMYFB_DEPTH;
	dummy_fb_info->var.transp.length = 0;
	dummy_fb_info->var.activate = FB_ACTIVATE_NOW;
	dummy_fb_info->var.vmode = FB_VMODE_NONINTERLACED;
	dummy_fb_info->monspecs = dummy_monspecs;
	dummy_fb_info->mode = &dummy_modedb[0];
}

static int dummy_remove(struct platform_device *device)
{
        printk(KERN_INFO "dummyfb: REMOVE");
	struct fb_info* info = platform_get_drvdata(device);
	
	if(info)
	{
		unregister_framebuffer(info);
		kfree(info->screen_base);
		framebuffer_release(info);
	}
	return 0;
}

static int dummy_probe(struct platform_device *device)
{
	printk(KERN_INFO "dummyfb: PROBE");
	printk(KERN_INFO "dummyfb: memsize=%d", DUMMYFB_MEMSIZE);
	int ret = -ENOMEM;
	struct fb_info* info = framebuffer_alloc(0, &device->dev);

	if(!info)
		goto noalloced;

	init_fb_info(info);
	fbmem = kmalloc(DUMMYFB_MEMSIZE, GFP_KERNEL);
	if(!fbmem)
		goto fballoced;

	memset(fbmem, 0, DUMMYFB_MEMSIZE);
	info->fix.smem_start = virt_to_phys((void *)fbmem);
	info->fix.smem_len = DUMMYFB_MEMSIZE;
	info->screen_base = (char __iomem *)fbmem;

	ret = register_framebuffer(info);
	if(ret < 0)
	{
		ret = -EINVAL;
		goto memalloced;
	}
	platform_set_drvdata(device, info);
	fb_info(info, "%s frame buffer device\n", info->fix.id);
	return 0;

memalloced:	
	kfree(fbmem);
fballoced:
	framebuffer_release(info);
noalloced:
	return ret;
}

static struct platform_driver dummy_driver = {
	.probe	= dummy_probe,
	.remove = dummy_remove,
	.driver = {
		.name = DUMMY_FB_NAME
	}
};

static struct platform_device* dummy_device;

static int __init dummyfb_init(void)
{
	int ret = platform_driver_register(&dummy_driver);

	if(!ret)
	{
		dummy_device = platform_device_alloc(DUMMY_FB_NAME, 0);		

		if(dummy_device)
			ret = platform_device_add(dummy_device);
		else
			ret = -ENOMEM;
		if(ret)
		{
			platform_device_put(dummy_device);
			platform_driver_unregister(&dummy_driver);
		}
	}

	printk(KERN_INFO "dummyfb: initialized (%d)", ret);
	return ret;
}

static void __exit dummyfb_exit(void)
{
	platform_device_unregister(dummy_device);
	platform_driver_unregister(&dummy_driver);
	printk(KERN_INFO "dummyfb: exit");
}

module_init(dummyfb_init);
module_exit(dummyfb_exit);
