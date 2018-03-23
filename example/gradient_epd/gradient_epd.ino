/* 
 * Demo program for e-paper display hardware driver
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

#include <cstdint>
#include <cstring>
#include <Arduino.h>
#include <SPI.h>
#include "EpaperDriver.hpp"

using std::uint8_t;
using std::size_t;


static constexpr int MAX_WIDTH  = 264;
static constexpr int MAX_HEIGHT = 176;

static uint8_t prevImage[MAX_WIDTH * MAX_HEIGHT / 8] = {};
static EpaperDriver epd(EpaperDriver::Size::EPD_2_71_INCH, prevImage);

void setup() {
	// Configure pins for the "Texas Instruments SimpleLink
	// MSP-EXP432P401R LaunchPad" (a.k.a. TI MSP432) microcontroller
	epd.panelOnPin = 11;
	epd.borderControlPin = 13;
	epd.dischargePin = 12;
	epd.resetPin = 10;
	epd.busyPin = 8;
	epd.chipSelectPin = 19;
	
	epd.setFrameTime(800);
	Serial.begin(9600);
	delay(1000);
}


static uint8_t image[MAX_WIDTH * MAX_HEIGHT / 8];
static int16_t curRowDelta[MAX_WIDTH + 1];
static int16_t nextRowDelta[MAX_WIDTH + 1];
static int orientation = 0;

void loop() {
	Serial.print("orientation = ");
	Serial.println(orientation);
	
	// Scalar constants
	int width = epd.getWidth();
	int height = epd.getHeight();
	int gradientLength;
	switch (orientation) {
		case 0:
		case 2:
			gradientLength = height;
			break;
		case 1:
		case 3:
		case 4:
			gradientLength = width;
			break;
		case 5:
			gradientLength = sqrt(static_cast<long>(width) * width + static_cast<long>(height) * height);
			break;
	}
	
	// Render image to memory. Note: val, err, curRowDelta, and nextRowDelta
	// are fractions with an implicit denominator of (gradientLength * 64).
	std::memset(image, 0, sizeof(image));
	std::memset(curRowDelta, 0, sizeof(curRowDelta));
	std::memset(nextRowDelta, 0, sizeof(nextRowDelta));
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			
			// Calculate ideal pixel value
			int val;
			if (orientation < 4) {
				switch (orientation) {
					case 0:  val = y * 2 + 1;  break;  // Vertical gradient: top = white, bottom = black
					case 1:  val = x * 2 + 1;  break;  // Horizontal gradient: left = white, right = black
					case 2:  val = (height - y) * 2 - 1;  break;  // Vertical gradient: top = black, bottom = white
					case 3:  val = (width  - x) * 2 - 1;  break;  // Horizontal gradient: left = black, right = white
				}
				val = val * 32;
			} else if (orientation == 4) {  // Radial gradient: center = white, perimeter = black
				long dx = (x * 2 + 1 - width) * 64;
				long dy = (y * 2 + 1 - height) * 64;
				val = sqrt(dx * dx + dy * dy);
				if (val > gradientLength * 64)
					val = gradientLength * 64;
			} else if (orientation == 5) {  // Radial gradient: top left = white, bottom right = black
				long dx = (x * 2 + 1) * 32;
				long dy = (y * 2 + 1) * 32;
				val = sqrt(dx * dx + dy * dy);
				if (val > gradientLength * 64)
					val = gradientLength * 64;
			}
			
			// Floyd-Steinberg dithering
			val += curRowDelta[x];
			int c = val >= gradientLength * 32 ? 1 : 0;  // Quantize to bi-level
			int err = c * gradientLength * 64 - val;
			if (x > 0)
				nextRowDelta[x - 1] -= err * 3 / 16;
			nextRowDelta[x] -= err * 5 / 16;
			nextRowDelta[x + 1] -= err * 1 / 16;
			curRowDelta[x + 1] -= err * 7 / 16;
			
			size_t i = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
			image[i / 8] |= c << (i % 8);
		}
		std::memcpy(curRowDelta, nextRowDelta, sizeof(curRowDelta));
		std::memset(nextRowDelta, 0, sizeof(nextRowDelta));
	}
	
	// Draw image to screen
	epd.changeImage(image);
	delay(4000);
	
	// Change parameters for next iteration
	orientation = (orientation + 1) % 6;
}


static int sqrt(long x) {
	int y = 0;
	for (int i = 1 << 15; i != 0; i >>= 1) {
		y |= i;
		if (y > 46340 || y * y > x)
			y ^= i;
	}
	return y;
}
