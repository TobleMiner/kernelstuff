dummyfb
=======

A simple virtual framebuffer driver. This module is intended for use by other kenel modules.

# Usage

## Initialisation

The function

```int dummyfb_create(struct dummyfb** dummyfb_ptr, struct dummyfb_param param)```

creates a new framebuffer device and populates the supplied dummyfb structure pointer. See dummyfb.h for details on ```dummyfb_param```.


**NOTE: Do not try to set any bitdepth other than 24 bpp. The driver is not yet designed to handle anything but 24 bpp**

## Teardown

The function

```int dummyfb_destroy(struct dummyfb* dummyfb)```

tears down a dummyfb instance, destroying the associated framebuffer device. A non-zero return value indicates that the device is busy and can not be destroyed. In that case just
loop until zero is returned:
```
while(dummyfb_destroy(dummyfb)) {
	usleep_range(100000, 500000);
}
```

## Accessing the framebuffer

Query size of framebuffer memory:
```size_t dummyfb_get_fbsize(struct dummyfb* dummyfb)```

Get pointer to framebuffer memory:
```char* dummyfb_get_fbmem(struct dummyfb* dummyfb)```

Copy all data from framebuffer:
```void dummyfb_copy(void* buffer, struct dummyfb* dummyfb)```

Copy chunk of data from framebuffer:
```void dummyfb_copy_chunk(void* buffer, size_t len, struct dummyfb* dummyfb)```
