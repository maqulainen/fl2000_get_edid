#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#  bytes2bmp.py
#  
#  Copyright 2016 MaQ <maq@Slynon>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  


def main(args):
	from PIL import Image
	import cStringIO as StringIO
	f = open(args[1], 'r')
	#hex_data = StringIO.StringIO(f.read())
	#img = Image.frombuffer('RGB', (64,64), f.read()) #portability change
	mode = 'RGB'
	#mode = 'RGBA'
	#mode = 'CMYK'
	#mode = 'RGB;L'
	
	#The distance in bytes between two consecutive lines in the image.
	#If 0, the image is assumed to be packed (no padding between lines).
	stride = 2
	
	orientation = 1
	
	img = Image.frombuffer(mode, (64,64), f.read(), 'raw', mode, stride, orientation)
	#img = Image.open(stream)
	#print(img.size)
	img.show()
	#img.size (320, 240)
	return 0

if __name__ == '__main__':
	import sys
	sys.exit(main(sys.argv))
