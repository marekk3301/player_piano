#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define MIN_FORCE 3600
#define MAX_FORCE 4095

#define SDA_PIN 6
#define SCL_PIN 7

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

struct MidiEvent {
  uint32_t startTime;  // ms since song start
  uint16_t duration;   // ms
  uint8_t velocity;    // 0-127 MIDI velocity
};

#define NUM_EVENTS 5

MidiEvent song[NUM_EVENTS] = {
  { 0, 200, 127 },
  { 500, 200, 64 },
  { 1000, 300, 90 },
  { 1600, 100, 127 },
  { 2000, 400, 50 }
};

uint32_t songStartTime = 0;
uint8_t currentEvent = 0;
bool playing = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Starting I2C Scanner...");
  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  
  byte error, address;
  int nDevices = 0;

  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Unknown error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address, HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("Done\n");
  
  pwm.begin();
  pwm.setPWMFreq(1000);

  Serial.println("Ready to play song!");
  delay(2000);
  
  songStartTime = millis();
  playing = true;
}

void loop() {
  if (!playing) return;

  // uint32_t now = millis() - songStartTime;
  
  // if (currentEvent < NUM_EVENTS) {
  //   if (now >= song[currentEvent].startTime) {
  //     playKey(0, song[currentEvent].duration, song[currentEvent].velocity);
  //     currentEvent++;
  //   }
  // } else {
  //   playing = false;
  //   Serial.println("Song finished.");
  // }

  for (int i = 10; i<=50; i+=10) {
    for (int j = 0; j<20; j++) {
      playKey(0, 50, 127);
      delay(70-i);
    } 
    delay(1100);
  }

  // playKey(0, 50, 127);
  // delay(30);
  // playKey(0, 50, 100);
  // delay(30);
  // playKey(0, 50, 70);
  // delay(30);
  // playKey(0, 50, 30);
  // delay(30);

}

void playKey(uint8_t key_channel, uint16_t duration_ms, uint8_t velocity) {
  // MIDI velocity (0-127) mapped to pulse length
  uint16_t pulse = map(velocity, 0, 127, MIN_FORCE, MAX_FORCE);
  
  pwm.setPWM(key_channel, 0, pulse);
  delay(duration_ms);
  pwm.setPWM(key_channel, 0, 0);
}
