/*
*End to End LoRa® Network
*
*AUTHORS: Serdar Pehlivan and Arman Yavuz
*
*MIT License
*
*Copyright (c) 2022 Serdar Pehlivan and Arman Yavuz
*
*Permission is hereby granted, free of charge, to any person obtaining a copy
*of this software and associated documentation files (the "Software"), to deal
*in the Software without restriction, including without limitation the rights
*to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the Software is
*furnished to do so, subject to the following conditions:
*
*The above copyright notice and this permission notice shall be included in all
*copies or substantial portions of the Software.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*SOFTWARE.
*/

#ifndef EEL_DEBUG_H
#define EEL_DEBUG_H

#include "Arduino.h"

#define EEL_DEBUG

#ifdef EEL_DEBUG
	#define PRINT(x) Serial.print(F(x));
	#define PRINTLN(x) Serial.println(F(x));
	#define PRINTHEX(x) Serial.print(x, HEX);
	#define PRINTLNHEX(x) Serial.println(x, HEX);
	#define PRINTDELAY(x) delay(x);
#else
	#define PRINT(x) 
	#define PRINTLN(x) 
	#define PRINTHEX(x) 
	#define PRINTLNHEX(x) 
	#define PRINTDELAY(x) 
#endif

#endif