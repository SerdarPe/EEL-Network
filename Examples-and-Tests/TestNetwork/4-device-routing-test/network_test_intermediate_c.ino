#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x0005
#define MY_CHANNEL (uint8_t)23

#define A_ADDRESS (uint16_t)0x0003
#define A_CHANNEL (uint8_t)23

#define B_ADDRESS (uint16_t)0x0004
#define C_CHANNEL (uint8_t)23

#define D_ADDRESS (uint16_t)0x0006
#define D_CHANNEL (uint8_t)23
#define MY_PORT 50

EEL_Driver* nd;

void setup() {
  Serial.begin(115200);
  Serial.println("C-HELLO");
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new EEL_Device(new LoRa_E220(5, 4, 6, 2, 3, UART_BPS_RATE_19200)));
  nd->set_port(MY_PORT, 64);
  nd->ban_address(A_ADDRESS);

}

void loop() {
  nd->run();
  uint8_t msg_length = 0;
  int8_t port_status = nd->is_port_written(MY_PORT);
  char* msg = nd->read_port(MY_PORT, &msg_length, NULL);
  if (port_status > 0) {
    *(msg + msg_length) = '\0';
    Serial.print("msg: ");Serial.println(msg);
  }

}
