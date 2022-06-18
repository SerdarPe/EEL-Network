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
  //strcpy(databuffer, "My name is EEL... NETWORK!");
  strcpy(databuffer, "Lorem ipsum.");
  Serial.print("MSG: ");Serial.println(databuffer);
  Serial.print("STRLEN ");Serial.println(strlen(databuffer));
  
  datagram = CreateDatagram(0, 50, databuffer, strlen(databuffer));
  //nd->SendDatagramImmediate((uint16_t)0x001F, ToS::NORMAL, datagram);
  nd->ban_address(B_ADDRESS);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), XORBan, FALLING);
}

uint8_t isSent = 1;
unsigned long interval = 0;
volatile uint32_t _stop = millis();

void loop() {
  while(millis() > _stop + STOP_TIME);
  
	if(millis() - interval > 5000 || isSent){
		uint8_t retval = nd->SendDatagramImmediate(B_ADDRESS, ToS::NORMAL, datagram);
    //Serial.println("MSG_SENT");
    interval = millis();
    isSent = 0;
		if(retval == 0x01){
		  PRINTLN("bruh")
		}
		else if(retval == 0x00){
			PRINTLN("MSG_SENT")
		}
	}
	
	nd->run();
}
