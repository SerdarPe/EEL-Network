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

#ifndef EEL_DRIVER_H
#define EEL_DRIVER_H

#include "Arduino.h"
#include "avr/wdt.h"

#include <string.h>
#include "EEL_Debug.h"
#include "EEL_Transport.h"
#include "EEL_Network.h"
#include "EEL_Link.h"
#include "EEL_Device.h"
#include "Error_Utils.h"

#ifndef INBUF_SZ
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		#define INBUF_SZ 16
	#elif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__)
		#define INBUF_SZ 8
	#else
		#define INBUF_SZ 4
	#endif
#endif

#ifndef OUTBUF_SZ
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		#define OUTBUF_SZ 8
	#elif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__)
		#define OUTBUF_SZ 4
	#else
		#define OUTBUF_SZ 4
	#endif
#endif

class EEL_Driver{
	public:
		EEL_Driver(uint16_t my_address, uint8_t my_channel, EEL_Device* device);
		
		void run();

		/*API-Like functions to enable communication with network*/
		uint8_t SendDatagramBuffer(uint16_t destination, ToS priority, struct Datagram* datagram);
		uint8_t SendDatagram(uint16_t destination, ToS priority, struct Datagram* datagram);

		int8_t set_port(uint8_t port, uint8_t buffer_size);
		uint8_t* read_port(uint8_t port_no, uint8_t* length, uint8_t* source);
		int8_t is_port_written(uint8_t port_no);

		int8_t ban_address(uint16_t address);
		int8_t remove_ban(uint16_t address);
		
		ROUTE_STATUS route_status(uint16_t address);
		void request_route(uint16_t destination, REQUEST_REASON reason);
		/*-------------------------------------------------------*/
	private:
		uint8_t _sfd;
		uint32_t _timeout;
		bool _receive_error;
		uint16_t receive_error_count_;
		Frame* _header;
		void _start_frame();
		void receive_fail_recovery();

		EEL_Transport* transport_;
		EEL_Network* network_;
		EEL_Link* link_;
		EEL_Device* device_;

		uint8_t* pktbuf[INBUF_SZ];
		void discard();
		void discard(void* p);
		uint8_t* remove();
		uint8_t buffer(uint8_t* p);
		uint8_t* remove_priority();

		uint8_t* pktbuf_o[OUTBUF_SZ];
		void discard_o();
		void discard_o(void* p);
		uint8_t* remove_o();
		uint8_t buffer_o(uint8_t* p);
		uint8_t* remove_priority_o();

		void* serialize(uint8_t length, void* packet);
		void* deserialize(void* packet);

		uint8_t SendPacket();
		uint8_t SendPacket(uint8_t channel);
		uint8_t SendPacketDirect(uint16_t destination_address, uint8_t channel);

		void clean_up();

		void(* resetFunc) (void) = 0;	//Credits to: https://www.theengineeringprojects.com/2015/11/reset-arduino-programmatically.html
};

#endif