#define EEL_DEBUG

#include <EEL_Driver.h>
#include "LoRa_E220.h"

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x003F
#define MY_CHANNEL (uint8_t)23

#define B_ADDRESS (uint16_t)0x001F
#define B_CHANNEL (uint8_t)23

#define SAMPLE_THROW_LIMIT 32

#define V_BUF_SIZE 88
volatile byte audiobuf[V_BUF_SIZE];
volatile uint16_t audio_ptr = 0;
volatile uint8_t send_audio = 0;
volatile uint8_t sampleThrow = 0;

ISR(ADC_vect) {
  if(send_audio){
    return;
  }
  if (sampleThrow == 2) {
    audiobuf[audio_ptr++] = ADCH << 1;
    if (audio_ptr == V_BUF_SIZE) {
      send_audio = 1;
      audio_ptr = 0;
    }
  }
  
  if (++sampleThrow == SAMPLE_THROW_LIMIT) {
    sampleThrow = 0;
  }
}

void setup_ADC() {
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
  ADCSRA |= B00000111;
  ADCSRA &= B11111111;
  
  sei();
  
  ADCSRA |= B01000000;
}

EEL_Driver* nd;
Datagram* route;
Datagram* voice;
void setup() {
  Serial.begin(2000000);
  
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new EEL_Device(new LoRa_E220(17, 16, 7, 5, 4, UART_BPS_RATE_57600)));

  int8_t ban_response = nd->ban_address(B_ADDRESS);
  
  char* small_msg = (char*)malloc(8);
  route = CreateDatagram(0, 50, small_msg, 8);
}


void loop() {
  // Requesting route first
	uint8_t retval = nd->SendDatagram(B_ADDRESS, ToS::HIGH_PR, route);
	if(retval == 0x00){
    //Route is found
		setup_ADC();
		while(1){
			if(send_audio){
        voice = CreateDatagram(0, 42, audiobuf, V_BUF_SIZE);
        for(int i = 0; i < V_BUF_SIZE; i++){
          Serial.print("Voice ");Serial.print((byte)audiobuf[i]);Serial.println(",");
        }
				nd->SendDatagram(B_ADDRESS, ToS::HIGH_PR, voice);
				FreeDatagram(&voice);
				send_audio = 0;
			}
		}
	}
   	
	nd->run();
}
