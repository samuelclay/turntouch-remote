#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SPI.h>
#include <new.h>
#include <pinchange.h>
#include <RH_RF22.h>
#include <RHReliableDatagram.h>

void sleepNow(void);
void wakeup();
bool run_remote();
void blink(int loops, int loop_time, bool half);
bool detectButtonChange(bool *button_on);

// #define BODS 7      // BOD Sleep bit in MCUCR
// #define BODSE 2     // BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

#define SERIAL_PRINT  1
#define PRESS_ACTIVE  1
#define PRESS_TOGGLE  2
#define PRESS_MODE    3
#define MODE_CHANGE_DURATION 750 // ms
#define BUTTON_DEBOUNCE_DURATION 30 // ms

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
const int ce_pin           = 2;
const int csn_pin          = 3;
const int led_pin          = 0;
const uint8_t button_pins[] = { 9,10,7,8 };
#else
const int ce_pin           = 9;
const int csn_pin          = 10;
const int led_pin          = 4;
const uint8_t button_pins[] = { 5,6,7,8 };
#endif

const uint8_t num_button_pins = 4;
unsigned long button_debounces[num_button_pins];
uint8_t button_states[num_button_pins];
uint8_t button_presses[num_button_pins];
unsigned long button_timestamps[num_button_pins];
volatile unsigned long awakeTime = 0;
const uint64_t pipe = 1;
RH_RF22 radioDriver;
RHReliableDatagram radio(radioDriver, CLIENT_ADDRESS);

void setup() {
    Serial.begin(115200);
    Serial.print("\n\nTurn Touch Remote\n\n");

    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);

    blink(2, 100, true);
    
    if (!radio.init())
        Serial.println(" ---> Radio client init failed!\n");

    radio.setTimeout(80);
    radioDriver.setModemConfig(RH_RF22::FSK_Rb125Fd125);
    
    int i = num_button_pins;
    while (i--) {
        pinMode(button_pins[i], INPUT_PULLUP);
        digitalWrite(button_pins[i], HIGH);
        button_timestamps[i] = 0;
        button_presses[i] = 0;
    }

    blink(3, 200, true);
}

void loop() {
    bool button_on = run_remote();

    if (!button_on) {
        if ((millis() - awakeTime) > 250) {
            awakeTime = millis();
            sleepNow();
        }
    }
}

bool run_remote() {
    bool button_on = false;
    bool different = detectButtonChange(&button_on);

    // Send the state of the buttons to the LED board
    if (different) {
        digitalWrite(led_pin, HIGH);
        
        awakeTime = millis();
#ifdef SERIAL_PRINT
        for (uint8_t i=0; i < sizeof(button_presses); i++) {
            Serial.print(button_presses[i], HEX);
        }
#endif
        uint8_t data = 0;
        for (uint8_t i=0; i < sizeof(button_presses); i++) {
            data |= (button_presses[i] << 2*i);
        }
        uint8_t len = sizeof(data);
        bool sent = radio.sendtoWait(&data, len, SERVER_ADDRESS);
        radio.waitPacketSent();
        uint8_t buf[num_button_pins];
        uint8_t from;
        
        if (!sent) { 
#ifdef SERIAL_PRINT
            Serial.println(" ... send failed!");
#endif
        } else if (radio.recvfromAckTimeout(buf, &len, 100, &from)) {
#ifdef SERIAL_PRINT
            Serial.print("\n --> Received from 0x");
            Serial.print(from, HEX);
            Serial.print(": ");
            for (uint8_t i=0; i < sizeof(buf); i++) {
                Serial.print(buf[i], HEX);
            }
            Serial.println();
#endif
        } else {
#ifdef SERIAL_PRINT
            Serial.print(" ... no reply!");
            Serial.println();
#endif
        }
        different = false;
        digitalWrite(led_pin, LOW);
    }

    return button_on;
}

bool detectButtonChange(bool *button_on) {
    // Read the current state of buttons, and test if the current state
    // is different from the last state we sent
    int i = num_button_pins;
    unsigned long button_offset = 0;
    bool different = false;
    
    while (i--) {
        bool button_different = false;
        uint8_t state = !digitalRead(button_pins[i]);

        if (state) *button_on = true;
        
        if (state && !button_debounces[i]) {
            awakeTime = millis();
            button_debounces[i] = millis();
        }
        if (state != button_states[i]) {
            if ((millis() - button_debounces[i]) > BUTTON_DEBOUNCE_DURATION) {
                different = true;
                button_different = true;
                button_states[i] = state;
                button_debounces[i] = 0;
                if (button_timestamps[i]) {
                    button_offset = millis() - button_timestamps[i];
                }
            }
        }

        if (state && button_different) {
            // Start pressing button
            button_timestamps[i] = millis();
            button_presses[i] = PRESS_ACTIVE;
#ifdef SERIAL_PRINT
            Serial.print(" --> Button #");
            Serial.print((int)i+1);
            Serial.print(" pressed: ");
#endif
        } else if (state && !button_different && button_timestamps[i]) {
            // Check if has hit new mode
            button_offset = millis() - button_timestamps[i];

            if (button_offset >= MODE_CHANGE_DURATION) {
#ifdef SERIAL_PRINT
                Serial.print(" --> Mode #");
                Serial.print((int)i+1);
                Serial.print(" by default in ");
                Serial.print((unsigned long)button_offset);
                Serial.print("ms -- ");
#endif
                button_presses[i] = PRESS_MODE;
                button_timestamps[i] = 0;
                different = true;
            }
        } else if (!state && button_different) {
            // Button was pressed, now released
#ifdef SERIAL_PRINT
            Serial.print(" --> Button #");
            Serial.print((int)i+1);
            Serial.print(" released: ");
            Serial.print((unsigned long)button_offset);
            Serial.print(" -- ");
#endif
            button_presses[i] = PRESS_TOGGLE;
            button_timestamps[i] = 0;
        } else if (button_presses[i]) {
            button_presses[i] = 0;
        }
    }
    
    return different;
}

void sleepNow(void)
{
    
    int i = num_button_pins;
    while (i--) {
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
        attachPcInterrupt(button_pins[i], wakeup, CHANGE);
#else
        PCattachInterrupt(button_pins[i], wakeup, CHANGE);
#endif
    }
    Serial.println(" ---> Sleeping...");
    radioDriver.sleep();
    delay(20);
    
    
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
    int p = num_button_pins;
    while (p--) {
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
        detachPcInterrupt(button_pins[p]);
#else
        PCdetachInterrupt(button_pins[p]);
#endif
    }
    sleep_disable();
    sei();                         //enable interrupts again (but INT0 is disabled from above)

    delay(15);
}

void wakeup() {
    awakeTime = millis();
}

void blink(int loops, int loop_time, bool half) {
    // return;
    while (loops--) {
        digitalWrite(led_pin, HIGH);
        delay(loop_time);
        digitalWrite(led_pin, LOW);
        delay(loop_time / (half ? 2 : 1));
    }
}
