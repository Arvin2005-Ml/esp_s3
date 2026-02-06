#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =====================
// WIFI CONFIG  ✅ (FIX)
// =====================
const char* ssid     = "arvinshokouhi";
const char* password = "Arvin1384";

// =====================
// SERVER CONFIG
// =====================
const char* SERVER_URL = "http://10.156.45.78:8000/update-data";

// =====================
// WIFI CONNECT
// =====================
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// =====================
// REAL RANDOM FLOAT
// =====================
float randFloat(float minVal, float maxVal) {
  return minVal + ((float)esp_random() / UINT32_MAX) * (maxVal - minVal);
}

// =====================
// SEND RANDOM DATA
// =====================
void sendRandomData() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi disconnected");
    return;
  }

  float temperature = randFloat(18.0, 35.0);
  float humidity    = randFloat(30.0, 90.0);
  float moisture    = randFloat(5.0, 100.0);

  StaticJsonDocument<256> json;
  json["temperature"] = temperature;
  json["humidity"]    = humidity;
  json["moisture"]    = moisture;

  String payload;
  serializeJson(json, payload);

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  Serial.println("📤 Payload:");
  Serial.println(payload);
  Serial.print("📥 HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println("✅ Server Response:");
    Serial.println(http.getString());
  }

  http.end();
}

// =====================
// SETUP / LOOP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  connectWiFi();
}

void loop() {
  sendRandomData();
  delay(5000); // هر ۵ ثانیه
}
