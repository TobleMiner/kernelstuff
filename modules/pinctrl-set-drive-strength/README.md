Set the drive strength of individual GPIOs
==========================================

This module allows you to set the per GPIO drive strength
via pinctrl on devices that support it.

# Building

Install kernel headers and the usual kernel build tools. Then
type ```make```.

# Usage

```insmod setdrv.ko gpio=<gpio number> drive_strength=<drive strength in mA>```

Note that insertion of the module will also fail when setting of the drive strength
was successful. This is a design feature since this module does not do anything useful
outside its init function. Just check `dmesg` for actual failure/success messages.
