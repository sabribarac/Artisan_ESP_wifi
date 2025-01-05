#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_MAX31856.h>

// Add your wifi credentials
const char* ssid = "xxx";
const char* password = "xx";

WebSocketsServer webSocket = WebSocketsServer(81);

// MAX31856 thermocouple setup
// This is the pinout on my nano esp32
Adafruit_MAX31856 max31856 = Adafruit_MAX31856(D12,D11,D10,D9);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!max31856.begin()) {
    Serial.println("Failed to initialize MAX31856! Check wiring.");
    while (1);
  }
  max31856.setThermocoupleType(MAX31856_TCTYPE_K);
  Serial.println("MAX31856 initialized.");

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81.");
}

void loop() {
  webSocket.loop();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client %u disconnected.\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("Client %u connected from %s.\n", num, ip.toString().c_str());
      break;
    }

    case WStype_TEXT: {
      Serial.printf("Client %u sent: %s\n", num, payload);

      // Parse incoming JSON
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        Serial.println("Failed to parse JSON.");
        return;
      }

      // Extract command and ID
      const char* command = doc["command"];
      int id = doc["id"];

      if (strcmp(command, "getData") == 0) {
        // Read temperature
        double temperature = max31856.readThermocoupleTemperature();
        if (isnan(temperature)) {
          Serial.println("Failed to read from thermocouple!");
          temperature = 0.0;
        }

        // Prepare response
        StaticJsonDocument<256> response;
        response["id"] = id;
        JsonObject data = response.createNestedObject("data");
        data["BT"] = temperature;  // Send temperature as "BT"

        // Serialize response to string
        String responseString;
        serializeJson(response, responseString);

        // Send response to client
        webSocket.sendTXT(num, responseString);
        Serial.println("Sent: " + responseString);
      }
      break;
    }

    default:
      break;
  }
}
