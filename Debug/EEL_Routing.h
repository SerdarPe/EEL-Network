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

#ifndef EEL_ROUTING
#define EEL_ROUTING

#include "Arduino.h"

#include <stdlib.h>
#include "EEL_Debug.h"
#include "Error_Utils.h"

#define MAX_ROUTINGTABLE_SIZE	16
#define MAX_NEIGHBORTABLE_SIZE	16
#define MAX_LASTRREQTABLE_SIZE	16
#define MAX_REQUESTEDROUTE_SIZE	4

enum ROUTE_STATUS{
	ROUTE_ACTIVE,
	ROUTE_DOWN,
	ROUTE_INVALID,
	ROUTE_INACTIVE,
	ROUTE_NO_ENTRY,
	ROUTE_BROADCAST
};

enum AODV_RESPONSE{
	HANDLE_RREQ,
	HANDLE_RREP,
	HANDLE_RERR,
	FORWARD_RREQ,
	FORWARD_RREP,
	FORWARD_RERR
	/*Insert other cases here*/

};

enum AODV_TYPE{
	TYPE_RREQ = 0,
	TYPE_RREP = 1,
	TYPE_RERR = 2,
	TYPE_ACK = 3	
};

enum REQUEST_REASON{
	REASON_INVALID = 0,
	REASON_NULL = 1
};

enum REPLY_REASON{
	REASON_DEST = 0,
	REASON_INTERMEDIATE = 1,
	REASON_GRATUITOUS = 2,
	REASON_HELLO = 3
};

struct RREQ_Packet{
	uint8_t Type;
	uint8_t Flags;
	uint8_t HopCount;
	uint16_t RREQ_ID;
	uint16_t DestAddr;
	uint16_t DestSeq;
	uint16_t OrgAddr;
	uint16_t OrgSeq;
	uint8_t CRC;
};

struct RREQ_Unique{
	uint16_t RREQ_ID;
	uint16_t OrgAddr;
	uint32_t Timestamp;
};

struct RequestedRouteElement{
	uint16_t DestAddr;
	uint32_t Deadline;
};

struct RREP_Packet{
	uint8_t Type;
	uint8_t Flags;
	uint8_t HopCount;
	uint16_t DestAddr;
	uint16_t DestSeq;
	uint16_t OrgAddr;
	uint8_t CRC;
};

struct RERR_Extra{
	uint16_t additional_address;
	struct RERR_Extra* ExtraArgs;	//NULL if no more, not null otherwise
};

struct RERR_Packet{
	uint8_t Type;
	uint8_t ArgsCount;				//Amount of extras
	uint16_t UnreachAddr;
	struct RERR_Extra* ExtraArgs;	//NULL if no more, not null otherwise
};

struct RoutingTableElement{
	uint16_t DestAddr;
	uint16_t NextHopAddr;
	uint8_t NextHopChan;
	uint16_t SeqNo;
	uint8_t HopCount;
	uint8_t Flags;
	uint32_t RouteLifetime;
};

class EEL_Routing {
	public:
		EEL_Routing();

		AODV_RESPONSE Handle(void* msg);

		ROUTE_STATUS CheckRouteAvailability(uint16_t DestAddr);
		
		void* RequestRoute(uint16_t DestAddr, uint16_t OrgAddr, REQUEST_REASON reason);
		void* RouteError(uint16_t UnreachAddr);

		void* HandleRREQ(RREQ_Packet* rreq, uint16_t prev_hop, uint8_t prev_ch, uint16_t my_address, uint8_t my_channel, uint16_t* next_hop, uint8_t* next_ch, uint8_t ttl_in, uint8_t* ttl_out);
		void* HandleRREP(RREP_Packet* rrep, uint16_t prev_hop, uint8_t prev_ch, uint16_t my_address, uint16_t* next_hop, uint8_t* next_ch);
		void* HandleRERR(RERR_Packet* rerr, uint16_t prev_hop, uint8_t prev_ch, uint16_t* next_hop, uint8_t* next_ch, uint8_t ttl_in, uint8_t* ttl_out);
	
		void ForwardRREP(RREP_Packet* rrep, uint16_t dest, uint8_t ch);
		void ForwardRREQ(RREQ_Packet* rreq, uint16_t dest, uint8_t ch);
		void ForwardRERR(RERR_Packet* rerr, uint16_t dest, uint8_t ch);

		uint8_t GetRoute(uint16_t DestAddr, uint16_t* NextHop, uint8_t* NextCh);
		uint8_t UpdateRouteLifetime(uint16_t DestAddr);

		RoutingTableElement* GetRoutingTableElement(uint16_t DestAddr);	
		bool AddToRoutingTable(RoutingTableElement* e);
		uint8_t RemoveFromRoutingTable(RoutingTableElement* e);
		uint8_t RemoveFromRoutingTable(uint16_t address);

		bool AddToRequestedRouteTable(uint16_t DestAddr);
		int8_t RemoveRequestedRouteTableElement(uint16_t DestAddr);
		RequestedRouteElement* GetRequestedRouteTableElement(uint16_t DestAddr);

		// Neighbor Related Functions
		bool AddToNeighbors(uint16_t src, uint8_t ch);
		void RefreshNeighborTimestamp(uint16_t Addr, uint8_t Chan);
		void ResetGetNextNeighbor();
		RoutingTableElement* GetNextNeighbor();
		RoutingTableElement* GetNeighbor(uint16_t Addr);
		RoutingTableElement** GetNeighborList();
		uint8_t inline GetNeighborCount();

		// Last RREQ Table Related Functions
		bool EEL_Routing::AddToLastRREQsTable(RREQ_Unique* r);
		RREQ_Unique* EEL_Routing::GetLastRREQ(uint16_t RREQ_ID, uint16_t OrgAddr);

		//Lifetime Related Functions
		void CheckLifetimeOfRoutes();
		void* CheckNeighborLifetime(RoutingTableElement* ne);

		//Hello Related Functions
		void* GenerateHelloMessage(uint16_t my_address, uint8_t my_channel, uint8_t* ttl_out);
		
		bool CheckDuplicate(RREQ_Packet* rreq);
		uint8_t InterpretFlags(uint8_t flags, AODV_TYPE type);

		static bool IsRouteInvaild(RoutingTableElement* e);
		static bool IsRouteInvaild(int8_t flags);

		void CleanUp();
	private:
		void* _msg_in;
		void* _msg_out;

		uint8_t route_count_ = 0;
		uint8_t neighbor_count_ = 0;
		uint8_t current_neighbor_ = 0;

		int16_t RREQ_ID_ = 0;
		int16_t SequenceNumber_ = 0;
		RoutingTableElement* RoutingTable_[MAX_ROUTINGTABLE_SIZE];
		RREQ_Unique* LastRREQsTable_[MAX_LASTRREQTABLE_SIZE];
		RequestedRouteElement* RequestedRouteTable_[MAX_REQUESTEDROUTE_SIZE];

		bool _enable_hello_routes = false;

		uint32_t _last_hello_stamp = 0;
		uint32_t _last_lifetime_stamp = 0;
		uint32_t _last_neighbor_stamp = 0;
		uint32_t _last_rreq_timestamp = 0;

		RREQ_Packet* GenerateRREQ(uint16_t DestAddr, uint16_t OrgAddr, REQUEST_REASON reason);
		RREP_Packet* GenerateRREP(uint16_t DestAddr, uint16_t OrgAddr, uint16_t seq, uint8_t hop, REPLY_REASON reason); //Destination is destination of the RREQ, Originator is the originator of the RREQ
		RERR_Packet* GenerateRERR(uint16_t UnreachAddr);
};

#endif
