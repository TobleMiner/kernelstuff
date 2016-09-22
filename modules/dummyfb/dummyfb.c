#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include "dummyfb.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tobas Schramm");
MODULE_DESCRIPTION("Test dummy fb module");
MODULE_VERSION("0.1");

#define DUMMY_FB_NAME "dummyfb"

static struct list_head modelist;

#define NUM_MODES 1
static struct fb_videomode dummy_modedb[NUM_MODES] =
{
	{
		.name =		"DEFAULT",
		.refresh =	60,
		.xres =		640,
		.yres =		480,
		.sync =		0,
		.vmode =	FB_VMODE_NONINTERLACED
	}
};

static struct fb_monspecs dummy_monspecs =
{
	.modedb = dummy_modedb,
	.modedb_len = NUM_MODES,
	.input = FB_DISP_RGB,
	.signal = FB_SIGNAL_NONE
};

static struct fb_ops dummy_fbops = 
{
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit


/*	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
	.fb_check_var = dummy_check_var,
	.fb_set_par = dummy_set_par,
	.fb_setcolreg = dummy_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit*/
};

static int dummy_check_var(struct fb_var_screeninfo* var, struct fb_info* info)
{
	printk(KERN_INFO "dummy_fb: check_var");
	if(var->bits_per_pixel != 24)
		return -EINVAL;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	return 0;
}

static int dummy_set_par(struct fb_info* info)
{
	printk(KERN_INFO "dummy_fb: set_par");
	return 0;
}

static int dummy_setcolreg(u_int regno, u_int red, u_int green, u_int blue, u_int trans, struct fb_info* info)
{
	printk(KERN_INFO "dummy_fb: setcolreg");
	return -EINVAL;
}

static void init_fb_info(struct fb_info* dummy_fb_info)
{
	dummy_fb_info->fbops = &dummy_fbops;
	dummy_fb_info->flags = FBINFO_DEFAULT;
	strcpy(dummy_fb_info->fix.id, "Dummy");
	dummy_fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	dummy_fb_info->fix.line_length = 640;
	dummy_fb_info->fix.smem_len = 640 * 480 * 3;
	dummy_fb_info->fix.accel = FB_ACCEL_NONE;
	memset(&dummy_fb_info->var, 0, sizeof(dummy_fb_info->var));
	dummy_fb_info->var.xres = 640;
	dummy_fb_info->var.yres = 480;
	dummy_fb_info->var.xres_virtual = 640;
	dummy_fb_info->var.yres_virtual = 480;
	dummy_fb_info->var.red.length = 8;
	dummy_fb_info->var.red.offset = 16;
	dummy_fb_info->var.green.length = 8;
	dummy_fb_info->var.green.offset = 8;
	dummy_fb_info->var.blue.length = 8;
	dummy_fb_info->var.blue.offset = 0;
	dummy_fb_info->var.bits_per_pixel = 24;
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
		vfree(info->screen_base);
		framebuffer_release(info);
	}

	return 0;
}

static int dummy_probe(struct platform_device *device)
{
	printk(KERN_INFO "dummyfb: PROBE");
	int ret = -EINVAL;
	struct fb_info* info = framebuffer_alloc(0, &device->dev);

	if(!info)
		return -ENOMEM;

	init_fb_info(info);
	char* fbmem = vmalloc(640 * 480 * 3);
	if(!fbmem)
	{
		framebuffer_release(info);
		return -ENOMEM;
	}
	memset(fbmem, 0, 640 * 480 * 3);
	info->screen_base = (char __iomem *)fbmem;

	ret = register_framebuffer(info);
	if(ret < 0)
	{
		vfree(fbmem);
		framebuffer_release(info);
		return ret;
	}
	platform_set_drvdata(device, info);
	fb_info(info, "%s frame buffer device\n", info->fix.id);
	return 0;
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

	printk(KERN_INFO "dummy_fb: initialized (%d)", ret);
	return ret;
}

static void __exit dummyfb_exit(void)
{
	platform_device_unregister(dummy_device);
	platform_driver_unregister(&dummy_driver);
	printk(KERN_INFO "dummy_fb: exit");
}

module_init(dummyfb_init);
module_exit(dummyfb_exit);
