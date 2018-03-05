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
			resetPin == -1 ||
			busyPin == -1 ||
			borderControlPin == -1 ||
			dischargePin == -1)
		return Status::INVALID_PIN_CONFIG;
	
	pinMode(chipSelectPin   , OUTPUT);
	pinMode(resetPin        , OUTPUT);
	pinMode(busyPin         , INPUT);
	pinMode(borderControlPin, OUTPUT);
	pinMode(dischargePin    , OUTPUT);
	digitalWrite(chipSelectPin   , HIGH);
	digitalWrite(borderControlPin, HIGH);
	digitalWrite(resetPin        , HIGH);
	digitalWrite(dischargePin    , LOW);
	delay(5);
	digitalWrite(resetPin, LOW);
	delay(5);
	return Status::OK;
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
