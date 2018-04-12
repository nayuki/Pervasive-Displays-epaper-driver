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

/*---- Early definitions ----*/

#include <cstdint>
#include <cstring>
#include <Arduino.h>
#include <SPI.h>
#include "EpaperDriver.hpp"

using std::uint8_t;
using std::uint32_t;
using std::uint64_t;
using std::size_t;


struct MandelParam {
	uint32_t centerReal;
	uint32_t centerImag;
	uint64_t scale;
	uint32_t iterations;
};



/*---- User-configurable constants ----*/

static MandelParam MANDEL_PARAMS[] = {
	{UINT32_C(4160749568), UINT32_C(        0), UINT64_C(409418147942772), UINT32_C( 100)},
	{UINT32_C(4268123751), UINT32_C(206173343), UINT64_C( 76110833702561), UINT32_C(1000)},
	{UINT32_C(4141279712), UINT32_C(173778818), UINT64_C(  8529544748808), UINT32_C(  40)},
	{UINT32_C(  96753273), UINT32_C( 95817235), UINT64_C(   668147672136), UINT32_C(120)},
	{UINT32_C(4232095836), UINT32_C(221903081), UINT64_C(   573637445697), UINT32_C(1000)},
	{UINT32_C(4092048422), UINT32_C( 26421526), UINT64_C( 13432716691602), UINT32_C( 300)},
};

static unsigned long INTERVAL = 30000;  // Target amount of time (in milliseconds) between images drawn



/*---- Main functions and global variables ----*/

// Variables/constants for setup()
static constexpr int WIDTH  = 264;
static constexpr int HEIGHT = 176;
static uint8_t prevImage[WIDTH * HEIGHT / 8] = {};
static EpaperDriver epd(EpaperDriver::Size::EPD_2_71_INCH, prevImage);

void setup() {
	// Configure pins for your microcontroller
	#if defined(CORE_TEENSY)
		// PJRC Teensy 3.x
		epd.panelOnPin = 3;
		epd.borderControlPin = 1;
		epd.dischargePin = 2;
		epd.resetPin = 22;
		epd.busyPin = 23;
		epd.chipSelectPin = 0;
	#elif defined(__MSP432P401R__)
		// "Texas Instruments SimpleLink MSP-EXP432P401R LaunchPad" (a.k.a. TI MSP432)
		epd.panelOnPin = 11;
		epd.borderControlPin = 13;
		epd.dischargePin = 12;
		epd.resetPin = 10;
		epd.busyPin = 8;
		epd.chipSelectPin = 19;
	#else
		#error "Define your pin mapping here"
	#endif
	
	epd.setFrameTime(1000);
	Serial.begin(9600);
	delay(1000);
}


// Variables/constants for loop()
static uint8_t image[WIDTH * HEIGHT / 8];
static size_t paramIndex = 0;
static bool initialDraw = true;

void loop() {
	Serial.print("Computing image #");
	Serial.print(paramIndex);
	Serial.print("...");
	unsigned long startTime = millis();
	
	// Cache current parameters
	MandelParam &param = MANDEL_PARAMS[paramIndex];
	uint32_t centerReal = param.centerReal;
	uint32_t centerImag = param.centerImag;
	uint64_t scale      = param.scale     ;
	uint32_t iterations = param.iterations;
	
	// Render image to memory
	std::memset(image, 0, sizeof(image));
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			size_t i = static_cast<size_t>(y) * static_cast<size_t>(WIDTH) + static_cast<size_t>(x);
			image[i / 8] |= calcMandelbrotPixel(centerReal, centerImag, scale, iterations, x, y) << (i % 8);
		}
	}
	
	unsigned long elapsed = millis() - startTime;
	Serial.print("  Elapsed ");
	Serial.print(elapsed);
	Serial.print("ms...");
	
	if (initialDraw)
		initialDraw = false;
	else if (elapsed < INTERVAL)
		delay(INTERVAL - elapsed);
	
	// Draw image to screen
	epd.changeImage(image);
	Serial.println("  Displayed.");
	
	// Change parameters for next iteration
	paramIndex++;
	if (paramIndex >= sizeof(MANDEL_PARAMS) / sizeof(MANDEL_PARAMS[0]))
		paramIndex = 0;
}



/*---- Mandelbrot computation functions ----*/

constexpr int FRACTION_BITS = 28;
constexpr uint32_t SMALL_ONE = UINT32_C(1) << FRACTION_BITS;
constexpr uint64_t BIG_ONE = UINT64_C(1) << (FRACTION_BITS * 2);
constexpr uint64_t BIG_HALF = BIG_ONE >> 1;
constexpr uint64_t BIG_FOUR = UINT64_C(4) << (FRACTION_BITS * 2);


static int calcMandelbrotPixel(uint32_t centerReal, uint32_t centerImag, uint64_t scale, uint32_t iterations, int x, int y) {
	uint32_t cr = roundFixedPoint(((x * 2 + 1) - 264) * scale) + centerReal;
	uint32_t ci = roundFixedPoint((176 - (y * 2 + 1)) * scale) + centerImag;
	uint32_t zr = 0;
	uint32_t zi = 0;
	uint32_t sr = zr;
	uint32_t si = zi;
	for (uint32_t i = 0; i < iterations; i++) {
		uint64_t a = widenSigned(zr);
		uint64_t b = widenSigned(zi);
		uint64_t c = a * a;
		uint64_t d = b * b;
		if (c + d > BIG_FOUR)
			return 0;
		zr = roundFixedPoint(c - d) + cr;
		zi = roundFixedPoint(a * b * 2) + ci;
		
		// Periodicity detection
		if (sr == zr && si == zi)
			break;
		if ((i & (i - 1)) == 0) {
			sr = zr;
			si = zi;
		}
	}
	return 1;
}


static uint32_t roundFixedPoint(uint64_t x) {
	uint64_t frac = x & (BIG_ONE - 1);
	uint32_t result = static_cast<uint32_t>(x >> FRACTION_BITS);
	if (frac > BIG_HALF || (frac == BIG_HALF && (x & BIG_ONE) != 0))
		result++;
	return result;
}


static uint64_t widenSigned(uint32_t x) {
	return x | (static_cast<uint64_t>(-(x >> 31)) << 32);
}
