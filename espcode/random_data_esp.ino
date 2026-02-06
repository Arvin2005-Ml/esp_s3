// esp_sensor.ino
#include <WiFi.h>
#include <HTTPClient.h>

// --- تنظیمات ---
const char* WIFI_SSID = "arvinshokouhi";
const char* WIFI_PASS = "Arvin1384";

// ✅ آدرس کامل با http://
const char* SERVER_URL = "http://10.146.144.197:8000/update-data";

// ارسال هر 15 ثانیه
constexpr unsigned long POST_INTERVAL_MS = 15000;
unsigned long lastPost = 0;

void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Connecting to WiFi");
    uint8_t retry = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(600);
        Serial.print(".");
        retry++;
        if (retry > 30) {
            Serial.println("\nWiFi connection failed. Restarting...");
            ESP.restart();
        }
    }
    Serial.print("\nConnected! IP address: ");
    Serial.println(WiFi.localIP());
}

float readSoilMoisture() {
    return 30.0f + static_cast<float>(random(0, 5000)) / 100.0f;
}

float readTemperature() {
    return 20.0f + static_cast<float>(random(0, 800)) / 10.0f;
}

float readHumidity() {
    return 40.0f + static_cast<float>(random(0, 3000)) / 100.0f;
}

float readLightLevel() {
    return 100.0f + static_cast<float>(random(0, 30000)) / 100.0f;
}

String buildJsonPayload(float moisture, float temp, float humidity) {
    char buffer[200];
    snprintf(
        buffer,
        sizeof(buffer),
        "{\"temperature\":%.2f,\"humidity\":%.2f,\"moisture\":%.2f}",
        temp, humidity, moisture
    );
    return String(buffer);
}

void postSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        connectToWiFi();
        return;
    }

    float moisture = readSoilMoisture();
    float temp = readTemperature();
    float humidity = readHumidity();
    float light = readLightLevel();

    String jsonPayload = buildJsonPayload(moisture, temp, humidity);

    Serial.println("Posting data to server:");
    Serial.println(jsonPayload);

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
        Serial.printf("Server response code: %d\n", httpCode);
        String response = http.getString();
        Serial.println("Response body:");
        Serial.println(response);
    } else {
        Serial.printf("POST failed. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    randomSeed(esp_random());
    connectToWiFi();
}

void loop() {
    unsigned long now = millis();
    if (now - lastPost >= POST_INTERVAL_MS) {
        lastPost = now;
        postSensorData();
    }
    delay(100);
}
