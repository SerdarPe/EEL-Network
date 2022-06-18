#define DESTINATION_ADDL 0x1F
#define DESTINATION_ADDH 0x00
#define CHANNEL 23
#define FREQUENCY_868
#include <HardwareSerial.h>
#include "Arduino.h"
#include "LoRa_E220.h"
#define BUF_SZ_O 0x0F
#define BUF_SZ_I 0x07
#define BUF_SZ (uint16_t)767
#define SEND_SZ (uint16_t)191
#define BUF_SZ_RATIO ((BUF_SZ + 1) / (SEND_SZ + 1))

LoRa_E220 e220ttl(17, 16, 8, 5, 4, UART_BPS_RATE_57600);
volatile byte audiobuf[BUF_SZ + 1];
volatile uint16_t ptr = 0;
uint16_t ring_n = 0;
volatile uint8_t rdy_send = 0;

ISR(ADC_vect) {
  if (1) {
    audiobuf[ptr++] = ADCH;
    
    if(ptr == 192 || ptr == 384 || ptr == 576 || ptr == 768){
      rdy_send = 1;
    }
    
    if (ptr == BUF_SZ + 1) {
      ptr = 0;
    }
  }
}

void setUpADC() {
  /**********************************************************************************************************
   * Referenced code: http://www.glennsweeney.com/tutorials/interrupt-driven-analog-conversion-with-an-atmega328p
   * Title: Interrupt-Driven Analog Conversion With an ATMega328p
   * Author: Glenn Sweeney
   * Date: 2012
  **********************************************************************************************************/
  ADMUX &= B11011111;
  ADMUX |= B00100000;
  ADMUX |= B01000000;
  ADMUX &= B11110000;
  ADMUX &= B11110000;
  ADCSRA |= B10000000;
  ADCSRA |= B00100000;
  ADCSRB &= B11111000;
  ADCSRA |= B00000111;
  ADCSRA |= B00001000;
  sei();
  ADCSRA |= B01000000;
}

uint32_t t_start;
uint32_t total_bytes = 0;

void setup() {
  //Serial.begin(2000000);
  pinMode(A5, OUTPUT);
  PORTC &= B11011111;
  e220ttl.begin();
  setUpADC();
  t_start = millis();
}

void loop() {
  if (rdy_send) {
    PORTC |= B00100000;
    rdy_send = 0;
    
    while(!digitalRead(8));
    e220ttl.sendDataNW(DESTINATION_ADDH, DESTINATION_ADDL, CHANNEL, (audiobuf + (ring_n * (SEND_SZ + 1))), SEND_SZ + 1);
    if (++ring_n == BUF_SZ_RATIO) {
      ring_n = 0;
    }
  }
  PORTC &= B11011111;
}
