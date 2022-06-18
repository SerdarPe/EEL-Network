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

#include "EEL_Driver.h"

#define ADDL(x) ((uint8_t)((x) >> 8))
#define ADDH(x) ((uint8_t)((x) & 0x00FF))

#ifdef BROADCAST_ADDRESS
#undef BROADCAST_ADDRESS
#define BROADCAST_ADDRESS (uint16_t)0xFFFF
#endif

#define MAX_RECEIVE_ERROR_COUNT (uint16_t)100

#define RECEIVE_INTERVAL 1000 /*in ms*/
#define PROCESS_INCOMING_INTERVAL 750 /*in ms*/
#define PROCESS_OUTGOING_INTERVAL 300 /*in ms*/
#define FALSE_FRAME_DELAY 80 /*in ms*/
#define TIMEOUT 40 /*in ms*/
#define RECEIVE_DELAY 20

void EEL_Driver::run(){
	unsigned long start;			//32-bit integer
	
	/*------------------------RECEIVE & BUFFER INCOMING PACKETS------------------------*/
	wdt_enable(WDTO_8S);
	start = millis();
	while(millis() - start < RECEIVE_INTERVAL){
		if(this->device_->available()){
			
			_start_frame();
			if(_receive_error){
				continue;
			}

			_timeout = millis();
			while(this->device_->available_bytes() < (sizeof(Frame) - sizeof(Frame::sfd))){
				if(millis() - _timeout >= TIMEOUT){
					receive_fail_recovery();
					_receive_error = true;
					break;
				}
			}
			if(_receive_error){
				continue;
			}
			
			device_->receive((char*)_header + sizeof(Frame::sfd), sizeof(Frame) - sizeof(Frame::sfd), false);		//We get the header first, so we can get the actual size
			uint8_t length = FRAME_LENGTH(_header);
			
			delay(RECEIVE_DELAY);

			uint8_t crc_result = 0;
			if((crc_result = CheckCRC8(_header->crc, (uint8_t*)(_header) + offsetof(Frame, payload_length), FRAME_CRC_SIZE)) != 0x00){
				receive_fail_recovery();
				continue;
			}

			uint8_t* inp = (uint8_t*)malloc(sizeof(Frame) + length);
			if(inp == NULL){break;}
			uint8_t inp_shift = sizeof(Frame);
			uint8_t bytes_read = 0;
			_timeout = millis();
			while(bytes_read != length){
				if(millis() - _timeout >= TIMEOUT){
					free(inp);			//Free if fail, otherwise it leaks
					receive_fail_recovery();
					_receive_error = true;
					break;
				}
				uint8_t available_bytes = this->device_->available_bytes();
				if(length - bytes_read > available_bytes){
					bytes_read += this->device_->receive(inp + inp_shift, available_bytes, false);
					inp_shift += bytes_read;
				}
				else{
					bytes_read += this->device_->receive(inp + inp_shift, length - bytes_read, false);
					inp_shift += bytes_read;
				}
			}
			if(_receive_error){
				continue;
			}
			
			memcpy(inp, _header, sizeof(Frame));							//Copy the header back into the packet

			uint8_t buf_resp = this->buffer(inp);

			receive_error_count_ = 0;
		}
	}
	/*------------------------------------------------------------------------------------------------*/

	/*------------------------CALL LAYER FUNCTIONS TO HANDLE BUFFERED INCOMING FRAMES------------------------*/
	wdt_reset();
	start = millis();
	while(millis() - start < PROCESS_INCOMING_INTERVAL){
		uint8_t* pkt = this->remove_priority();
		if(pkt != NULL){
			LINK_RESPONSE link_response = link_->FromPHY(pkt);
			switch(link_response){
				case FRAME_NULL:{}
					//A NULL argument is passed to Link Layer, invalid
				break;

				case FRAME_ERROR:{	
				}
					//Something went wrong (during transmission), break and free packet
				break;

				case MAC_BLOCKED:{}
					//You are not allowed to send packets to me
				break;

				case TO_NETWORK:{
					void* net_packet = link_->ToNetworkLayer();
					NETWORK_RESPONSE net_response = network_->FromLinkLayer(net_packet, NULL, NULL);
					switch(net_response){
						case PACKET_NULL:{
						}
						break;

						case PACKET_ERROR:{
						}
						break;

						case TO_AODV:{
							AODV_RESPONSE router_response = network_->HandleRouting();
							switch(router_response){
								case HANDLE_RREQ:{
									uint16_t target_addr;
									uint8_t target_ch;
									uint8_t ttl_out;
									net_packet = network_->GetPacketIn();
									void* msg = network_->GetRouter()->HandleRREQ((RREQ_Packet*)NET_DATA(net_packet), link_->GetFrame()->sender_address, link_->GetFrame()->sender_channel, network_->GetMyAddress(), network_->GetMyChannel(), &target_addr, &target_ch, NET_TTL(net_packet), &ttl_out);
									if(msg == NULL){
										break;
									}
									network_->FromRouting(target_addr, msg);
									SendPacketDirect(target_addr, target_ch);
								}
								break;

								case HANDLE_RREP:{
									uint16_t target_addr;
									uint8_t target_ch;
									net_packet = network_->GetPacketIn();
									void* msg = network_->GetRouter()->HandleRREP((RREP_Packet*)NET_DATA(net_packet), link_->GetFrame()->sender_address, link_->GetFrame()->sender_channel, network_->GetMyAddress(), &target_addr, &target_ch);
									if(msg == NULL){
										break;
									}
									network_->FromRouting(target_addr, msg);
									SendPacketDirect(target_addr, target_ch);
								}
								break;

								case HANDLE_RERR:{
									uint16_t target_addr;
									uint8_t target_ch;
									uint8_t ttl_out;
									net_packet = network_->GetPacketIn();
									void* msg = network_->GetRouter()->HandleRERR((RERR_Packet*)NET_DATA(net_packet), link_->GetFrame()->sender_address, link_->GetFrame()->sender_channel, &target_addr, &target_ch, NET_TTL(net_packet), &ttl_out);
									if(msg == NULL){
										break;
									}
									network_->FromRouting(target_addr, msg);
									SendPacketDirect(target_addr, target_ch);
								}
								break;
							}
						}
						break;

						case TO_UDP:{
							transport_->FromNetworkLayer(network_->ToTransportLayer());
							network_->GetRouter()->UpdateRouteLifetime(FRAME_SENDER(pkt));
							network_->GetRouter()->UpdateRouteLifetime(NET_SOURCE(net_packet));
						}
						break;

						case FORWARD_PACKET:{
							network_->GetRouter()->UpdateRouteLifetime(FRAME_SENDER(pkt));
							network_->GetRouter()->UpdateRouteLifetime(NET_SOURCE(net_packet));
							net_packet = network_->GetPacketOut();
							/*-----------------FORWARD_PACKET()------------------------------*/
							ROUTE_STATUS status = network_->GetNextHop(NET_DEST(net_packet));

							switch(status) {
								case ROUTE_ACTIVE:{
									uint8_t sp = SendPacket();
								}
								break;

								/*case ROUTE_BROADCAST:{
									uint8_t sp = SendPacket();
								}
								break;*/

								case ROUTE_INVALID:{
									free(network_->RemovePacketOut());
									RERR_Packet* rerr = network_->GetRouter()->RouteError(network_->GetNextHop());
									network_->FromRouting(BROADCAST_ADDRESS, rerr);
									SendPacketDirect(BROADCAST_ADDRESS, 0);
								}
								break;
								case ROUTE_NO_ENTRY:{
									free(network_->RemovePacketOut());
									RERR_Packet* rerr = network_->GetRouter()->RouteError(NET_DEST(net_packet));
									network_->FromRouting(BROADCAST_ADDRESS, rerr);
									SendPacketDirect(BROADCAST_ADDRESS, 0);
								}
								break;
							}
							/*-----------------------------------------------------------------*/
						}
						break;
					}
				}
				break;
			}
			free(pkt);	//Every received packets are coming in as a whole "block", pointers inside should not be freed, as they are not individually dynamically allocated\
						So, the received packets will be de-allocated this way just fine
			clean_up();
		}
		else{
			break;
		}
	}
	/*------------------------------------------------------------------------------------------------*/

	/*------------------------CALL PHY TO SEND BUFFERED OUTGOING FRAMES------------------------*/
	wdt_reset();
	start = millis();
	while (millis() - start < PROCESS_OUTGOING_INTERVAL){
		uint8_t *pkt = this->remove_priority_o();
		if(pkt != NULL){
			uint16_t dest = FRAME_DEST(pkt);
			uint8_t channel = FRAME_DEST_CHANNEL(pkt);
			uint8_t size = sizeof(struct Frame) + FRAME_LENGTH(pkt);
			if(dest == BROADCAST_ADDRESS){
				this->device_->broadcast(pkt, size);
			}
			else{
				this->device_->send(ADDL(dest), ADDH(dest), channel, pkt, size);
			}
			free(pkt);
			//clean_up();
		}
		else{
			break;
		}
	}
	/*------------------------------------------------------------------------------------------------*/

	/*------------------------PERIODIC FUNCTIONS OF LAYERS------------------------*/
	wdt_reset();
		/*GENERATE_HELLO_MESSAGE() PERIODIC FUNCTION*/
		{
			uint8_t ttl_out;
			void* hello_msg = this->network_->GetRouter()->GenerateHelloMessage(network_->GetMyAddress(), network_->GetMyChannel(), &ttl_out);
			if (hello_msg != NULL) {
				network_->FromRouting(BROADCAST_ADDRESS, hello_msg);
				SendPacketDirect(BROADCAST_ADDRESS, 0);
				clean_up();
			}
		}
		/*-------------------------------------------*/

		/*CHECK_LIFETIME() PERIODIC FUNCTION*/
		
		{
			this->network_->GetRouter()->CheckLifetimeOfRoutes();
		}
		
		/*-------------------------------------------*/

		/*CHECK_NEIGHBORS() PERIODIC FUNCTION*/
		{
			network_->GetRouter()->ResetGetNextNeighbor();
			RoutingTableElement* ne = NULL;
			while((ne = network_->GetRouter()->GetNextNeighbor()) != NULL){
				void* error_msg = this->network_->GetRouter()->CheckNeighborLifetime(ne);
				if (error_msg != NULL) {
					network_->FromRouting(BROADCAST_ADDRESS, error_msg);
					SendPacketDirect(BROADCAST_ADDRESS, 0);
					clean_up();
				}
				ne = NULL;
			}
		}
		/*---------------------------------------------*/

	/*------------------------------------------------------------------------------------------------*/
	wdt_disable();
}

void EEL_Driver::receive_fail_recovery(){
	++receive_error_count_;
	if(receive_error_count_ >= MAX_RECEIVE_ERROR_COUNT){
		//resetFunc();
		device_->clean_rx_buffer();
		device_->clean_tx_buffer();
	}
	_timeout = millis();
	while(millis() - _timeout >= TIMEOUT){
		if(device_->available_bytes() >= sizeof(Frame::sfd)){
			if(device_->peek() == 0xAB){return;}
			device_->read();
		}
		else{
			device_->clean_rx_buffer();
		}
	}
}

void EEL_Driver::_start_frame(){
	_sfd = 0;
	_receive_error = false;
	
	_timeout = millis();
	while(this->device_->available_bytes() < sizeof(Frame::sfd)){
		if(millis() - _timeout >= TIMEOUT){
			receive_fail_recovery();
			_receive_error = true;
			return;
		}
	}
	device_->receive(&_sfd, 1, false);
	if(_sfd != 0xAB){receive_fail_recovery();_receive_error = true;return;}

	device_->receive(&_sfd, 1, false);
	if(_sfd != 0xAA){receive_fail_recovery();_receive_error = true;return;}

	device_->receive(&_sfd, 1, false);
	if(_sfd != 0xAA){receive_fail_recovery();_receive_error = true;return;}
	
	device_->receive(&_sfd, 1, false);
	if(_sfd != 0xAA){receive_fail_recovery();_receive_error = true;return;}
}

EEL_Driver::EEL_Driver(uint16_t my_address, uint8_t my_channel, EEL_Device* device){
	_header = (Frame*)malloc(sizeof(Frame));
	
	//RESET INBUT BUFFER
	for(int i = 0; i < INBUF_SZ; i++){
		this->pktbuf[i] = NULL;
	}

	//RESET OUTPUT BUFFER
	for(int i = 0; i < OUTBUF_SZ; i++){
		this->pktbuf_o[i] = NULL;
	}

	this->device_ = device;
	this->link_ = new EEL_Link(my_address, my_channel);
	this->network_ = new EEL_Network(my_address, my_channel);
	this->transport_ = new EEL_Transport();
}

/*------------------------API-LIKE FUNCTIONS TO ENABLE COMMUNICATION------------------------*/

/*Allocates buffer for a specific port
 *@return Port number itself on success
 */
int8_t EEL_Driver::set_port(uint8_t port_no, uint8_t buffer_size){
	return transport_->SetPort(port_no, buffer_size);
}

/*Read the port's buffer. Do not de-alloc the returned pointer
 *@param length pass a char pointer to get the length of the written message, NULL is accepted
 *@param source pass a char pointer to get the source port that sent the message, NULL is accepted
 *@return Port's buffer pointer
 */
uint8_t* EEL_Driver::read_port(uint8_t port_no, uint8_t* length, uint8_t* source){
	return transport_->ReadPort(port_no, length, source);
}

/*Check if the port has received a message or not
 *@return 0x02 if written, 0x00 if not, 0x80 if such port does not exist on the device
 */
int8_t EEL_Driver::is_port_written(uint8_t port_no){
	return transport_->IsPortWritten(port_no);
}

/*Function to check status of a route
 *@return ROUTE_STATUS
 */
ROUTE_STATUS EEL_Driver::route_status(uint16_t address){
	return network_->RouteStatus(address);
}

/*Function to ban specific addresses
 *Returns 0x00 on sucess, 0x01 on buffer full, 0x02 if already banned
 */
int8_t EEL_Driver::ban_address(uint16_t address){
	return this->link_->AddBlackList(address);
}

/*Returns 0x00 on success, 0x01 otherwise
 */
int8_t EEL_Driver::remove_ban(uint16_t address){
	return this->link_->RemoveBlackList(address);
}

/*Sends a datagram to the desired destination. Datagrams are not sent immediately,
 *rather stored in a buffer and will be sent in respect to their priorities.
 *
 *If no such route exists for the destination, datagram will be dropped and a route request will be sent instead.
 *Dropped datagrams are not buffered.
 *
 *Returns 0x00 - on success, 0x01 - malloc error, 0x02 - invalid route, 0x04 - no route, 0x08 - buffer full
 */
uint8_t EEL_Driver::SendDatagramBuffer(uint16_t destination, ToS priority, struct Datagram* datagram){
	wdt_enable(WDTO_8S);
	uint8_t return_value = 0;

	uint8_t datagram_length = sizeof(struct Datagram) + datagram->payload_length;
	this->network_->FromTransportLayer(destination, priority, NET_PROTOCOL_TYPE::UDP, datagram_length, (void*)datagram);
	
	ROUTE_STATUS status = network_->GetNextHop(destination);
	switch(status){
		case ROUTE_ACTIVE:{
			uint16_t next_hop = network_->GetNextHop();
			uint8_t next_channel = network_->GetNextChannel();
			void* net_data = this->network_->ToLinkLayer();

			uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
			this->link_->FromNetworkLayer(next_hop, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);
			
			void* frame = link_->ToPHY();
			uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
			((struct Frame*)frame)->receiver_channel = next_channel;												//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
			
			((struct Frame*)frame)->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
			
			void* frame_s = serialize(frame_length, frame);

			if(frame_s != NULL){
				if(buffer_o(frame_s)){free(frame_s);return_value = 0x08;}
				else{return_value = 0x00;}
			}
			else{
				return_value = 0x01;
			}
		}
		break;

		case ROUTE_BROADCAST:{
			void* net_data = this->network_->ToLinkLayer();

			uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
			this->link_->FromNetworkLayer(BROADCAST_ADDRESS, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);

			void* frame = link_->ToPHY(NULL, NULL);
			uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
			void* frame_s = serialize(frame_length, frame);
			if(frame_s != NULL){
				if(buffer_o(frame_s)){free(frame_s);return_value = 0x08;}
				return_value = 0x00;
			}
			else{
				return_value = 0x01;
			}
		}
		break;

		case ROUTE_INVALID:{
			request_route(destination, REASON_INVALID);
			return_value = 0x02;
		}
		break;

		case ROUTE_NO_ENTRY:{
			request_route(destination, REASON_NULL);
			return_value = 0x04;
		}
		break;
	}
	clean_up();
	wdt_disable();
	return return_value;
}

/*Sends a datagram to the desired destination.
 *If no such route exists for the destination, datagram will be dropped and a route request will be sent instead.
 *Dropped datagrams are not buffered.
 *Returns 0x00 - on success, 0x01 - malloc error, 0x02 - invalid route, 0x04 no route
 */
uint8_t EEL_Driver::SendDatagram(uint16_t destination, ToS priority, struct Datagram* datagram){
	wdt_enable(WDTO_8S);

	uint8_t datagram_length = sizeof(struct Datagram) + datagram->payload_length;
	this->network_->FromTransportLayer(destination, priority, NET_PROTOCOL_TYPE::UDP, datagram_length, (void*)datagram);
	
	ROUTE_STATUS status = network_->GetNextHop(destination);
	switch(status){
		case ROUTE_ACTIVE:{
			uint16_t next_hop = network_->GetNextHop();
			uint8_t next_channel = network_->GetNextChannel();
			void* net_data = this->network_->ToLinkLayer();
			
			uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
			this->link_->FromNetworkLayer(next_hop, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);
			
			void* frame = link_->ToPHY();
			uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
			((struct Frame*)frame)->receiver_channel = next_channel;												//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
			
			((struct Frame*)frame)->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
			
			void* frame_s = serialize(frame_length, frame);

			if(frame_s != NULL){
				this->device_->send(ADDL(next_hop), ADDH(next_hop), next_channel, frame_s, frame_length);
				free(frame_s);
				clean_up();

				wdt_disable();
				return 0x00;
			}
			else{
				wdt_disable();
				return 0x01;
			}
		}
		break;

		case ROUTE_BROADCAST:{
			void* net_data = this->network_->ToLinkLayer();

			uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
			this->link_->FromNetworkLayer(BROADCAST_ADDRESS, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);

			void* frame = link_->ToPHY(NULL, NULL);
			uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
			void* frame_s = serialize(frame_length, frame);
			if(frame_s != NULL){
				this->device_->broadcast(frame_s, frame_length);
				free(frame_s);
				clean_up();

				wdt_disable();
				return 0x00;
			}
			else{
				wdt_disable();
				return 0x01;
			}
		}
		break;

		case ROUTE_INVALID:{
			request_route(destination, REASON_INVALID);
			wdt_disable();
			return 0x02;
		}
		break;

		case ROUTE_NO_ENTRY:{
			request_route(destination, REASON_NULL);
			wdt_disable();
			return 0x04;
		}
		break;
	}
}

uint8_t EEL_Driver::SendPacket(){
	void* net_data = this->network_->ToLinkLayer();
	if(net_data == NULL){return 0x10;}

	uint16_t next_hop = network_->GetNextHop();
	uint8_t next_channel = network_->GetNextChannel();

	uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
	this->link_->FromNetworkLayer(next_hop, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);
	if(next_hop == BROADCAST_ADDRESS){
		void* frame = link_->ToPHY(&next_hop, &next_channel);
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->broadcast(frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
	else{
		void* frame = link_->ToPHY(&next_hop, NULL);
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		((struct Frame*)frame)->receiver_channel = next_channel;				//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
		((struct Frame*)frame)->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
		
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->send(ADDL(next_hop), ADDH(next_hop), next_channel, frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
}

uint8_t EEL_Driver::SendPacket(uint8_t channel){
	uint16_t next_hop = 0;
	uint8_t next_channel = 0;

	void* net_data = this->network_->ToLinkLayer();
	if(net_data == NULL){return 0x10;}

	uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
	this->link_->FromNetworkLayer(next_hop, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);
	if(next_hop == BROADCAST_ADDRESS){
		void* frame = link_->ToPHY(&next_hop, &next_channel);
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->broadcast(frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
	else{
		void* frame = link_->ToPHY(&next_hop, NULL);
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		((struct Frame*)frame)->receiver_channel = channel;														//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
		((struct Frame*)frame)->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	//THIS LINE SHOULD NOT EXISTS IF A LOGICAL ADDRESS IS IMPLEMENTED
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->send(ADDL(next_hop), ADDH(next_hop), next_channel, frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
}

uint8_t EEL_Driver::SendPacketDirect(uint16_t destination_address, uint8_t channel){
	void* net_data = this->network_->ToLinkLayer();
	if(net_data == NULL){return 0x10;}

	uint8_t packet_length = sizeof(struct Packet) + NET_LENGTH(net_data);
	this->link_->FromNetworkLayer(destination_address, LINK_PROTOCOL_TYPE::NETWORK, packet_length, net_data);
	if(destination_address == BROADCAST_ADDRESS){
		void* frame = link_->ToPHY();
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->broadcast(frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
	else{
		void* frame = link_->ToPHY();
		if(frame == NULL){return 0x11;}
		uint8_t frame_length = sizeof(struct Frame) + FRAME_LENGTH(frame);
		((struct Frame*)frame)->receiver_channel = channel;														//THIS LINE SHOULD NOT EXIST IF A LOGICAL ADDRESS IS IMPLEMENTED
		((struct Frame*)frame)->crc = CalculateCRC8((uint8_t*)(frame) + offsetof(Frame, payload_length), FRAME_CRC_SIZE);	//THIS LINE SHOULD NOT EXIST IF A LOGICAL ADDRESS IS IMPLEMENTED
		void* frame_s = serialize(frame_length, frame);
		if(frame_s != NULL){
			this->device_->send(ADDL(destination_address), ADDH(destination_address), channel, frame_s, frame_length);
			free(frame_s);
			clean_up();
			return 0x00;
		}
		/*else malloc failure*/
		return 0x01;
	}
}



void EEL_Driver::request_route(uint16_t destination, REQUEST_REASON reason){
	if(network_->GetPacketOut() != NULL){
		network_->CleanUp();
	}
	network_->RequestRoute(destination, reason);
	uint8_t sp = SendPacket();
	clean_up();
}
/*------------------------------------------------------------------------------------------------*/


/*------------------------INPUT BUFFER FUNCTIONS------------------------*/

uint8_t EEL_Driver::buffer(uint8_t* p){
	for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf[i] == NULL){
			if((this->pktbuf[i] = (uint8_t*)deserialize(p)) != NULL){
				return 0x00;	//Success
			}
			else{
				free(p);
				return 0x01;	//Frame couldn't be deserialized, possible transmission error
			}
        }
	}
	free(p);
	return 0x02;				//Buffer full
}

//This one discards the least priority packet
void EEL_Driver::discard(){
    int currentIndex = -1;
    ToS currentToS = MAX_TOS;
    for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf[i] != NULL){
			ToS priority = EEL_Network::DecodePriority(((Packet*)((Frame*)pktbuf[i])->data));
            if(currentToS > priority){
                currentIndex = i;
                currentToS = priority;
            }
        }
	}
    if(currentIndex > -1){
        free(this->pktbuf[currentIndex]);
        this->pktbuf[currentIndex] = NULL;
		for(int j = currentIndex; j < INBUF_SZ - 1; j++){
			if(this->pktbuf[j + 1] != NULL){
				this->pktbuf[j] = this->pktbuf[j + 1];
				this->pktbuf[j + 1] = NULL;
			}
		}
    }
    else{
        return;
    }
}

//This one discards a specific packet
void EEL_Driver::discard(void* p){
	for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf[i] == p){
			free(p);
			this->pktbuf[i] == NULL;
			for(int j = i; j < INBUF_SZ - 1; j++){
				if(this->pktbuf[j + 1] != NULL){
					this->pktbuf[j] = this->pktbuf[j + 1];
					this->pktbuf[j + 1] = NULL;
				}
			}
			return;
        }
	}
}

/*
 *Returns the first non-null packet from buffer
 */
uint8_t* EEL_Driver::remove(){
	for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf[i] != NULL){
			uint8_t* pkt = this->pktbuf[i];
			this->pktbuf[i] = NULL;
			for(int j = i; j < INBUF_SZ - 1; j++){
				if(this->pktbuf[j + 1] != NULL){
					this->pktbuf[j] = this->pktbuf[j + 1];
					this->pktbuf[j + 1] = NULL;
				}
			}
			return pkt;
		}
	}
	return NULL;
}

//Returns the most priority packet from buffer
uint8_t* EEL_Driver::remove_priority(){
    int currentIndex = -1;
    ToS currentToS = NO_TOS;
	ToS priority;
    for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf[i] != NULL){
			priority = EEL_Network::DecodePriority(((Packet*)((Frame*)pktbuf[i])->data));
			//priority = (ToS)NET_TOS(FRAME_DATA(pktbuf[i]));
            if(currentToS < priority){
                currentIndex = i;
                currentToS = priority;
            }
        }
	}
    if(currentIndex > -1){
        uint8_t* pkt = this->pktbuf[currentIndex];
		this->pktbuf[currentIndex] = NULL;
		for(int i = currentIndex; i < INBUF_SZ - 1; i++){
			if(this->pktbuf[i + 1] != NULL){
				this->pktbuf[i] = this->pktbuf[i + 1];
				this->pktbuf[i + 1] = NULL;
			}
		}
		return pkt;
    }
    else{
        return NULL;
    }
}

/*------------------------------------------------------------------------------------------------*/

/*------------------------OUTPUT BUFFER FUNCTIONS------------------------*/

uint8_t EEL_Driver::buffer_o(uint8_t* p){
	for(int i = 0; i < INBUF_SZ; i++){
		if(this->pktbuf_o[i] == NULL){
			this->pktbuf_o[i] = p;
			return 0x00;	//Buffer-OK
        }
	}
	return 0x02;			//Buffer full
}

//Discard the first least-priority packet
void EEL_Driver::discard_o(){
    int currentIndex = -1;
    ToS currentToS = MAX_TOS;
    for(int i = 0; i < OUTBUF_SZ; i++){
		if(this->pktbuf_o[i] != NULL){
			ToS priority = EEL_Network::DecodePriority(((Packet*)((Frame*)pktbuf[i])->data));
            if(currentToS > priority){
                currentIndex = i;
                currentToS = priority;
            }
        }
	}
    if(currentIndex > -1){
        free(this->pktbuf_o[currentIndex]);
        this->pktbuf_o[currentIndex] = NULL;
		for(int j = currentIndex; j < OUTBUF_SZ - 1; j++){
			if(this->pktbuf_o[j + 1] != NULL){
				this->pktbuf_o[j] = this->pktbuf_o[j + 1];
				this->pktbuf_o[j + 1] = NULL;
			}
		}
		return;
    }
    else{
        return;
    }
}

void EEL_Driver::discard_o(void* p){
	for(int i = 0; i < OUTBUF_SZ; i++){
		if(this->pktbuf_o[i] == p){
			free(p);
			this->pktbuf_o[i] == NULL;
			for(int j = i; j < OUTBUF_SZ - 1; j++){
				if(this->pktbuf_o[j + 1] != NULL){
					this->pktbuf_o[j] = this->pktbuf_o[j + 1];
					this->pktbuf_o[j + 1] = NULL;
				}
			}
			return;
        }
	}
}

//Returns the first packet.
uint8_t* EEL_Driver::remove_o(){
	for(int i = 0; i < OUTBUF_SZ; i++){
		if(this->pktbuf_o[i] != NULL){
			uint8_t* pkt = this->pktbuf_o[i];
			this->pktbuf_o[i] = NULL;
			for(int j = i; j < OUTBUF_SZ - 1; j++){
				if(this->pktbuf_o[j + 1] != NULL){
					this->pktbuf_o[j] = this->pktbuf_o[j + 1];
					this->pktbuf_o[j + 1] = NULL;
				}
			}
			return pkt;
		}
	}
	return NULL;
}

//Returns the first highest-priority packet.
uint8_t* EEL_Driver::remove_priority_o(){
    int currentIndex = -1;
    ToS currentToS = NO_TOS;
    for(int i = 0; i < OUTBUF_SZ; i++){
		if(this->pktbuf_o[i] != NULL){
			ToS priority = EEL_Network::DecodePriority(((Packet*)((Frame*)pktbuf_o[i])->data));
            if(currentToS < priority){
                currentIndex = i;
                currentToS = priority;
            }
        }
	}
    if(currentIndex > -1){
		uint8_t* pkt = this->pktbuf_o[currentIndex];
		this->pktbuf_o[currentIndex] = NULL;
		for(int i = currentIndex; i < OUTBUF_SZ - 1; i++){
			if(this->pktbuf_o[i + 1] != NULL){
				this->pktbuf_o[i] = this->pktbuf_o[i + 1];
				this->pktbuf_o[i + 1] = NULL;
			}
		}
        return pkt;
    }
    else{
        return NULL;
    }
}
/*------------------------------------------------------------------------------------------------*/

void* EEL_Driver::serialize(uint8_t length, void* packet){
	char* s = (char*)malloc(length);
	if(s == NULL){
		return NULL;
	}
	uint8_t nextPtr = 0;
	memcpy(s, packet, sizeof(Frame));
	
	nextPtr += sizeof(Frame);
	LINK_PROTOCOL_TYPE lprotocol = FRAME_PROTOCOL(packet);
	switch(lprotocol){
		case NETWORK:{
			Packet* net_packet = FRAME_DATA(packet);
			memcpy(s + nextPtr, net_packet, sizeof(Packet));

			nextPtr += sizeof(Packet);
			NET_PROTOCOL_TYPE netprotocol = NET_PROT(net_packet);
			switch (netprotocol){
				case UDP:{
					Datagram* udp_packet = NET_DATA(net_packet);
					 
					memcpy(s + nextPtr, udp_packet, sizeof(Datagram));
					nextPtr += sizeof(Datagram);

					uint8_t* payload = ((Datagram*)udp_packet)->data;
					uint8_t payload_length = ((Datagram*)udp_packet)->payload_length;
					memcpy(s + nextPtr, payload, payload_length);
				}
				break;

				case AODV_CONTROL:{
					AODV_TYPE aodv_type = (AODV_TYPE)*(char*)(NET_DATA(net_packet));
					switch(aodv_type){
						case TYPE_RREQ:{
							RREQ_Packet* rreq = NET_DATA(net_packet);

							memcpy(s + nextPtr, rreq, sizeof(RREQ_Packet));
							nextPtr += sizeof(RREQ_Packet);
						}
						break;
						
						case TYPE_RREP:{
							RREP_Packet* rrep = NET_DATA(net_packet);

							memcpy(s + nextPtr, rrep, sizeof(RREP_Packet));
							nextPtr += sizeof(RREP_Packet);
						}
						break;

						case TYPE_RERR:{
							RERR_Packet* rerr = NET_DATA(net_packet);

							memcpy(s + nextPtr, rerr, sizeof(RERR_Packet));
							nextPtr += sizeof(RERR_Packet);

							RERR_Extra* extra = rerr->ExtraArgs;
							while(extra != NULL){
								memcpy(s + nextPtr, extra, sizeof(RERR_Extra));
								nextPtr += sizeof(RERR_Extra);

								extra = extra->ExtraArgs;
							}
						}
						break;
					}
				}
				break;

				default:{
					//NONE OF THE CASES MEANS SOMETHING WENT WRONG
					free(s);
					return NULL;
					break;
				}
			}
		}
		break;
		default:{
			free(s);
			return NULL;
			break;
		}
	}
	return s;
}

//General function to fix the received packet's pointers
void* EEL_Driver::deserialize(void* packet){

	//Correct the Frame's data pointer
	FRAME_DATA(packet) = (char*)packet + sizeof(Frame);

	LINK_PROTOCOL_TYPE lprotocol = FRAME_PROTOCOL(packet);
	switch(lprotocol){
		case NETWORK:{
			//Correct the Network Packet's data pointer
			Packet* net_packet = FRAME_DATA(packet);
			NET_DATA(net_packet) = (char*)net_packet + sizeof(Packet);

			NET_PROTOCOL_TYPE netprotocol = NET_PROT(net_packet);
			switch (netprotocol){
				case UDP:{
					//Correct the Datagram's data pointer
					Datagram* udp_packet = NET_DATA(net_packet);
					DATAGRAM_DATA(udp_packet) = (char*)udp_packet + sizeof(Datagram);
				}
				break;

				case AODV_CONTROL:{
					AODV_TYPE subType = (AODV_TYPE)*(char*)(NET_DATA(net_packet));
					
					switch(subType){
						case TYPE_RREQ:{
							//Already deserialized
						}
						break;

						case TYPE_RREP:{
							//Already deserialized
						}
						break;

						case TYPE_RERR:{
							RERR_Packet* rerr = NET_DATA(net_packet);
							if(rerr->ExtraArgs != NULL){
								rerr->ExtraArgs = (RERR_Extra*)((char*)rerr + sizeof(RERR_Packet));
								RERR_Extra* extra = rerr->ExtraArgs;
								while(extra->ExtraArgs != NULL){
									extra->ExtraArgs = (RERR_Extra*)((char*)extra + sizeof(RERR_Extra));
									extra = extra->ExtraArgs;
								}
							}
						}
						break;

						case TYPE_ACK:{
							//Unused
						}
						break;

						default:{
							return NULL;
						}
						break;
					}
				}
				break;

				default:{
					return NULL;
				}
				break;
			}
		}
		break;

		default:{
			return NULL;
		}
		break;
	}
	return packet;
}

void EEL_Driver::clean_up(){
	link_->CleanUp();
	network_->CleanUp();
	transport_->CleanUp();
}