#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define MIN_FORCE  3600 // min pulse length for solenoid to fire - adjust later will be higher when the key is present
#define MAX_FORCE  4095 // max len for 12bit PWM

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  pwm.begin();
  pwm.setPWMFreq(1000);  // PWM frequency

  Serial.println("Let's go!");
}

void loop() {

  playKey(0, 50, 255);
  delay(1000);
  playKey(0, 50, 150);
  delay(700);
  playKey(0, 300, 255);
  delay(1500);
 
}

int playKey(int key_number, int length, int strength) { // length in ms, strength 0-255
  int pulse = map(strength, 0, 255, MIN_FORCE, MAX_FORCE);
  pwm.setPWM(key_number, 0, pulse);
  delay(length);
  pwm.setPWM(key_number, 0, 0);
}
