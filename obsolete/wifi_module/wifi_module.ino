#include <WiFi.h>
#include <WebSocketsServer_Generic.h>
#include <ArduinoJson.h>
#include <list>

#define ARDUINO_SERIAL Serial1
#define BAUD_RATE 115200

// Replace with your Wi-Fi credentials
const char* ssid = "cosfajnego";
const char* password = "P@ssw0rd";

WebSocketsServer webSocket = WebSocketsServer(81); // Port 81 for WebSocket

struct Command {
  int note;
  int duration;
  int velocity;
  uint32_t timestamp;
};

std::list<Command> commandQueue;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.printf("User %u connected\n", num+1);
      webSocket.sendTXT(num, "Piano Connected");
      break;
    }
    case WStype_DISCONNECTED: {
      Serial.printf("User %u disconnected\n", num+1);
      break;
    }
    case WStype_TEXT: {
      String jsonStr = String((char*)payload);
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonStr);
      if (error) {
        Serial.println("JSON parse error");
        return;
      }

      if (doc["type"] == "play") {
        commandQueue.clear();
        JsonArray data = doc["data"].as<JsonArray>();
        for (JsonObject cmd : data) {
          commandQueue.push_back(Command{
            cmd["note"].as<int>(),
            cmd["duration"].as<int>(),
            cmd["velocity"].as<int>(),
            millis() + cmd["delay"].as<uint32_t>()
          });
        }
        Serial.printf("Received %d commands\n", commandQueue.size());
      }
        break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(BAUD_RATE);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  delay(500);
  ARDUINO_SERIAL.begin(BAUD_RATE, SERIAL_8N1, 17, 18);
  Serial.println("Start serial connection");
  delay(1000);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();

  if (!commandQueue.empty() && millis() >= commandQueue.front().timestamp) {
    Command cmd = commandQueue.front();
    commandQueue.pop_front();
    
    char buf[32];
    sprintf(buf, "N %d %d %d\n", cmd.note, cmd.velocity, cmd.duration);
    ARDUINO_SERIAL.print(buf);
    // Serial.println("Sent: " + commandStr); // Debug
  }
}

