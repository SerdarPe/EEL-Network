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

#include "EEL_Transport.h"

#include <string.h>

#define PORT_READ_FLAG 0x01
#define PORT_WRITE_FLAG 0x02
#define PORT_LARGE_FLAG 0x04
#define PORT_NULL_FLAG 0x80

#define DATAGRAM_CRC_SIZE 3

EEL_Transport::EEL_Transport(){
	//RESET PORTS BUFFER
	for(int i = 0; i < PORTS_SIZE; i++){
		port_buffers_[i] = NULL;
	}

	//RESET DATAGRAM
	datagram_in = NULL;
}

/* Function to create a datagram unit from a payload. Payload is copied(a duplicate) into the newly created datagram.
 * Do not manipulate the datagram directly.
 */
struct Datagram* CreateDatagram(uint8_t source_port, uint8_t destination_port, void* data, uint8_t data_length){
	struct Datagram* p_datagram = (struct Datagram*)malloc(sizeof(struct Datagram));
	if(p_datagram == NULL){
		return NULL;
	}
	p_datagram->payload_length = data_length;
	p_datagram->source_port = source_port;
	p_datagram->destination_port = destination_port;
	p_datagram->data = (char*)malloc(data_length);
	memcpy(p_datagram->data, data, data_length);
	p_datagram->crc = CalculateCRC8((uint8_t*)(p_datagram), DATAGRAM_CRC_SIZE);

	return p_datagram;
}

//Function to free the datagram easily, NEVER pass an array of datagram pointers
void FreeDatagram(Datagram** datagram){
	if(*datagram){
		free((*datagram)->data);
		free(*datagram);
		*datagram = NULL;
	}
}

void DatagramEditSource(uint8_t source_port, struct Datagram* datagram){
	if(datagram == NULL){return;}
	datagram->source_port = source_port;
	datagram->crc = CalculateCRC8((uint8_t*)(datagram), DATAGRAM_CRC_SIZE);
}

void DatagramEditDestination(uint8_t destination_port, struct Datagram* datagram){
	if(datagram == NULL){return;}
	datagram->destination_port = destination_port;
	datagram->crc = CalculateCRC8((uint8_t*)(datagram), DATAGRAM_CRC_SIZE);
}

void EEL_Transport::FromNetworkLayer(void* data){
	if(data == NULL){return;}
		
	this->datagram_in = (struct Datagram*)data;

	if(CheckCRC8(datagram_in->crc, (uint8_t*)(datagram_in), DATAGRAM_CRC_SIZE) != 0x00){
		PRINTLN("DATAGRAM_CRC_ERROR")
		PRINT("RECEIVED_CRC: ")PRINTLNHEX(datagram_in->crc)
		PRINT("CALCULATED_CRC: ")PRINTLNHEX(CalculateCRC8((uint8_t*)(datagram_in), DATAGRAM_CRC_SIZE))
		return;
	}

	PortBufferElement* e = GetPort(datagram_in->destination_port);
	if(e != NULL){
		if(datagram_in->payload_length > e->buffer_size){
			e->length = e->buffer_size;
			e->flags |= (PORT_LARGE_FLAG);
			e->source_port = datagram_in->source_port;
			memcpy(e->buffer, datagram_in->data, e->length);

		}
		else{
			e->length = datagram_in->payload_length;
			e->flags &= ~(PORT_LARGE_FLAG);
			e->source_port = datagram_in->source_port;
			memcpy(e->buffer, datagram_in->data, e->length);
		}
		e->flags &= ~(PORT_READ_FLAG);
		e->flags |= (PORT_WRITE_FLAG);
	}
}

uint8_t* EEL_Transport::ReadPort(uint8_t port_no, uint8_t* length, uint8_t* source){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		if(length != NULL){
			*length = e->length;
		}
		if(source != NULL){
			*source = e->source_port;
		}
		e->flags |= (PORT_READ_FLAG);
		e->flags &= ~(PORT_WRITE_FLAG);
		return e->buffer;
	}
	else{
		if(length != NULL){
			*length = 0;
		}
		if(source != NULL){
			*source = 0;
		}
		return NULL;
	}
}

int8_t EEL_Transport::SetPort(uint8_t port_no, uint8_t buffer_size){
	if(GetPort(port_no) != NULL){
		return -1;
	}

	PortBufferElement* e = (PortBufferElement*)malloc(sizeof(PortBufferElement));
	e->buffer = (uint8_t*)malloc(buffer_size);
	e->buffer_size = buffer_size;
	e->length = 0;
	e->port_no = port_no;
	e->source_port = 0;
	e->flags = 0;
	AddPort(e);
	return port_no;
}

void EEL_Transport::RemovePort(uint8_t port_no){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		free(e->buffer);
		free(e);
		port_buffers_[GetPortIndex(port_no)] = NULL;
	}
}

int8_t EEL_Transport::IsPortWritten(uint8_t port_no){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		return e->flags & PORT_WRITE_FLAG;
	}
	return PORT_NULL_FLAG;
}

PortBufferElement* EEL_Transport::GetPort(uint8_t port_no){
	for(int i = 0; i < PORTS_SIZE; i++){
		PortBufferElement* e = port_buffers_[i];
		if(e != NULL){
			if(port_no == e->port_no){
				return e;
			}
		}
		continue;
	}
	return NULL;
}

int8_t EEL_Transport::GetPortIndex(uint8_t port_no){
	for(int i = 0; i < PORTS_SIZE; i++){
		PortBufferElement* e = port_buffers_[i];
		if(e != NULL){
			if(port_no == e->port_no){
				return i;
			}
		}
		continue;
	}
	return -1;
}

int8_t EEL_Transport::AddPort(PortBufferElement* e){
	for(int i = 0; i < PORTS_SIZE; i++){
		if(port_buffers_[i] == NULL){
			port_buffers_[i] = e;
			return 1;
		}
		continue;
	}
	return 0;
}

void EEL_Transport::CleanUp(){
	free(datagram_out);
	datagram_out = NULL;
	datagram_in = NULL;
}