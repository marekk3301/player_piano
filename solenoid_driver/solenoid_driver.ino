#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ----- Configuration -----
#define MIN_FORCE 3600
#define MAX_FORCE 4095
#define PWM_FREQ 1000

#define NUM_BOARDS 6
Adafruit_PWMServoDriver pwm[NUM_BOARDS] = {
  Adafruit_PWMServoDriver(0x40),
  Adafruit_PWMServoDriver(0x41),
  Adafruit_PWMServoDriver(0x42),
  Adafruit_PWMServoDriver(0x43),
  Adafruit_PWMServoDriver(0x44),
  Adafruit_PWMServoDriver(0x45)
};

#define MAX_ACTIVE_NOTES 32

struct ActiveNote {
  int note;
  unsigned long endTime;
};

ActiveNote activeNotes[MAX_ACTIVE_NOTES];
int activeCount = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  for (int i = 0; i < NUM_BOARDS; i++) {
    pwm[i].begin();
    pwm[i].setPWMFreq(PWM_FREQ);
  }

  Serial.println("Arduino Polyphonic Receiver Ready.");
}

void loop() {
  static String input = "";

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(input);
      input = "";
    } else {
      input += c;
    }
  }

  checkNoteStops();
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.charAt(0) != 'N') {
    Serial.println("Invalid command");
    return;
  }

  int firstSpace = cmd.indexOf(' ');
  int secondSpace = cmd.indexOf(' ', firstSpace + 1);
  int thirdSpace = cmd.indexOf(' ', secondSpace + 1);

  if (firstSpace == -1 || secondSpace == -1 || thirdSpace == -1) {
    Serial.println("Malformed command");
    return;
  }

  int note = cmd.substring(firstSpace + 1, secondSpace).toInt();
  int velocity = cmd.substring(secondSpace + 1, thirdSpace).toInt();
  int duration = cmd.substring(thirdSpace + 1).toInt();

  if (note < 21 || note > 108) {
    Serial.println("Note out of range");
    return;
  }
  Serial.print("Playing: ");
  Serial.println(cmd);
  Serial.println(millis());
  startNote(note, velocity, duration);
}

void startNote(int note, int velocity, int duration) {
  int channel = note - 21;
  int boardIndex = channel / 16;
  int channelIndex = channel % 16;
  int pulse = map(velocity, 0, 127, MIN_FORCE, MAX_FORCE);

  pwm[boardIndex].setPWM(channelIndex, 0, pulse);

  // Register note for stopping later
  if (activeCount < MAX_ACTIVE_NOTES) {
    activeNotes[activeCount].note = note;
    activeNotes[activeCount].endTime = millis() + duration;
    activeCount++;
  }
}

void checkNoteStops() {
  unsigned long now = millis();
  for (int i = 0; i < activeCount; ) {
    if (now >= activeNotes[i].endTime) {
      stopNote(activeNotes[i].note);
      // Remove from active list (compact array)
      activeNotes[i] = activeNotes[activeCount - 1];
      activeCount--;
    } else {
      i++;
    }
  }
}

void stopNote(int note) {
  int channel = note - 21;
  int boardIndex = channel / 16;
  int channelIndex = channel % 16;
  pwm[boardIndex].setPWM(channelIndex, 0, 0);
}
