#define SAMPLE_RATE 2000
#define speakerPin 10
#define FREQUENCY_868
#include "Arduino.h"
#include "LoRa_E220.h"
//#include <SoftwareSerial.h>
#define BUF_SZ 0x0BF
#define BUF_SZ_REC 0xBF
#define BUF_SZ_RATIO ((BUF_SZ + 1) / (BUF_SZ_REC + 1))
#define SOUND_PLOT
LoRa_E220 e220ttl(17, 16, 8, 5, 4, UART_BPS_RATE_57600);
//SoftwareSerial ssl(5, 4);
unsigned char const *audio = 0;
volatile uint16_t audioptr = 0;
volatile uint8_t ring[BUF_SZ + 1];
volatile uint8_t pblock = 0;
volatile uint8_t stop = 0;
volatile uint8_t ring_input = 0;
volatile byte temp = 0;

ISR(TIMER1_COMPA_vect) {
  if (stop) {
    stopPlayback();
  }
  else {
    OCR2A = audio[audioptr] & 0xFE;
    #ifdef SOUND_PLOT
    Serial.println((byte)(audio[audioptr] & 0xFE));
    #endif
    audioptr = (audioptr + 1);
    if (audioptr == BUF_SZ + 1) {
      audioptr = 0;
    }
  }
}

void startPlayback(unsigned char const *data)
{
  if (pblock) {
    return;
  }
  pblock = 1;

  pinMode(speakerPin, OUTPUT);
  digitalWrite(speakerPin, LOW);

  audio = data;

  cli();

  // Set up Timer 2 to do pulse width modulation on the speakerpin.
  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));

  // Set fast PWM mode  (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);

  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.(10 for MEGA)
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));

  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set initial pulse width to the first sample.
  //OCR2A = sounddata_data[0]; //data[0];
  OCR2A = 0;

  // Set up Timer 1 to send a sample every interrupt.
  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000

  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);

  sei();
}

void stopPlayback()
{
  pblock = 0;
  // Disable playback per-sample interrupt.
  TIMSK1 &= ~_BV(OCIE1A);

  // Disable the per-sample timer completely.
  TCCR1B &= ~_BV(CS10);

  // Disable the PWM timer.
  TCCR2B &= ~_BV(CS10);

  digitalWrite(speakerPin, LOW);
  audioptr = 0;
}

void setup() {
  // put your setup code here, to run once:
  #ifdef SOUND_PLOT
  Serial.begin(2000000);
  #endif
  //pinMode(13, OUTPUT);
  //pinMode(12, OUTPUT);
  e220ttl.begin();
  //v.arr = (unsigned char*)malloc(BUF_SZ + 1);
  startPlayback(ring);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (e220ttl.available()) {
    e220ttl.readBytes(ring, BUF_SZ_REC + 1);
  }
}
