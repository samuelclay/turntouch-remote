#include <Arduino.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SPI.h>

#define SERIAL_PRINT 1

#define PRESS_ACTIVE  1
#define PRESS_TOGGLE  2
#define PRESS_MODE    3

#include "nRF24L01.h"
#include "RF24.h"
#include "pinchange.h"

void sleepNow(void);
void wakeup();
bool run_remote();

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
const uint8_t button_pins[] = { 7,8,9,10 };
#else
const int rx_pin           = 0;
const int tx_pin           = 1;
const int ce_pin           = 9;
const int csn_pin          = 10;
const int led_pin          = 4;
const int interrupt_pin    = 2;
const uint8_t button_pins[] = { 2,2,2,3 };
#endif 

const uint8_t num_button_pins = sizeof(button_pins);
uint8_t button_states[num_button_pins];
uint8_t button_presses[num_button_pins];
unsigned long button_timestamps[num_button_pins];

const uint64_t pipe = 0xF2F2F2F200LL;
RF24 radio(ce_pin, csn_pin);

int awakems = 0;

extern "C" void __cxa_pure_virtual() {}  

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
    radio.setRetries(1, 0);

    radio.openWritingPipe(pipe);
    radio.stopListening();

    int i = num_button_pins;
    while (i--) {
        pinMode(button_pins[i], INPUT_PULLUP);
        digitalWrite(button_pins[i], HIGH);
        button_timestamps[i] = 0;
        button_presses[i] = 0;
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
    bool button_on = run_remote();
    if (!button_on) {
//        if (awakems && (millis() - awakems) > 50) {
            sleepNow();
//        } else if (awakems == 0) {
//            awakems = millis();
//        } else {
//            awakems++;
//        }
//    } else {
//        awakems = 0;
    }
}

bool run_remote() {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_button_pins;
    bool button_on = false;
    unsigned long button_offset;
    bool different = false;
    while (i--) {
        bool button_different = false;
        uint8_t state = !digitalRead(button_pins[i]);
        if (state != button_states[i]) {
#ifdef SERIAL_PRINT
            Serial.print("Sensor state #");
            Serial.print((int)i+1);
            Serial.print(": ");
            Serial.print(state ? "ON  " : "OFF ");
#endif
            different = true;
            button_different = true;
            button_states[i] = state;
            if (button_timestamps[i]) {
                button_offset = millis() - button_timestamps[i];
            }
        }

        if (state) button_on = true;
        
        if (state && button_different) {
            // Start pressing button
            button_timestamps[i] = millis();
            button_presses[i] = PRESS_ACTIVE;
              Serial.print("\nStarting active: ");
              Serial.print((int)i+1);
              Serial.println();
        } else if (state && !button_different && button_timestamps[i]) {
            // Check if has hit new mode
            button_offset = millis() - button_timestamps[i];

            if (button_offset >= 500) {
              Serial.print("\nMode by default: ");
              Serial.print((int)i+1);
              Serial.print(" -- ");
              Serial.print((unsigned long)button_offset);
              Serial.println();
                button_presses[i] = PRESS_MODE;
                button_timestamps[i] = 0;
                different = true;
            }
        } else if (!state && button_different) {
              Serial.print("\nEnding active: ");
              Serial.print((int)i+1);
              Serial.print(" -- ");
              Serial.print((unsigned long)button_offset);
              Serial.println();

            // Debounce pressed button, seeing if toggle or new mode
            if (button_offset > 10) {
                button_presses[i] = PRESS_TOGGLE;
            } else {
                Serial.print("\nDebounced, ignored: ");
                Serial.print((int)i+1);
                Serial.print(" -- ");
                Serial.print((unsigned long)button_offset);                
            }
            button_timestamps[i] = 0;
        } else if (button_presses[i]) {
            button_presses[i] = 0;
        }
        
    }

    // Send the state of the buttons to the LED board
    if (different) {      
        awakems = 0;
#ifdef SERIAL_PRINT        
        Serial.print("...");
#endif
        int send_tries = 30;
        while (send_tries--) {
            digitalWrite(led_pin, HIGH);
            bool ok = radio.write(button_presses, num_button_pins);
            digitalWrite(led_pin, LOW);
            if (ok) {
#ifdef SERIAL_PRINT
                Serial.print("ok");
                Serial.println();
#endif
                break;
            } else {
#ifdef SERIAL_PRINT
                Serial.print(".");
#endif
            }
        }
        if (!send_tries) {
            Serial.print("failed.\n");
        }
        digitalWrite(led_pin, button_on ? HIGH : LOW);
        different = false;
    }
    
    return button_on;
}

void sleepNow(void)
{
    digitalWrite(interrupt_pin, LOW);
    radio.powerDown();

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//    GIMSK |= _BV(INT0);                       //enable INT0
//    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
    int i = num_button_pins;
    while (i--) {
        attachPcInterrupt(button_pins[i], wakeup, CHANGE);
    }
#else
    attachInterrupt(0, wakeup, CHANGE);
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
    int p = num_button_pins;
    while (p--) {
        detachPcInterrupt(button_pins[p]);
    }
#endif
    sleep_disable();               
//    sei();                         //enable interrupts again (but INT0 is disabled from above)
    
    radio.powerUp();
    digitalWrite(interrupt_pin, HIGH);
    delay(15);
}

void wakeup() {
    awakems = 0;
}
