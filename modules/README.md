modules
=======

Some kernel modules I have written. 

# adafruit-matrix-rpi
A fast kernel module for driving HUB75 LED panels from Raspberry PI GPIO. Depends on dummyfb.

# adafruit-matrix
A fist try at writing a generic, hardware idependent driver for HUB75 displays. Discarded due to bad kernel GPIO subsystem performance.

# blink
A very simple kernel GPIO subsystem example with hrtimers

# chrdev
A simple character device driver. Quite horrible code quality.

# dummyfb
A universal virtual framebuffer driver for use with other kernel modules.

# helloworld
A bare minimum kernel module

# hrtimer
A simple hrtimer test

# nrf24l01
A feature rich driver for NRF24L01+ wireless modules

# setmessage
A test driver for cross kernel module function calls to chrdev
