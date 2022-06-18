#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x001F
#define MY_CHANNEL (uint8_t)22
#define MY_PORT 50



EEL_Driver* nd;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(2000000);
  Serial.println("HELLO");
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, 13, 4, 5, 2, 3, UART_BPS_RATE_115200);
  nd->set_port(MY_PORT, 64);
}

void loop() {
  // put your main code here, to run repeatedly:
  nd->run();
  char* msg = nd->read_port(MY_PORT, NULL, NULL);
  //if(strcmp(msg, "Hello Network!") == 0)
  Serial.print("msg:");
  Serial.println(msg);
}
