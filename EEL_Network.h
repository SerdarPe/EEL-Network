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

#ifndef EEL_NETWORK
#define EEL_NETWORK

#include "Arduino.h"

#include <stdlib.h>

#include "EEL_Debug.h"
#include "EEL_Routing.h"
#include "Error_Utils.h"

#define NET_ADDL(x) (uint8_t)(((struct Packet*)(x))->destination_address & 0xFF)
#define NET_ADDH(x) (uint8_t)(((struct Packet*)(x))->destination_address >> 8)
#define NET_DEST(x) (uint16_t)(((struct Packet*)(x))->destination_address)
#define NET_SOURCE(x) (uint16_t)(((struct Packet*)(x))->source_address)
#define NET_CH(x) (uint8_t)(((struct Packet*)(x))->source_channel & 0x0F)
#define NET_LENGTH(x) (uint8_t)(((struct Packet*)(x))->payload_length)
#define NET_TOS(x) (int8_t)(((struct Packet*)(x))->type_of_service)
#define NET_PROT(x) (NET_PROTOCOL_TYPE)(((struct Packet*)(x))->protocol)
#define NET_TTL(x) (uint8_t)(((struct Packet*)(x))->ttl)
#define NET_DATA(x) (((struct Packet*)(x))->data)

enum NETWORK_RESPONSE{
	PACKET_ERROR,
	TO_AODV,
	TO_UDP,
	FORWARD_PACKET,
	PACKET_CRC_ERROR,
	PACKET_NULL
};

enum ToS{
	NO_TOS = -1,
	LOW_PR = 0,
	MEDIUM_PR = 1,
	NORMAL = 1,
	HIGH_PR = 2,
	REAL_TIME = 100,
	MAX_TOS = 127
};

struct Packet{
	uint8_t payload_length;
	int8_t	type_of_service;		//Packet Routing Priority
	uint8_t ttl;					//Time to Live (Max Hop Count)
	uint8_t source_channel;			//Source Channel (4-MSB: Reserved, 4-LSB: Channel)
	uint16_t source_address;		//Source Address (8-MSB: ADDH, 8-LSB: ADDL)
	uint16_t destination_address;	//Destination Address
	uint8_t protocol;				//Protocol type
	uint8_t crc;
	//uint8_t packet_id;
	//uint16_t frag_offset;
	void* data;
};

enum NET_PROTOCOL_TYPE{
	AODV_CONTROL	= 0,
	UDP		= 1
};

class EEL_Network{
	public:
		EEL_Network(uint16_t my_address, uint8_t my_channel);

		void* ToTransportLayer();
		void FromTransportLayer(void* p);
		void FromTransportLayer(uint16_t destination, ToS type_of_service, NET_PROTOCOL_TYPE protocol_type, uint8_t payload_length, void* data);
		void* ToLinkLayer(uint16_t* next_hop, uint8_t* next_channel);
		void* ToLinkLayer();
		void FromRouting(uint16_t destination, void* aodv_msg);
		NETWORK_RESPONSE FromLinkLayer(void* p);
		NETWORK_RESPONSE FromLinkLayer(void* p, uint16_t* next_hop, uint8_t* next_channel);

		static ToS DecodePriority(struct Packet* p);
		ToS DecodePriority();

		void CleanUp();

		void** FragmentPacket(void* data, uint16_t size, uint8_t maxFrag);
		void* DefragmentPacket(void* data, uint8_t fragOffset);

		uint16_t GetNextHop();
		uint8_t GetNextChannel();

		EEL_Routing* GetRouter();
		struct Packet* GetPacketIn();
		struct Packet* GetPacketOut();
		void* RemovePacketOut();
		uint16_t GetMyAddress();
		uint8_t GetMyChannel();
		void SetPacketOut(struct Packet*);

		struct Packet* CreatePacket(uint8_t payload_length, uint8_t type_of_service, uint16_t destination_address, NET_PROTOCOL_TYPE protocol_type, void* data);
		
		AODV_RESPONSE HandleRouting();
		
		ROUTE_STATUS RouteStatus(uint16_t address);
		ROUTE_STATUS GetNextHop(uint16_t destination, uint16_t* next_hop, uint8_t* next_channel);
		ROUTE_STATUS GetNextHop(uint16_t destination);
		void RequestRoute(uint16_t destination, REQUEST_REASON reason);
		
	private:
		EEL_Routing* router_;
		
		uint16_t my_address_;
		uint8_t my_channel_;

		struct Packet* packet_in_;	//pointer to the packet that will be processed
		struct Packet* packet_out_;
		uint16_t next_hop_;
		uint8_t next_channel_;

};

#endif
