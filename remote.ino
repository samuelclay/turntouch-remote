//#include <SoftwareSerial.h>
#include <avr/power.h>
#include <avr/sleep.h>
//#include <avr/interrupt.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

void sleepNow(void);
#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
const int rx_pin           = 2;
const int tx_pin           = 2;
const int ce_pin           = 3;
const int csn_pin          = 7;
const int led_pin          = 0;
const int interrupt_pin    = 2;
const uint8_t toggle_pins[] = { 1,8,9,10 };
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
//SoftwareSerial mySerial(rx_pin, tx_pin);

int awakems = 0;
volatile bool different = false;

void setup(void) {
    pinMode(rx_pin, INPUT);
    pinMode(tx_pin, OUTPUT);
//    mySerial.begin(4800);
//    printf_begin();
//    mySerial.print("\n\nTurn Touch Remote\n\n");

    radio.begin();
    radio.setChannel(38);
    radio.setDataRate(RF24_250KBPS);
    radio.setAutoAck(pipe, true);
    radio.setRetries(1, 1);

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
    int blinks = 3;
    int pause = 250;
    while (blinks--) {
        delay(pause);
        digitalWrite(led_pin, HIGH);
        delay(pause);
        digitalWrite(led_pin, LOW);
    }
}
//
//void wakeup() {
//    mySerial.print("\n WAKE UP \n");
//    
//    different = true;
//}

void loop(void) {
    run_remote();
    delay(10);
//        if (awakems > 50) {
//          awakems = 0;
//          sleepNow();
//        } else {
//          awakems += 1;
//        }
}

void run_remote() {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_toggle_pins;
    bool toggle_on = false;
    while (i--) {
        uint8_t state = !digitalRead(toggle_pins[i]);
        if (state != toggle_states[i]) {
//            mySerial.print("Sensor state ");
//            mySerial.print((int)toggle_pins[i]);
//            mySerial.print(": ");
//            mySerial.print(state ? "ON" : "OFF");
//            mySerial.println();
            different = true;
            toggle_states[i] = state;
        }
        if (state) toggle_on = true;
    }

    // Send the state of the buttons to the LED board
    if (different) {      
        awakems = 0;
//        mySerial.print("Now sending...");
        radio.powerUp();
        radio.stopListening();
        bool ok = radio.write(toggle_states, num_toggle_pins);
        if (ok) {
//            mySerial.print("ok\n");
        } else {
//            mySerial.print("failed\n");
        }
        radio.startListening();
        digitalWrite(led_pin, toggle_on ? HIGH : LOW);
        different = false;
    }
}

//void sleepNow(void)
//{
//    digitalWrite(interrupt_pin, LOW);
//    if (role == role_remote) {
//        radio.powerDown();
//    } else {
//        radio.stopListening();
//        radio.powerDown();
//    }
//    if (role == role_remote) {
//#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//        GIMSK |= _BV(INT0);                       //enable INT0
//        MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
//#else
//        attachInterrupt(0, wakeATMega, CHANGE);
//#endif
//    }
//    ACSR |= _BV(ACD);                         //disable the analog comparator
//    ADCSRA &= ~_BV(ADEN);                     //disable ADC
//    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//    sleep_enable();
//    
//    //turn off the brown-out detector.
//    //must have an ATtiny45 or ATtiny85 rev C or later for software to be able to disable the BOD.
//    //current while sleeping will be <0.5uA if BOD is disabled, <25uA if not.
//    cli();
//    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
//    mcucr2 = mcucr1 & ~_BV(BODSE);
//    MCUCR = mcucr1;
//    MCUCR = mcucr2;
//    sei();                         //ensure interrupts enabled so we can wake up again
//    sleep_cpu();                   //go to sleep
//    cli();                         //wake up here, disable interrupts
//    if (role == role_remote) {
//#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//        GIMSK = 0x00;                  //disable INT0
//#else
//
//#endif
//    }
//    sleep_disable();               
//    sei();                         //enable interrupts again (but INT0 is disabled from above)
//    if (role == role_receiver) {
//      radio.startListening();
//    }
//    digitalWrite(interrupt_pin, HIGH);
//    delay(5);
//}
//
//#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
//ISR(INT0_vect) {
//    awakems = 0;
//}
//ISR(PCINT18_vect) {
//    awakems = 0;
//}  
//#else
//void wakeATMega() {
//    awakems = 0;
//}
//#endif

