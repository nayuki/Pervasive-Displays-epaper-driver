/* 
 * Hardware driver for Pervasive Displays' e-paper displays
 * 
 * Copyright (c) Project Nayuki
 * https://www.nayuki.io/
 */

#include <Arduino.h>
#include <SPI.h>
#include "EpaperDriver.hpp"

using std::uint8_t;
using Status = EpaperDriver::Status;


Status EpaperDriver::powerOn() {
	if (chipSelectPin == -1 ||
			panelOnPin == -1 ||
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
