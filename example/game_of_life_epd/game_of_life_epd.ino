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
static int imageWidth  = epd.getWidth ();
static int imageHeight = epd.getHeight();


class BitGrid final {
	public: uint8_t *data;  // Must have length ceil(width * height / 8), and have all elements initialized.
	public: int width;      // Must be non-negative.
	public: int height;     // Must be non-negative.
	
	// Returns 0 or 1 if (x, y) is in bounds, otherwise returns 0.
	public: int getCell(int x, int y) const {
		if (x < 0 || x >= width || y < 0 || y >= height)
			return 0;
		int i = y * width + x;
		return (data[i >> 3] >> (i & 7)) & 1;
	}
	
	// (x, y) must be in bounds and val must be 0 or 1, otherwise the method does nothing.
	public: void setCell(int x, int y, int val) {
		if (x < 0 || x >= width || y < 0 || y >= height || (val != 0 && val != 1))
			return;
		int i = y * width + x;
		data[i >> 3] &= ~(1 << (i & 7));
		data[i >> 3] |= val << (i & 7);
	}
};

static constexpr int boardScale = 6;  // Must be at least 2
static constexpr int boardWidth  = (MAX_WIDTH  - 1 + (boardScale - 1)) / boardScale;
static constexpr int boardHeight = (MAX_HEIGHT - 1 + (boardScale - 1)) / boardScale;
static uint8_t boardData[(boardWidth * boardHeight + 7) / 8] = {};
static BitGrid board;


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
	
	epd.setFrameTime(800);
	board.data = boardData;
	board.width = boardWidth;
	board.height = boardHeight;
	
	// Initialize the grid randomly
	randomSeed(analogRead(0));
	for (int y = 0; y < boardHeight; y++) {
		for (int x = 0; x < boardWidth; x++)
			board.setCell(x, y, random(100) < 30 ? 1 : 0);
	}
	delay(1000);
}


static uint8_t image[MAX_WIDTH * MAX_HEIGHT / 8];
static int remainingUpdates = 0;

void loop() {
	// Render image to memory
	std::memset(image, 0, sizeof(image));
	for (int y = 0; y < imageHeight; y++) {
		for (int x = 0; x < imageWidth; x++) {
			size_t i = static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x);
			int c;
			if (x % boardScale == 0 || y % boardScale == 0)
				c = 1;
			else
				c = board.getCell(x / boardScale, y / boardScale);
			image[i / 8] |= c << (i % 8);
		}
	}
	
	// Draw image to screen
	if (remainingUpdates <= 0) {
		epd.changeImage(image);
		remainingUpdates = 30;
	} else {
		epd.updateImage(image);
		remainingUpdates--;
	}
	
	// Compute next board state
	nextGameOfLifeState();
}


static void nextGameOfLifeState() {
	int rowBytes = (boardWidth + 7) / 8;
	uint8_t newTopRow [rowBytes];
	uint8_t newPrevRow[rowBytes];
	uint8_t newCurRow [rowBytes];
	
	for (int y = 0; y < boardHeight; y++) {
		std::memset(newCurRow, 0, sizeof(newCurRow));
		for (int x = 0; x < boardWidth; x++) {
			int sum = -board.getCell(x, y);
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					int xx = (x + dx + boardWidth ) % boardWidth ;
					int yy = (y + dy + boardHeight) % boardHeight;
					sum += board.getCell(xx, yy);
				}
			}
			bool alive = board.getCell(x, y) != 0;
			newCurRow[x >> 3] |= (sum == 3 || (alive && sum == 2) ? 1 : 0) << (x & 7);
		}
		
		if (y >= 2) {
			for (int x = 0; x < boardWidth; x++)
				board.setCell(x, y - 1, (newPrevRow[x >> 3] >> (x & 7)) & 1);
		}
		std::memcpy(y == 0 ? newTopRow : newPrevRow, newCurRow, sizeof(newCurRow));
	}
	for (int x = 0; x < boardWidth; x++) {
		board.setCell(x, 0, (newTopRow[x >> 3] >> (x & 7)) & 1);
		board.setCell(x, boardHeight - 1, (newPrevRow[x >> 3] >> (x & 7)) & 1);
	}
}
