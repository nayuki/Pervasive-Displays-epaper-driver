/* 
 * Hardware driver for Pervasive Displays' e-paper displays
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
using Status = EpaperDriver::Status;


#ifdef __MSP432P401R__
	#define __MSP432P401R__ true
#else
	#define __MSP432P401R__ false
#endif


EpaperDriver::EpaperDriver() :
	EpaperDriver(nullptr) {}


EpaperDriver::EpaperDriver(uint8_t prevPix[]) :
	previousPixels(prevPix) {}


void EpaperDriver::changeImage(const uint8_t pixels[]) {
	if (previousPixels == nullptr)
		return;
	
	for (int i = 0; i < 176; i++)  // Stage 1: Compensate
		drawLine(i, &previousPixels[i * (264 / 8)], 3, 2);
	for (int i = 0; i < 176; i++)  // Stage 2: White
		drawLine(i, &previousPixels[i * (264 / 8)], 2, 0);
	
	for (int i = 0; i < 176; i++)  // Stage 3: Inverse
		drawLine(i, &pixels[i * (264 / 8)], 3, 0);
	for (int i = 0; i < 176; i++)  // Stage 4: Normal
		drawLine(i, &pixels[i * (264 / 8)], 2, 3);
	
	for (int i = 0; i < 176; i++)  // Nothing frame
		drawLine(i, previousPixels, 0, 0);
	
	std::memcpy(previousPixels, pixels, (264 * 176 / 8) * sizeof(pixels[0]));
}


void EpaperDriver::drawLine(int row, const uint8_t pixels[], uint32_t mapWhiteTo, uint32_t mapBlackTo) {
	if (!isOn)
		return;
	spiRawPair(0x70, 0x0A);
	digitalWrite(chipSelectPin, LOW);
	SPI.transfer(0x72);
	SPI.transfer(0x00);
	
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
	
	// Send even pixels
	uint32_t evenMap =
		(mapWhiteTo << 2 | mapWhiteTo) <<  0 |
		(mapWhiteTo << 2 | mapBlackTo) <<  4 |
		(mapBlackTo << 2 | mapWhiteTo) << 16 |
		(mapBlackTo << 2 | mapBlackTo) << 20;
	for (int i = 264 / 8 - 1; i >= 0; i--) {
		uint8_t p = pixels[i];
		uint8_t b = static_cast<uint8_t>(
			(DO_MAP(evenMap, p >> 4) << 4) |
			(DO_MAP(evenMap, p >> 0) << 0));
		SPI.transfer(b);
	}
	
	// Send the scan bytes
	for (int i = 176 / 4 - 1; i >= 0; i--) {
		if (i == row / 4)
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
	for (int i = 0; i < 264 / 8; i++) {
		uint8_t p = pixels[i];
		uint8_t b = static_cast<uint8_t>(
			(DO_MAP(oddMap, p >> 5) << 0) |
			(DO_MAP(oddMap, p >> 1) << 4));
		SPI.transfer(b);
	}
	
	#undef DO_MAP
	digitalWrite(chipSelectPin, HIGH);
	spiWrite(0x02, 0x07);  // Turn on OE: output data from COG driver to panel
}


Status EpaperDriver::powerOn() {
	if (panelOnPin == -1 ||
			chipSelectPin == -1 ||
			resetPin == -1 ||
			busyPin == -1 ||
			borderControlPin == -1 ||
			dischargePin == -1)
		return Status::INVALID_PIN_CONFIG;
	if (isOn)
		return Status::ALREADY_ON;
	isOn = true;
	
	pinMode(panelOnPin      , OUTPUT);
	pinMode(chipSelectPin   , OUTPUT);
	pinMode(resetPin        , OUTPUT);
	pinMode(busyPin         , INPUT);
	pinMode(borderControlPin, OUTPUT);
	pinMode(dischargePin    , OUTPUT);
	digitalWrite(panelOnPin      , HIGH);
	digitalWrite(chipSelectPin   , HIGH);
	digitalWrite(borderControlPin, HIGH);
	digitalWrite(resetPin        , HIGH);
	digitalWrite(dischargePin    , LOW);
	delay(5);
	digitalWrite(resetPin, LOW);
	delay(5);
	digitalWrite(resetPin, HIGH);
	delay(5);
	return powerInit();
}


Status EpaperDriver::powerInit() {
	while (digitalRead(busyPin) == HIGH)  // Wait until idle
		delay(1);
	
	// Configure and start SPI
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	if (__MSP432P401R__)
		SPI.setDataMode(SPI_MODE1);
	else
		SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(SPI_CLOCK_DIV2);
	
	if (spiGetId() != 0x12) {  // G1 COG driver ID is 0x11, G2 is 0x12
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
	SPI.transfer(0x00);
	SPI.transfer(0x00);
	SPI.transfer(0x00);
	SPI.transfer(0x7F);
	SPI.transfer(0xFF);
	SPI.transfer(0xFE);
	SPI.transfer(0x00);
	SPI.transfer(0x00);
	digitalWrite(chipSelectPin, HIGH);
	
	spiWrite(0x07, 0xD1);  // High power mode osc setting
	spiWrite(0x08, 0x02);  // Power setting
	spiWrite(0x09, 0xC2);  // Set Vcom level
	spiWrite(0x04, 0x03);  // Power setting
	spiWrite(0x03, 0x01);  // Driver latch on
	spiWrite(0x03, 0x00);  // Driver latch off
	delay(5);
	
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


void EpaperDriver::powerOff() {
	if (!isOn)
		return;
	isOn = false;
	
	spiWrite(0x0B, 0x00);  // Undocumented
	spiWrite(0x03, 0x01);  // Latch reset turn on
	spiWrite(0x05, 0x03);  // Power off charge pump, Vcom off
	spiWrite(0x05, 0x01);  // Power off charge pump negative voltage, VGL & VDL off
	spiWrite(0x04, 0x80);  // Discharge internal
	spiWrite(0x05, 0x00);  // Power off charge pump positive voltage, VGH & VDH off
	spiWrite(0x07, 0x01);  // Turn off osc
	delay(50);
	
	SPI.end();
	digitalWrite(borderControlPin, LOW);
	delay(10);
	digitalWrite(resetPin, LOW);
	digitalWrite(chipSelectPin, LOW);
	digitalWrite(dischargePin, HIGH);
	delay(150);
	digitalWrite(dischargePin, LOW);
	digitalWrite(panelOnPin, LOW);
}


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
