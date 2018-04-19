Hardware driver for Pervasive Displays' e-paper panels
======================================================

Introduction
------------

This project contains the following C++ code by Nayuki:

* A library for controlling e-paper panels manufactured by Pervasive Displays. The hardware driver logic is based on knowledge from Pervasive Displays' [document](http://www.pervasivedisplays.com/_literature_220873/COG_Driver_Interface_Timing_for_small_size_G2_V231).

* Demo programs (Arduino sketches) to illustrate how the library is used. This set includes different genres of artwork (text, photo, gradient, etc.). The majority of the logic in these programs are not specifically tied to e-paper hardware.

To build one example program (e.g. example/foo/foo.ino), copy every library source code file (src/*.{cpp,hpp}) to that example directory (example/foo/). Newer versions of the Arduino IDE (1.8.0+) can use this set of files as is. Older Arduino IDEs don't support the .hpp file extension, so .hpp files need to be renamed to .h, and every piece of code like `#include "bar.hpp"` needs to be changed to `#include "bar.h"`. Finally open the .ino file in the Arduino IDE and build it.

For convenience, a Python script (gather-files-for-build.py) is provided which performs all the preprocessing steps for a build. It creates a new "build" directory, copies all the examples there, copies the library code into each example, renames .hpp files to .h, and patches the file names in `#include` directives.

### Usage pseudocode

    #include <cstdint>
    #include "EpaperDriver.hpp"
    
    EpaperDriver epd(Size::EPD_2_71_INCH);
    epd.panelOnPin = (...);
    (... assign other pins ...)
    epd.dischargePin = (...);
    
    // Memory must be initialized to avoid undefined behavior when reading
    uint8_t prevImage[264 / 8 * 176] = {};
    epd.previousPixels = prevImage;
    
    const uint8_t *image = (...);  // The image we want to draw
    Status st = epd.changeImage(image);  // The main functionality
    if (st != Status::OK)
        print(st);  // Diagnostic info


Software features
-----------------

Supported features:

* Drawing a full image from a pointer to a raster bitmap array (in RAM or flash).
* Changing precisely the pixels that differ from one full image to the next (fast partial update), without clearing and redrawing all pixels.
* Automatically saving the image and painting the negative previous image.
* Specifying the frame draw repeat behavior by number of iterations, time duration, or temperature.
* Specifying arbitrary pin assignments for input and output signal lines.
* Managing the drawing commands to maximize image quality (reduce ghosting, noise, and other artifacts).
* Supporting multiple e-paper panel sizes from one family of products.
* Powering the device on and off properly.

Unsupported features:

* Keeping the device on after an image is drawn (reduces latency).
* Painting individual lines, in arbitrary order, without necessarily painting the entire screen.
* Low-level drawing control for pixel polarity, unequal number of frame repeats, border byte, etc.
* Reading `PROGMEM` data using special functions (for microcontrollers that can't use ordinary pointers to read constant data).


Hardware support
----------------

These combinations of hardware are tested to work:

* Microcontrollers:
  [PJRC Teensy](https://www.pjrc.com/teensy/index.html) 3.1, 3.2, 3.6;
  [Texas Instruments SimpleLink MSP-EXP432P401R LaunchPad Development Kit (MSP432)](http://www.ti.com/tool/MSP-EXP432P401R).

* Driver boards:
  [Pervasive Displays EPD Extension Kit Gen 2 (EXT2) (B3000MS034)](http://www.pervasivedisplays.com/kits/ext2_kit),
  [RePaper EPD Extension Board](https://web.archive.org/web/20161214070359/http://repaper.org/doc/extension_board.html).

* E-paper panels - Pervasive Displays, Aurora Mb film (V231), second generation chip-on-glass (G2 COG), external timing controller (eTC):
  [1.44-inch (128×96) (E2144CS021)](http://www.pervasivedisplays.com/products/144),
  [2.00-inch (200×96) (E2200CS021)](http://www.pervasivedisplays.com/products/200),
  [2.71-inch (264×176) (E2271CS021)](http://www.pervasivedisplays.com/products/271).

Unsupported hardware:

* Microcontrollers:
  Any microcontroller whose digital output pins are 5 V instead of 3.3 V,
  any microcontroller without a 3.3 V power output pin.

* E-paper panels by Pervasive Displays with any of:
  Aurora Ma film (V230),
  first generation chip-on-glass (G1 COG),
  internal timing controller (iTC).

* E-paper panels made by other manufacturers.


License
-------

Copyright © 2018 Project Nayuki. (MIT License)  
https://www.nayuki.io/page/pervasive-displays-epaper-panel-hardware-driver

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

* The Software is provided "as is", without warranty of any kind, express or
  implied, including but not limited to the warranties of merchantability,
  fitness for a particular purpose and noninfringement. In no event shall the
  authors or copyright holders be liable for any claim, damages or other
  liability, whether in an action of contract, tort or otherwise, arising from,
  out of or in connection with the Software or the use or other dealings in the
  Software.
