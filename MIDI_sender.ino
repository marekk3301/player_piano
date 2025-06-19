// (ESP32)
// parses MIDI files and sends commmands to solenoid_driver on Arduino UNO

struct NoteEvent {
  uint32_t startTime;  // ms since song start
  uint16_t duration;   // ms
  uint8_t note;        // MIDI note number (21–108)
  uint8_t velocity;    // 0-127
};

// Hardcoded test song
NoteEvent song[] = {
  { 0, 200, 37, 127 }, 
  { 700, 300, 108, 90 }, 
  { 1200, 500, 37, 110 }, 
  { 2000, 300, 37, 100 },
  { 2000, 500, 108, 80 },  // chord example: play 72 and 76 simultaneously
};

#define NUM_EVENTS (sizeof(song) / sizeof(song[0]))

#define ARDUINO_SERIAL Serial2
#define BAUD_RATE 115200

uint32_t songStartTime;
int currentEvent = 0;
bool playing = false;

void setup() {
  Serial.begin(115200);
  ARDUINO_SERIAL.begin(BAUD_RATE, SERIAL_8N1, 16, 17);
  Serial.println("ESP32 Sender Ready.");
  delay(1000);
  
  startPlayback();
}

void loop() {
  if (!playing) return;

  uint32_t now = millis() - songStartTime;

  while (currentEvent < NUM_EVENTS && now >= song[currentEvent].startTime) {
    sendNote(song[currentEvent]);
    currentEvent++;
  }

  if (currentEvent >= NUM_EVENTS) {
    playing = false;
    Serial.println("Song finished.");
  }
}

void startPlayback() {
  songStartTime = millis();
  currentEvent = 0;
  playing = true;
  Serial.println("Starting playback...");
}

void sendNote(NoteEvent event) {
  char buf[32];
  sprintf(buf, "N %d %d %d\n", event.note, event.velocity, event.duration);
  ARDUINO_SERIAL.print(buf);
  Serial.print("Sent: ");
  Serial.print(buf);
}
