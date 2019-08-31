Set kernel symbols on the fly!
==============================

This module allows you to set the value of integer kernel
symbols to an arbitrary value on kallsyms-enabled kernels.

Obviously this is extemly dangerous and might crash your kernel,
incinerate your device and reappoint George W. Bush.

# Building

Install kernel headers and the usual kernel build tools. Then
type ```make```.

# Usage

```insmod setsym.ko name=<some symbol name> type=<numeric symbol type> val=<value>```

Use ```modinfo setsym.ko``` to view all available symbol types.

Note that insertion of the module will also fail when changing the value of the symbol
was successful. This is a design feature since this module does not do anything useful
outside its init function. Just check `dmesg` for actual failure/success messages.
