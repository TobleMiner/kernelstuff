#!/bin/env python

from PIL import Image
import sys
import struct

def pixToBin(pix):
	return struct.pack("BBB", pix[2], pix[1], pix[0]);

if len(sys.argv) < 2:
	raise Exception("Please specify a filename")
	sys.exit(1)

filename = sys.argv[1]
image = Image.open(filename)

for y in range(image.height):
	for x in range(image.width):
		sys.stdout.buffer.write(pixToBin(image.getpixel((x,y))))
