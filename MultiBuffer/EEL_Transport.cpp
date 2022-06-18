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
#include "EEL_Debug.h"

#define PORT_READ_FLAG 0x01
#define PORT_WRITE_FLAG 0x02
#define PORT_LARGE_FLAG 0x04
#define PORT_NO_EMPTY_SPACE 0x08
#define PORT_NULL_FLAG 0x80

#define BUF_EXTRA_DATA_LEN 2 //1byte-msg_len, 1-byte-sourceport,

#define BUF_EXTRA_SHIFT_MSGLEN 0
#define BUF_EXTRA_SHIFT_SRCPORT 1

EEL_Transport::EEL_Transport(){
	//RESET PORTS BUFFER
	for(int i = 0; i < PORTS_SIZE; i++){
		port_buffers_[i] = NULL;
	}

	//RESET DATAGRAM
	datagram_in_ = NULL;
	datagram_out_ = NULL;
}

struct Datagram* CreateDatagram(uint8_t source_port, uint8_t destination_port, void* data, uint8_t data_length){
	struct Datagram* p_datagram = (struct Datagram*)malloc(sizeof(struct Datagram));
	if(p_datagram == NULL){return NULL;}

	p_datagram->payload_length = data_length;
	p_datagram->source_port = source_port;
	p_datagram->destination_port = destination_port;
	p_datagram->crc = CalculateCRC8((uint8_t*)(p_datagram), 3);

	p_datagram->data = (char*)malloc(data_length);
	memcpy(p_datagram->data, data, data_length);
	//*(p_datagram->data + data_length) = '\0';

	return p_datagram;
}

//Use this function to free the datagram easily, NEVER pass an array of datagram pointers
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
	datagram->crc = CalculateCRC8((uint8_t*)(datagram), 3);
}

void DatagramEditDestination(uint8_t destination_port, struct Datagram* datagram){
	if(datagram == NULL){return;}
	datagram->destination_port = destination_port;
	datagram->crc = CalculateCRC8((uint8_t*)(datagram), 3);
}

void EEL_Transport::FromNetworkLayer(void* data){
	if(data == NULL){return;}
	this->datagram_in_ = (struct Datagram*)data;

	if(CheckCRC8(datagram_in_->crc, (uint8_t*)(datagram_in_), 3) != 0x00){PRINTLN("DATAGRAM_CRC_ERROR")return;}

	PortBufferElement* e = GetPort(datagram_in_->destination_port);
	if(e != NULL){
		int16_t remaining_size = e->buffer_size;
		uint8_t* buf_ptr = e->buffer;
		while(*(buf_ptr) != 0){
			remaining_size -= *(buf_ptr) + BUF_EXTRA_DATA_LEN;
			buf_ptr += *(buf_ptr) + BUF_EXTRA_DATA_LEN;

			if(remaining_size <= 0){
				e->flags |= (PORT_NO_EMPTY_SPACE);
				return;
			}
			//If not enough space left for at least 1 byte of data, zero the remaining space
			if(remaining_size <= BUF_EXTRA_DATA_LEN){
				while(remaining_size--){*(buf_ptr++) = 0;}
				e->flags |= (PORT_NO_EMPTY_SPACE);
				return;
			}
		}
		e->flags &= ~(PORT_NO_EMPTY_SPACE);
		
		if(datagram_in_->payload_length + BUF_EXTRA_DATA_LEN > --remaining_size){ 		//+1 for trailing "0" or "--remaining_size"
			//Copy whatever data we can into remaining space
			uint8_t _length = remaining_size - BUF_EXTRA_DATA_LEN;
			*(buf_ptr++) = _length;
			*(buf_ptr++) = datagram_in_->source_port;
			memcpy(buf_ptr, datagram_in_->data, _length);
			*(buf_ptr + _length) = 0;
			if(_length != 0){e->message_count++;}

			e->flags |= (PORT_LARGE_FLAG | PORT_NO_EMPTY_SPACE);
		}
		else{
			//	---------------------------------------------------------------
			//	| MSG_LEN | SRC_PORT | DATA | MSG_LEN | SRC_PORT | DATA | ... |
			//	---------------------------------------------------------------
			*(buf_ptr++) = datagram_in_->payload_length;
			*(buf_ptr++) = datagram_in_->source_port;
			e->flags &= ~(PORT_LARGE_FLAG);
			memcpy(buf_ptr, datagram_in_->data, datagram_in_->payload_length);
			*(buf_ptr + datagram_in_->payload_length) = 0;
			e->message_count++;
		}
		//e->flags &= ~(PORT_READ_FLAG);
		e->flags |= (PORT_WRITE_FLAG);
		return;
	}
	//else - No such dest port exists
}

uint16_t EEL_Transport::RemainingSize(PortBufferElement* e){
	if(e == NULL){return 0;}

	int16_t remaining_size = e->buffer_size;
	uint8_t* buf_ptr = e->buffer;
	while(*(buf_ptr) != 0){
		remaining_size -= *(buf_ptr) + BUF_EXTRA_DATA_LEN;
		buf_ptr += *(buf_ptr) + BUF_EXTRA_DATA_LEN;
		if(remaining_size <= 0){
			e->flags |= (PORT_NO_EMPTY_SPACE);
			break;
		}
		if(remaining_size <= BUF_EXTRA_DATA_LEN){
			while(remaining_size--){*(buf_ptr++) = 0;}
			e->flags |= (PORT_NO_EMPTY_SPACE);
			break;
		}
	}
	return remaining_size;
}

uint16_t EEL_Transport::RemainingSize(uint8_t port_no){
	PortBufferElement* e = GetPort(port_no);
	return RemainingSize(e);
}

uint8_t* EEL_Transport::ReadPort(uint8_t port_no, uint8_t* length, uint8_t* source){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		uint8_t* buf_ptr = e->buffer;
		if(*(buf_ptr) != 0){
			//There is a message in here
			uint8_t _length = *(buf_ptr++);
			if(length != NULL){*length = _length;}
			uint8_t _source = *(buf_ptr++);
			if(source != NULL){*source = _source;}
			uint8_t* _msg;
			if((_msg = (uint8_t*)malloc(_length)) == NULL){return NULL;}
			memcpy(_msg, buf_ptr, _length);

			buf_ptr = e->buffer;
			*(buf_ptr) = 0;
			//Move the trailing messages into head of the buffer
			uint8_t* buf_move = e->buffer + _length + BUF_EXTRA_DATA_LEN;
			while(buf_move <= e->buffer + e->buffer_size){
				if((_length = *(buf_move)) != 0){
					memmove(buf_ptr, buf_move, _length + BUF_EXTRA_DATA_LEN);
					buf_ptr += *(buf_ptr) + BUF_EXTRA_DATA_LEN;
					*(buf_ptr) = 0;
					buf_move += _length + BUF_EXTRA_DATA_LEN;
				}
				else{
					break;
				}
			}
			e->message_count--;
			//e->flags |= (PORT_READ_FLAG);
			//e->flags &= ~(PORT_WRITE_FLAG);
			return _msg;
		}
		else{
			if(length != NULL){*length = 0;}
			if(source != NULL){*source = 0;}
			e->flags |= (PORT_READ_FLAG);
			return NULL;
		}
	}
	else{
		if(length != NULL){*length = 0;}
		if(source != NULL){*source = 0;}
		return NULL;
	}
}

uint8_t* EEL_Transport::ReadPort(uint8_t port_no, uint8_t* length, uint8_t* source, uint8_t* buffer_out, uint8_t buffer_out_length){
	if(buffer_out == NULL || buffer_out_length == 0){return NULL;}

	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		uint8_t* buf_ptr = e->buffer;
		if(*(buf_ptr) != 0){
			//There is a message in here
			uint8_t _length = *(buf_ptr++);
			if(length != NULL){*length = _length;}
			uint8_t _source = *(buf_ptr++);
			if(source != NULL){*source = _source;}
			
			if(buffer_out_length > _length){memcpy(buffer_out, buf_ptr, _length);}
			else{memcpy(buffer_out, buf_ptr, buffer_out_length);}

			buf_ptr = e->buffer;
			*(buf_ptr) = 0;
			//Move the trailing messages into head of the buffer
			uint8_t* buf_move = e->buffer + _length + BUF_EXTRA_DATA_LEN;
			while(buf_move < e->buffer + e->buffer_size){
				if((_length = *(buf_move)) != 0){
					memmove(buf_ptr, buf_move, _length + BUF_EXTRA_DATA_LEN);
					buf_ptr += _length + BUF_EXTRA_DATA_LEN;			//*(buf_ptr) + BUF_EXTRA_DATA_LEN;
					*(buf_ptr) = 0;
					buf_move += _length + BUF_EXTRA_DATA_LEN;
				}
				else{
					//No more message to move
					break;
				}
			}
			e->message_count--;
			//e->flags |= (PORT_READ_FLAG);
			//e->flags &= ~(PORT_WRITE_FLAG);
			return buffer_out;
		}
		else{
			if(length != NULL){*length = 0;}
			if(source != NULL){*source = 0;}
			e->flags |= (PORT_READ_FLAG);
			//e->message_count = 0;
			return NULL;
		}
	}
	else{
		if(length != NULL){*length = 0;}
		if(source != NULL){*source = 0;}
		return NULL;
	}
}

int16_t EEL_Transport::SetPort(uint8_t port_no, uint8_t buffer_size){
	if(GetPort(port_no) != NULL){return -2;}

	PortBufferElement* e = (PortBufferElement*)malloc(sizeof(PortBufferElement));
	if(e == NULL){return -1;}
	e->buffer_size = buffer_size;
	e->buffer = (uint8_t*)malloc(buffer_size);
	if(e->buffer == NULL){free(e);return -1;}
	*(e->buffer) = 0;
	*(e->buffer + (--buffer_size)) = 0;
	e->port_no = port_no;
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

bool EEL_Transport::IsPortWritten(uint8_t port_no){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){
		//return (e->flags & PORT_WRITE_FLAG) > 0;
		if(*(e->buffer) != 0){return true;}
	}
	return false;
}

uint8_t EEL_Transport::MessageCount(uint8_t port_no){
	PortBufferElement* e = GetPort(port_no);
	if(e != NULL){return e->message_count;}
	return 0;
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
	free(datagram_out_);
	datagram_out_ = NULL;
	datagram_in_ = NULL;
}

void EEL_Transport::free_datagram(){
	if(this->datagram_in_ != NULL)
		free(this->datagram_in_);
}