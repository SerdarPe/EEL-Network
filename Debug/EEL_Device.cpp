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

#include "EEL_Device.h"

#define CHANNEL_BASE 23
#define CHANNEL_MAX 27 //23 + 4 = 27, Hence 4 different channels

EEL_Device::EEL_Device(LoRa_E220* device){
	this->_device = device;
	this->_device->begin();
}

uint8_t EEL_Device::receive(void* buffer, uint8_t size){
    return _device->readBytes(buffer, size);
}

uint8_t EEL_Device::receive(void* buffer, uint8_t size, bool buffer_flush){
	return _device->readBytes(buffer, size, buffer_flush);
}

uint8_t EEL_Device::peek(){
	return _device->serialDef.stream->peek();
}

uint8_t EEL_Device::read(){
	return _device->serialDef.stream->read();
}

uint8_t EEL_Device::send(uint8_t ADDH, uint8_t ADDL, uint8_t CH, void* buffer, uint8_t size){
    uint8_t bytes_sent = _device->sendData(ADDH, ADDL, CH, buffer, size, 0);
	if(_delay_enabled){delay( (1000 * (uint32_t)size) / (_device->get_baud() >> 3) );}
	PRINT("DATA_SENT: ")PRINTLNHEX(bytes_sent)
	PRINT("ADDH: ")PRINTLNHEX(ADDH)
	PRINT("ADDL: ")PRINTLNHEX(ADDL)
	PRINT("CH: ")PRINTLNHEX(CH)
	return bytes_sent;
}

void EEL_Device::broadcast(void* buffer, uint8_t size){
	for(int i = CHANNEL_BASE; i < CHANNEL_MAX; i++){
		_device->sendData(0xFF, 0xFF, i, buffer, size, 0);
		if(_delay_enabled){delay( (1000 * (uint32_t)size) / (_device->get_baud() >> 3) );}
	}
	PRINTLN("BROADCASTING...")
}

bool EEL_Device::available(){
	return _device->available() > 0;
}

uint8_t EEL_Device::available_bytes(){
	return _device->available();
}