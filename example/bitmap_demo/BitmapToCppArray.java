/* 
 * Bitmap image file to C++ array code converter
 * 
 * Copyright (c) Project Nayuki
 * https://www.nayuki.io/
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
