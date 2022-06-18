
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
  // clear ADLAR in ADMUX (0x7C) to right-adjust the result
  // ADCL will contain lower 8 bits, ADCH upper 2 (in last two bits)
  ADMUX &= B11011111;
  ADMUX |= B00100000;

  // Set REFS1..0 in ADMUX (0x7C) to change reference voltage to the
  // proper source (01)
  ADMUX |= B01000000;

  // Clear MUX3..0 in ADMUX (0x7C) in preparation for setting the analog
  // input
  ADMUX &= B11110000;

  // Set MUX3..0 in ADMUX (0x7C) to read from AD8 (Internal temp)
  // Do not set above 15! You will overrun other parts of ADMUX. A full
  // list of possible inputs is available in Table 24-4 of the ATMega328
  // datasheet
  ADMUX &= B11110000;  //A0
  // ADMUX |= B00001000; // Binary equivalent

  // Set ADEN in ADCSRA (0x7A) to enable the ADC.
  // Note, this instruction takes 12 ADC clocks to execute
  ADCSRA |= B10000000;

  // Set ADATE in ADCSRA (0x7A) to enable auto-triggering.
  ADCSRA |= B00100000;

  // Clear ADTS2..0 in ADCSRB (0x7B) to set trigger mode to free running.
  // This means that as soon as an ADC has finished, the next will be
  // immediately started.
  ADCSRB &= B11111000;

  // Set the Prescaler to 128 (16000KHz/128 = 125KHz)
  // Above 200KHz 10-bit results are not reliable.
  ADCSRA |= B00000111;

  // Set ADIE in ADCSRA (0x7A) to enable the ADC interrupt.
  // Without this, the internal interrupt will not trigger.
  ADCSRA |= B00001000;

  // Set ADPS to 4 (16MHz / 32 = 500KHz)
  // About 38.4k sampling rate
  ADCSRA |= B00000111;
  ADCSRA &= B11111111;

  // Enable global interrupts
  // AVR macro included in <avr/interrupts.h>, which the Arduino IDE
  // supplies by default.
  sei();

  // Kick off the first ADC
  // Set ADSC in ADCSRA (0x7A) to start the ADC conversion
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
