#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "pinchange.h"


void sleepNow(void);
void wakeup();
bool run_remote();
void blink(int loops, int loop_time, bool half);

#define BODS 7      // BOD Sleep bit in MCUCR
#define BODSE 2     // BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

#define SERIAL_PRINT  1
#define PRESS_ACTIVE  1
#define PRESS_TOGGLE  2
#define PRESS_MODE    3
#define MODE_CHANGE_DURATION 2500 // ms

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
const int ce_pin           = 3;
const int csn_pin          = 2;
const int led_pin          = 9;
const uint8_t button_pins[] = { 7,8,8,10 };
#else
const int rx_pin           = 0;
const int tx_pin           = 1;
const int ce_pin           = 9;
const int csn_pin          = 10;
const int led_pin          = 4;
const uint8_t button_pins[] = { 2,2,2,3 };
#endif

const uint8_t num_button_pins = sizeof(button_pins);
uint8_t button_states[num_button_pins];
uint8_t button_presses[num_button_pins];
unsigned long button_timestamps[num_button_pins];
volatile int awakems = 0;
const uint64_t pipe = 1;
RF24 radio(ce_pin, csn_pin);
extern "C" void __cxa_pure_virtual() {}

void setup() {
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
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    blink(3, 200, true);
}

void loop() {
    bool button_on = run_remote();
    if (!button_on) {
        sleepNow();
    }
}

bool run_remote() {
    // Read the current state of buttons, and test if the current state
    // is different from the last state we sent
    int i = num_button_pins;
    bool button_on = false;
    unsigned long button_offset = 0;
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
#ifdef SERIAL_PRINT
            Serial.print("\nStarting active: ");
            Serial.print((int)i+1);
            Serial.println();
#endif
        } else if (state && !button_different && button_timestamps[i]) {
            // Check if has hit new mode
            button_offset = millis() - button_timestamps[i];

            if (button_offset >= MODE_CHANGE_DURATION) {
#ifdef SERIAL_PRINT
                Serial.print("\nMode by default: ");
                Serial.print((int)i+1);
                Serial.print(" -- ");
                Serial.print((unsigned long)button_offset);
                Serial.println();
#endif
                button_presses[i] = PRESS_MODE;
                button_timestamps[i] = 0;
                different = true;
            }
        } else if (!state && button_different) {
            // Button was pressed, now released
#ifdef SERIAL_PRINT
            Serial.print("\nEnding active: ");
            Serial.print((int)i+1);
            Serial.print(" -- ");
            Serial.print((unsigned long)button_offset);
            Serial.println();
#endif

            // Debounce pressed button, seeing if toggle or new mode
            // if (button_offset > 10) {
                button_presses[i] = PRESS_TOGGLE;
                button_timestamps[i] = 0;
//             } else {
// #ifdef SERIAL_PRINT
//                 Serial.print("\nDebounced, ignored: ");
//                 Serial.print((int)i+1);
//                 Serial.print(" -- ");
//                 Serial.print((unsigned long)button_offset);
// #endif
//             }
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
        int send_tries = 3;
        while (send_tries--) {
            bool ok = radio.write(button_presses, num_button_pins);
            if (ok) {
#ifdef SERIAL_PRINT
                Serial.print("ok");
                Serial.println();
                blink(5, 25, true);
#endif
                break;
            } else {
#ifdef SERIAL_PRINT
                blink(3, 20, false);
                Serial.print(".");
#endif
            }
        }
        if (!send_tries) {
#ifdef SERIAL_PRINT
            blink(3, 300, true);
            Serial.print("failed.\n");
#endif
        }
        // digitalWrite(led_pin, button_on ? HIGH : LOW);
        different = false;
    }

    return button_on;
}

void sleepNow(void)
{
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
    delay(15);
}

void wakeup() {
    awakems = 0;
}

void blink(int loops, int loop_time, bool half) {
    return;
    while (loops--) {
        digitalWrite(led_pin, HIGH);
        delay(loop_time);
        digitalWrite(led_pin, LOW);
        delay(loop_time / (half ? 2 : 1));
    }
}
