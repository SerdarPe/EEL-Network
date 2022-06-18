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

#ifndef EEL_TRANSPORT_H
#define EEL_TRANSPORT_H

#include "Arduino.h"

#include <stdlib.h>
#include "EEL_Debug.h"
#include "Error_Utils.h"

#define DATAGRAM_LENGTH(x) (uint8_t)(((struct Datagram*)(x))->payload_length)
#define DATAGRAM_SOURCE(x) (uint16_t)(((struct Datagram*)(x))->source_port)
#define DATAGRAM_DEST(x) (uint16_t)(((struct Datagram*)(x))->destination_port)
#define DATAGRAM_DATA(x) (((struct Datagram*)(x))->data)

#define PORTS_SIZE 16
struct PortBufferElement{
	uint8_t flags;
	uint8_t length;
	uint8_t buffer_size;
	uint8_t source_port;
	uint8_t port_no;
	uint8_t* buffer;
};

struct Datagram{
	uint8_t payload_length;
	uint8_t source_port;
	uint8_t destination_port;
	uint8_t crc;
	char* data;
};
struct Datagram* CreateDatagram(uint8_t source_port, uint8_t destination_port, void* data, uint8_t data_length);
void FreeDatagram(Datagram** datagram);
void DatagramEditSource(uint8_t source_port, struct Datagram* datagram);
void DatagramEditDestination(uint8_t destination_port, struct Datagram* datagram);

class EEL_Transport{
	public:
		EEL_Transport();

		void FromApplicationLayer(void* data);
		void FromApplication(struct Datagram* datagram);
		
		void* ToNetworkLayer();
		void FromNetworkLayer(void* data);

		int8_t SetPort(uint8_t port_no, uint8_t buffer_size);
		uint8_t* ReadPort(uint8_t port_no, uint8_t* length, uint8_t* source);
		void RemovePort(uint8_t port_no);
		int8_t IsPortWritten(uint8_t port_no);

		void CleanUp();
	private:
		struct Datagram* datagram_in;
		struct Datagram* datagram_out;

		PortBufferElement* port_buffers_[PORTS_SIZE];

		PortBufferElement* GetPort(uint8_t port_no);
		int8_t GetPortIndex(uint8_t port_no);
		int8_t AddPort(PortBufferElement* e);
		
};

#endif