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

#include "EEL_Link.h"

#define BROADCAST_ADDRESS (uint16_t)0xFFFF

EEL_Link::EEL_Link(uint16_t my_address, uint8_t my_channel){
	this->my_address_ = my_address;
	this->my_channel_ = my_channel;

	/*RESET BLACKLIST*/
	for(int i = 0; i < BLACK_LIST_SIZE; i++){
		black_list_[i] = 0;
	}
	/*---------------*/
	
	/*RESET FRAME*/
	this->frame_in_ = NULL;
}

void* EEL_Link::ToNetworkLayer(){
	if(frame_in_->protocol == LINK_PROTOCOL_TYPE::NETWORK){
		return frame_in_->data;
	}
	else{
		return NULL;
	}
}

void EEL_Link::FromNetworkLayer(uint16_t net_address, LINK_PROTOCOL_TYPE protocol_type, uint8_t payload_length, void* data){
	if(data == NULL){
		return;
	}
	uint16_t next_hop = 0;
	uint8_t next_channel = 0;
	TranslateAddress(net_address, &next_hop, &next_channel);
	frame_out_ = CreateFrame(next_hop, next_channel, protocol_type, payload_length, data);
}

void* EEL_Link::ToPHY(uint16_t* next_hop, uint8_t* next_channel){
	if(next_hop != NULL){
		*next_hop = frame_out_->receiver_address;
	}
	
	if(next_channel != NULL){
		*next_channel = frame_out_->receiver_channel;
	}
	return frame_out_;
}

void* EEL_Link::ToPHY(){
	next_hop_ = frame_out_->receiver_address;
	next_channel_ = frame_out_->receiver_channel;
	return frame_out_;
}


LINK_RESPONSE EEL_Link::FromPHY(void* p){
	if(p == NULL){
		frame_in_ = NULL;
		return FRAME_NULL;
	}

	frame_in_ = (struct Frame*)p;
	
	if(CheckBlackList(FRAME_SENDER(frame_in_))){
		frame_in_ = NULL;
		return MAC_BLOCKED;
	}

	switch(FRAME_PROTOCOL(frame_in_)){
		case NETWORK:{
			return TO_NETWORK;
		}
		break;

		default:{
			return FRAME_ERROR;
		}
		break;
	}

	return FRAME_ERROR;
}


struct Frame* EEL_Link::CreateFrame(uint16_t receiver_address, uint8_t receiver_channel, LINK_PROTOCOL_TYPE protocol, uint8_t payload_length, void* data){
	if(data == NULL){
		return NULL;
	}

	struct Frame* frame = (struct Frame*)malloc(sizeof(struct Frame));
	if(frame == NULL){
		return NULL;
	}
	frame->sfd = EEL_Link::GetSFD();
	frame->sender_address = my_address_;
	frame->sender_channel = my_channel_;
	frame->receiver_address = receiver_address;
	frame->receiver_channel = receiver_channel;
	frame->protocol = protocol;
	frame->payload_length = payload_length;
	frame->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	// ... + sizeof(uint32_t)), sizeof(Frame) - sizeof(unit32_t) - sizeof(uint8_t) - sizeof(void*));
																										// ... + offsetof(Frame, payload_length), ...);
	frame->data = data;
	return frame;
}

uint16_t EEL_Link::GetNextHop(){
	return next_hop_;
}

uint8_t EEL_Link::GetNextChannel(){
	return next_channel_;
}

void EEL_Link::TranslateAddress(uint16_t net_address, uint16_t* next_hop, uint8_t* next_channel){
	//THIS HAS TO RETURN "MAC" ADDRESS AND CHANNEL ASSOCIATED WITH THE NETWORK ADDRESS
	//FOR NOW WE DON'T HAVE A SEPERATE NETWORK ADDRESS
	if(net_address == BROADCAST_ADDRESS){
		*next_hop = BROADCAST_ADDRESS;
		*next_channel = 0;
		return;
	}
	*next_hop = net_address;
	*next_channel = 0;
}

uint8_t EEL_Link::CheckBlackList(uint16_t address){
	for(int i = 0; i < BLACK_LIST_SIZE; i++){
		if(black_list_[i] == address){
			return 0x01;	//Address is blacklisted
		}
	}
	return 0x00;			//Address is not blacklisted
}

uint8_t EEL_Link::AddBlackList(uint16_t address){
	int8_t currentIndex = -1;
	if(CheckBlackList(address)){return 0x02;}	//Already added
	for(int i = 0; i < BLACK_LIST_SIZE; i++){
		if(black_list_[i] == 0){
			if(currentIndex < 0){
				currentIndex = i;	//First empty space will be taken
			}
			
		}
	}
	if(currentIndex != -1){
		black_list_[currentIndex] = address;
		return 0x00;				//Success
	}
	return 0x01;					//Blacklist buffer full
}

uint8_t EEL_Link::RemoveBlackList(uint16_t address){
	for(int i = 0; i < BLACK_LIST_SIZE; i++){
		if(black_list_[i] == address){
			black_list_[i] = 0;
			return 0x00;	//Success
		}
	}
	return 0x01;			//No such address in the list
}

struct Frame* EEL_Link::GetFrame(){
	return frame_in_;
}

void EEL_Link::CleanUp(){
	free(frame_out_);
	frame_out_ = NULL;
	frame_in_ = NULL;
}