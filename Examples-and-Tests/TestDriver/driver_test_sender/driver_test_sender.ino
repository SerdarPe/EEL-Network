#define EEL_DEBUG

#include <EEL_Driver.h>
#include "LoRa_E220.h"

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x000F
#define MY_CHANNEL (uint8_t)22

EEL_Driver* nd;

void setup() {
  Serial.begin(2000000);
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, 5, 4, 6, 2, 3, UART_BPS_RATE_115200);
  char* databuffer = (char*)malloc(128);
  strcpy(databuffer, "My name is EEL");
  Serial.print("MSG: ");Serial.println(databuffer);
  Serial.print("STRLEN ");Serial.println(strlen(databuffer));
  Datagram* datagram = CreateDatagram(0, 50, databuffer, strlen(databuffer));
  nd->SendDatagram((uint16_t)0x001F, ToS::NORMAL, datagram);
}

void loop() {
  nd->run();
}
