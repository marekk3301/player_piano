#define SOLENOID_PIN 5

void setup() {
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);
  Serial.begin(9600);
}

int speed = 255;

void loop() {
  speed -= 2;
  if (speed <= 200) {
    speed = 255;
  } 
  Serial.println(speed);
    
  analogWrite(SOLENOID_PIN, speed);  // Activate solenoid
  delay(50);                         // Duration (e.g. 50ms)
  analogWrite(SOLENOID_PIN, 0);   // Turn off
  delay(1000);
}
