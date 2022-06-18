#define EEL_DEBUG

#include "EEL_Driver.h"
#include <string.h>

#undef MY_ADDRESS
#undef MY_CHANNEL

#define MY_ADDRESS (uint16_t)0x001F
#define MY_CHANNEL (uint8_t)23
#define MY_PORT 42

#define SENDER_ADDRESS (uint16_t)0x003F
#define SENDER_CHANNEL (uint8_t)23

#define SAMPLE_RATE 300
#define speakerPin 11
#define BUF_SZ 88
uint8_t pblock = 0;
volatile uint8_t no_data = 1;
volatile uint16_t audioptr = 0;
unsigned char const *audio = 0;
volatile uint8_t ring[BUF_SZ];

EEL_Driver* nd;

ISR(TIMER1_COMPA_vect) {
  if(no_data){
    //OCR2A = 0;
  }
    OCR2A = audio[audioptr++] & 0xFE;
    
    if (audioptr == BUF_SZ) {
		  no_data = 1;
		  audioptr = 0;
    }
}

void startPlayback(unsigned char const *data){
  if (pblock) {
    return;
  }
  pblock = 1;

  pinMode(speakerPin, OUTPUT);
  digitalWrite(speakerPin, LOW);

  audio = data;

  cli();

  ASSR &= ~(_BV(EXCLK) | _BV(AS2));

  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);

  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));

  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  OCR2A = 0;

  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000

  TIMSK1 |= _BV(OCIE1A);
  
  sei();
}

void setup() {
  // put your setup code here, to run once
  Serial.begin(2000000);delay(50);
  //Serial.println("HELLO");
  
  startPlayback(ring);
  
  //LoRa_E220* e220 = new LoRa_E220(69, 68, 24, 23, 22, UART_BPS_RATE_57600);
  LoRa_E220* e220 = new LoRa_E220(17, 16, 7, 5, 4, UART_BPS_RATE_57600);
  nd = new EEL_Driver(MY_ADDRESS, MY_CHANNEL, new EEL_Device(e220));
  
  int8_t ban_response = nd->ban_address(SENDER_ADDRESS);
  
  int8_t port_response = nd->set_port(MY_PORT, 255);
  
  pinMode(A5, OUTPUT);
  digitalWrite(A5, LOW);
}


void loop() {
  
	nd->run();
	if(no_data){
		if(nd->read_port(MY_PORT, NULL, NULL, ring, BUF_SZ) != NULL){
      PORTC |= _BV(PC5);
      no_data = 0;
      for(int i = 0; i < BUF_SZ; i++){
        Serial.print("Voice ");Serial.print((byte)ring[i]);Serial.println(",");
      }
    }
	}
	
}
