/* 
 * Hardware driver for Pervasive Displays' e-paper panels
 * 
 * Copyright (c) Project Nayuki
 * https://www.nayuki.io/
 */

#include <cstring>
#include <Arduino.h>
#include <SPI.h>
#include "EpaperDriver.hpp"

using std::uint8_t;
using std::uint32_t;
using Size = EpaperDriver::Size;
using Status = EpaperDriver::Status;


#ifdef __MSP432P401R__
	#define __MSP432P401R__ true
#else
	#define __MSP432P401R__ false
#endif



/*---- Constructor ----*/

EpaperDriver::EpaperDriver(Size sz, uint8_t prevPix[]) :
	previousPixels(prevPix),
	size(sz) {}



/*---- Drawing control methods ----*/

void EpaperDriver::setFrameRepeats(int iters) {
	if (iters > 0)
		frameRepeat = -iters;
}


void EpaperDriver::setFrameTime(int millis) {
	if (millis > 0)
		frameRepeat = millis;
}


void EpaperDriver::setFrameTimeByTemperature(int tmpr) {
	frameRepeat = 630;
	if      (tmpr <= -10)  frameRepeat *= 17;
	else if (tmpr <= - 5)  frameRepeat *= 12;
	else if (tmpr <=   5)  frameRepeat *=  8;
	else if (tmpr <=  10)  frameRepeat *=  4;
	else if (tmpr <=  15)  frameRepeat *=  3;
	else if (tmpr <=  20)  frameRepeat *=  2;
	else if (tmpr <=  40)  frameRepeat *=  1;
	else  frameRepeat = frameRepeat * 7 / 10;
}



/*---- Drawing methods ----*/

void EpaperDriver::changeImage(const uint8_t pixels[]) {
	changeImage(nullptr, pixels);
}


void EpaperDriver::changeImage(const uint8_t prevPix[], const uint8_t pixels[]) {
	if (prevPix == nullptr)
		prevPix = previousPixels;
	if (prevPix == nullptr)
		return;
	powerOn();
	
	// Stage 1: Compensate
	int iters;
	if (frameRepeat < 0) {  // Known number of iterations
		iters = -frameRepeat;
		drawFrame(prevPix, 3, 2, iters);
	} else if (frameRepeat > 0) {
		// Measure number of iterations needed to spend 'frameRepeat' milliseconds
		iters = 0;
		unsigned long startTime = millis();
		do {
			drawFrame(prevPix, 3, 2, 1);
			iters++;
		} while (millis() - startTime < static_cast<unsigned long>(frameRepeat));
	} else
		return;
	
	drawFrame(prevPix, 2, 0, iters);  // Stage 2: White
	drawFrame(pixels , 3, 0, iters);  // Stage 3: Inverse
	drawFrame(pixels , 2, 3, iters);  // Stage 4: Normal
	
	if (previousPixels != nullptr)
		std::memcpy(previousPixels, pixels, getBytesPerLine() * getHeight() * sizeof(pixels[0]));
	powerOff();
}


void EpaperDriver::drawFrame(const uint8_t pixels[],
		uint32_t mapWhiteTo, uint32_t mapBlackTo, int iterations) {
	int bytesPerLine = getBytesPerLine();
	int height = getHeight();
	for (int i = 0; i < iterations; i++) {
		for (int y = 0; y < height; y++)
			drawLine(y, &pixels[y * bytesPerLine], mapWhiteTo, mapBlackTo, 0x00);
	}
}


void EpaperDriver::drawLine(int row, const uint8_t pixels[], uint32_t mapWhiteTo, uint32_t mapBlackTo, uint8_t border) {
	spiRawPair(0x70, 0x0A);
	digitalWrite(chipSelectPin, LOW);
	SPI.transfer(0x72);
	if (size == Size::EPD_2_00_INCH || size == Size::EPD_2_71_INCH)
		SPI.transfer(border);
	
	// 'mapping' is a 3-bit to 4-bit look-up table. It has 8 entries of 4 bits each, thus it is 32 bits wide.
	// 'input' is any integer value, but only bits 0 and 2 are examined (i.e. masked with 0b101).
	// The 4-bit aligned block in mapping that is returned depends on the value of (input & 5).
	// If (input & 5) == 0b000, then bits 0 to 3 (inclusive) in mapping are returned.
	// If (input & 5) == 0b001, then bits 4 to 7 (inclusive) in mapping are returned.
	// If (input & 5) == 0b100, then bits 16 to 19 (inclusive) in mapping are returned.
	// If (input & 5) == 0b101, then bits 20 to 23 (inclusive) in mapping are returned.
	// The other 16 bits in mapping have no effect on the output, regardless of the input value.
	#define DO_MAP(mapping, input) \
		(((mapping) >> (((input) & 5) << 2)) & 0xF)
	int bytesPerLine = getBytesPerLine();
	
	// Send even pixels
	uint32_t evenMap =
		(mapWhiteTo << 2 | mapWhiteTo) <<  0 |
		(mapWhiteTo << 2 | mapBlackTo) <<  4 |
		(mapBlackTo << 2 | mapWhiteTo) << 16 |
		(mapBlackTo << 2 | mapBlackTo) << 20;
	for (int x = bytesPerLine - 1; x >= 0; x--) {
		uint8_t p = pixels[x];
		uint8_t b = static_cast<uint8_t>(
			(DO_MAP(evenMap, p >> 4) << 4) |
			(DO_MAP(evenMap, p >> 0) << 0));
		SPI.transfer(b);
	}
	
	// Send the scan bytes
	for (int y = getHeight() / 4 - 1; y >= 0; y--) {
		if (y == row / 4)
			SPI.transfer(3 << (row % 4 * 2));
		else
			SPI.transfer(0x00);
	}
	
	// Send odd pixels
	uint32_t oddMap =
		(mapWhiteTo << 2 | mapWhiteTo) <<  0 |
		(mapWhiteTo << 2 | mapBlackTo) << 16 |
		(mapBlackTo << 2 | mapWhiteTo) <<  4 |
		(mapBlackTo << 2 | mapBlackTo) << 20;
	for (int x = 0; x < bytesPerLine; x++) {
		uint8_t p = pixels[x];
		uint8_t b = static_cast<uint8_t>(
			(DO_MAP(oddMap, p >> 5) << 0) |
			(DO_MAP(oddMap, p >> 1) << 4));
		SPI.transfer(b);
	}
	
	#undef DO_MAP
	if (size == Size::EPD_1_44_INCH)
		SPI.transfer(border);
	digitalWrite(chipSelectPin, HIGH);
	spiWrite(0x02, 0x07);  // Turn on OE: output data from COG driver to panel
}



/*---- Image dimension methods ----*/

int EpaperDriver::getWidth() const {
	switch (size) {
		case Size::EPD_1_44_INCH:  return 128;
		case Size::EPD_2_00_INCH:  return 200;
		case Size::EPD_2_71_INCH:  return 264;
		default:  return -1;
	}
}


int EpaperDriver::getBytesPerLine() const {
	return getWidth() / 8;
}


int EpaperDriver::getHeight() const {
	switch (size) {
		case Size::EPD_1_44_INCH:  return  96;
		case Size::EPD_2_00_INCH:  return  96;
		case Size::EPD_2_71_INCH:  return 176;
		default:  return -1;
	}
}



/*---- Power methods ----*/

Status EpaperDriver::powerOn() {
	// Check arguments and state
	if (panelOnPin < 0 ||
			chipSelectPin < 0 ||
			resetPin < 0 ||
			busyPin < 0 ||
			(size == Size::EPD_2_71_INCH && borderControlPin < 0) ||
			dischargePin < 0)
		return Status::INVALID_PIN_CONFIG;
	
	// Set I/O pin directions
	pinMode(panelOnPin   , OUTPUT);
	pinMode(chipSelectPin, OUTPUT);
	pinMode(resetPin     , OUTPUT);
	pinMode(busyPin      , INPUT);
	if (size == Size::EPD_2_71_INCH)
		pinMode(borderControlPin, OUTPUT);
	pinMode(dischargePin , OUTPUT);
	
	// Set initial pin values
	digitalWrite(panelOnPin   , HIGH);
	digitalWrite(chipSelectPin, HIGH);
	if (size == Size::EPD_2_71_INCH)
		digitalWrite(borderControlPin, HIGH);
	digitalWrite(resetPin     , HIGH);
	digitalWrite(dischargePin , LOW);
	delay(5);
	
	// Pulse the reset pin
	digitalWrite(resetPin, LOW);
	delay(5);
	digitalWrite(resetPin, HIGH);
	delay(5);
	return powerInit();
}


Status EpaperDriver::powerInit() {
	// Wait until idle
	while (digitalRead(busyPin) == HIGH)
		delay(1);
	
	// Configure and start SPI
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setClockDivider(SPI_CLOCK_DIV2);
	if (__MSP432P401R__)
		SPI.setDataMode(SPI_MODE1);
	else
		SPI.setDataMode(SPI_MODE0);
	
	// Check chip ID. G1 COG driver's ID is 0x11, G2 is 0x12
	if (spiGetId() != 0x12) {
		powerOff();
		return Status::INVALID_CHIP_ID;
	}
	
	spiWrite(0x02, 0x40);  // Disable OE
	if ((spiRead(0x0F) & 0x80) == 0) {
		powerOff();
		return Status::BROKEN_PANEL;
	}
	spiWrite(0x0B, 0x02);  // Power saving mode
	
	// Channel select
	spiRawPair(0x70, 0x01);
	digitalWrite(chipSelectPin, LOW);
	SPI.transfer(0x72);
	static const uint8_t chanSel144[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00};
	static const uint8_t chanSel200[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xE0, 0x00};
	static const uint8_t chanSel271[] = {0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFE, 0x00, 0x00};
	const uint8_t *chanSel;
	switch (size) {
		case Size::EPD_1_44_INCH:  chanSel = chanSel144;  break;
		case Size::EPD_2_00_INCH:  chanSel = chanSel200;  break;
		case Size::EPD_2_71_INCH:  chanSel = chanSel271;  break;
		default:  return Status::INVALID_SIZE;
	}
	for (int i = 0; i < 8; i++)
		SPI.transfer(chanSel[i]);
	digitalWrite(chipSelectPin, HIGH);
	
	spiWrite(0x07, 0xD1);  // High power mode osc setting
	spiWrite(0x08, 0x02);  // Power setting
	spiWrite(0x09, 0xC2);  // Set Vcom level
	spiWrite(0x04, 0x03);  // Power setting
	spiWrite(0x03, 0x01);  // Driver latch on
	spiWrite(0x03, 0x00);  // Driver latch off
	delay(5);
	
	// Give a few attempts to turn on power
	for (int i = 0; i < 4; i++) {
		spiWrite(0x05, 0x01);  // Start charge pump positive voltage, VGH & VDH on
		delay(150);
		spiWrite(0x05, 0x03);  // Start charge pump negative voltage, VGL & VDL on
		delay(90);
		spiWrite(0x05, 0x0F);  // Set charge pump Vcom on
		delay(40);
		if ((spiRead(0x0F) & 0x40) != 0) {  // Check DC/DC
			spiWrite(0x02, 0x06);  // Output enable to disable
			return Status::OK;
		}
	}
	powerOff();
	return Status::DC_FAIL;
}


void EpaperDriver::powerFinish() {
	// Array length must be the maximum value of getBytesPerLine() among all sizes
	uint8_t whiteLine[33] = {};
	for (int i = 0, height = getHeight(); i < height; i++)  // Nothing frame
		drawLine(i, whiteLine, 0, 0, 0x00);
	
	if (size == Size::EPD_1_44_INCH || size == Size::EPD_2_00_INCH)
		drawLine(-4, whiteLine, 0, 0, 0xAA);  // Border dummy line
	else if (size == Size::EPD_2_71_INCH) {
		drawLine(-4, whiteLine, 0, 0, 0x00);  // Dummy line
		// Pulse the border pin
		delay(25);
		digitalWrite(borderControlPin, LOW);
		delay(100);
		digitalWrite(borderControlPin, HIGH);
	}
}


void EpaperDriver::powerOff() {
	powerFinish();
	
	spiWrite(0x0B, 0x00);  // Undocumented
	spiWrite(0x03, 0x01);  // Latch reset turn on
	spiWrite(0x05, 0x03);  // Power off charge pump, Vcom off
	spiWrite(0x05, 0x01);  // Power off charge pump negative voltage, VGL & VDL off
	delay(300);
	spiWrite(0x04, 0x80);  // Discharge internal
	spiWrite(0x05, 0x00);  // Power off charge pump positive voltage, VGH & VDH off
	spiWrite(0x07, 0x01);  // Turn off osc
	SPI.end();
	delay(50);
	
	if (size == Size::EPD_2_71_INCH)
		digitalWrite(borderControlPin, LOW);
	digitalWrite(panelOnPin, LOW);
	delay(10);
	digitalWrite(resetPin, LOW);
	digitalWrite(chipSelectPin, LOW);
	
	// Pulse the discharge pin
	digitalWrite(dischargePin, HIGH);
	delay(150);
	digitalWrite(dischargePin, LOW);
}



/*---- SPI methods ----*/

void EpaperDriver::spiWrite(uint8_t cmdIndex, uint8_t cmdData) {
	spiRawPair(0x70, cmdIndex);
	spiRawPair(0x72, cmdData);
}


uint8_t EpaperDriver::spiRead(uint8_t cmdIndex) {
	spiRawPair(0x70, cmdIndex);
	return spiRawPair(0x73, 0x00);
}


uint8_t EpaperDriver::spiGetId() {
	return spiRawPair(0x71, 0x00);
}


uint8_t EpaperDriver::spiRawPair(uint8_t b0, uint8_t b1) {
	// Initially must have chipSelectPin at HIGH, held for at least 80 nanoseconds
	digitalWrite(chipSelectPin, LOW);
	SPI.transfer(b0);
	uint8_t result = SPI.transfer(b1);
	digitalWrite(chipSelectPin, HIGH);
	return result;
}
