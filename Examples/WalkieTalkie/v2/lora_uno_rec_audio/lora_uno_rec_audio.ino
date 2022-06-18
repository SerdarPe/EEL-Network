#define SAMPLE_RATE 9600
#define speakerPin 11
#define FREQUENCY_868
#include "Arduino.h"
#include "LoRa_E220.h"
#define BUF_SZ 0xBF  //0xBF = 191
#define BUF_SZ_REC 0xBF
#define BUF_SZ_RATIO ((BUF_SZ + 1) / (BUF_SZ_REC + 1))

LoRa_E220 e220ttl(17, 16, 8, 5, 4, UART_BPS_RATE_57600);
unsigned char const *audio = 0;
volatile uint16_t audioptr = 0;
volatile uint8_t ring[BUF_SZ + 1];
volatile uint8_t pblock = 0;
volatile uint8_t stop = 0;
volatile uint8_t ring_input = 0;

ISR(TIMER1_COMPA_vect) {
  /**********************************************************************************************************
   * Referenced code: https://web.archive.org/web/20080102075024/http://www.arduino.cc/playground/Code/PCMAudio
   * Title: PCMAudio
   * Author: Michael Smith
   * Date: 2008
  **********************************************************************************************************/
  if (stop) {
    stopPlayback();
  }
  else {
    OCR2A = audio[audioptr] & 0xFE;

    audioptr = (audioptr + 1);
    if (audioptr == BUF_SZ + 1) {
      audioptr = 0;
    }
  }
}

void startPlayback(unsigned char const *data){
  /**********************************************************************************************************
   * Referenced code: https://web.archive.org/web/20080102075024/http://www.arduino.cc/playground/Code/PCMAudio
   * Title: PCMAudio
   * Author: Michael Smith
   * Date: 2008
  **********************************************************************************************************/
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
  OCR1A = F_CPU / SAMPLE_RATE;
  TIMSK1 |= _BV(OCIE1A);

  sei();
}

void stopPlayback(){
  /**********************************************************************************************************
   * Referenced code: https://web.archive.org/web/20080102075024/http://www.arduino.cc/playground/Code/PCMAudio
   * Title: PCMAudio
   * Author: Michael Smith
   * Date: 2008
  **********************************************************************************************************/
  pblock = 0;
  TIMSK1 &= ~_BV(OCIE1A);
  TCCR1B &= ~_BV(CS10);
  TCCR2B &= ~_BV(CS10);

  digitalWrite(speakerPin, LOW);
  audioptr = 0;
}

void setup() {
  e220ttl.begin();
  startPlayback(ring);
}

void loop() {
  if (e220ttl.available()) {
    while(!digitalRead(8));
    e220ttl.readBytes(ring, BUF_SZ_REC + 1, false);
  }
}
