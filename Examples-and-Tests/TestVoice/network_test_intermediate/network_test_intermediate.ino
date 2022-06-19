
#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x002F
#define MY_CHANNEL (uint8_t)23

#define STOP_TIME (uint32_t)900000

uint8_t *HeapPointer, *StackPointer;

EEL_Driver* nd;

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
  Serial.print("0x");Serial.println((uint16_t)NULL, HEX);
  
  LoRa_E220* e220 = new LoRa_E220(17, 16, 7, 5, 4, UART_BPS_RATE_57600);
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new EEL_Device(e220));

  int8_t port_response = nd->set_port(50, 255);
  Serial.print("SET_PORT_RETURN_VALUE: ");Serial.println(port_response);
}

volatile uint32_t _stop = millis();
char* msg_buf = (char*)malloc(256);
void loop() {
  while(millis() > _stop + STOP_TIME);
  nd->run();
}
