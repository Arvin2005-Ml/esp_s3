/**
 * ============================================================================
 *  Project: EMO 2.0 - REVOLUTIONARY UI (Fixed for LovyanGFX Compilation)
 *  Optimized for ESP32-S3 (Internal RAM Only)
 *  Display: ILI9488 Parallel 16-bit | Touch: XPT2046 SPI
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

// ==========================================
// 1. CONFIGURATION & DRIVERS
// ==========================================

const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 
const char* SERVER_BASE   = "http://10.103.41.197:8000"; 

// --- Modern Color Palette (Cyber/Minimal) ---
#define C_BG_DARK     0x10A2  // Dark Grey/Blueish
#define C_BG_CARD     0x2124  // Slightly lighter for cards
#define C_ACCENT      0x5CF4  // Soft Blue/Purple
#define C_TEXT_MAIN   0xFFFF  // White
#define C_TEXT_SUB    0x9CD3  // Grey Text
#define C_SUCCESS     0x0660  // Muted Green
#define C_ALERT       0xE8E4  // Soft Red

// --- LGFX Driver ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 16000000; 
      cfg.pin_d0 = 4;  cfg.pin_d1 = 5;  cfg.pin_d2 = 6;  cfg.pin_d3 = 7;
      cfg.pin_d4 = 15; cfg.pin_d5 = 16; cfg.pin_d6 = 17; cfg.pin_d7 = 18;
      cfg.pin_d8 = 8;  cfg.pin_d9 = 9;  cfg.pin_d10 = 14; cfg.pin_d11 = 21;
      cfg.pin_d12 = 12; cfg.pin_d13 = 13; cfg.pin_d14 = 10; cfg.pin_d15 = 11;
      cfg.pin_wr = 38; cfg.pin_rd = 42; cfg.pin_rs = 39;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 40; cfg.pin_rst = 41;
      cfg.panel_width = 320; cfg.panel_height = 480;
      cfg.readable = true; cfg.dlen_16bit = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0; cfg.x_max = 319;
      cfg.y_min = 0; cfg.y_max = 479;
      cfg.pin_int = 1;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.spi_host = SPI3_HOST;
      cfg.freq = 1000000;
      cfg.pin_sclk = 47; cfg.pin_mosi = 48; 
      cfg.pin_miso = 20; cfg.pin_cs = 2;         
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite faceSpr(&tft);

// ==========================================
// 2. DATA STRUCTURES & STATE
// ==========================================

struct TaskItem { String id; String name; bool done; };
struct AppState {
  float moisture = 0; float temp = 0; int humidity = 0; int light = 0;
  String mood = "NEUTRAL"; 
  std::vector<TaskItem> tasks;
  int viewIndex = 0; 
  bool needsRedraw = true;
  String notification = "";
  unsigned long notifTime = 0;
};
AppState app;

unsigned long lastBlink = 0;
bool isBlinking = false;
int eyeHeight = 60; 

// ==========================================
// 3. LOGIC & NETWORK
// ==========================================

void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  tft.fillCircle(300, 20, 4, C_ACCENT); 
  
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/get-data");
  http.setTimeout(1500);
  int code = http.GET();
  
  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && !doc["sensor"].isNull()) {
        float newMoisture = doc["sensor"]["moisture"];
        app.moisture = newMoisture;
        app.temp = doc["sensor"]["temperature"];
        app.humidity = doc["sensor"]["humidity"];
        app.light = doc["sensor"]["light"];
        
        if(app.mood != "EXCITED" || millis() - app.notifTime > 5000) {
             app.mood = doc["sensor"]["mood"].as<String>();
        }

        if(app.viewIndex == 1) {
             app.tasks.clear();
             for(JsonObject v : doc["tasks"].as<JsonArray>()) {
                 app.tasks.push_back({v["_id"], v["title"], v["done"]});
             }
             app.needsRedraw = true;
        }
    }
  }
  http.end();
  tft.fillCircle(300, 20, 4, C_BG_CARD);
}

// ==========================================
// 4. UI DRAWING FUNCTIONS (FIXED)
// ==========================================

// --- Helper: Draw a sleek card ---
void drawCard(int x, int y, int w, int h, uint16_t color) {
  // FIX 1: Removed the last argument (background color)
  tft.fillSmoothRoundRect(x, y, w, h, 12, color);
  
  // FIX 2: Changed drawSmoothRoundRect to drawRoundRect
  tft.drawRoundRect(x, y, w, h, 12, C_BG_DARK + 0x18E3); 
}

// --- Component: Header ---
void drawHeader() {
  tft.fillRect(0, 0, 320, 40, C_BG_DARK);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(C_TEXT_SUB);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("EMO OS 2.0", 20, 20);
  
  uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? C_SUCCESS : C_ALERT;
  tft.drawArc(280, 20, 8, 6, 220, 320, wifiColor);
  tft.drawArc(280, 20, 5, 3, 220, 320, wifiColor);
  tft.fillCircle(280, 20, 2, wifiColor);
}

// --- Component: The Face (Eyes) ---
void drawFace() {
  unsigned long now = millis();
  if (!isBlinking && now - lastBlink > (random(2000, 5000))) {
      isBlinking = true;
      lastBlink = now;
  }
  if (isBlinking) {
     eyeHeight -= 10;
     if (eyeHeight <= 4) eyeHeight = 4;
     if (now - lastBlink > 150) { 
         isBlinking = false;
         lastBlink = now;
     }
  } else {
     eyeHeight += 10;
     if (eyeHeight > 80) eyeHeight = 80; 
  }

  faceSpr.fillSprite(C_BG_DARK);

  uint16_t eyeColor = C_ACCENT;
  if(app.mood == "HAPPY") eyeColor = C_SUCCESS;
  if(app.mood == "SAD") eyeColor = C_ACCENT; 
  if(app.mood == "EXCITED") eyeColor = 0xFDA0; 
  if(app.mood == "ANGRY") eyeColor = C_ALERT;

  int eyeW = 70;
  int eyeY = 60; 

  // FIX 3: Removed the last argument (background color) for both eyes
  faceSpr.fillSmoothRoundRect(40, eyeY - (eyeHeight/2), eyeW, eyeHeight, 20, eyeColor);
  faceSpr.fillSmoothRoundRect(170, eyeY - (eyeHeight/2), eyeW, eyeHeight, 20, eyeColor);

  faceSpr.pushSprite(20, 60); 
}

// --- Component: Info Pills (Bottom) ---
void drawInfoArea() {
  int startY = 220;
  
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(C_TEXT_MAIN, C_BG_DARK);
  tft.drawString(app.mood, 160, startY);

  int cardW = 135;
  int cardH = 60;
  int rowY = startY + 40;

  drawCard(20, rowY, cardW, cardH, C_BG_CARD);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(C_TEXT_SUB); tft.drawString("TEMP", 20 + cardW/2, rowY + 15);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT_MAIN); tft.drawString(String(app.temp, 1) + " C", 20 + cardW/2, rowY + 40);

  drawCard(165, rowY, cardW, cardH, C_BG_CARD);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(C_TEXT_SUB); tft.drawString("SOIL", 165 + cardW/2, rowY + 15);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  
  uint16_t humColor = (app.moisture < 30) ? C_ALERT : C_SUCCESS;
  tft.setTextColor(humColor); tft.drawString(String((int)app.moisture) + "%", 165 + cardW/2, rowY + 40);

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(0x4208); 
  tft.drawString("v Swipe for Tasks v", 160, 460);
}

// --- Screen: Tasks View ---
void drawTasksView() {
    if(!app.needsRedraw) return;
    
    tft.fillScreen(C_BG_DARK);
    
    tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.setTextColor(C_TEXT_MAIN);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("My Tasks", 20, 30);
    
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(C_TEXT_SUB);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("^ BACK", 300, 30);

    int y = 70;
    if(app.tasks.size() == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No tasks today!", 160, 240);
    }

    for(auto t : app.tasks) {
        uint16_t cardColor = t.done ? 0x1245 : C_BG_CARD; 
        uint16_t txtColor = t.done ? 0x6408 : C_TEXT_MAIN;
        
        drawCard(20, y, 280, 50, cardColor);
        
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(txtColor);
        tft.drawString(t.name, 40, y + 25);
        
        if(t.done) tft.fillCircle(270, y+25, 6, C_SUCCESS);
        else tft.drawCircle(270, y+25, 6, C_TEXT_SUB);
        
        y += 60;
        if(y > 460) break; 
    }
    app.needsRedraw = false;
}

// ==========================================
// 5. SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(C_BG_DARK);
  
  // tft.calibrateTouch(nullptr, 0xFFFFFF, 0x000000, 30); 

  faceSpr.setColorDepth(16);
  faceSpr.createSprite(280, 120);

  tft.setTextDatum(MC_DATUM);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT_MAIN);
  tft.drawString("SYSTEM BOOT", 160, 220);
  tft.drawRect(60, 240, 200, 10, C_TEXT_SUB);
  
  delay(500);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int w = 0;
  while (WiFi.status() != WL_CONNECTED && w < 10) {
      delay(500); w++;
      tft.fillRect(63, 243, 100 + (w*10), 4, C_ACCENT);
  }
  
  tft.fillScreen(C_BG_DARK);
  drawHeader();
  drawInfoArea(); 
}

int32_t startY = -1;
unsigned long lastNetCheck = 0;

void loop() {
  // 1. Animation Loop
  if (app.viewIndex == 0) {
      drawFace(); 
      static unsigned long lastInfoDraw = 0;
      if (millis() - lastInfoDraw > 2000) {
          drawInfoArea();
          lastInfoDraw = millis();
      }
  }

  // 2. Touch Logic
  int32_t x, y;
  static int32_t lastX = 0, lastY = 0;
  bool touched = tft.getTouch(&x, &y);
  
  if (touched) {
      if (startY == -1) { startY = y; }
      lastX = x; lastY = y;
  } else {
      if (startY != -1) {
          int dist = startY - lastY;
          
          if (dist > 50 && app.viewIndex == 0) {
              app.viewIndex = 1;
              app.needsRedraw = true;
              drawTasksView();
          }
          else if (dist < -50 && app.viewIndex == 1) {
              app.viewIndex = 0;
              tft.fillScreen(C_BG_DARK);
              drawHeader();
              drawInfoArea(); 
          }
          else if (abs(dist) < 10 && app.viewIndex == 0) {
               app.mood = "EXCITED";
               app.notifTime = millis();
               drawFace(); 
          }
          startY = -1;
      }
  }

  // 3. Network Task
  if (millis() - lastNetCheck > 10000) { 
      fetchData();
      lastNetCheck = millis();
  }
  
  delay(10); 
}