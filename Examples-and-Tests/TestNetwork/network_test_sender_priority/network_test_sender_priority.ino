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

Datagram* low_pr;
Datagram* med_pr;
Datagram* hi_pr;

void XORBan(){
  Serial.println("XORBAN");
  if(nd->remove_ban(B_ADDRESS)){
    nd->ban_address(B_ADDRESS);
  }
}

void setup() {
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
  
  char* lowbuffer = (char*)malloc(32);
  strcpy(lowbuffer, "Low Priority_");
  Serial.print("MSG: ");Serial.println(lowbuffer);
  Serial.print("STRLEN ");Serial.println(strlen(lowbuffer));

  char* medbuffer = (char*)malloc(32);
  strcpy(medbuffer, "Med Priority");
  Serial.print("MSG: ");Serial.println(medbuffer);
  Serial.print("STRLEN ");Serial.println(strlen(medbuffer));

  char* hibuffer = (char*)malloc(32);
  strcpy(hibuffer, "Hi Priority");
  Serial.print("MSG: ");Serial.println(hibuffer);
  Serial.print("STRLEN ");Serial.println(strlen(hibuffer));
  
  low_pr = CreateDatagram(0, 50, lowbuffer, strlen(lowbuffer));
  med_pr = CreateDatagram(0, 50, medbuffer, strlen(medbuffer));
  hi_pr = CreateDatagram(0, 50, hibuffer, strlen(hibuffer));
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
    uint8_t retval_0 = nd->SendDatagram(B_ADDRESS, ToS::LOW_PR, low_pr);
    uint8_t retval_1 = nd->SendDatagram(B_ADDRESS, ToS::MEDIUM_PR, med_pr);
    uint8_t retval_2 = nd->SendDatagram(B_ADDRESS, ToS::HIGH_PR, hi_pr);
    interval = millis();
    isSent = 0;
    if(retval_0 == 0x01 || retval_1 == 0x01 || retval_2 == 0x01){
      PRINTLN("ERROR");
    }
    if(retval_0 == 0x00){PRINTLN("MSG_SENT: LOW")}
    if(retval_1 == 0x00){PRINTLN("MSG_SENT: MED")}
    if(retval_2 == 0x00){PRINTLN("MSG_SENT: HIGH")}
  }
  
  nd->run();
}
