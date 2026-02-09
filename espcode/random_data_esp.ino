#include <WiFi.h>
#include <HTTPClient.h>

// -------- WiFi Config --------
const char* WIFI_SSID = "arvinshokouhi";
const char* WIFI_PASS = "Arvin1384";

// -------- Server Config --------
const char* SERVER_URL = "http://10.187.156.197:8000/update-data";

// ارسال هر 15 ثانیه
constexpr unsigned long POST_INTERVAL_MS = 15000;
unsigned long lastPost = 0;

// -------- WiFi Connection --------
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
            Serial.println("\nWiFi failed. Restarting ESP...");
            ESP.restart();
        }
    }
    Serial.print("\nConnected! IP: ");
    Serial.println(WiFi.localIP());
}

// -------- Fake Sensor Readings --------
float readSoilMoisture() {
    return 20.0f + random(0, 6000) / 100.0f; // %
}

float readTemperature() {
    return 18.0f + random(0, 1500) / 100.0f; // °C
}

float readHumidity() {
    return 35.0f + random(0, 5000) / 100.0f; // %
}

float readLightLevel() {
    return random(200, 3000); // lux
}

float readGas() {
    return random(100, 900); // ppm
}

float readWaterLevel() {
    return random(0, 101); // %
}

bool readTouch() {
    return random(0, 2); // true / false
}

bool readPumpState() {
    return random(0, 2); // true / false
}

// -------- Build JSON --------
String buildJsonPayload(
    float moisture,
    float temperature,
    float humidity,
    float light,
    float gas,
    float waterLevel,
    bool touch,
    bool pumpState
) {
    char buffer[350];
    snprintf(
        buffer,
        sizeof(buffer),
        "{"
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"moisture\":%.2f,"
        "\"light\":%.2f,"
        "\"gas\":%.2f,"
        "\"water_level\":%.2f,"
        "\"touch\":%s,"
        "\"pump_state\":%s"
        "}",
        temperature,
        humidity,
        moisture,
        light,
        gas,
        waterLevel,
        touch ? "true" : "false",
        pumpState ? "true" : "false"
    );
    return String(buffer);
}

// -------- POST Data --------
void postSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
        return;
    }

    float moisture   = readSoilMoisture();
    float temperature = readTemperature();
    float humidity   = readHumidity();
    float light      = readLightLevel();
    float gas        = readGas();
    float waterLevel = readWaterLevel();
    bool touch       = readTouch();
    bool pumpState   = readPumpState();

    String jsonPayload = buildJsonPayload(
        moisture,
        temperature,
        humidity,
        light,
        gas,
        waterLevel,
        touch,
        pumpState
    );

    Serial.println("Sending JSON:");
    Serial.println(jsonPayload);

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);

    Serial.print("HTTP Response Code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
        Serial.println("Response:");
        Serial.println(http.getString());
    }

    http.end();
}

// -------- Setup & Loop --------
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
