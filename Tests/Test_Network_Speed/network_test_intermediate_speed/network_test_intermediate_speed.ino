#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x002F
#define MY_CHANNEL (uint8_t)23

#define STOP_TIME (uint32_t)300000

uint8_t *HeapPointer, *StackPointer;

EEL_Driver* nd;

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
  Serial.print("0x");Serial.println((uint16_t)NULL, HEX);
  
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new LoRa_E220(17, 16, 7, 5, 4, UART_BPS_RATE_19200));
}

volatile uint32_t _stop = millis();
uint8_t MetricsPrinted = 1;

void loop() {
   while(millis() > _stop + STOP_TIME){
    if(MetricsPrinted){
      MetricsPrinted = 0;
      PRINT("time spent receiving(ms): ")Serial.println(nd->receive_packet_total_time);
      PRINT("total bytes received: ")Serial.println(nd->receive_packet_total_bytes);
      PRINT("bps: ")Serial.println(1000 *((float)nd->receive_packet_total_bytes / nd->receive_packet_total_time));

      PRINT("time spent processing routing(ms): ")Serial.println((float)nd->aodv_total_time / 1000);
      PRINT("total routing messages: ")Serial.println(nd->aodv_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->aodv_total_time / 1000) / (float)nd->aodv_packet_count);

      PRINT("time spent processing udp(ms): ")Serial.println((float)nd->udp_total_time / 1000);
      PRINT("total udp messages: ")Serial.println(nd->udp_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->udp_total_time / 1000) / (float)nd->udp_packet_count);

      PRINT("time spent forwarding(ms): ")Serial.println((float)nd->forward_packet_total_time / 1000);
      PRINT("total forwarded messages: ")Serial.println(nd->forward_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->forward_packet_total_time / 1000) / (float)nd->forward_packet_count);

      PRINT("time spent processing packets(ms): ")Serial.println((float)nd->general_packet_total_time / 1000);
      PRINT("total processed messages: ")Serial.println(nd->general_packet_count);
      PRINT("average processing time(ms): ")Serial.println(((float)nd->general_packet_total_time / 1000) / (float)nd->general_packet_count);

     }
   }
   
   // put your main code here, to run repeatedly:
  nd->run();
}
