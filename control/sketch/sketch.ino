#import "sketch.h"

const int world_pin = 13;
const int toggle_south_pin = 12;
const int led_south_pin = 11;
const int toggle_west_pin = 10;
const int led_west_pin = 9;
const int toggle_east_pin = 8;
const int led_east_pin = 7;
const int toggle_north_pin = 6;
const int led_north_pin = 5;

const unsigned long CAPTURE_INTERVAL = 500;
unsigned long capture_time = 0;
TTMode selected_mode;
TTMode captured_mode;

void reset_leds() {
    digitalWrite(led_south_pin, LOW);
    digitalWrite(led_west_pin, LOW);
    digitalWrite(led_east_pin, LOW);
    digitalWrite(led_north_pin, LOW);
    digitalWrite(world_pin, HIGH);
}

void setup() {
  Serial.begin(9600); 
  pinMode(world_pin, OUTPUT);
  pinMode(led_south_pin, OUTPUT);
  pinMode(toggle_south_pin, INPUT);
  pinMode(led_west_pin, OUTPUT);
  pinMode(toggle_west_pin, INPUT);
  pinMode(led_east_pin, OUTPUT);
  pinMode(toggle_east_pin, INPUT);
  pinMode(led_north_pin, OUTPUT);
  pinMode(toggle_north_pin, INPUT);
  
  reset_leds();
}


void loop() {
  int current_time = millis();
  TTMode new_mode = check_toggles();
  
  if (new_mode != captured_mode) {
    captured_mode = new_mode;
    capture_time = current_time;
  }
  if (new_mode != selected_mode) {
    blink_led(new_mode);
  }
  
  
  if (current_time - capture_time > CAPTURE_INTERVAL) {
    if (new_mode != selected_mode) {
      selected_mode = new_mode;
      reset_leds();
      set_leds();
    }
  }
  
  delay(100);
}

TTMode check_toggles() {
  bool toggle_south_on = (digitalRead(toggle_south_pin) == LOW);
  if (toggle_south_on) {
    return SOUTH;
  }
  bool toggle_west_on = (digitalRead(toggle_west_pin) == LOW);
  if (toggle_west_on) {
    return WEST;
  }
  bool toggle_east_on = (digitalRead(toggle_east_pin) == LOW);
  if (toggle_east_on) {
    return EAST;
  }
  bool toggle_north_on = (digitalRead(toggle_north_pin) == LOW);
  if (toggle_north_on) {
    return NORTH;
  }
  
  return selected_mode;
}

void set_leds() {
  switch (selected_mode) {
    case NORTH:
      digitalWrite(led_north_pin, HIGH);
      break;
    case EAST:
      digitalWrite(led_east_pin, HIGH);
      break;
    case SOUTH:
      digitalWrite(led_south_pin, HIGH);
      break;
    case WEST:
      digitalWrite(led_west_pin, HIGH);
      break;
  }
}

void blink_led(TTMode direction) {
    switch (direction) {
    case NORTH:
      digitalWrite(led_north_pin, HIGH);
      delay(100);
      digitalWrite(led_north_pin, LOW);
      break;
    case EAST:
      digitalWrite(led_east_pin, HIGH);
      delay(100);
      digitalWrite(led_east_pin, LOW);
      break;
    case SOUTH:
      digitalWrite(led_south_pin, HIGH);
      delay(100);
      digitalWrite(led_south_pin, LOW);
      break;
    case WEST:
      digitalWrite(led_west_pin, HIGH);
      delay(100);
      digitalWrite(led_west_pin, LOW);
      break;
  }  
}
