/* 
 * Demo program for e-paper display hardware driver
 * 
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/
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
	
	epd.setFrameTime(300);
	Serial.begin(9600);
	delay(1000);
}


static uint8_t image[MAX_WIDTH * MAX_HEIGHT / 8];
static int checkerSize = 1;

void loop() {
	Serial.print("checkerSize = ");
	Serial.println(checkerSize);
	
	// Scalar constants
	int width = epd.getWidth();
	int height = epd.getHeight();
	
	// Render image to memory
	std::memset(image, 0, sizeof(image));
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			size_t i = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
			int c = (x / checkerSize + y / checkerSize) % 2;
			image[i / 8] |= c << (i % 8);
		}
	}
	
	// Draw image to screen
	epd.changeImage(image);
	delay(3000);
	
	// Change parameters for next iteration
	checkerSize++;
	if (checkerSize > 100)
		checkerSize = 1;
}
