#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x0004
#define MY_CHANNEL (uint8_t)23
#define MY_PORT 50

EEL_Driver* nd;

void setup() {
  Serial.begin(2000000);
  delay(1000);
  
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new LoRa_E220(69, 68, 24, 23, 22, UART_BPS_RATE_19200));
  int8_t port_response = nd->set_port(MY_PORT, 128);
}

void loop() {
  nd->run();
  uint8_t msg_length = 0;
  int8_t port_status = nd->is_port_written(MY_PORT);
  char* msg = nd->read_port(MY_PORT, &msg_length, NULL);
  if(port_status == 0x02){
    *(msg + msg_length) = '\0';
    Serial.print("msg:");
    Serial.println(msg);
  }

}
