/* 
 * Demo program for e-paper display hardware driver
 * 
 * Copyright (c) Project Nayuki
 * https://www.nayuki.io/
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
