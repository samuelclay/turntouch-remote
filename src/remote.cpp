#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <SPI.h>
#include <new.h>
#include <pinchange.h>
#include <RH_NRF24.h>

void sleepNow(void);
void wakeup();
bool run_remote();
void blink(int loops, int loop_time, bool half);

// #define BODS 7      // BOD Sleep bit in MCUCR
// #define BODSE 2     // BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

#define SERIAL_PRINT  1
#define PRESS_ACTIVE  1
#define PRESS_TOGGLE  2
#define PRESS_MODE    3
#define MODE_CHANGE_DURATION 750 // ms
#define BUTTON_DEBOUNCE_DURATION 25 // ms

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
volatile int awakems = 0;
const uint64_t pipe = 1;
RH_NRF24 radio(ce_pin, csn_pin);

void setup() {
    Serial.begin(115200);
    Serial.print("\n\nTurn Touch Remote\n\n");

    blink(2, 100, true);
    
    if (!radio.init())
        Serial.println("init failed");
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
    if (!radio.setChannel(92))
        Serial.println("setChannel failed");
    if (!radio.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPowerm18dBm))
        Serial.println("setRF failed"); 
    uint8_t address[] = {0xE7, 0xE7, 0xE7};
    if (!radio.setNetworkAddress(address, sizeof(address)))
        Serial.println("setNetworkAddress failed");
        
    uint8_t retryDelay = 0x02;
    uint8_t retryCount = 0x0F;
    radio.spiWriteRegister(RH_NRF24_REG_04_SETUP_RETR, ((retryDelay << 4) & RH_NRF24_ARD) | (retryCount & RH_NRF24_ARC));

    radio.spiWriteRegister(RH_NRF24_REG_1D_FEATURE, RH_NRF24_EN_DPL | RH_NRF24_EN_DYN_ACK | RH_NRF24_EN_ACK_PAY);
    radio.spiWriteRegister(RH_NRF24_REG_01_EN_AA, 0x01);
    
    int i = num_button_pins;
    while (i--) {
        pinMode(button_pins[i], INPUT_PULLUP);
        digitalWrite(button_pins[i], HIGH);
        button_timestamps[i] = 0;
        button_presses[i] = 0;
    }

    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    blink(3, 200, true);
}

void loop() {
    bool button_on = false;
    if (awakems > 1) {
        button_on = run_remote();
    }
    if (!button_on) {
        if (awakems > 1000) {
            awakems = 0;
            sleepNow();
        } else {
            awakems += 1;
        }
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

        if (state) button_on = true;
        
        if (state && !button_debounces[i]) {
            awakems = 0;
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
            Serial.print(" --> Starting active: ");
            Serial.print((int)i+1);
            Serial.print(" -- ");
#endif
        } else if (state && !button_different && button_timestamps[i]) {
            // Check if has hit new mode
            button_offset = millis() - button_timestamps[i];

            if (button_offset >= MODE_CHANGE_DURATION) {
#ifdef SERIAL_PRINT
                Serial.print("\n --> Mode by default: ");
                Serial.print((int)i+1);
                Serial.print(" -- ");
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
            Serial.print(" --> Ending active: ");
            Serial.print((int)i+1);
            Serial.print(" -- ");
            Serial.print((unsigned long)button_offset);
            Serial.print(" -- ");
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
        digitalWrite(led_pin, HIGH);
        
        awakems = 0;
        int tries_left = 1;
        while (tries_left--) {
            for (uint8_t i=0; i < sizeof(button_presses); i++) {
                Serial.print(button_presses[i], HEX);
            }
            uint8_t data = 0;
            for (uint8_t i=0; i < sizeof(button_presses); i++) {
                data |= (button_presses[i] << 2*i);
            }
            uint8_t len = sizeof(data);
            bool sent = radio.send(&data, len);
            Serial.print(" sent:");
            Serial.print(data, BIN);
            radio.waitPacketSent();
            uint8_t buf[num_button_pins];
            
            if (!sent) { 
                Serial.println(" ... send failed!");
            } else if (radio.recv(buf, &len)) {
                Serial.print(" --> Received: ");
                for (uint8_t i=0; i < sizeof(buf); i++) {
                    Serial.print(buf[i], HEX);
                }
                Serial.println();
                break;
            } else {
                Serial.print("\n ... no reply ... ");
                if (!tries_left) {
                    Serial.println("\n ... gave up!");
                }
            }
        }
        different = false;
        digitalWrite(led_pin, LOW);
        radio.setModeIdle();
    }

    return button_on;
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
    awakems = 0;
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
