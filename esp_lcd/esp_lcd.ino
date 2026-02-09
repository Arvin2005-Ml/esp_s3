/**
 * ============================================================================
 *  Project: EMO 2.0 - REVOLUTIONARY UI (Fixed for LovyanGFX Compilation)
 *  Optimized for ESP32-S3 (Internal RAM Only)
 *  Display: ILI9488 Parallel 16-bit | Touch: XPT2046 SPI
 *  NEW FEATURE: 3-Page Button Navigation (Face, Stats, Tasks) + Clock
 *  FIX: LovyanGFX drawArc usage corrected.
 *  FIX: UI overlap and Touch Calibration added.
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h> // For clock/NTP

// ==========================================
// 1. CONFIGURATION & DRIVERS
// ==========================================

const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 
const char* SERVER_BASE   = "http://10.103.41.197:8000"; 

// --- Modern Color Palette (Cyber/Minimal) ---
#define C_BG_DARK     0x10A2  // Dark Grey/Blueish
#define C_BG_CARD     0x2124  // Slightly lighter for cards
#define C_ACCENT      0x5CF4  // Soft Blue/Purple (Main Accent)
#define C_TEXT_MAIN   0xFFFF  // White
#define C_TEXT_SUB    0x9CD3  // Grey Text
#define C_SUCCESS     0x0660  // Muted Green
#define C_ALERT       0xE8E4  // Soft Red
#define C_NAV_BAR     0x18A3  // Darker for navigation bar

// --- LGFX Driver (As defined by user) ---
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
// Size reduced to prevent overlap with status text
LGFX_Sprite faceSpr(&tft);

// ==========================================
// 2. DATA STRUCTURES & STATE
// ==========================================

struct TaskItem { String id; String name; bool done; };
struct AppState {
  float moisture = 0; float temp = 0; int humidity = 0; int light = 0;
  // NEW SENSOR FIELDS
  float gas = 0; float water_level = 0; bool touch = false; bool pump_state = false; 
  String mood = "NEUTRAL"; 
  std::vector<TaskItem> tasks;
  int viewIndex = 0; // 0: Face, 1: Stats, 2: Tasks
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
  tft.drawCircle(300, 20, 4, C_ACCENT); 
  
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/get-data");
  http.setTimeout(1500);
  int code = http.GET();
  
  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && !doc["sensor"].isNull()) {
        app.moisture = doc["sensor"]["moisture"];
        app.temp = doc["sensor"]["temperature"];
        app.humidity = doc["sensor"]["humidity"];
        app.light = doc["sensor"]["light"];
        
        app.gas = doc["sensor"]["gas"];
        app.water_level = doc["sensor"]["water_level"];
        app.touch = doc["sensor"]["touch"];
        app.pump_state = doc["sensor"]["pump_state"];

        if(app.mood != "EXCITED" || millis() - app.notifTime > 5000) {
             app.mood = doc["sensor"]["mood"].as<String>();
        }

        if(app.viewIndex == 2) { 
             app.tasks.clear();
             for(JsonObject v : doc["tasks"].as<JsonArray>()) {
                 app.tasks.push_back({v["_id"], v["title"], v["done"]});
             }
             app.needsRedraw = true;
        }
        
        if(app.viewIndex == 1) {
             app.needsRedraw = true;
        }
    }
  }
  http.end();
  tft.drawCircle(300, 20, 4, C_BG_DARK); 
}

// Function to toggle task state on server
void toggleTask(String id, bool currentState) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    http.begin(String(SERVER_BASE) + "/toggle-task/" + id + "?state=" + (currentState ? "false" : "true"));
    http.setTimeout(1500);
    int code = http.POST("");
    
    if (code == 200) {
        fetchData();
    }
    http.end();
}

// ==========================================
// 4. UI DRAWING FUNCTIONS
// ==========================================

// --- Helper: Draw a sleek card ---
void drawCard(int x, int y, int w, int h, uint16_t color) {
  tft.fillSmoothRoundRect(x, y, w, h, 12, color);
  tft.drawRoundRect(x, y, w, h, 12, C_BG_DARK + 0x18E3); 
}

// --- Component: Header (Includes Time - **FIXED: Removed Seconds**) ---
void drawHeader() {
  tft.fillRect(0, 0, 320, 40, C_BG_DARK);
  
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextColor(C_TEXT_SUB);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("EMO OS 2.0", 20, 20);
  
  // Time Display (NEW: Only Hours:Minutes)
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    tft.setTextDatum(MR_DATUM);
    tft.drawString("--:--", 260, 20);
  } else {
    char timeString[6]; // Changed from 9 to 6 for "HH:MM\0"
    strftime(timeString, 6, "%H:%M", &timeinfo); // **FIXED: Removed %S**
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_TEXT_MAIN);
    tft.drawString(timeString, 260, 20);
  }
  
  // WiFi Status
  uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? C_SUCCESS : C_ALERT;
  tft.drawArc(295, 20, 8, 6, 220, 320, wifiColor);
  tft.drawArc(295, 20, 5, 3, 220, 320, wifiColor);
  tft.fillCircle(295, 20, 2, wifiColor);
}

// --- Component: The Face (Cute View) - CORRECTED drawArc USAGE ---
void drawFace() {
  unsigned long now = millis();
  
  // Blinking Logic
  if (!isBlinking && now - lastBlink > (random(2000, 5000))) {
      isBlinking = true;
      lastBlink = now;
  }
  if (isBlinking) {
     eyeHeight -= 20; 
     if (eyeHeight <= 4) eyeHeight = 4;
     if (now - lastBlink > 100) { 
         isBlinking = false;
         lastBlink = now;
     }
  } else {
     eyeHeight += 10;
     if (eyeHeight > 70) eyeHeight = 70; 
  }

  // Draw to Sprite
  faceSpr.fillSprite(C_BG_DARK);

  uint16_t eyeColor = C_ACCENT;
  if(app.mood == "HAPPY") eyeColor = 0x5C05; 
  if(app.mood == "SAD") eyeColor = C_ACCENT; 
  if(app.mood == "EXCITED") eyeColor = 0xFDA0; 
  if(app.mood == "ANGRY") eyeColor = C_ALERT;

  int eyeW = 70;
  int eyeY = 60; 

  // Eyes
  faceSpr.fillSmoothRoundRect(40, eyeY - (eyeHeight/2), eyeW, eyeHeight, 20, eyeColor);
  faceSpr.fillSmoothRoundRect(170, eyeY - (eyeHeight/2), eyeW, eyeHeight, 20, eyeColor);

  // Mouth 
  int moodArc = 0;
  if(app.mood == "HAPPY" || app.mood == "EXCITED") moodArc = 1; 
  if(app.mood == "SAD") moodArc = -1; 
  
  int r0 = 20; // Inner Radius
  int r1 = 25; // Outer Radius
  
  if (moodArc == 1) {
      // FIX: Using drawEllipseArc to resolve compilation error
      faceSpr.drawEllipseArc(125, 100, r0, r1, r0, r1, 10.0, 170.0, C_TEXT_MAIN); // Smiling Arc
  } else if (moodArc == -1) {
      // FIX: Using drawEllipseArc
      faceSpr.drawEllipseArc(125, 120, r0, r1, r0, r1, 190.0, 350.0, C_TEXT_MAIN); // Frowning Arc
  }

  // Mood Text Below Face
  faceSpr.setFont(&fonts::FreeSansBold9pt7b);
  faceSpr.setTextColor(C_TEXT_MAIN);
  faceSpr.setTextDatum(MC_DATUM);
  faceSpr.drawString(app.mood, 125, 160);

  // The sprite is 250x200. Pushing it to (35, 80) means it occupies (35, 80) to (285, 280).
  faceSpr.pushSprite(35, 80); 
}

// --- Screen 1: Main (Face) View - **FIXED: Text position** ---
void drawMainView() {
    tft.fillScreen(C_BG_DARK);
    drawHeader();
    drawFace();
    
    // Status text moved lower to avoid overlap with the face sprite (which ends at y=280)
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(C_TEXT_SUB);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Status: " + String((int)app.moisture) + "% Soil | " + String(app.temp, 1) + " C", 160, 300); // Moved to y=300
    tft.drawString("Tap the face to cheer EMO up!", 160, 340); // Moved to y=340
    
    app.needsRedraw = false;
}

// --- Screen 2: Sensor Stats View ---
void drawSensorStatsView() {
    if(!app.needsRedraw) return;
    tft.fillScreen(C_BG_DARK);
    drawHeader();
    
    tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.setTextColor(C_TEXT_MAIN);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Sensor Stats", 20, 65);

    int cardW = 135;
    int cardH = 60;
    int x1 = 20; int x2 = 165;
    int y = 90;
    
    auto drawStat = [&](int x, int y, const char* title, String value, uint16_t valColor) {
        drawCard(x, y, cardW, cardH, C_BG_CARD);
        tft.setFont(&fonts::FreeSans9pt7b);
        tft.setTextColor(C_TEXT_SUB); tft.drawString(title, x + cardW/2, y + 15);
        tft.setFont(&fonts::FreeSansBold12pt7b);
        tft.setTextColor(valColor); tft.drawString(value, x + cardW/2, y + 40);
    };

    // Row 1
    drawStat(x1, y, "TEMP", String(app.temp, 1) + " C", C_TEXT_MAIN);
    uint16_t humColor = (app.moisture < 30) ? C_ALERT : C_SUCCESS;
    drawStat(x2, y, "SOIL", String((int)app.moisture) + "%", humColor);
    y += cardH + 10;

    // Row 2
    drawStat(x1, y, "HUMIDITY", String(app.humidity) + "%", C_TEXT_MAIN);
    drawStat(x2, y, "LIGHT", String(app.light) + " Lux", C_TEXT_MAIN);
    y += cardH + 10;
    
    // Row 3 (New Sensors)
    uint16_t gasColor = (app.gas > 100) ? C_ALERT : C_SUCCESS;
    drawStat(x1, y, "GAS (PPM)", String(app.gas, 0), gasColor);
    uint16_t waterColor = (app.water_level < 5) ? C_ALERT : C_SUCCESS;
    drawStat(x2, y, "WATER LVL", String(app.water_level, 0) + " cm", waterColor);
    y += cardH + 10;

    // Row 4 (Binary States)
    drawStat(x1, y, "TOUCH", app.touch ? "Active" : "Idle", app.touch ? C_ALERT : C_TEXT_SUB);
    drawStat(x2, y, "PUMP", app.pump_state ? "ON" : "OFF", app.pump_state ? C_SUCCESS : C_TEXT_SUB);

    app.needsRedraw = false;
}

// --- Screen 3: Tasks View ---
void drawTasksView() {
    if(!app.needsRedraw) return;
    
    tft.fillScreen(C_BG_DARK);
    drawHeader();
    
    tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.setTextColor(C_TEXT_MAIN);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("My Tasks", 20, 65);

    int y = 90;
    if(app.tasks.size() == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setFont(&fonts::FreeSans9pt7b);
        tft.setTextColor(C_TEXT_SUB);
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
        if(y > 400) break; // Limit tasks visible above the navbar
    }
    app.needsRedraw = false;
}

// --- Component: Navigation Bar ---
void drawNavBar(int activeIndex) {
    const int BAR_H = 60;
    const int BAR_Y = 480 - BAR_H;
    tft.fillRect(0, BAR_Y, 320, BAR_H, C_NAV_BAR);
    tft.drawFastHLine(0, BAR_Y, 320, C_BG_DARK);

    const int BUTTON_W = 320 / 3;
    const char* labels[] = {"Face", "Stats", "Tasks"};
    
    tft.setFont(&fonts::FreeSansBold9pt7b);
    tft.setTextDatum(MC_DATUM);

    for (int i = 0; i < 3; i++) {
        int x = (i * BUTTON_W) + (BUTTON_W / 2);
        
        uint16_t bgColor = (i == activeIndex) ? C_ACCENT : C_NAV_BAR;
        uint16_t txtColor = (i == activeIndex) ? C_TEXT_MAIN : C_TEXT_SUB;
        
        if (i == activeIndex) {
            tft.fillRect(i * BUTTON_W, BAR_Y, BUTTON_W, BAR_H, bgColor);
        }
        
        tft.setTextColor(txtColor, bgColor);
        tft.drawString(labels[i], x, BAR_Y + BAR_H/2);
    }
}


// ==========================================
// 5. SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(C_BG_DARK);
  
  // *** ADDED: Touch Calibration ***
  tft.setTextDatum(MC_DATUM);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT_MAIN);
  tft.drawString("Touch Calibration...", 160, 180);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.drawString("Please touch the markers to calibrate.", 160, 220);
  uint16_t calData[8];
  tft.calibrateTouch(calData, C_TEXT_MAIN, C_BG_DARK, 15); 
  tft.fillScreen(C_BG_DARK);
  // ******************************

  // Initialize Sprite
  faceSpr.setColorDepth(16);
  faceSpr.createSprite(250, 200); // 250x200 sprite size

  tft.setTextDatum(MC_DATUM);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT_MAIN);
  tft.drawString("WiFi Connecting...", 160, 220);
  tft.drawRect(60, 240, 200, 10, C_TEXT_SUB);
  
  delay(500);

  // WiFi Setup
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int w = 0;
  while (WiFi.status() != WL_CONNECTED && w < 20) { 
      delay(250); w++;
      tft.fillRect(63, 243, 194.0 * ((float)w / 20.0), 4, C_ACCENT);
  }
  
  // Time Setup
  if (WiFi.status() == WL_CONNECTED) {
      // Adjust the 3 * 3600 for your local timezone offset in seconds (e.g., Tehran is +3:30, so 12600)
      configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
  }
  
  // Initial draw
  drawMainView(); // Start on Face View
  drawNavBar(app.viewIndex);
}

unsigned long lastNetCheck = 0;

void loop() {
  // 1. Navigation & Screen Redraw
  if (app.needsRedraw) {
      if (app.viewIndex == 0) {
          drawMainView();
      } else if (app.viewIndex == 1) {
          drawSensorStatsView();
      } else if (app.viewIndex == 2) {
          drawTasksView();
      }
      drawNavBar(app.viewIndex); // Redraw navbar on view change
      app.needsRedraw = false;
  }
  
  // 2. Animation Loop (Only for Main View)
  if (app.viewIndex == 0) {
      drawFace(); 
      // Update header time frequently
      static unsigned long lastHeaderDraw = 0;
      if (millis() - lastHeaderDraw > 10000) { // Time check every 10 seconds (minutes are less frequent)
          drawHeader();
          lastHeaderDraw = millis();
      }
  }

  // 3. Touch Logic (Button and Interaction)
  int32_t x, y;
  bool touched = tft.getTouch(&x, &y);
  static bool wasTouched = false;

  if (touched && !wasTouched) {
      wasTouched = true;
      
      const int BAR_H = 60;
      const int BAR_Y = 480 - BAR_H;
      const int BUTTON_W = 320 / 3;
      
      // A) Navigation Bar Tap (Bottom 60 pixels)
      if (y > BAR_Y) { 
          int newIndex = -1;
          if (x < BUTTON_W) newIndex = 0; // Face
          else if (x < 2 * BUTTON_W) newIndex = 1; // Stats
          else newIndex = 2; // Tasks

          if (newIndex != app.viewIndex) {
              app.viewIndex = newIndex;
              app.needsRedraw = true; // Trigger full screen redraw
          }
      }
      // B) Task Toggle (View 2) - **IMPROVED BOUNDARIES**
      else if (app.viewIndex == 2) {
          if (y > 80 && y < 400 && x > 15 && x < 305) { // Check if touch is within the general task list area
            int taskY = y - 90; // y relative to the start of the first task (y=90)
            int taskIndex = taskY / 60; // 60 is card height + spacing
            int yOffset = taskY % 60; // y offset within the 60-pixel block
            
            // Only consider a touch if it's on the card itself (not the 10-pixel gap)
            if (taskIndex >= 0 && taskIndex < app.tasks.size() && yOffset < 50) { 
                toggleTask(app.tasks[taskIndex].id, app.tasks[taskIndex].done);
            }
          }
      }
      // C) Face Tap (View 0)
      else if (app.viewIndex == 0) {
           // Tap to interact with EMO
           app.mood = "EXCITED"; 
           app.notifTime = millis();
      }
  }
  
  if (!touched) {
      wasTouched = false;
  }

  // 4. Network Task
  if (millis() - lastNetCheck > 10000) { 
      fetchData();
      lastNetCheck = millis();
  }
  
  delay(10); 
}