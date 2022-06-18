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

#include "EEL_Routing.h"

//These should've been defined in somewhere else...
#define BROADCAST_ADDRESS (uint16_t)0xFFFF

#ifndef max
     #define max(a,b) (a > b ? a : b)
#endif

#define ACTIVE_ROUTE_TIMEOUT (uint32_t)20000
#define DELETE_PERIOD (uint32_t)20000

#define ALLOWED_HELLO_LOSS 2
#define HELLO_INTERVAL (uint32_t)15000
#define NEIGHBOR_LIFETIME (uint32_t)30000 //ALLOWED_HELLO_LOSS * HELLO_INTERVAL

#define T0_TIME (uint32_t)50000
#define T1_TIME (uint32_t)50000

#define RREQ_RATE (uint32_t)5000
#define LIFETIME_CHECK_INTERVAL (uint32_t)5000
#define NEIGHBOR_CHECK_INTERVAL (uint32_t)10000
#define PATH_DISCOVERY_TIME (uint32_t)20000

#define RREQ_FLAG_SEQUENCE_UNKNOWN 0x01	//Sequence no. is unknown
#define RREQ_FLAG_DEST 0x02				//Dest-only
#define RREQ_FLAG_GRATUITOUS 0x04		//Unused - Not Implemented

#define RREP_FLAG_ACKREQ 0x01			//Unused - Not Implemented
#define RREP_FLAG_REPAIR 0x02			//Unused - Not Implemented
#define RREP_FLAG_HELLO 0x04			//Hello message indicator

#define ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN 0x01	//This is referred as invalid sequence number in the rfc3561
#define ROUTING_TABLE_FLAG_ROUTE_INVALID 0x02		//Invalidity is route being not used for a long time, referred as invalid route in the rfc3561, but you can call it inactivity
#define ROUTING_TABLE_FLAG_IS_NEIGHBOR 0x04			//Is my neighbor

#define RREQ_CRC_SIZE 13
#define RREP_CRC_SIZE 9

EEL_Routing::EEL_Routing(){
		_msg_in = NULL;
		_msg_out = NULL;

		int i = 0;
		for(i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
			RoutingTable_[i] = NULL;
		}

		for(i = 0; i < MAX_LASTRREQTABLE_SIZE; i++){
			LastRREQsTable_[i] = NULL;
		}

		for(i = 0; i < MAX_REQUESTEDROUTE_SIZE; i++){
			RequestedRouteTable_[i] = NULL;
		}

}

AODV_RESPONSE EEL_Routing::Handle(void* msg){
	AODV_TYPE subType = (AODV_TYPE)*((uint8_t*)(msg));
	switch(subType){
		case TYPE_RREQ:
			return HANDLE_RREQ;
		break;

		case TYPE_RREP:
			return HANDLE_RREP;
		break;

		case TYPE_RERR:
			return HANDLE_RERR;
		break;

		default:
		break;
	}
}

// below method will check if node can directly forward data in queue or need to start route discovery procedure.
// this function will be called before every data sending procedure.

ROUTE_STATUS EEL_Routing::CheckRouteAvailability(uint16_t DestAddr) {
	RoutingTableElement* e = this->GetRoutingTableElement(DestAddr);
	if (e != NULL) {	// node knows the route to destination
		if ((e->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID))) {	// node knows the destination but route is invalid
			return ROUTE_INVALID;
		}
		return ROUTE_ACTIVE;
	}
	return ROUTE_NO_ENTRY;		// node doesn't know the route to destination
}

void* EEL_Routing::RequestRoute(uint16_t DestAddr, uint16_t OrgAddr, REQUEST_REASON reason){
	if(millis() < _last_rreq_timestamp + RREQ_RATE){PRINTLN("RREQ_RATE_LIMIT")return NULL;}
	_msg_out = GenerateRREQ(DestAddr, OrgAddr, reason);
	_last_rreq_timestamp = millis();
	return _msg_out;
}

void* EEL_Routing::RouteError(uint16_t UnreachAddr){
	_msg_out = GenerateRERR(UnreachAddr);
	return _msg_out;
}

// method that generates a RREQ packet for Route request
RREQ_Packet* EEL_Routing::GenerateRREQ(uint16_t DestAddr, uint16_t OrgAddr, REQUEST_REASON reason){
	
	RREQ_Packet* rreq = (RREQ_Packet*)malloc(sizeof(RREQ_Packet));
	if(rreq == NULL){return NULL;}

	rreq->Type = AODV_TYPE::TYPE_RREQ;
	rreq->DestAddr = DestAddr;
	rreq->RREQ_ID = RREQ_ID_++;			//Increment after assign lvalue
	rreq->Flags = 0;

	if(reason == REASON_INVALID){
		RoutingTableElement* e = GetRoutingTableElement(DestAddr);
		if(e->Flags & (ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN)){
			rreq->Flags |= RREQ_FLAG_SEQUENCE_UNKNOWN;
		}
		rreq->DestSeq = e->SeqNo;
	}
	else if(reason == REASON_NULL){
		rreq->DestSeq = 0;
		rreq->Flags |= (RREQ_FLAG_SEQUENCE_UNKNOWN);	//Unknown DestSeq
	}
	else{
		free(rreq);
		return NULL;
	}
	rreq->HopCount = 0;
	rreq->OrgAddr = OrgAddr;
	rreq->OrgSeq = ++SequenceNumber_;
	rreq->CRC = CalculateCRC8(rreq, RREQ_CRC_SIZE);
	return rreq;
}

// method that generates a RREP packet in reply to Route requests
RREP_Packet* EEL_Routing::GenerateRREP(uint16_t DestAddr, uint16_t OrgAddr, uint16_t seq, uint8_t hop, REPLY_REASON reason){	
	RREP_Packet* rrep = (RREP_Packet*)malloc(sizeof(RREP_Packet));
	if(rrep == NULL){return NULL;}

	rrep->Type = AODV_TYPE::TYPE_RREP;
	rrep->DestAddr = DestAddr;
	rrep->Flags = 0;
	if (reason != REASON_HELLO) {
		rrep->OrgAddr = OrgAddr;
	}

	if(reason == REASON_DEST){
		rrep->DestSeq = seq;
		rrep->HopCount = 0;
	}
	else if(reason == REASON_INTERMEDIATE){
		rrep->DestSeq = seq;
		rrep->HopCount = hop;
	}
	else if(reason == REASON_HELLO) {
		rrep->DestSeq = seq;
		rrep->Flags |= (RREP_FLAG_HELLO);
		rrep->HopCount = hop;
	}
	
	rrep->CRC = CalculateCRC8(rrep, RREP_CRC_SIZE);
	return rrep;
}

// method that generates a RERR packet in case of error detection
RERR_Packet* EEL_Routing::GenerateRERR(uint16_t UnreachAddr) {
	RERR_Packet* rerr = (RERR_Packet*)malloc(sizeof(RERR_Packet));
	if(rerr == NULL){return NULL;}
	
	rerr->Type = AODV_TYPE::TYPE_RERR;
	rerr->ArgsCount = 0;
	rerr->UnreachAddr = UnreachAddr;
	rerr->ExtraArgs = NULL;
	
	return rerr;
}

// method that handles RREQ receive operations
void* EEL_Routing::HandleRREQ(RREQ_Packet* rreq, uint16_t prev_hop, uint8_t prev_ch, uint16_t my_address, uint8_t my_channel, uint16_t* next_hop, uint8_t* next_ch, uint8_t ttl_in, uint8_t* ttl_out) {	
	// check RREQ_ID and discard packet if it already came to us.
	if(CheckCRC8(rreq->CRC, rreq, RREQ_CRC_SIZE) != 0x00){PRINTLN("RREQ_CRC_ERROR")return NULL;}

	RoutingTableElement* e;
	if((e = GetRoutingTableElement(prev_hop)) == NULL){			//Create route for previous_hop (neighbor)
		e = (RoutingTableElement*)malloc(sizeof(RoutingTableElement));
		if(e == NULL){return NULL;}
		e->DestAddr = prev_hop;
		e->NextHopAddr = prev_hop;
		e->NextHopChan = prev_ch;
		e->Flags = 0;
		e->Flags = ROUTING_TABLE_FLAG_IS_NEIGHBOR | ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN;
		e->SeqNo = 0;
		e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
		AddToRoutingTable(e);
		e = NULL;
	}
	else{
		//Updates the route
		e->NextHopChan = prev_ch;
		e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID);
		e->Flags |= ROUTING_TABLE_FLAG_IS_NEIGHBOR;
		e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
		e = NULL;
	}

	if(rreq->OrgAddr == my_address){return NULL;}

	if(CheckDuplicate(rreq)){return NULL;}
	else{
		RREQ_Unique* unique = (RREQ_Unique*)malloc(sizeof(RREQ_Unique));
		if(unique == NULL){return NULL;}
		unique->RREQ_ID = rreq->RREQ_ID;
		unique->OrgAddr = rreq->OrgAddr;
		unique->Timestamp = millis();
		AddToLastRREQsTable(unique);
	}

	rreq->HopCount++;
	if((e = GetRoutingTableElement(rreq->OrgAddr)) == NULL){			//Create reverse route
		e = (RoutingTableElement*)malloc(sizeof(RoutingTableElement));
		if(e == NULL){return NULL;}
		e->Flags = 0;
		e->SeqNo = rreq->OrgSeq;
		e->DestAddr = rreq->OrgAddr;
		e->NextHopAddr = prev_hop;
		e->NextHopChan = prev_ch;
		e->HopCount = rreq->HopCount;
		e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;	
		AddToRoutingTable(e);
		e = NULL;
	}
	else{																//Or update it
		if((e->Flags & ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN) || (rreq->OrgSeq > e->SeqNo)){
			e->SeqNo = rreq->OrgSeq;
			e->Flags &= ~(ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN | ROUTING_TABLE_FLAG_ROUTE_INVALID);
			e->DestAddr = rreq->OrgAddr;
			e->NextHopAddr = prev_hop;
			e->NextHopChan = prev_ch;
			e->HopCount = rreq->HopCount;
			if(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR){e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;}
			else{e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;}
			e = NULL;
		}
	}

	//RREP-Generation or not
	RREP_Packet* rrep = NULL;
	if(rreq->DestAddr == my_address){
		rrep = (RREP_Packet*)malloc(sizeof(RREP_Packet));
		if(rrep == NULL){return NULL;}
		rrep->Type = AODV_TYPE::TYPE_RREP;
		if(rreq->DestSeq > SequenceNumber_){
			SequenceNumber_ = rreq->DestSeq;		
		}
		rrep->DestAddr = rreq->DestAddr;
		rrep->DestSeq = SequenceNumber_;
		rrep->OrgAddr = rreq->OrgAddr;
		rrep->HopCount = 0;

		*next_hop = prev_hop;
		*next_ch = prev_ch;
		PRINT("GENERATE_RREP_FOR: ")PRINTHEX(rreq->OrgAddr)PRINT(", NEXT_HOP: ")PRINTLNHEX(prev_hop)
		rrep->CRC = CalculateCRC8(rrep, RREP_CRC_SIZE);
		_msg_out = rrep;
		return _msg_out;
	}
	else if((e = GetRoutingTableElement(rreq->DestAddr)) != NULL && !(rreq->Flags & RREQ_FLAG_DEST)){	//Intermediate node route reply
		if(!(e->Flags & ROUTING_TABLE_FLAG_ROUTE_INVALID) && !(e->Flags & ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN)){
			if((e->SeqNo >= rreq->DestSeq) || (rreq->Flags & RREQ_FLAG_SEQUENCE_UNKNOWN)){
				rrep = (RREP_Packet*)malloc(sizeof(RREP_Packet));
				if(rrep == NULL){return NULL;}
				rrep->Type = AODV_TYPE::TYPE_RREP;
				rrep->DestAddr = e->DestAddr;
				rrep->DestSeq = e->SeqNo;
				rrep->OrgAddr = rreq->OrgAddr;
				rrep->HopCount = e->HopCount;
				rrep->Flags = 0;

				*next_hop = prev_hop;
				*next_ch = prev_ch;
				rrep->CRC = CalculateCRC8(rrep, RREP_CRC_SIZE);
				_msg_out = rrep;
				return _msg_out;
			}
		}
	}

	//Forward RREQ
	*ttl_out = --ttl_in;
	rreq->DestSeq = max(rreq->DestSeq, SequenceNumber_);

	*next_hop = BROADCAST_ADDRESS;
	*next_ch = 0;

	RREQ_Packet* rreq_forward = (RREQ_Packet*)malloc(sizeof(RREQ_Packet));
	if(rreq_forward == NULL){return NULL;}
	memcpy(rreq_forward, rreq, sizeof(RREQ_Packet));
	rreq_forward->CRC = CalculateCRC8(rreq_forward, RREQ_CRC_SIZE);
	_msg_out = rreq_forward;
	return _msg_out;
}

// method that handles RREP receive operations
void* EEL_Routing::HandleRREP(RREP_Packet* rrep, uint16_t prev_hop, uint8_t prev_ch, uint16_t my_address, uint16_t* next_hop, uint8_t* next_ch) {
	if(CheckCRC8(rrep->CRC, rrep, 9) != 0x00){PRINTLN("RREP_CRC_ERROR")return NULL;}

	if ((rrep->Flags & (RREP_FLAG_HELLO))) {
		PRINT("HELLO_FROM: ")PRINTLNHEX(rrep->DestAddr)
		RoutingTableElement* e;
		if((e = GetRoutingTableElement(rrep->DestAddr)) == NULL){	//Creating a reverse route after receiving hello messages might be problematic if there are so many nodes around us
			if(_enable_hello_routes){
				e = (RoutingTableElement*)malloc(sizeof(RoutingTableElement));
				if(e == NULL){return NULL;}
				e->Flags = 0;
				e->Flags |= ROUTING_TABLE_FLAG_IS_NEIGHBOR;
				e->DestAddr = prev_hop;
				e->NextHopAddr = prev_hop;
				e->NextHopChan = prev_ch;
				e->SeqNo = rrep->DestSeq;
				e->HopCount = 1;
				e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
				AddToRoutingTable(e);
			}
		}
		else{
			e->SeqNo = rrep->DestSeq;
			e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID | ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN);
			e->Flags |= ROUTING_TABLE_FLAG_IS_NEIGHBOR;
			e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
		}
		return NULL;
	}
	else {
		RoutingTableElement* e;
		if((e = GetRoutingTableElement(prev_hop)) == NULL){			//Create route for previous_hop (neighbor)
			e = (RoutingTableElement*)malloc(sizeof(RoutingTableElement));
			if(e == NULL){return NULL;}
			e->DestAddr = prev_hop;
			e->NextHopAddr = prev_hop;
			e->NextHopChan = prev_ch;
			e->Flags = 0;
			e->Flags = ROUTING_TABLE_FLAG_IS_NEIGHBOR | ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN;
			e->SeqNo = 0;
			e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
			AddToRoutingTable(e);
			e = NULL;
		}
		else{
			e->NextHopChan = prev_ch;
			e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID);
			e->Flags |= ROUTING_TABLE_FLAG_IS_NEIGHBOR;
			e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;
			e = NULL;
		}
		
		rrep->HopCount++;
		if((e = GetRoutingTableElement(rrep->DestAddr)) == NULL){		//Create forward route
			e = (RoutingTableElement*)malloc(sizeof(RoutingTableElement));
			if(e == NULL){return NULL;}
			e->DestAddr = rrep->DestAddr;
			e->NextHopAddr = prev_hop;
			e->NextHopChan = prev_ch;
			e->Flags = 0;
			e->SeqNo = 0;
			e->HopCount = rrep->HopCount;
			e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;
			AddToRoutingTable(e);
			e = NULL;
		}
		else{			//Update
			if(!(e->Flags & ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN)){		
				if( (rrep->DestSeq > e->SeqNo) || ((rrep->DestSeq == e->SeqNo) && ((e->Flags & ROUTING_TABLE_FLAG_ROUTE_INVALID) || (rrep->HopCount < e->HopCount))) ){
					e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID | ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN);
					e->NextHopAddr = prev_hop;
					e->NextHopChan = prev_ch;
					e->HopCount = rrep->HopCount;
					if(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR){e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;}
					else{e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;}
					e->SeqNo = rrep->DestSeq;
				}
			}
			else{
				e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID | ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN);
				e->NextHopAddr = prev_hop;
				e->NextHopChan = prev_ch;
				e->HopCount = rrep->HopCount;
				if(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR){e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;}
				else{e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;}
				e->SeqNo = rrep->DestSeq;
			}
		}
		e = NULL;
		
		if (rrep->OrgAddr == my_address) {
			//RREP came to originator. No need to forward rrep
			return NULL;
		}
		else{
			if((e = GetRoutingTableElement(rrep->OrgAddr)) != NULL){
				if (!(e->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID))){
					*next_hop = e->NextHopAddr;
					*next_ch = e->NextHopChan;

					RREP_Packet* rrep_forward = (RREP_Packet*)malloc(sizeof(RREP_Packet));
					if(rrep_forward == NULL){return NULL;}
					memcpy(rrep_forward, rrep, sizeof(RREP_Packet));
					
					PRINT("RREP_FORWARD")PRINTHEX(e->NextHopAddr)PRINT(" @ ")PRINTLNHEX(e->NextHopChan);
					rrep_forward->CRC = CalculateCRC8(rrep_forward, RREP_CRC_SIZE);
					_msg_out = rrep_forward;
					return _msg_out;
				}
			}
		}
	}
}

// method will handle RERR receive operations
void* EEL_Routing::HandleRERR(RERR_Packet* rerr, uint16_t prev_hop, uint8_t prev_ch, uint16_t* next_hop, uint8_t* next_ch, uint8_t ttl_in, uint8_t* ttl_out) {
	bool forward = false;
	RoutingTableElement* e = NULL;
	for (int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++) {	// Main RERR UnreachDest
		e = RoutingTable_[i];
		if(e == NULL){continue;}
		if((e->DestAddr == rerr->UnreachAddr) && (e->NextHopAddr == prev_hop)){
			RemoveFromRoutingTable(e);
			forward = true;
			break;
		}
	}

	RERR_Extra* rerr_extra = rerr->ExtraArgs;	//First extra
	while (rerr_extra != NULL) {				//Iterating through extras
		for (int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++) {
			e = RoutingTable_[i];
			if(e == NULL){continue;}
			if((e->DestAddr == rerr_extra->additional_address) && (e->NextHopAddr == prev_hop)){
				RemoveFromRoutingTable(e);
				forward = true;
				break;
			}
		}
		rerr_extra = rerr_extra->ExtraArgs;
	}

	if (forward) {
		*next_hop = BROADCAST_ADDRESS;
		*next_ch = 0; //Broadcasts does not need channels defined
		// this should maybe better be handled in network
		*ttl_out = --ttl_in;

		rerr_extra = NULL;
		uint8_t rerr_size = sizeof(RERR_Packet) + (rerr->ArgsCount * sizeof(RERR_Extra));
		RERR_Packet* rerr_forward = (RERR_Packet*)malloc(rerr_size);
		if(rerr_forward == NULL){return NULL;}
		memcpy(rerr_forward, rerr, rerr_size);
		if(rerr_forward->ExtraArgs != NULL){	//fixing pointers for serialization()
			rerr_forward->ExtraArgs = (RERR_Extra*)((char*)rerr + sizeof(RERR_Packet));
			rerr_extra = rerr_forward->ExtraArgs;
			while(rerr_extra->ExtraArgs != NULL){
				rerr_extra->ExtraArgs = (RERR_Extra*)((char*)rerr_extra + sizeof(RERR_Extra));
				rerr_extra = rerr_extra->ExtraArgs;
			}
		}

		_msg_out = rerr_forward;
		return _msg_out;
	}
	return NULL;
}

bool EEL_Routing::AddToRoutingTable(RoutingTableElement* e){
	int8_t invalidIndex = -1;
	RoutingTableElement* ce = NULL;
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		ce = RoutingTable_[i];
		if(ce != NULL){
			if(!(ce->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID))){
				//It means there is a valid entry in here
				continue;
			}
			else{
				//Not safe but this entry can be overwritten
				invalidIndex = i;
				continue;
			}
		}
		else{
			//Nothing is here, safe to write
			RoutingTable_[i] = e;
			if(e->DestAddr == e->NextHopAddr){++neighbor_count_;}
			++route_count_;

			PRINTLN("ROUTE_TABLE_ADD")
			PRINT("DestAddr: ")PRINTLNHEX(e->DestAddr)
			PRINT("NextHopAddr: ")PRINTLNHEX(e->NextHopAddr)
			PRINT("NextHopChan: ")PRINTLNHEX(e->NextHopChan)
			PRINT("Flags: ")PRINTLNHEX(e->Flags)
			
			return true;
		}
	}
	
	if(invalidIndex != -1){
		if(e->DestAddr == e->NextHopAddr){++neighbor_count_;}
		++route_count_;
		free(this->RoutingTable_[invalidIndex]);
		this->RoutingTable_[invalidIndex] = e;
		return true;	
	}
	
	return false;
}

bool EEL_Routing::AddToLastRREQsTable(RREQ_Unique* r) {
	if(r == NULL){return false;}
	for (int i = 0; i < MAX_LASTRREQTABLE_SIZE; i++) {
		if(LastRREQsTable_[i] != NULL){
			if ((millis() < LastRREQsTable_[i]->Timestamp + PATH_DISCOVERY_TIME)) {continue;}
			else {
				free(LastRREQsTable_[i]);
				LastRREQsTable_[i] = r;
				return true;
			}
		}
		else{
			LastRREQsTable_[i] = r;
			return true;
		}
	}
	return false;
}

RREQ_Unique* EEL_Routing::GetLastRREQ(uint16_t RREQ_ID, uint16_t OrgAddr) {
	for(int i = 0; i < MAX_LASTRREQTABLE_SIZE; i++) {
		if(LastRREQsTable_[i] != NULL) {
			if(	(LastRREQsTable_[i]->RREQ_ID == RREQ_ID) && (LastRREQsTable_[i]->OrgAddr == OrgAddr) ){
				return LastRREQsTable_[i];
			}
		}
	}
	return NULL;
}

uint8_t EEL_Routing::RemoveFromRoutingTable(RoutingTableElement* e){
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		if(this->RoutingTable_[i] == e){
			if(RoutingTable_[i]->DestAddr == RoutingTable_[i]->NextHopAddr){--neighbor_count_;}
			--route_count_;
			PRINT("REMOVED: ")PRINTHEX(e->DestAddr)PRINT(" <- ")PRINTLNHEX(e->NextHopAddr)
			free(this->RoutingTable_[i]);
			this->RoutingTable_[i] = NULL;
			return 0x00;
		}
	}
	return 0x01;
}

uint8_t EEL_Routing::RemoveFromRoutingTable(uint16_t address){
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		if(RoutingTable_[i]->DestAddr == address){
			if(RoutingTable_[i]->DestAddr == RoutingTable_[i]->NextHopAddr){--neighbor_count_;}
			--route_count_;
			free(RoutingTable_[i]);
			RoutingTable_[i] = NULL;
			return 0x00;
		}
	}
	return 0x01;
}

// this will be called periodically in order to determine connectivity between neighbor nodes.
void* EEL_Routing::CheckNeighborLifetime(RoutingTableElement* ne) {
	if (millis() > _last_neighbor_stamp + NEIGHBOR_CHECK_INTERVAL) {
		if(ne == NULL){PRINTLN("NE_NULL")_last_neighbor_stamp = millis();return NULL;}
		
		if ((millis() > ne->RouteLifetime)) {			//neighbor timeout
			PRINT("NEIGHBOR_TIMEOUT: ")PRINTLNHEX(ne->DestAddr)
			RERR_Packet* rerr = NULL;
			RERR_Extra* err_extra = NULL;
			RoutingTableElement* e = NULL;
			for (int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++) {
				e = RoutingTable_[i];
				if(e == NULL){continue;}
				if(e->NextHopAddr != ne->DestAddr){continue;}			//My nexthop must be the dead neighbor, otherwise the route is not broken
				if(rerr == NULL){
					rerr = GenerateRERR(e->DestAddr);
					if(rerr == NULL){_last_neighbor_stamp = millis();return NULL;}
					err_extra = rerr->ExtraArgs;
				}
				else{
					//Keep generating extras
					rerr->ArgsCount++;
					err_extra = (RERR_Extra*)malloc(sizeof(RERR_Extra));
					if(err_extra == NULL){
						RERR_Extra* child = rerr->ExtraArgs;
						RERR_Extra* parent = NULL;
						free(rerr);
						while(child != NULL){
							parent = child;
							child = parent->ExtraArgs;
							free(parent);
						}
						_last_neighbor_stamp = millis();
						return NULL;
					}
					err_extra->additional_address = e->DestAddr;
					err_extra = err_extra->ExtraArgs;
				}
				if(e->DestAddr != ne->DestAddr){RemoveFromRoutingTable(e);}
			}

			// broadcast the RERR
			if(rerr != NULL){
				RemoveFromRoutingTable(ne);
				_msg_out = rerr;
				_last_neighbor_stamp = millis();
				return _msg_out;
			}
		}
	}
	return NULL;
}

// below method will be called every HELLO_INTERVAL milliseconds
// it will generate and send hello messages to neighbors, acts as a "heartbeat"
void* EEL_Routing::GenerateHelloMessage(uint16_t my_address, uint8_t my_channel, uint8_t* ttl_out) {
	RoutingTableElement* e;
	if (millis() > _last_hello_stamp + HELLO_INTERVAL) {
		for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
			e = RoutingTable_[i];
			if (e == NULL) {continue;}
			if (!(e->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID))) {
				RREP_Packet* hello = (RREP_Packet*)malloc(sizeof(RREP_Packet));
				if(hello == NULL){return NULL;}
				hello->Type = AODV_TYPE::TYPE_RREP;
				hello->DestAddr = my_address;
				hello->DestSeq = SequenceNumber_;
				hello->HopCount = 0;
				hello->Flags = RREP_FLAG_HELLO;
				hello->OrgAddr = 0;
				hello->CRC = CalculateCRC8(hello, RREP_CRC_SIZE);
				_msg_out = hello;
				_last_hello_stamp = millis();
				return _msg_out;
			}
		}
	}
	return NULL;
}

void EEL_Routing::CheckLifetimeOfRoutes() {		//this method will be triggered once every LIFETIME_CHECK_INTERVAL seconds
	if (millis() > _last_lifetime_stamp + LIFETIME_CHECK_INTERVAL) {
		RoutingTableElement* e = NULL;
		for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
			e = this->RoutingTable_[i];
			if (e != NULL){
				if(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR){continue;}
				if (millis() > e->RouteLifetime) {
					if (!(e->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID))) {
						PRINT("INVALIDATING: ")PRINTLNHEX(e->DestAddr)
						e->Flags |= (ROUTING_TABLE_FLAG_ROUTE_INVALID);
						e->RouteLifetime = millis() + DELETE_PERIOD;
					}
					else {
						PRINT("REMOVING: ")PRINTLNHEX(e->DestAddr)
						this->RemoveFromRoutingTable(e);
					}
				}
			}
		}
		_last_lifetime_stamp = millis();
	}
}
	

RoutingTableElement* EEL_Routing::GetRoutingTableElement(uint16_t DestAddr){
	RoutingTableElement* e = NULL;
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		e = this->RoutingTable_[i];
		if (e != NULL) {
			if ((e->DestAddr == DestAddr)){
				return e;
			}
		}
	}
	return NULL;
}

RequestedRouteElement* EEL_Routing::GetRequestedRouteTableElement(uint16_t DestAddr){
	RequestedRouteElement* re = NULL;
	for(int i = 0; i < MAX_REQUESTEDROUTE_SIZE; i++){
		re = RequestedRouteTable_[i];
		if (re != NULL) {
			if ((re->DestAddr == DestAddr)){
				if(millis() < re->Deadline){return re;}
				return NULL;
			}
		}
	}
	return NULL;
}

uint8_t EEL_Routing::GetRoute(uint16_t DestAddr, uint16_t* NextHop, uint8_t* NextCh){
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		RoutingTableElement* e = RoutingTable_[i];
		if (e != NULL) {
			if ((e->DestAddr == DestAddr)){
				e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID);
				if(!(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR)){e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;}
				*NextHop =  e->NextHopAddr;
				*NextCh = e->NextHopChan;
				return 0x00;
			}
		}
	}
	return 0x01;
}

uint8_t EEL_Routing::UpdateRouteLifetime(uint16_t DestAddr){
	RoutingTableElement* e = GetRoutingTableElement(DestAddr);
	if(e != NULL){
		if(e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR){e->RouteLifetime = millis() + NEIGHBOR_LIFETIME;}
		else{e->RouteLifetime = millis() + ACTIVE_ROUTE_TIMEOUT;}
		e->Flags &= ~(ROUTING_TABLE_FLAG_ROUTE_INVALID);
		return 0x00;
	}
	return 0x01;
}

void EEL_Routing::ResetGetNextNeighbor(){
	current_neighbor_ = 0;
}

RoutingTableElement* EEL_Routing::GetNextNeighbor(){
	while(current_neighbor_ < MAX_ROUTINGTABLE_SIZE){
		RoutingTableElement* e = RoutingTable_[current_neighbor_++];
		if (e != NULL) {
			if ((e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR)){
				return e;
			}
		}
	}
	return NULL;
}

RoutingTableElement* EEL_Routing::GetNeighbor(uint16_t Addr){
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		RoutingTableElement* e = RoutingTable_[i];
		if (e != NULL) {
			if ((e->DestAddr == Addr) && (e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR)){
				return e;
			}
		}
	}
	return NULL;
}

//Not very safe to use, free() after you're done with the list, NEVER free() anything inside the list
RoutingTableElement** EEL_Routing::GetNeighborList(){
	if(neighbor_count_ == 0){return NULL;}

	RoutingTableElement** neighbor_list = (RoutingTableElement**)malloc(neighbor_count_ * sizeof(RoutingTableElement*));
	if(neighbor_list == NULL){return NULL;}

	uint8_t neighbor_ptr = 0;
	for(int i = 0; i < MAX_ROUTINGTABLE_SIZE; i++){
		RoutingTableElement* e = RoutingTable_[i];
		if (e != NULL) {
			if ((e->Flags & ROUTING_TABLE_FLAG_IS_NEIGHBOR)){
				if(neighbor_ptr == neighbor_count_){break;}
				neighbor_list[neighbor_ptr++] = e;
			}
		}
	}
	return neighbor_list;
}

uint8_t inline EEL_Routing::GetNeighborCount(){
	return neighbor_count_;
}

bool EEL_Routing::CheckDuplicate(RREQ_Packet* rreq){
	RREQ_Unique* lastRREQ = this->GetLastRREQ(rreq->RREQ_ID, rreq->OrgAddr);
	if(lastRREQ == NULL){return false;}

	if((millis() < lastRREQ->Timestamp + PATH_DISCOVERY_TIME)){PRINTLN("DUPLICATE_FOUND")return true;}
	return false;
}

uint8_t EEL_Routing::InterpretFlags(uint8_t flags, AODV_TYPE type){
	uint8_t tableFlag = 0;
	switch(type){
		case TYPE_RREQ:
			if(flags & RREQ_FLAG_SEQUENCE_UNKNOWN){
				tableFlag |= ROUTING_TABLE_FLAG_SEQUENCE_UNKNOWN;
			}
			break;
		
		case TYPE_RREP:
			
			break;
	}

	return tableFlag;
}

static bool EEL_Routing::IsRouteInvaild(RoutingTableElement* e){
	if(e->Flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID)){return true;}
	return false;
}

static bool EEL_Routing::IsRouteInvaild(int8_t flags){
	if(flags & (ROUTING_TABLE_FLAG_ROUTE_INVALID)){return true;}
	return false;
}

void EEL_Routing::CleanUp(){
	if(_msg_out != NULL){
		AODV_TYPE type = (AODV_TYPE)*(char*)(_msg_out);
		switch(type){
			case AODV_TYPE::TYPE_RREQ:{
				free(_msg_out);
				_msg_out = NULL;
			}
			break;

			case AODV_TYPE::TYPE_RREP:{
				free(_msg_out);
				_msg_out = NULL;
			}
			break;

			case AODV_TYPE::TYPE_RERR:{
				//First parent is the rerr
				RERR_Packet* rerr = (RERR_Packet*)_msg_out;
				RERR_Extra* child = rerr->ExtraArgs;
				RERR_Extra* parent = NULL;
				free(rerr);
				while(child != NULL){
					parent = child;
					child = parent->ExtraArgs;
					free(parent);
				}
			}
		}
	}
	_msg_out = NULL;
	//msg = NULL;
}