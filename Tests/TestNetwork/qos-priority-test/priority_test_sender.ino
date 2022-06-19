#define EEL_DEBUG

#include <EEL_Driver.h>
#include "LoRa_E220.h"

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x0003
#define MY_CHANNEL (uint8_t)23

#define B_ADDRESS (uint16_t)0x0004
#define B_CHANNEL (uint8_t)23

EEL_Driver* nd;

Datagram* low_pr;
Datagram* med_pr;
Datagram* hi_pr;

void setup() {
  Serial.begin(2000000);
  delay(1000);
  
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new LoRa_E220(69, 68, 24, 23, 22, UART_BPS_RATE_19200));
  
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
}

uint8_t isSent = 1;
unsigned long interval = 0;

void loop() {
  
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
