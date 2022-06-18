
#define DESTINATION_ADDL 0x1F
#define DESTINATION_ADDH 0x00
#define CHANNEL 23
#define FREQUENCY_868
#include <HardwareSerial.h>
#include "Arduino.h"
#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#define BUF_SZ_O 0x0F
#define BUF_SZ_I 0x07
#define BUF_SZ 0xBF
#define SEND_SZ 0xBF
#define BUF_SZ_RATIO ((BUF_SZ + 1) / (SEND_SZ + 1))
#define ADC_INT_FREQ 9600
#define ADC_SAMPLING_RATE 2400
#define ADC_SAMPLE_MAX 240
#define SAMPLE_THROW_LIMIT 4
#define SOUND_PLOT

LoRa_E220 e220ttl(17, 16, 8, 5, 4, UART_BPS_RATE_57600);
//LoRa_E220 e220ttl(&Serial1, 5, 2, 3, UART_BPS_RATE_115200);
//SoftwareSerial ssl(10, 4);
volatile byte audiobuf[BUF_SZ + 1];

uint16_t sampleLimit = 0;
volatile uint8_t sampleThrow = 0;
volatile uint16_t ptr = 0;
volatile uint8_t temp = 0;

uint8_t ptr_send = 0;
volatile uint8_t rdy_send = 0;
ISR(ADC_vect) {

  if (sampleThrow == 1) {
    //temp = ADCH;
    //audiobuf[ptr] = ((((ADCL >> 2) | ADCH << 6) & 0xFF) << 1);
    audiobuf[ptr] = ADCH << 1;
    #ifdef SOUND_PLOT
    Serial.println((byte)audiobuf[ptr]);
    #endif
    
    if ((ptr + 1) % (SEND_SZ + 1) == 0) {
      rdy_send = 1;
    }

    ptr = (ptr + 1);
    if (ptr == BUF_SZ + 1) {
      ptr = 0;
    }
  }
  
  sampleThrow += 1;
  if (sampleThrow == SAMPLE_THROW_LIMIT) {
    sampleThrow = 0;
  }
}

void setUpADC() {
  ADMUX &= B11011111;
  ADMUX |= B00100000;

  ADMUX |= B01000000;

  ADMUX &= B11110000;

  ADMUX &= B11110000;  //A0

  ADCSRA |= B10000000;

  ADCSRA |= B00100000;

  ADCSRB &= B11111000;

  ADCSRA |= B00000111;

  ADCSRA |= B00001000;

  ADCSRA &= B11111111;	//Change PS

  sei();

  ADCSRA |= B01000000;
}
unsigned long tStart;
unsigned long tFrst;
unsigned long tEnd;
void setup() {
  // put your setup code here, to run once:
  #ifdef SOUND_PLOT
  Serial.begin(2000000); 
  Serial.print(0);Serial.println(" ");
  Serial.print(400);Serial.println(" ");
  #endif
  //tFrst = micros();
  //Serial.println(tFrst);
  //sampleLimit seems to be making the sounds bassy
  sampleLimit = (uint16_t)(ADC_SAMPLE_MAX * (float)(ADC_INT_FREQ - ADC_SAMPLING_RATE) / ADC_INT_FREQ) - 1;
  e220ttl.begin();
  setUpADC();
  //tStart = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  //e220ttl.sendFixedMessage(DESTINATION_ADDH, DESTINATION_ADDL, CHANNEL, (audiobuf + (ptr_send * (SEND_SZ + 1))), SEND_SZ + 1);
  if (rdy_send) {
    rdy_send = 0;
    e220ttl.sendDataNW(DESTINATION_ADDH, DESTINATION_ADDL, CHANNEL, (audiobuf + (ptr_send * (SEND_SZ + 1))), SEND_SZ + 1);
    ptr_send = (ptr_send + 1);
    if (ptr_send == BUF_SZ_RATIO) {
      ptr_send = 0;
    }
  }
}
