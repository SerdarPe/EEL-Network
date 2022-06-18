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

#ifndef EEL_LINK_H
#define EEL_LINK_H

#include "Arduino.h"

#include <stdlib.h>
#include "EEL_Debug.h"
#include "Error_Utils.h"

#define ADDRESS_TABLE_SIZE 16
#define BLACK_LIST_SIZE 8

#define FRAME_DATA(x) (((struct Frame*)(x))->data)
#define FRAME_PROTOCOL(x) (LINK_PROTOCOL_TYPE)(((struct Frame*)(x))->protocol)
#define FRAME_LENGTH(x) (uint8_t)(((struct Frame*)(x))->payload_length)
#define FRAME_SENDER(x) (uint16_t)(((struct Frame*)(x))->sender_address)
#define FRAME_SENDER_CHANNEL(x) (uint16_t)(((struct Frame*)(x))->sender_channel)
#define FRAME_DEST(x) (uint16_t)(((struct Frame*)(x))->receiver_address)
#define FRAME_CRC(x) (uint8_t)(((struct Frame*)(x))->crc)
#define FRAME_DEST_CHANNEL(x) (uint16_t)(((struct Frame*)(x))->receiver_channel)

#define FRAME_CRC_SIZE 8

enum LINK_RESPONSE{
	FRAME_ERROR,
	MAC_BLOCKED,
	TO_NETWORK,
	FRAME_CRC_ERROR,
	FRAME_NULL
};

enum LINK_PROTOCOL_TYPE{
	NULL_PROTOCOL	= 0,
	NETWORK			= 1
};

struct Frame{
	uint32_t sfd;
	uint8_t payload_length;
	uint16_t sender_address;
	uint8_t sender_channel;
	uint16_t receiver_address;
	uint8_t receiver_channel;
	uint8_t protocol;
	uint8_t crc;
	void* data;
};

class EEL_Link{
	public:
		EEL_Link();
		EEL_Link(uint16_t my_address, uint8_t my_channel);

		void* ToNetworkLayer();
		void FromNetworkLayer(void* p);
		void FromNetworkLayer(uint16_t net_address, LINK_PROTOCOL_TYPE protocol_type, uint8_t payload_length, void* data);
		void* ToPHY(uint16_t* next_hop, uint8_t* next_channel);
		void* ToPHY();
		LINK_RESPONSE FromPHY(void* p);

		void free_frame();
		
		uint8_t AddBlackList(uint16_t address);
		uint8_t RemoveBlackList(uint16_t address);

		void CleanUp();

		struct Frame* GetFrame();
		uint16_t GetNextHop();
		uint8_t GetNextChannel();

		static uint32_t GetSFD(){return (uint32_t)0xAAAAAAAB;}
		
	private:
		uint16_t my_address_;
		uint8_t my_channel_;

		struct Frame* frame_in_;
		struct Frame* frame_out_;
		
		uint16_t next_hop_;
		uint8_t next_channel_;

		struct Frame* CreateFrame(uint16_t receiver_address, uint8_t receiver_channel, LINK_PROTOCOL_TYPE protocol, uint8_t payload_length, void* data);
		//Address address_table_[ADDRESS_TABLE_SIZE];

		void TranslateAddress(uint16_t net_address, uint16_t* next_hop, uint8_t* next_channel);

		uint16_t black_list_[BLACK_LIST_SIZE];
		uint8_t CheckBlackList(uint16_t address);
		
};


#endif