/* 
 * Hardware driver for Pervasive Displays' e-paper panels
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


/* 
 * A driver for Pervasive Displays' e-paper display (EPD) panels.
 * This allows a monochrome bitmap image to be drawn to the EPD.
 * 
 * Example usage pseudocode:
 *   EpaperDriver epd(Size::EPD_2_71_INCH);
 *   epd.panelOnPin = (...);
 *   (... assign other pins ...)
 *   epd.dischargePin = (...);
 *   
 *   // Memory must be initialized to avoid undefined behavior when reading
 *   uint8_t prevImage[264 / 8 * 176] = {};
 *   epd.previousPixels = prevImage;
 *   
 *   const uint8_t *image = (...);  // The image we want to draw
 *   Status st = epd.changeImage(image);  // The main functionality
 *   if (st != Status::OK)
 *     print(st);  // Diagnostic info
 */
class EpaperDriver final {
	
	/*---- Helper enums ----*/
	
	// Different EPD panel sizes, all of the Aurora Mb (V231) film type, with external timing controller (eTC).
	// The Aurora Ma (V230) film type (eTC), as well as internal timing controller (iTC) panels, are not supported.
	public: enum class Size : unsigned char {
		// Reserve index 0 to avoid accidental use of
		// uninitialized memory that happens to be zero
		INVALID = 0,
		EPD_1_44_INCH,
		EPD_2_00_INCH,
		EPD_2_71_INCH,
	};
	
	
	// Return codes for various methods.
	public: enum class Status : unsigned char {
		INTERNAL_ERROR = 0,
		OK,
		INVALID_PIN_CONFIG,
		INVALID_CHIP_ID,
		BROKEN_PANEL,
		DC_FAIL,
		INVALID_ARGUMENT,
	};
	
	
	
	/*---- Fields ----*/
	
	// Pin configuration. Before calling powerOn(), each pin must be set to a
	// non-negative unique value. All the pins listed here must be connected
	// between the microcontroller and the EPD hardware. Also, it is implied
	// that the microcontroller's default pins for SPI clock, MOSI, and MISO
	// must be connected to the EPD hardware.
	public: signed char panelOnPin       = -1;
	public: signed char chipSelectPin    = -1;
	public: signed char resetPin         = -1;
	public: signed char busyPin          = -1;  // Required for size EPD_2_71_INCH, ignored otherwise
	public: signed char borderControlPin = -1;
	public: signed char dischargePin     = -1;
	
	// Writable array for reading and writing the previous image. Can be null.
	// If this is not null, then the memory must be initialized because it will be read.
	public: std::uint8_t *previousPixels = nullptr;
	
	// The size of the EPD being driven.
	public: Size size;
	
	// Controls how many times or for how long a frame of each stage
	// is redrawn. Zero is invalid. Default value is a sane setting.
	// Positive value indicates the number of milliseconds.
	// Negative value indicates the number of repetitions.
	private: int frameRepeat;
	
	
	
	/*---- Constructor ----*/
	
	// Creates a driver with the given size and given previous image array (can be null).
	// This constructor doesn't perform any I/O or modify hardware configuration.
	public: explicit EpaperDriver(Size sz = Size::INVALID, std::uint8_t prevPix[] = nullptr);
	
	
	
	/*---- Drawing control methods ----*/
	
	// Sets the number of times that a frame of each stage is redrawn.
	// The number of iterations must be positive, otherwise the value is invalid.
	public: void setFrameRepeats(int iters);
	
	
	// Sets the duration (in milliseconds) that a frame of each stage is redrawn.
	// The time length must be positive, otherwise the value is invalid.
	public: void setFrameTime(int millis);
	
	
	// Sets the frame redraw behavior based on temperature (in degrees Celsius), based
	// on the vendor's table of recommended values. All input values are acceptable.
	public: void setFrameTimeByTemperature(int tmpr);
	
	
	
	/*---- Drawing methods ----*/
	
	// Changes the displayed image from some previous image to the given image.
	// - If the argument prevPix is not null, then it is used
	//   as the previous image (only read, not written).
	// - Else if the field previousImage is not null,
	//   then it is used as the previous image.
	// - Otherwise with both prevPix and previousImage
	//   being null, this method returns an error.
	// 
	// All elements of both the previous and current image arrays must have initialized values,
	// because the arrays will be read. (For example, it is unacceptable to allocate the previous
	// image array on the stack or with malloc(), without setting a value on each element.)
	// 
	// Four images are drawn to the screen in sequence: the negative of the previous image, an all-
	// white screen, the negative of the given image, and finally the positive of the given image.
	// If previousImage is not null (regardless of the value of prevPix), then the given
	// image is copied to previousImage, which may be read on the next call to changeImage().
	// 
	// All image arrays follow these rules:
	// - Array length is equal to width * height / 8, which must be an integer.
	// - Each bit represents a monochrome pixel. 0 means white, and 1 means black.
	// - The pixel at (x, y) (with both coordinates counting from 0), letting
	//   i = y * width + x, is stored at byte index floor(i / 8) (counting from 0),
	//   bit index i % 8 (where 0 is the least significant bit and 7 is the most significant).
	//   In other words, pixel bits are packed into bytes in little endian,
	//   and the 2D array is mapped into 1D in row-major order.
	// - There are no padding/ignored/wasted bits. Every bit affects the visible image.
	public: Status changeImage(const std::uint8_t pixels[], const std::uint8_t prevPix[] = nullptr);
	
	
	// Changes the displayed image much like changeImage(), but performs fewer drawing cycles.
	// The arguments are treated in the same way, and the previousImage array (if not null) is updated.
	// This method updates exactly the pixels on screen where the given image differs from the previous image,
	// and doesn't make the whole screen flicker. But image quality may be degraded around the edges
	// of changed pixels, so it's a good idea to use changeImage() periodically for a clean redraw.
	public: Status updateImage(const std::uint8_t pixels[], const std::uint8_t prevPix[] = nullptr);
	
	
	// Draws the given image the given number of times, mapping white pixels
	// to the given 2-bit value and black pixels to the given 2-bit value.
	private: void drawFrame(const std::uint8_t pixels[],
		std::uint32_t mapWhiteTo, std::uint32_t mapBlackTo, int iterations);
	
	
	// Draws the given line of pixels to the given row number, mapping white pixels
	// to the given 2-bit value and black pixels to the given 2-bit value.
	// Either 0 <= row < height to draw to a normal row,
	// or row = -4 to deactivate all the row selector bytes.
	private: void drawLine(int row, const std::uint8_t pixels[],
		std::uint32_t mapWhiteTo, std::uint32_t mapBlackTo, std::uint8_t border);
	
	
	// Draws the given line of differential pixels to the given row number.
	// For each column, if the pixel value in 'pixels' differs from that in 'prevPix',
	// then the pixel value in 'pixels' is drawn, otherwise a nothing value is drawn.
	// It is necessary to draw nothing on unchanged pixels because overdriving
	// the pixels with the same value can cause image degradation.
	private: void updateLine(int row, const std::uint8_t prevPix[], const std::uint8_t pixels[]);
	
	
	
	/*---- Image dimension methods ----*/
	
	// Returns the width of the image, in pixels. The value
	// is in the range [8, 264] and is a multiple of 8.
	public: int getWidth() const;
	
	
	// Returns the number of bytes per line, which is the
	// width divided by 8. The value is in the range [1, 33].
	public: int getBytesPerLine() const;
	
	
	// Returns the height of the image, in pixels. The value
	// is in the range [8, 176] and is a multiple of 8.
	public: int getHeight() const;
	
	
	
	/*---- Power methods ----*/
	
	// Powers on the G2 COG driver, followed by initialization.
	private: Status powerOn();
	
	
	// Initializes the G2 COG driver.
	private: Status powerInit();
	
	
	// Writes a nothing frame and dummy line, followed by power-off.
	private: void powerFinish();
	
	
	// Powers off the G2 COG driver.
	private: void powerOff();
	
	
	
	/*---- SPI methods ----*/
	
	// Sends a command over SPI to the device, containing exactly one data byte.
	// This cannot be used for writes that contain less or more than one data byte.
	private: void spiWrite(std::uint8_t cmdIndex, std::uint8_t cmdData);
	
	
	// Sends a command over SPI to the device, containing exactly one dummy byte,
	// reading the one-byte response, and returning it. This cannot
	// be used for reads that contain less or more than one data byte.
	private: std::uint8_t spiRead(std::uint8_t cmdIndex);
	
	
	// Sends a particular command over SPI to the device, reading the one-byte response,
	// and returning the value which is the chip-on-glass (COG) driver identification code.
	private: std::uint8_t spiGetId();
	
	
	// Sends the given two raw bytes over SPI to the device, returning the byte read from
	// the latter byte transfer, and holding the chip select pin low during the transfers.
	private: std::uint8_t spiRawPair(std::uint8_t b0, std::uint8_t b1);
	
};
