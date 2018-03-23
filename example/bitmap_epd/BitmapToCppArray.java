/* 
 * Bitmap image file to C++ array code converter
 * 
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/pervasive-displays-epaper-panel-hardware-driver
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import javax.imageio.ImageIO;


public final class BitmapToCppArray {
	
	public static void main(String[] args) throws IOException {
		if (args.length != 3) {
			System.err.println("Usage: java BmpToCppArray InputImage.bmp/png OutputVariableName OutputCode.hpp");
			System.err.println("Example: java BmpToCppArray Image0.png img0 image0.hpp");
			System.exit(1);
			return;
		}
		
		BufferedImage image = ImageIO.read(new File(args[0]));
		if (image.getWidth() % 8 != 0)
			throw new IllegalArgumentException("Invalid width");
		
		int[] array = new int[image.getWidth() / 8 * image.getHeight()];
		for (int y = 0; y < image.getHeight(); y++) {
			for (int x = 0; x < image.getWidth(); x++) {
				int pixel = image.getRGB(x, y) & 0xFFFFFF;
				if (pixel != 0x000000 && pixel != 0xFFFFFF)
					throw new IllegalArgumentException("Pixel not black or white");
				int i = y * image.getWidth() + x;
				array[i / 8] |= (1 - (pixel & 1)) << (i % 8);
			}
		}
		
		try (PrintWriter out = new PrintWriter(new FileWriter(args[2]))) {
			out.println("#include <cstdint>");
			out.println("static const std::uint8_t " + args[1] + "[] = {");
			for (int i = 0; i < array.length; i++) {
				if (i % 20 == 0)
					out.print("\t");
				out.printf("0x%02X,", array[i]);
				if ((i + 1) % 20 == 0 || i == array.length - 1)
					out.println();
				else
					out.print(" ");
			}
			out.println("};");
		}
	}
	
}
