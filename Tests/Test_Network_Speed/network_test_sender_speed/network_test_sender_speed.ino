#define EEL_DEBUG

#include <EEL_Driver.h>
#include "LoRa_E220.h"

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x003F
#define MY_CHANNEL (uint8_t)23

#define B_ADDRESS (uint16_t)0x001F
#define B_CHANNEL (uint8_t)23

#define STOP_TIME (uint32_t)300000

uint8_t *HeapPointer, *StackPointer;

EEL_Driver* nd;
Datagram* datagram;

void XORBan(){
  Serial.println("XORBAN");
  if(nd->remove_ban(B_ADDRESS)){
    nd->ban_address(B_ADDRESS);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(2000000);
  delay(1000);
  Serial.println("HELLO");
  Serial.print("SS_CACHE: ");Serial.println(_SS_MAX_RX_BUFF, HEX);
  StackPointer = (uint8_t *)malloc(4); //We do a small allocation.
  HeapPointer = StackPointer; // We save the value of the heap pointer.
  free(StackPointer); // We use the dreaded free() to zero the StackPointer.
  StackPointer = (uint8_t *)(SP); //Get the Stack Pointer
  Serial.print("0x");
  Serial.println((int)StackPointer, HEX);
  Serial.print("0x");
  Serial.println((int)HeapPointer, HEX);
  
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new LoRa_E220(17, 16, 7, 5, 4, UART_BPS_RATE_19200));
  
  char* databuffer = (char*)malloc(128);
  strcpy(databuffer, "Lorem ipsum.");
  Serial.print("MSG: ");Serial.println(databuffer);
  Serial.print("STRLEN ");Serial.println(strlen(databuffer));
  
  datagram = CreateDatagram(0, 50, databuffer, strlen(databuffer));
  nd->ban_address(B_ADDRESS);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), XORBan, FALLING);
}

uint8_t isSent = 1;
unsigned long interval = 0;
volatile uint32_t _stop = millis();

uint8_t MetricsPrinted = 1;

uint8_t RouteFindStart_lock = 1;
uint8_t RouteFoundEnd_lock = 1;
uint32_t RouteFindStart = 1;
uint32_t RouteFoundEnd = 1;

uint32_t sdi_total_time = 1;
uint32_t sdi_total_count = 0;

uint32_t sdi_udp_time = 1;
uint32_t sdi_udp_sent = 0;

void loop() {
  while(millis() > _stop + STOP_TIME){
    if(MetricsPrinted){
      MetricsPrinted = 0;
      PRINT("time spent receiving(ms): ")Serial.println(nd->receive_packet_total_time);
      PRINT("total bytes received: ")Serial.println(nd->receive_packet_total_bytes);
      PRINT("bps: ")Serial.println(1000 *((float)nd->receive_packet_total_bytes / nd->receive_packet_total_time));

      PRINT("time spent processing routing(ms): ")Serial.println((float)nd->aodv_total_time / 1000);
      PRINT("total routing messages: ")Serial.println(nd->aodv_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->aodv_total_time / 1000) / (float)nd->aodv_packet_count);

      PRINT("time spent processing udp(ms): ")Serial.println((float)nd->udp_total_time / 1000);
      PRINT("total udp messages: ")Serial.println(nd->udp_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->udp_total_time / 1000) / (float)nd->udp_packet_count);

      PRINT("time spent forwarding(ms): ")Serial.println((float)nd->forward_packet_total_time / 1000);
      PRINT("total forwarded messages: ")Serial.println(nd->forward_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->forward_packet_total_time / 1000) / (float)nd->forward_packet_count);

      PRINT("time spent processing packets(ms): ")Serial.println((float)nd->general_packet_total_time / 1000);
      PRINT("total processed messages: ")Serial.println(nd->general_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->general_packet_total_time / 1000) / (float)nd->general_packet_count);

      PRINT("time spent sending(ms): ")Serial.println((float)sdi_total_time / 1000);
      PRINT("total sent: ")Serial.println(sdi_total_count);
      PRINT("average sending time(ms): ")Serial.println(((float)sdi_total_time / 1000) / (float)sdi_total_count);

      PRINT("time spent sending(udp only)(ms): ")Serial.println((float)sdi_udp_time / 1000);
      PRINT("total sent: ")Serial.println(sdi_udp_sent);
      PRINT("average sending time(ms): ")Serial.println(((float)sdi_udp_time / 1000) / (float)sdi_udp_sent);

      PRINT("Route_Creation_Time: ")Serial.println(RouteFoundEnd - RouteFindStart);
    }
  }
  
	if(millis() - interval > 5000 || isSent){
		uint32_t sdi_start = micros();
		uint8_t retval = nd->SendDatagram(B_ADDRESS, ToS::NORMAL, datagram);
		sdi_total_time += micros() - sdi_start;
		sdi_total_count++;
		//Serial.println("MSG_SENT");
		interval = millis();
		isSent = 0;
		if(retval == 0x01){
			PRINTLN("ERROR")
		}
		else if(retval == 0x00){
			if(RouteFoundEnd_lock){
				RouteFoundEnd_lock = 0;
				RouteFoundEnd = millis();
			}
			sdi_udp_time += micros() - sdi_start;
			sdi_udp_sent++;
				PRINTLN("MSG_SENT")
		}
		else if(retval == 0x04 || retval == 0x02){
			if(RouteFindStart_lock){
				RouteFindStart_lock = 0;
				RouteFindStart = millis();
			}
		}
	}
	
	nd->run();
}
