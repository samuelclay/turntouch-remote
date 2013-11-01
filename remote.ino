#include <SPI.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"


RF24 radio(9, 10);

const int led_pin          = 7;
const int role_pin         = 8;
const int interrupt_pin    = 2;
const uint8_t toggle_pins[] = { 3,4,5,6 };
const uint8_t num_toggle_pins = sizeof(toggle_pins);
uint8_t toggle_states[num_toggle_pins];

const uint64_t pipe = 0xF8E8F0F000LL;

int sleepCount = 0;
volatile bool different = false;

typedef enum { 
    role_remote = 1, 
    role_receiver = 2
} role_e;
role_e role;
const char* role_friendly_name[] = { 
    "invalid", 
    "Remote", 
    "Receiver" 
};

void setup(void) {
    // set up the role pin
    pinMode(role_pin, INPUT);
    digitalWrite(role_pin, HIGH);
    delay(20); // Just to get a solid reading on the role pin

    // read the address pin, establish our role
    role = digitalRead(role_pin) ? role_remote : role_receiver;
    digitalWrite(role_pin, LOW);
    
    Serial.begin(57600);
    printf_begin();
    Serial.print(sprintf("ROLE: %s\n",role_friendly_name[role]));

    radio.begin();

    if (role == role_remote) {
        radio.openWritingPipe(pipe);
    } else {
        radio.openReadingPipe(1,pipe);
    }

    if (role == role_receiver) {
        radio.startListening();
    }

//    radio.printDetails();

    if (role == role_remote) {
        int i = num_toggle_pins;
        while (i--) {
            pinMode(toggle_pins[i], INPUT);
            digitalWrite(toggle_pins[i], HIGH);
        }
        pinMode(8, OUTPUT);
        digitalWrite(8, LOW);
    }

    // Turn LED's ON until we start getting keys
    pinMode(interrupt_pin, INPUT);
    digitalWrite(interrupt_pin, HIGH);
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    int i = role == role_receiver     ? 5  : 3;
    int pause = role == role_receiver ? 50 : 250;
    while (i--) {
        delay(pause);
        digitalWrite(led_pin, HIGH);
        delay(pause);
        digitalWrite(led_pin, LOW);
    }
}

// Interrupt Service Routine attached to INT0 vector
ISR(EXT_INT0_vect) {
    Serial.print("\n WAKE UP (ISR) \n");
}

void wakeup() {
    Serial.print("\n WAKE UP \n");
    
    different = true;
}

void loop(void) {
    if (role == role_remote) {
        run_remote();
        delay(50);
//        if (sleepCount > 50) {
//          sleepCount = 0;
//          sleepNow();
//        } else {
//          sleepCount += 1;
//        }
    }

    if (role == role_receiver) {
        run_receiver();
        run_serial_receiver();
    }
}

void run_remote() {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_toggle_pins;
    bool toggle_on = false;
    while (i--) {
        uint8_t state = !digitalRead(toggle_pins[i]);
        if (state != toggle_states[i]) {
            printf("Sensor state %d: %d\n", toggle_pins[i], state);
            different = true;
            toggle_states[i] = state;
        }
        if (state) toggle_on = true;
    }

    // Send the state of the buttons to the LED board
    if (different) {      
        sleepCount = 0;
        printf("Now sending...");
        radio.stopListening();
        bool ok = radio.write(toggle_states, num_toggle_pins);
        if (ok) {
            printf("ok\n");
        } else {
            printf("failed\n");
        }
        radio.startListening();
        digitalWrite(led_pin, toggle_on ? HIGH : LOW);
        different = false;
    }
}

void run_receiver() {
    if (radio.available()) {
        // Dump the payloads until we've gotten everything
        bool done = false;
        while (!done) {
            done = radio.read( toggle_states, num_toggle_pins );
            bool toggle_on = false;
            Serial.print("\n:START:");
            for (int i=0; i < num_toggle_pins; i++) {
                printf("%d:", toggle_states[i]);
                if (toggle_states[i]) toggle_on = true;
            }
            Serial.print("END:");
            digitalWrite(led_pin, toggle_on ? HIGH : LOW);
        }
    }
}

void run_serial_receiver() {
    uint8_t incomingByte;
    // send data only when you receive data:
    if (Serial.available() > 0) {
        // read the incoming byte:
        incomingByte = Serial.read();
        
        // say what you got with both the ASCII and decimal representations
        Serial.print("I received: ");
        Serial.write(incomingByte);
        Serial.print(" : ");
        Serial.println(incomingByte, DEC);
    }
}

void sleepNow()
{
    /* Now is the time to set the sleep mode. In the Atmega8 datasheet
     * http://www.atmel.com/dyn/resources/prod_documents/doc2486.pdf on page 35
     * there is a list of sleep modes which explains which clocks and 
     * wake up sources are available in which sleep modus.
     *
     * In the avr/sleep.h file, the call names of these sleep modus are to be found:
     *
     * The 5 different modes are:
     *     SLEEP_MODE_IDLE         -the least power savings 
     *     SLEEP_MODE_ADC
     *     SLEEP_MODE_PWR_SAVE
     *     SLEEP_MODE_STANDBY
     *     SLEEP_MODE_PWR_DOWN     -the most power savings
     *
     *  the power reduction management <avr/power.h>  is described in 
     *  http://www.nongnu.org/avr-libc/user-manual/group__avr__power.html
     */  
  
  Serial.print("\nBeginning sleep.");
  
  digitalWrite(8, HIGH);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here

  sleep_enable();          // enables the sleep bit in the mcucr register
                             // so sleep is possible. just a safety pin 
  
  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_twi_disable();
  
  attachInterrupt(0, wakeup, CHANGE);
    pinMode(interrupt_pin, INPUT);
    digitalWrite(interrupt_pin, LOW);    // Enable pullup resistor
    sei();                    // Enable global interrupts
    EIMSK |= (1 << INT0);     // Enable external interrupt INT0
    EICRA |= (1 << ISC01);    // Trigger INT0 on falling edge

  Serial.print("\nSleeping...");
  sleep_mode();            // here the device is actually put to sleep!!
 
  Serial.print("\nWake up from sleep...");
                             // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
  sleep_disable();         // first thing after waking from sleep:
                            // disable sleep...
  detachInterrupt(interrupt_pin); 
  
  digitalWrite(8, LOW);
  power_all_enable();
}

