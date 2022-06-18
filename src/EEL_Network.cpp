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

#include "EEL_Network.h"

#define BROADCAST_ADDRESS (uint16_t)0xFFFF

#define NETWORK_CRC_SIZE 9

EEL_Network::EEL_Network(uint16_t my_address, uint8_t my_channel){
	this->my_address_ = my_address;
	this->my_channel_ = my_channel;
	this->packet_in_ = NULL;

	this->router_ = new EEL_Routing();
}

ToS EEL_Network::DecodePriority(Packet* p){
	return (ToS)(p->type_of_service & 0x0F);
}

ToS EEL_Network::DecodePriority(){
	return (ToS)(packet_in_->type_of_service & 0x0F);
}

void* EEL_Network::ToTransportLayer(){
	if(packet_in_ == NULL){
		return NULL;
	}

	if(packet_in_->protocol == NET_PROTOCOL_TYPE::UDP){
		if(packet_in_->destination_address == my_address_){
			return packet_in_->data;
		}
		else{
			return NULL;
		}
	}
	else{
		return NULL;
	}
}

void EEL_Network::FromTransportLayer(uint16_t destination, ToS type_of_service, NET_PROTOCOL_TYPE protocol_type, uint8_t payload_length, void* data){
	if(data == NULL){
		return;
	}
	
	packet_out_ = CreatePacket(payload_length, type_of_service, destination, protocol_type, data);
}

void* EEL_Network::ToLinkLayer(){
	if(packet_out_ == NULL){
		return NULL;
	}
	return packet_out_;
}

NETWORK_RESPONSE EEL_Network::FromLinkLayer(void* p, uint16_t* next_hop, uint8_t* next_channel){
	if(p == NULL){
		return PACKET_NULL;
	}

	packet_in_ = (struct Packet*)p;

	if(CalculateCRC8((uint8_t*)(packet_in_), NETWORK_CRC_SIZE) != packet_in_->crc){
		return NETWORK_RESPONSE::PACKET_ERROR;
	}

	NET_PROTOCOL_TYPE pType = NET_PROT(packet_in_);
	switch(pType){
		case UDP:{
			if(packet_in_->destination_address != my_address_){
				//This packet is not meant for me, have to forward it
				uint8_t copy_length = packet_in_->payload_length + sizeof(Packet);
				packet_out_ = (struct Packet*)malloc(copy_length);
				if(packet_out_ == NULL){return NULL;}
				//Must decrement ttl etc...
				memcpy(packet_out_, packet_in_, copy_length);
				return FORWARD_PACKET;
			}
			else{
				//This packet is for me, send to transport
				return TO_UDP;
			}
		}
		break;

		case AODV_CONTROL:{
			return TO_AODV;
		}
		break;
	}
}

void EEL_Network::FromRouting(uint16_t destination, void* aodv_msg){
	if(aodv_msg == NULL){
		return;
	}

	uint8_t payload_length = 0;
	AODV_TYPE subType = (AODV_TYPE)*((uint8_t*)(aodv_msg));
	switch(subType){
		case TYPE_RREQ:{
			payload_length = sizeof(RREQ_Packet);
		}
		break;

		case TYPE_RREP:{
			payload_length = sizeof(RREP_Packet);
		}
		break;

		case TYPE_RERR:{
			payload_length = sizeof(RERR_Packet) + (((RERR_Packet*)aodv_msg)->ArgsCount * sizeof(RERR_Extra));
		}
		break;

		default:{
			return;
		}
		break;
	}
	if(packet_out_ != NULL){free(packet_out_);}
	packet_out_ = CreatePacket(payload_length, ToS::NORMAL, destination, NET_PROTOCOL_TYPE::AODV_CONTROL, aodv_msg);
}

void EEL_Network::RequestRoute(uint16_t destination, REQUEST_REASON reason){
	RREQ_Packet* rreq = router_->RequestRoute(destination, my_address_, reason);
	if(rreq == NULL){
		CleanUp();
		return;
	}
	next_hop_ = BROADCAST_ADDRESS;
	next_channel_ = 0;
	packet_out_ = CreatePacket(sizeof(RREQ_Packet), ToS::NORMAL, BROADCAST_ADDRESS, NET_PROTOCOL_TYPE::AODV_CONTROL, rreq);
}
struct Packet* EEL_Network::CreatePacket(uint8_t payload_length, uint8_t type_of_service, uint16_t destination_address, NET_PROTOCOL_TYPE protocol_type, void* data){
	if(data == NULL){
		return NULL;
	}

	struct Packet* packet = (struct Packet*)malloc(sizeof(struct Packet));
	if(packet == NULL){
		return NULL;
	}
	packet->destination_address = destination_address;
	packet->source_address = my_address_;
	packet->source_channel = my_channel_;
	//packet->packet_id = 0;
	//packet->frag_offset = 0;
	packet->ttl = 32;		//Should be manually assigned via function argument
	packet->type_of_service = type_of_service;
	packet->protocol = protocol_type;
	packet->payload_length = payload_length;

	packet->crc = CalculateCRC8((uint8_t*)(packet), NETWORK_CRC_SIZE);
	packet->data = data;
	return packet;
}

AODV_RESPONSE EEL_Network::HandleRouting(){
	return router_->Handle(packet_in_->data);
}

uint16_t EEL_Network::GetNextHop(){
	return next_hop_;
}

uint8_t EEL_Network::GetNextChannel(){
	return next_channel_;
}

uint16_t EEL_Network::GetMyAddress(){
	return my_address_;
}
uint8_t EEL_Network::GetMyChannel(){
	return my_channel_;
}

EEL_Routing* EEL_Network::GetRouter(){
	return router_;
}

struct Packet* EEL_Network::GetPacketIn(){
	return packet_in_;
}

struct Packet* EEL_Network::GetPacketOut(){
	return packet_out_;
}

void* EEL_Network::RemovePacketOut(){
	void* p_out = packet_out_;
	packet_out_ = NULL;
	return p_out;
}

ROUTE_STATUS EEL_Network::GetNextHop(uint16_t destination, uint16_t* next_hop, uint8_t* next_channel){
	if(destination == BROADCAST_ADDRESS){
		*next_hop = BROADCAST_ADDRESS;
		*next_channel = 0;
		return ROUTE_BROADCAST;
	}

	ROUTE_STATUS status = router_->CheckRouteAvailability(destination);
	switch(status){
		case ROUTE_ACTIVE:{
			router_->GetRoute(destination, next_hop, next_channel);
		}
		break;
		case ROUTE_INVALID:{

		}
		break;
		case ROUTE_NO_ENTRY:{
			*next_hop = BROADCAST_ADDRESS;
			*next_channel = 0;
		}
		break;
	}

	return status;
}

ROUTE_STATUS EEL_Network::GetNextHop(uint16_t destination){
	if(destination == BROADCAST_ADDRESS){
		next_hop_ = BROADCAST_ADDRESS;
		next_channel_ = 0;
		return ROUTE_BROADCAST;
	}

	RoutingTableElement* e = router_->GetRoutingTableElement(destination);
	if(e != NULL){
		next_hop_ = e->NextHopAddr;
		next_channel_ = e->NextHopChan;
		if (!EEL_Routing::IsRouteInvaild(e)) {
			return ROUTE_ACTIVE;
		}
		else {
			return ROUTE_INVALID;
		}
	}
	else{
		next_hop_ = BROADCAST_ADDRESS;
		next_channel_ = 0;

		return ROUTE_NO_ENTRY;
	}
}

ROUTE_STATUS EEL_Network::RouteStatus(uint16_t address){
	return router_->CheckRouteAvailability(address);
}

void** EEL_Network::FragmentPacket(void* data, uint16_t size, uint8_t maxFrag){
	uint8_t frags = 0;
	while(size > 0){
		if(size <= maxFrag){
			frags++;
			break;
		}
		size -= maxFrag;
		frags++;
	}
	
	uint8_t** fragData = (uint8_t**)malloc(frags * sizeof(uint8_t*));
	for(int i = 0; i < frags ; i++){
		*(fragData + i) = ((uint8_t*)data + (maxFrag * i));
	}
	
	return (void**)fragData;
}

void* EEL_Network::DefragmentPacket(void* data, uint8_t fragOffset){
	
}

void EEL_Network::CleanUp(){
	free(packet_out_);
	packet_out_ = NULL;
	//packet_in_ = NULL;

	router_->CleanUp();
}