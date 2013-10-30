#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

RF24 radio(9, 10);

const int role_pin         = 2;
const int toggle_north_pin = 3;
const int toggle_east_pin  = 4;
const int toggle_south_pin = 5;
const int toggle_west_pin  = 6;
const int led_pin          = 7;
const uint64_t pipe = 0xF8E8F0F000LL;

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

uint8_t led_state;
uint8_t sensor_state;

void setup(void) {
    // set up the role pin
    pinMode(role_pin, INPUT);
    digitalWrite(role_pin, HIGH);
    delay(20); // Just to get a solid reading on the role pin

    // read the address pin, establish our role
    role = digitalRead(role_pin) ? role_remote : role_receiver;

    Serial.begin(57600);
    printf_begin();
    Serial.print(sprintf("ROLE: %s\n\r",role_friendly_name[role]));

    radio.begin();

    if (role == role_remote) {
        radio.openWritingPipe(pipe);
    } else {
        radio.openReadingPipe(1,pipe);
    }

    if (role == role_receiver) {
        radio.startListening();
    }

    radio.printDetails();

    if (role == role_remote) {
        pinMode(toggle_north_pin, INPUT);
        digitalWrite(toggle_north_pin, HIGH);
        pinMode(toggle_east_pin, INPUT);
        digitalWrite(toggle_east_pin, HIGH);
        pinMode(toggle_south_pin, INPUT);
        digitalWrite(toggle_south_pin, HIGH);
        pinMode(toggle_west_pin, INPUT);
        digitalWrite(toggle_west_pin, HIGH);
    }

    // Turn LED's ON until we start getting keys
    pinMode(led_pin,OUTPUT);
    led_state = LOW;
    digitalWrite(led_pin, led_state);
    int i = role == role_receiver     ? 4  : 2;
    int pause = role == role_receiver ? 50 : 400;
    while (i--) {
        delay(pause);
        digitalWrite(led_pin, HIGH);
        delay(pause);
        digitalWrite(led_pin, LOW);
    }
}

void loop(void) {
    if (role == role_remote) {
        run_remote();
        delay(100);
    }

    if (role == role_receiver) {
        run_receiver();
        run_serial_receiver();
    }
}

void run_remote() {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    bool different = false;
    uint8_t state = ! digitalRead(toggle_north_pin);
    printf("Sensor state: %d\n", state);
    if (state != sensor_state) {
        different = true;
        sensor_state = state;
    }

    // Send the state of the buttons to the LED board
    if (different) {
        printf("Now sending...");
        radio.stopListening();
        bool ok = radio.write( &sensor_state, sizeof(uint8_t) );
        if (ok) {
            printf("ok\n\r");
        } else {
            printf("failed\n\r");
        }
        radio.startListening();
        led_state = sensor_state;
        digitalWrite(led_pin, led_state);
    }
}

void run_receiver() {
    if (radio.available()) {
        // Dump the payloads until we've gotten everything
        bool done = false;
        while (!done) {
            done = radio.read( &sensor_state, sizeof(uint8_t) );
            printf("Got buttons\n\r");
            led_state = sensor_state;
            digitalWrite(led_pin, led_state);
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
