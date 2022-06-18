#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x001F
#define MY_CHANNEL (uint8_t)23
#define MY_PORT 50

#define SENDER_ADDRESS (uint16_t)0x003F
#define SENDER_CHANNEL (uint8_t)23

#define STOP_TIME (uint32_t)300000

uint8_t *HeapPointer, *StackPointer;

EEL_Driver* nd;
char* msg_buf = NULL;

void XORBan(){
  Serial.println("XORBAN");
  if(nd->remove_ban(SENDER_ADDRESS)){
    nd->ban_address(SENDER_ADDRESS);
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
  
  LoRa_E220* e220 = new LoRa_E220(69, 68, 24, 23, 22, UART_BPS_RATE_19200);
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, e220);
  
  int8_t port_response = nd->set_port(MY_PORT, 128);
  Serial.print("SET_PORT_RETURN_VALUE: ");Serial.println(port_response);
  //int8_t ban_response = nd->ban_address(SENDER_ADDRESS);
  //Serial.print("BAN_ADDRESS_RETURN_VALUE: ");Serial.println(ban_response);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), XORBan, FALLING);

  msg_buf = (char*)malloc(128);
}

volatile uint32_t _stop = millis();

void loop() {
  while(millis() > _stop + STOP_TIME);
  
  nd->run();
  uint8_t msg_length = 0;
  bool port_written = nd->is_port_written(MY_PORT);
  if(nd->read_port(MY_PORT, &msg_length, NULL, msg_buf, 128) != NULL){
    *(msg_buf + msg_length) = '\0';
    Serial.print("msg:");
    Serial.println(msg_buf);
  }

}
