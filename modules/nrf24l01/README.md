NRF24L01 driver
===============

This driver is designed to provide a packet level interface for NRF24L01 wireless modules on Linux.

# Compilation

Install kernel headers and simply run make in this directory


# Installation

- Copy the resulting ```nrf24l01.ko``` to ```/lib/modules/extramodules-<your_kernel_version>``` or in case you don't have that directory directly to ```/lib/modules/<your_kernel_version>```.

- Run ```depmod``` as root

- Take a look at ```nrf24l01.dts```. This file specifies how the NRF module interfaces with your existing hardware. Currently it is specifically taylored for usage with a raspberry pi. The most important parameters here are reg, interrupts, nrf-ce, nrf-mode and nrf-addr-be:
	* reg: reg ist the chip select line to use. 1 means, that ```CE1``` of your SPI needs to connect to ```CSN``` on the nrf.
	* interrupts: interrupts selects the GPIO pin used for the ```IRQ``` line of the nrf. Set it to whatever gpio you are using. The second number after the GPIO pin is the interrupt mode. 2 means falling edge. This parameter has to remain unchanged.
	* nrf-ce: nrf-ce controls which GPIO will be used to enable the NRF module. This must be the GPIO number of the pin connected to the nrfs ```CE``` line.
	* nrf-mode: This driver supports various modes of operation for the nrf module. This is essentially a bitmask controlling general driver behavior. For now I'd recommend leaving this value set to 2.
	* nrf-addr-be: If set to 1 this flag reverses the byte order of all addresses used by the nrf. This is useful when interacting with other nrfs controlled by libraries that have reversed address byte order.

- Compile the device tree file using ```dtc -@ nrf24l01.dts nrf24l01.dtbo```

- Load the device tree file using ```dtoverlay nrf24l01.dtbo```. This will automatically load the driver. If you wish to load the driver automatically at bootup you can do so by loading the device tree file at boot. On a raspberry pi this is accomplished by copying ```nrf24l01.dtbo``` to ```/boot/overlays``` and adding the line ```dtoverlay=nrf24l01``` to ```/boot/config.txt```

# Usage

To use the modules they have to be set up first. I will demonstrate this with to PIs, each with one nrf connected and the driver installed.

On the first PI run as root:

```
echo DEADBEEF42 > /sys/class/nrf24/nrf24l01/tx_address
echo DEADBEEF42 > /sys/class/nrf24/nrf24l01/pipe0/address
echo DEADBEEF43 > /sys/class/nrf24/nrf24l01/pipe1/address

echo 1 > /sys/class/nrf24/nrf24l01/pipe0/dynamicpayload
echo 1 > /sys/class/nrf24/nrf24l01/pipe1/dynamicpayload

echo 1 > /sys/class/nrf24/nrf24l01/pipe0/autoack
echo 1 > /sys/class/nrf24/nrf24l01/pipe1/autoack

echo 0 > /sys/class/nrf24/nrf24l01/pipe0/payloadwidth
echo 0 > /sys/class/nrf24/nrf24l01/pipe1/payloadwidth

echo 2000 > /sys/class/nrf24/nrf24l01/rf/datarate
echo 73 > /sys/class/nrf24/nrf24l01/rf/channel

echo 500 > /sys/class/nrf24/nrf24l01/retransmit/delay
echo 15 > /sys/class/nrf24/nrf24l01/retransmit/count
```

On the second PI - once again as root - run:

```
echo DEADBEEF43 > /sys/class/nrf24/nrf24l01/tx_address
echo DEADBEEF43 > /sys/class/nrf24/nrf24l01/pipe0/address
echo DEADBEEF42 > /sys/class/nrf24/nrf24l01/pipe1/address

echo 1 > /sys/class/nrf24/nrf24l01/pipe0/dynamicpayload
echo 1 > /sys/class/nrf24/nrf24l01/pipe1/dynamicpayload

echo 1 > /sys/class/nrf24/nrf24l01/pipe0/autoack
echo 1 > /sys/class/nrf24/nrf24l01/pipe1/autoack

echo 0 > /sys/class/nrf24/nrf24l01/pipe0/payloadwidth
echo 0 > /sys/class/nrf24/nrf24l01/pipe1/payloadwidth

echo 2000 > /sys/class/nrf24/nrf24l01/rf/datarate
echo 73 > /sys/class/nrf24/nrf24l01/rf/channel

echo 500 > /sys/class/nrf24/nrf24l01/retransmit/delay
echo 15 > /sys/class/nrf24/nrf24l01/retransmit/count
```

Now you should be able to transfer data between the two PIs.

Test it by running ```cat /dev/nrf24l01``` on the firs PI

and ```echo 'Mess with the best, die like the rest' > /dev/nrf24l01``` on the second PI

# Troubleshooting

If you encounter any issues during operation it is generally a good idea to take a look at the kernel log, since most issues will generate a corresponding error message.

## Common errors

### Setting any of the addresses fails

This happens when SPI communication with the module doesn't work. There is most likely an issue with your wiring. Do also check you chip select line.

### One of the GPIOs can't be allocated

Either the CE GPIO or the IRQ GPIO is in use by something else. Just choose a different one.

### Driver can't allocate the SPI chip enable line

Make sure you don't load any other device tree overlays that use the SPI in ```/boot/config.txt``` (or wherever your board specifies boot time device tree overlays).
