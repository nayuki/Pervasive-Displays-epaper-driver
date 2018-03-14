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


static uint8_t prevImage[264 * 176 / 8] = {};
static EpaperDriver epd(prevImage);

void setup() {
	// Configure pins for the "Texas Instruments SimpleLink
	// MSP-EXP432P401R LaunchPad" (a.k.a. TI MSP432) microcontroller
	epd.panelOnPin = 11;
	epd.borderControlPin = 13;
	epd.dischargePin = 12;
	epd.resetPin = 10;
	epd.busyPin = 8;
	epd.chipSelectPin = 19;
	
	Serial.begin(9600);
	delay(1000);
}


static uint8_t image[264 * 176 / 8] = {};
static int checkerSize = 1;

void loop() {
	Serial.print("checkerSize = ");
	Serial.println(checkerSize);
	
	// Render image to memory
	std::memset(image, 0, sizeof(image));
	for (int y = 0; y < 176; y++) {
		for (int x = 0; x < 264; x++) {
			int i = y * 264 + x;
			int c = (x / checkerSize + y / checkerSize) % 2;
			image[i / 8] |= c << (i % 8);
		}
	}
	
	// Draw image to screen
	epd.powerOn();
	epd.changeImage(image);
	epd.powerOff();
	delay(3000);
	
	// Change parameters for next iteration
	checkerSize++;
	if (checkerSize > 100)
		checkerSize = 1;
}
