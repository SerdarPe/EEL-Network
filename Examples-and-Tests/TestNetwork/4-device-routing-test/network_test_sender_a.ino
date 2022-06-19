#define EEL_DEBUG

#include <EEL_Driver.h>
#include "LoRa_E220.h"

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x0003
#define MY_CHANNEL (uint8_t)23

#define B_ADDRESS (uint16_t)0x0004
#define B_CHANNEL (uint8_t)23

#define C_ADDRESS (uint16_t)0x0005
#define C_CHANNEL (uint8_t)23

#define D_ADDRESS (uint16_t)0x0006
#define D_CHANNEL (uint8_t)23

#define DEBUG_PRINTLN(x) {Serial.println(x);}

EEL_Driver* nd;
Datagram* datagram;

void setup() {
  Serial.begin(115200);
  Serial.println("A-HELLO");
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new EEL_Device(new LoRa_E220(69, 68, 24, 23, 22, UART_BPS_RATE_19200)));
  
  char* databuffer = (char*)malloc(6);
  strcpy(databuffer, "hello");
  Serial.print("MSG: ");Serial.println(databuffer);
  Serial.print("STRLEN ");Serial.println(strlen(databuffer));
  
  datagram = CreateDatagram(0, 50, databuffer, strlen(databuffer));
  nd->ban_address(C_ADDRESS);
  nd->ban_address(D_ADDRESS);
  
}

uint8_t isSent = 1;
unsigned long interval = 0;

void loop() {
  nd->run();
  if(millis() - interval > 5000 || isSent){
    uint8_t retval = nd->SendDatagram(D_ADDRESS, ToS::NORMAL, datagram);
    Serial.println("MSG_SENT");
    interval = millis();
    isSent = 0;
    if(retval != 0x00){
      Serial.print("error code: ");Serial.println(retval);
      if(retval == 0x01){
        DEBUG_PRINTLN("ERROR");
      }
    }
  }
}
