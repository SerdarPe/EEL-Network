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

#ifndef EEL_DEVICE_INTERFACE_H
#define EEL_DEVICE_INTERFACE_H

#include "LoRa_E220.h"
#include "EEL_Debug.h"
class EEL_Device{
    public:
		EEL_Device(LoRa_E220* device);

		uint8_t peek();
		uint8_t read();
		uint8_t receive(void* buffer, uint8_t size);
		uint8_t receive(void* buffer, uint8_t size, bool buffer_flush);
		
		uint8_t send(uint8_t ADDH, uint8_t ADDL, uint8_t CH, void* buffer, uint8_t size);
		void broadcast(void* buffer, uint8_t size);

		void clean_rx_buffer(){return _device->cleanUARTBuffer();}
		void clean_tx_buffer(){return _device->flush();}

		void enable_delay(){_delay_enabled = true;}
		void disable_delay(){_delay_enabled = false;}

		bool available();
		uint8_t available_bytes();
	private:		
		LoRa_E220* _device;
		bool _delay_enabled = true;
};

#endif

