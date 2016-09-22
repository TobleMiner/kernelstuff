#define DUMMYFB_WIDTH 640
#define DUMMYFB_HEIGHT 480
#define DUMMYFB_DEPTH 24
#define DUMMYFB_REFRESH 60

#define DUMMYFB_MEMSIZE (DUMMYFB_WIDTH * DUMMYFB_HEIGHT * (DUMMYFB_DEPTH >> 3))

static int dummy_check_var(struct fb_var_screeninfo* var, struct fb_info* info);
static int dummy_set_par(struct fb_info* info);
static int dummy_mmap(struct fb_info *info, struct vm_area_struct *vma);
