/*
 * End to End LoRa® Network
 * 
 * AUTHORS: Serdar Pehlivan and Arman Yavuz
 * 
 * MIT License
 * 
 * Copyright (c) 2022 Serdar Pehlivan and Arman Yavuz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Error_Utils.h"

uint8_t CalculateCRC8(void* arr, uint8_t length){
	/*
	 * Credits to: https://www.pololu.com/docs/0J44/6.7.6
	 */

	if(arr == NULL){return 0;}

	uint8_t i = 0;
	uint8_t crc = 0;
	for(i = 0; i < length; i++){
		crc = pgm_read_byte(&crc_table[crc ^ (uint8_t)*((char*)(arr) + i)]);
	}
	return crc;
}

uint8_t CheckCRC8(uint8_t original_crc, void* msg, uint8_t length){
	if(msg == NULL){return 0x02;}			//Null argument

	uint8_t crc = CalculateCRC8((uint8_t*)msg, length);
	if(crc == original_crc){return 0x00;}	//Matching CRC
	else{return 0x01;}						//Unmatching CRC
}