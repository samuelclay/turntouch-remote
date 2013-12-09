#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SPI.h>

#define SERIAL_PRINT 1

#include "nRF24L01.h"
#include "RF24.h"
#include "pinchange.h"

void sleepNow(void);
#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
const int rx_pin           = 0;
const int tx_pin           = 0;
const int ce_pin           = 3;
const int csn_pin          = 2;
const int led_pin          = 1; // 0
const int interrupt_pin    = 1; // 1
const uint8_t toggle_pins[] = { 7,8,9,10 };
#else
const int rx_pin           = 0;
const int tx_pin           = 1;
const int ce_pin           = 9;
const int csn_pin          = 10;
const int led_pin          = 4;
const int interrupt_pin    = 2;
const uint8_t toggle_pins[] = { 2,2,2,3 };
#endif 

const uint8_t num_toggle_pins = sizeof(toggle_pins);
uint8_t toggle_states[num_toggle_pins];

const uint64_t pipe = 0xF2F2F2F200LL;
RF24 radio(ce_pin, csn_pin);
#ifdef SERIAL_PRINT
//TinyDebugSerial mySerial = TinyDebugSerial(rx_pin, tx_pin);
#endif

int awakems = 0;
volatile bool different = false;

void setup() {
//    pinMode(rx_pin, INPUT);
//    pinMode(tx_pin, OUTPUT);
#ifdef SERIAL_PRINT
    Serial.begin(9600);
    Serial.print("\n\nTurn Touch Remote\n\n");
#endif
    radio.begin();
    radio.setChannel(38);
    radio.setDataRate(RF24_250KBPS);
    radio.setAutoAck(pipe, true);
//    radio.setRetries(100, 15);

    radio.openWritingPipe(pipe);
    radio.stopListening();

    int i = num_toggle_pins;
    while (i--) {
        pinMode(toggle_pins[i], INPUT_PULLUP);
        digitalWrite(toggle_pins[i], HIGH);
    }

    // Turn LED's ON until we start getting keys
    pinMode(interrupt_pin, INPUT);
    digitalWrite(interrupt_pin, HIGH);
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    int blinks = 2;
    int pause = 250;
    while (blinks--) {
        delay(pause);
        digitalWrite(led_pin, HIGH);
        delay(pause);
        digitalWrite(led_pin, LOW);
    }
}

void loop() {
    bool toggle_on = run_remote();
    if (!toggle_on) {
//        sleepNow();
    }
}

bool run_remote() {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_toggle_pins;
    bool toggle_on = false;
    while (i--) {
        uint8_t state = !digitalRead(toggle_pins[i]);
        if (state != toggle_states[i]) {
#ifdef SERIAL_PRINT
            Serial.print("Sensor state ");
            Serial.print((int)toggle_pins[i]);
            Serial.print(": ");
            Serial.print(state ? "ON" : "OFF");
            Serial.println();
#endif
            different = true;
            toggle_states[i] = state;
        }
        if (state) toggle_on = true;
    }

    // Send the state of the buttons to the LED board
    if (different) {      
        awakems = 0;
#ifdef SERIAL_PRINT        
        Serial.print("Now sending...");
#endif
        bool ok = radio.write(toggle_states, num_toggle_pins);
#ifdef SERIAL_PRINT
        if (ok) {
            Serial.print("ok\n");
        } else {
            Serial.print("failed\n");
        }
#endif
        digitalWrite(led_pin, toggle_on ? HIGH : LOW);
        different = false;
    }
    
    return toggle_on;
}

void sleepNow(void)
{
    digitalWrite(interrupt_pin, LOW);
    radio.powerDown();

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//    GIMSK |= _BV(INT0);                       //enable INT0
//    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
    int i = num_toggle_pins;
    while (i--) {
        attachPcInterrupt(toggle_pins[i], wakeup, CHANGE);
    }
#else
    attachInterrupt(0, wakeATMega, CHANGE);
#endif

    ACSR |= _BV(ACD);                         //disable the analog comparator
    ADCSRA &= ~_BV(ADEN);                     //disable ADC
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    
    //turn off the brown-out detector.
    //must have an ATtiny45 or ATtiny85 rev C or later for software to be able to disable the BOD.
    //current while sleeping will be <0.5uA if BOD is disabled, <25uA if not.
    cli();
    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
    sei();                         //ensure interrupts enabled so we can wake up again
    sleep_cpu();                   //go to sleep
    cli();                         //wake up here, disable interrupts
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//    GIMSK = 0x00;                  //disable INT0
    int p = num_toggle_pins;
    while (p--) {
        detachPcInterrupt(toggle_pins[p]);
    }
#endif
    sleep_disable();               
    sei();                         //enable interrupts again (but INT0 is disabled from above)
    
    radio.powerUp();
    digitalWrite(interrupt_pin, HIGH);
    delay(15);
}

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
void wakeup() {
    awakems = 0;
}
#else
void wakeATMega() {
    awakems = 0;
}
#endif

