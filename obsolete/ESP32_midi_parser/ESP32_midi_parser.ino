#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_PWMServoDriver.h>

// ----------- CONFIGURATION -------------
#define MIDI_FILE "/1. Outer Wilds.mid"

#define NUM_DRIVERS 6
#define FIRST_MIDI_NOTE 21
#define LAST_MIDI_NOTE 108
#define PWM_MIN 3600
#define PWM_MAX 4096
#define PWM_FREQ 1000
#define MIN_NOTE_GAP_MS 25
#define MAX_EVENTS 512

// ----------- PINOUT -------------
#define CHIP_SELECT 10
#define SCK_PIN 12
#define MISO_PIN 13
#define MOSI_PIN 11

// ----------- STRUCTURES -------------
struct MidiEvent {
  bool isNoteOn;
  uint8_t note;
  uint8_t velocity;
  uint32_t timeMillis;
};

// ----------- GLOBALS -------------
Adafruit_PWMServoDriver pwmDrivers[NUM_DRIVERS] = {
  Adafruit_PWMServoDriver(0x40),
  Adafruit_PWMServoDriver(0x41),
  Adafruit_PWMServoDriver(0x42),
  Adafruit_PWMServoDriver(0x43),
  Adafruit_PWMServoDriver(0x44),
  Adafruit_PWMServoDriver(0x45)
};

MidiEvent events[MAX_EVENTS];
int eventCount = 0;
int currentEvent = 0;
unsigned long playbackStart = 0;
uint32_t ticksPerQuarterNote = 480;
float tempoMicrosecPerQN = 500000.0;

// ----------- MIDI HELPERS -------------
uint32_t readBigEndian32(File file) {
  uint32_t val = 0;
  for (int i = 0; i < 4; i++) {
    val = (val << 8) | file.read();
  }
  return val;
}

uint32_t readVLQ(File file) {
  uint32_t val = 0;
  uint8_t byte;
  do {
    byte = file.read();
    val = (val << 7) | (byte & 0x7F);
  } while (byte & 0x80);
  return val;
}

float ticksToMillis(uint32_t ticks) {
  return (tempoMicrosecPerQN / 1000.0f) * ticks / ticksPerQuarterNote;
}

uint16_t velocityToPWM(uint8_t velocity) {
  return PWM_MIN + ((uint16_t)velocity * (PWM_MAX - PWM_MIN)) / 127;
}

void sortEvents() {
  for (int i = 0; i < eventCount - 1; i++) {
    for (int j = i + 1; j < eventCount; j++) {
      if (events[j].timeMillis < events[i].timeMillis) {
        MidiEvent temp = events[i];
        events[i] = events[j];
        events[j] = temp;
      }
    }
  }
}

// ----------- SOLENOID CONTROL -------------
void triggerSolenoid(uint8_t note, bool isOn, uint8_t velocity) {
  if (note < FIRST_MIDI_NOTE || note > LAST_MIDI_NOTE) return;

  uint8_t index = note - FIRST_MIDI_NOTE;
  uint8_t driver = index / 16;
  uint8_t channel = index % 16;

  if (driver >= NUM_DRIVERS) return;

  uint16_t pwmVal = isOn ? velocityToPWM(velocity) : 0;
  pwmDrivers[driver].setPWM(channel, 0, pwmVal);
}

// ----------- MIDI PARSING -------------
void parseMIDI(File midiFile) {
  uint32_t lastNoteOnMillis[128] = {0};

  // Skip header
  midiFile.seek(0);
  midiFile.seek(midiFile.position() + 8);
  midiFile.read(); midiFile.read(); // Format
  midiFile.read(); midiFile.read(); // Num tracks
  ticksPerQuarterNote = (midiFile.read() << 8) | midiFile.read();

  // Find and parse first track
  while (midiFile.available()) {
    char id[5] = {0};
    midiFile.readBytes(id, 4);
    if (strncmp(id, "MTrk", 4) != 0) break;

    uint32_t length = readBigEndian32(midiFile);
    uint32_t trackEnd = midiFile.position() + length;
    uint32_t absTicks = 0;
    uint8_t runningStatus = 0;

    while (midiFile.position() < trackEnd && eventCount < MAX_EVENTS) {
      uint32_t delta = readVLQ(midiFile);
      absTicks += delta;
      uint32_t millisNow = (uint32_t)ticksToMillis(absTicks);

      uint8_t status = midiFile.peek();
      if (status < 0x80) {
        status = runningStatus;
      } else {
        status = midiFile.read();
        runningStatus = status;
      }

      if ((status & 0xF0) == 0x90 || (status & 0xF0) == 0x80) {
        uint8_t note = midiFile.read();
        uint8_t vel = midiFile.read();
        bool isNoteOn = ((status & 0xF0) == 0x90) && vel > 0;

        if (note >= FIRST_MIDI_NOTE && note <= LAST_MIDI_NOTE) {
          if (isNoteOn) {
            lastNoteOnMillis[note] = millisNow;
            events[eventCount++] = {true, note, vel, millisNow};
          } else {
            uint32_t prevOn = lastNoteOnMillis[note];
            uint32_t adjustedOff = millisNow;

            events[eventCount++] = {false, note, vel, adjustedOff};
          }
        }
      } else if (status == 0xFF) {
        uint8_t metaType = midiFile.read();
        uint32_t len = readVLQ(midiFile);
        if (metaType == 0x51 && len == 3) {
          tempoMicrosecPerQN = 0;
          for (int i = 0; i < 3; i++) {
            tempoMicrosecPerQN = ((int)tempoMicrosecPerQN << 8) | midiFile.read();
          }
        } else {
          midiFile.seek(midiFile.position() + len);
        }
      } else {
        // Skip other events
        uint8_t len = ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
        for (int i = 0; i < len; i++) midiFile.read();
      }
    }
  }

  sortEvents();

  for (int i = 0; i < eventCount - 1; i++) {
    if (events[i].timeMillis == events[i + 1].timeMillis) {
      if (events[i].isNoteOn && !events[i + 1].isNoteOn &&
          events[i].note == events[i + 1].note) {
        // Swap to place OFF before ON
        MidiEvent temp = events[i];
        events[i] = events[i + 1];
        events[i + 1] = temp;
      }
    }
  }

  for (int i = 0; i < eventCount; i++) {
    if (!events[i].isNoteOn) {
      uint8_t note = events[i].note;
      for (int j = i + 1; j < eventCount; j++) {
        if (events[j].isNoteOn && events[j].note == note) {
          if (events[j].timeMillis - events[i].timeMillis < MIN_NOTE_GAP_MS) {
            events[i].timeMillis = events[j].timeMillis - MIN_NOTE_GAP_MS;
            if ((int32_t)events[i].timeMillis < 0) {
              events[i].timeMillis = 0;
            }
          }
          break; // only look at next ON
        }
      }
    }
  }

  sortEvents();  // Resort after adjusting OFF times

}

// ----------- SETUP -------------
void setupPWMDrivers() {
  Wire.begin(4, 5); // SDA, SCL
  for (int i = 0; i < NUM_DRIVERS; i++) {
    pwmDrivers[i].begin();
    pwmDrivers[i].setPWMFreq(PWM_FREQ);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  setupPWMDrivers();

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CHIP_SELECT);
  if (!SD.begin(CHIP_SELECT, SPI, 20000000)) {
    Serial.println("SD Card Initialization Failed!");
    return;
  }

  File midiFile = SD.open(MIDI_FILE);
  if (!midiFile) {
    Serial.println("Failed to open MIDI file!");
    return;
  }

  parseMIDI(midiFile);
  midiFile.close();

  Serial.print("Loaded ");
  Serial.print(eventCount);
  Serial.println(" events. Starting playback...");
  playbackStart = millis();
}

// -----------  LOOP -------------
void loop() {
  if (currentEvent >= eventCount) return;

  unsigned long now = millis() - playbackStart;

  while (currentEvent < eventCount && events[currentEvent].timeMillis <= now) {
    MidiEvent &evt = events[currentEvent++];
    triggerSolenoid(evt.note, evt.isNoteOn, evt.velocity);

    Serial.print(evt.isNoteOn ? "ON  " : "OFF ");
    Serial.print("Note ");
    Serial.print(evt.note);
    Serial.print(" PWM ");
    Serial.print(evt.isNoteOn ? velocityToPWM(evt.velocity) : 0);
    Serial.print(" @ ");
    Serial.print(evt.timeMillis);
    Serial.println("ms");
  }
}
