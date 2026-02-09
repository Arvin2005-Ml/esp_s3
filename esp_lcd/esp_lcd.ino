/**
============================================================================
Project: EMO 2.0 - REVOLUTIONARY UI (Fixed for LovyanGFX Compilation)
Optimized for ESP32-S3 (Internal RAM Only)
Display: ILI9488 Parallel 16-bit | Touch: XPT2046 SPI
NEW FEATURE: Eye Tracking, Minimal UI Animation, Job-Based/State Architecture
============================================================================
*/
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <time.h> 

// ==========================================
// 1. CONFIGURATION & DRIVERS
// ==========================================
const char* WIFI_SSID     = "arvinshokouhi"; // MUST BE CHANGED
const char* WIFI_PASS     = "Arvin1384";     // MUST BE CHANGED
const char* SERVER_BASE   = "http://10.103.41.197:8000"; // MUST BE CHANGED

// --- Modern Color Palette (Cyber/Minimal) ---
#define C_BG_DARK     0x10A2  // Dark Grey/Blueish
#define C_BG_CARD     0x2124  // Slightly lighter for cards
#define C_ACCENT      0x5CF4  // Soft Blue/Purple (Main Accent)
#define C_TEXT_MAIN   0xFFFF  // White
#define C_TEXT_SUB    0x9CD3  // Grey Text
#define C_SUCCESS     0x0660  // Muted Green
#define C_ALERT       0xE8E4  // Soft Red
#define C_NAV_BAR     0x18A3
#define C_PRIMARY     0x5CF4  // Primary Accent (Soft Blue/Purple) - Previously C_ACCENT
#define C_SECONDARY   0x2124
#define C_BG          0x10A2
#define C_WHITE       0xFFFF
#define C_GRAY        0x9CD3

  // Secondary/Card Background (Slightly Lighter) - Previously C_BG_CARD
  // Darker for navigation bar

// --- LGFX Driver (ESP32-S3 Specific ILI9488 16-bit) ---
class LGFX : public lgfx::LGFX_Device {
lgfx::Panel_ILI9488 _panel_instance;
lgfx::Bus_Parallel16 _bus_instance;
lgfx::Touch_XPT2046 _touch_instance;
public:
LGFX(void) {
{
auto cfg = _bus_instance.config();
// S3 Parallel 16-bit Pins
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
cfg.x_min = 300; cfg.x_max = 3800; // Calibrated later
cfg.y_min = 250; cfg.y_max = 3700; // Calibrated later
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

// Sprite optimized for EMO Face (Partial Redraw)
const int FACE_SPRITE_X = 35; 
const int FACE_SPRITE_Y = 80;
LGFX_Sprite faceSpr(&tft);

// ==========================================
// 2. DATA STRUCTURES & STATE
// ==========================================
struct TaskItem { String id; String name; bool done; };

struct AppState {
float moisture = 0; float temp = 0; int humidity = 0; int light = 0;
float gas = 0; float water_level = 0; bool touch = false; bool pump_state = false;
String mood = "NEUTRAL";
std::vector<TaskItem> tasks;
int viewIndex = 0; // 0: Face, 1: Stats, 2: Tasks
bool needsRedraw = true; // Full screen redraw flag
String notification = "";
unsigned long notifTime = 0;
};
AppState app;

// UI & Animation State
unsigned long lastBlink = 0;
bool isBlinking = false;
int eyeHeight = 70; // Max open height

// Eye Tracking State
int eyeOffsetX = 0; 
int eyeOffsetY = 0;
int eyeTargetX = 160; 
int eyeTargetY = 240;
const int EYE_MAX_MOVE = 7; 
const float EYE_SMOOTH_FACTOR = 0.15; 

// Debounce for Touch Task
unsigned long lastTaskToggle = 0;

// ==========================================
// 3. LOGIC & NETWORK (Job-Based Architecture)
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
            // مدیریت هر دو ساختار داده
            if (!doc["sensor"]["temperature"].isNull()) {
                app.temp = doc["sensor"]["temperature"].as<float>();
            } else if (!doc["sensor"]["temp"].isNull()) {
                app.temp = doc["sensor"]["temp"].as<float>();
            }
            
            // رطوبت خاک
            if (!doc["sensor"]["moisture"].isNull()) {
                app.moisture = doc["sensor"]["moisture"].as<float>();
            }
            
            // رطوبت هوا
            if (!doc["sensor"]["humidity"].isNull()) {
                app.humidity = doc["sensor"]["humidity"].as<int>();
            }
            
            // نور
            if (!doc["sensor"]["light"].isNull()) {
                app.light = doc["sensor"]["light"].as<int>();
            }
            
            // گاز (اختیاری)
            if (!doc["sensor"]["gas"].isNull()) {
                app.gas = doc["sensor"]["gas"].as<float>();
            }
            
            // سطح آب (اختیاری)
            if (!doc["sensor"]["water_level"].isNull()) {
                app.water_level = doc["sensor"]["water_level"].as<float>();
            }
            
            // لمس (اختیاری)
            if (!doc["sensor"]["touch"].isNull()) {
                app.touch = doc["sensor"]["touch"].as<bool>();
            }
            
            // پمپ (اختیاری)
            if (!doc["sensor"]["pump_state"].isNull()) {
                app.pump_state = doc["sensor"]["pump_state"].as<bool>();
            }

            // مدیریت خلق و خوی
            if(app.mood != "EXCITED" || millis() - app.notifTime > 5000) {
                if (!doc["sensor"]["mood"].isNull()) {
                    app.mood = doc["sensor"]["mood"].as<String>();
                }
            }

            // به‌روزرسانی نمایشگر
            if(app.viewIndex == 2) { 
                app.tasks.clear();
                for(JsonObject v : doc["tasks"].as<JsonArray>()) {
                    app.tasks.push_back({v["_id"], v["title"], v["done"]});
                }
                app.needsRedraw = true;
            }
            
            if(app.viewIndex == 1 || app.viewIndex == 0) {
                app.needsRedraw = true; 
            }
        }
    }
    http.end();
    tft.drawCircle(300, 20, 4, C_BG_DARK);
}


void toggleTask(String id, bool currentState) {
if (WiFi.status() != WL_CONNECTED || (millis() - lastTaskToggle < 500)) return; // Debounce 
lastTaskToggle = millis();

// ... POST request to toggle task on server
HTTPClient http;
http.begin(String(SERVER_BASE) + "/toggle-task/" + id + "?state=" + (currentState ? "false" : "true"));
http.setTimeout(1500);

int code = http.POST("");
if (code == 200) {
    // Force immediate data fetch to get new state, minimal lag
    fetchData(); 
}
http.end();
}

// ==========================================
// 4. UI DRAWING FUNCTIONS
// ==========================================

void drawCard(int x, int y, int w, int h, uint16_t color) {
tft.fillSmoothRoundRect(x, y, w, h, 12, color);
tft.drawRoundRect(x, y, w, h, 12, C_BG_DARK + 0x18E3); 
}

void drawHeader() {
tft.fillRect(0, 0, 320, 40, C_BG_DARK);
tft.setFont(&fonts::FreeSansBold9pt7b);
tft.setTextColor(C_TEXT_SUB);
tft.setTextDatum(ML_DATUM);
tft.drawString("EMO OS 2.0", 20, 20);

// Time Display (Minimal update)
struct tm timeinfo;
char timeString[6]; 
tft.setTextDatum(MR_DATUM);
if(!getLocalTime(&timeinfo)){
    tft.drawString("--:--", 260, 20);
} else {
    strftime(timeString, 6, "%H:%M", &timeinfo);
    tft.setTextColor(C_TEXT_MAIN);
    tft.drawString(timeString, 260, 20);
}

// WiFi Status (Partial Draw of arc animation only)
uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? C_SUCCESS : C_ALERT;
tft.drawArc(295, 20, 8, 6, 220, 320, wifiColor);
tft.drawArc(295, 20, 5, 3, 220, 320, wifiColor);
tft.fillCircle(295, 20, 2, wifiColor);
}

// --- Component: The Face (Cute View) - Partial Redraw Only ---
void drawFace() {
unsigned long now = millis();

// Blinking Logic
if (!isBlinking && now - lastBlink > (random(2000, 5000))) {
isBlinking = true; lastBlink = now;
}
// Blinking "Squash" Effect
if (isBlinking) {
eyeHeight -= 30; if (eyeHeight <= 4) eyeHeight = 4;
if (now - lastBlink > 150) { isBlinking = false; lastBlink = now; }
} else {
eyeHeight += 10; if (eyeHeight > 70) eyeHeight = 70;
}

// Eye Tracking Damping
int spriteCenterScreenX = FACE_SPRITE_X + faceSpr.width()/2;
int spriteCenterScreenY = FACE_SPRITE_Y + faceSpr.height()/2;
int maxDist = 40; 
int targetScreenShiftX = constrain(eyeTargetX - spriteCenterScreenX, -maxDist, maxDist);
int targetScreenShiftY = constrain(eyeTargetY - spriteCenterScreenY, -maxDist, maxDist);
int targetOffsetX = constrain((int)(targetScreenShiftX / (maxDist / (EYE_MAX_MOVE*1.0f))), -EYE_MAX_MOVE, EYE_MAX_MOVE);
int targetOffsetY = constrain((int)(targetScreenShiftY / (maxDist / (EYE_MAX_MOVE*1.0f))), -EYE_MAX_MOVE, EYE_MAX_MOVE);

// Smooth Interpolation
eyeOffsetX = (int)(eyeOffsetX + (targetOffsetX - eyeOffsetX) * EYE_SMOOTH_FACTOR); 
eyeOffsetY = (int)(eyeOffsetY + (targetOffsetY - eyeOffsetY) * EYE_SMOOTH_FACTOR); 

faceSpr.fillSprite(C_BG_DARK); 
uint16_t eyeColor = C_ACCENT;
if(app.mood == "HAPPY") eyeColor = 0x5C05; 
if(app.mood == "SAD") eyeColor = C_ACCENT;
if(app.mood == "EXCITED") eyeColor = 0xFDA0;
if(app.mood == "ANGRY") eyeColor = C_ALERT;

int eyeW = 70;
int eyeY = 60; 

int eyeShiftX = eyeOffsetX;
int eyeShiftY = eyeOffsetY;

// Left Eye
faceSpr.fillSmoothRoundRect(40 + eyeShiftX, eyeY - (eyeHeight/2) + eyeShiftY, eyeW, eyeHeight, 20, eyeColor);
// Right Eye
faceSpr.fillSmoothRoundRect(170 + eyeShiftX, eyeY - (eyeHeight/2) + eyeShiftY, eyeW, eyeHeight, 20, eyeColor);

// Mouth 
int moodArc = 0;
if(app.mood == "HAPPY" || app.mood == "EXCITED") moodArc = 1;
if(app.mood == "SAD") moodArc = -1;
int r0 = 20; 
int r1 = 25; 

// Gentle "Breathing" Shake 
float shake = sin(millis() / 500.0) * 0.8;
int mX = (int)shake;

if (moodArc == 1) {
faceSpr.drawEllipseArc(125 + mX, 100 + mX, r0, r1, r0, r1, 10.0, 170.0, C_TEXT_MAIN); 
} else if (moodArc == -1) {
faceSpr.drawEllipseArc(125 + mX, 120 + mX, r0, r1, r0, r1, 190.0, 350.0, C_TEXT_MAIN);
}

// Mood Text
faceSpr.setFont(&fonts::FreeSansBold9pt7b);
faceSpr.setTextColor(C_TEXT_MAIN);
faceSpr.setTextDatum(MC_DATUM);
faceSpr.drawString(app.mood, 125, 160);

// Push sprite is the only heavy draw on this screen
faceSpr.pushSprite(FACE_SPRITE_X, FACE_SPRITE_Y);
}

void drawMainView() {
tft.fillScreen(C_BG_DARK);
drawHeader();
drawFace();

tft.setFont(&fonts::FreeSans9pt7b);
tft.setTextColor(C_TEXT_SUB);
tft.setTextDatum(MC_DATUM);
tft.drawString("Status: " + String((int)app.moisture) + "% Soil | " + String(app.temp, 1) + " C", 160, 300);
tft.drawString("Tap the face to cheer EMO up!", 160, 340); 
app.needsRedraw = false;
}

void drawSensorStatsView() {
if(!app.needsRedraw) return;

tft.fillScreen(C_BG_DARK);
drawHeader();

tft.setFont(&fonts::FreeSansBold12pt7b);
tft.setTextColor(C_TEXT_MAIN);
tft.setTextDatum(ML_DATUM);
tft.drawString("Sensor Stats", 20, 65);

int cardW = 135; int cardH = 60;
int x1 = 20; int x2 = 165;
int y = 90;

// The drawStat lambda performs the drawing and includes the pop/blink visual job
auto drawStat = [&](int x, int y, const char* title, String value, uint16_t valColor) {
    drawCard(x, y + 1, cardW, cardH, C_BG_CARD); 
    // Small draw shadow to simulate depth/pop
    tft.fillSmoothRoundRect(x, y, cardW, cardH, 12, C_BG_CARD); 
    

    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(C_TEXT_SUB); 
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, x + cardW/2, y + 15);
    
    tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.setTextColor(valColor); 
    tft.drawString(value, x + cardW/2, y + 40); 
};

// Data Population (Partial/Immediate Update when Redraw is True)
drawStat(x1, y, "TEMP", String(app.temp, 1) + " C", C_TEXT_MAIN);
uint16_t humColor = (app.moisture < 30) ? C_ALERT : C_SUCCESS;
drawStat(x2, y, "SOIL", String((int)app.moisture) + "%", humColor);
y += cardH + 10;

drawStat(x1, y, "HUMIDITY", String(app.humidity) + "%", C_TEXT_MAIN);
drawStat(x2, y, "LIGHT", String(app.light) + " Lux", C_TEXT_MAIN);
y += cardH + 10;

uint16_t gasColor = (app.gas > 100) ? C_ALERT : C_SUCCESS;
drawStat(x1, y, "GAS (PPM)", String(app.gas, 0), gasColor);
uint16_t waterColor = (app.water_level < 5) ? C_ALERT : C_SUCCESS;
drawStat(x2, y, "WATER LVL", String(app.water_level, 0) + " cm", waterColor);
y += cardH + 10;

drawStat(x1, y, "TOUCH", app.touch ? "Active" : "Idle", app.touch ? C_ALERT : C_TEXT_SUB);
drawStat(x2, y, "PUMP", app.pump_state ? "ON" : "OFF", app.pump_state ? C_SUCCESS : C_TEXT_SUB);

app.needsRedraw = false;
}

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

for(size_t i = 0; i < app.tasks.size(); ++i) {
    const auto& t = app.tasks[i];
    uint16_t cardColor = t.done ? 0x1245 : C_BG_CARD; 
    uint16_t txtColor = t.done ? 0x6408 : C_TEXT_MAIN;
    
    drawCard(20, y, 280, 50, cardColor);
    
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(txtColor);
    tft.drawString(t.name, 40, y + 25);
    
    // Status Indicator
    if(t.done) tft.fillCircle(270, y+25, 6, C_SUCCESS);
    else tft.drawCircle(270, y+25, 6, C_TEXT_SUB);
    
    y += 60;
    if(y > 400) break; 
}
app.needsRedraw = false;
}

// ==========================================
// تابع اصلاح‌شده رسم نوار پایین (Navbar)
// ==========================================
void drawNavBar(int activeIndex) {
  // تنظیمات ابعاد
  const int BAR_Y = 430;
  const int BAR_H = 50;
  const int BUTTON_W = 320 / 3;
  
  // لیبل دکمه‌ها
  const char* labels[] = { "HOME", "DATA", "JOBS" }; 

  // تنظیم فونت و چیدمان متن
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.setTextDatum(MC_DATUM); // Middle Center Alignment

  for (int i = 0; i < 3; i++) {
    // محاسبه مرکز دکمه
    int x = (i * BUTTON_W) + (BUTTON_W / 2);
    
    // تشخیص دکمه فعال
    bool isActive = (i == activeIndex);

    // افکت فشردن دکمه (Squash Effect)
    // اگر دکمه فعال باشد، متن 2 پیکسل پایین‌تر می‌رود
    int shiftY = isActive ? 2 : 0;

    // انتخاب رنگ‌ها
    // C_BG و C_PRIMARY در کدهای قبلی تعریف شده‌اند
    uint16_t bgColor = isActive ? C_PRIMARY : C_BG; 
    uint16_t txtColor = isActive ? C_WHITE : C_GRAY;

    // 1. رسم پس‌زمینه دکمه
    tft.fillRect(i * BUTTON_W, BAR_Y, BUTTON_W, BAR_H, bgColor);
    
    // (اختیاری) خط جداکننده بین دکمه‌ها
    if (!isActive) {
        tft.drawFastVLine((i+1) * BUTTON_W, BAR_Y + 10, BAR_H - 20, 0x2124);
    }

    // 2. رسم متن با اعمال افکت جابجایی (shiftY)
    tft.setTextColor(txtColor);
    tft.drawString(labels[i], x, BAR_Y + (BAR_H / 2) + shiftY);
  }

  // خط تزئینی بالای نوار ابزار
  tft.drawFastHLine(0, BAR_Y, 320, 0x2124); 
}
// ==========================================
// 5. SETUP & LOOP
// ==========================================
void setup() {
Serial.begin(115200);
tft.init();
tft.setRotation(0);
tft.fillScreen(C_BG_DARK);
// --- Touch Calibration ---
tft.setTextDatum(MC_DATUM);
tft.setFont(&fonts::FreeSansBold12pt7b);
tft.setTextColor(C_TEXT_MAIN);
tft.drawString("Touch Calibration...", 160, 180);
tft.setFont(&fonts::FreeSans9pt7b);
tft.drawString("Please touch the markers to calibrate.", 160, 220);
uint16_t calData[8];
tft.calibrateTouch(calData, C_TEXT_MAIN, C_BG_DARK, 15);
tft.fillScreen(C_BG_DARK);
// -------------------------

faceSpr.setColorDepth(16);
faceSpr.createSprite(250, 200); 

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

// Time Setup (+3.5 hours for Tehran is 12600 seconds)
if (WiFi.status() == WL_CONNECTED) {
configTime(12600, 0, "pool.ntp.org", "time.nist.gov"); 
}

// Initial draw
drawMainView(); 
drawNavBar(app.viewIndex);
}

unsigned long lastNetCheck = 0;
// Static variable to track currently pressed nav button for squish effect
static int activeNavButton = -1; 

void loop() {
// 1. Navigation & Screen Redraw Job
if (app.needsRedraw) {
    // Only call the draw function if a full redraw is required
    if (app.viewIndex == 0) drawMainView();
    else if (app.viewIndex == 1) drawSensorStatsView();
    else if (app.viewIndex == 2) drawTasksView();
    drawNavBar(app.viewIndex); 
    app.needsRedraw = false; // Reset the full redraw flag
}

// 2. Animation Loop (Only for Main View) Job
if (app.viewIndex == 0) {
    drawFace(); 

    // Header Clock Redraw Job
    static unsigned long lastHeaderDraw = 0;
    if (millis() - lastHeaderDraw > 10000) { 
        drawHeader();
        lastHeaderDraw = millis();
    }
}


// 3. Touch Logic (Navigation, Task Toggle, Face Interaction) Job
int32_t x, y;
bool touched = tft.getTouch(&x, &y);
static bool wasTouched = false;
int pressedButton = -1; // Temp to store the currently touched nav button index

// Debounced touch start
if (touched) { 
    if(!wasTouched) { // New touch point (No multi-sample filtering, single read)
      wasTouched = true;
      const int BAR_H = 60;
      const int BAR_Y = 480 - BAR_H;
      const int BUTTON_W = 320 / 3;

      // A) Navigation Bar Tap (Bottom 60 pixels)
      if (y > BAR_Y) { 
          if (x < BUTTON_W) pressedButton = 0; // Face
          else if (x < 2 * BUTTON_W) pressedButton = 1; // Stats
          else pressedButton = 2; // Tasks

          // Immediate visual feedback (squish)
          activeNavButton = pressedButton;
          drawNavBar(app.viewIndex); 
          
          if (pressedButton != app.viewIndex) {
              app.viewIndex = pressedButton;
              app.needsRedraw = true; // Trigger full screen redraw
          }
      }

      // B) Task Toggle (View 2)
      else if (app.viewIndex == 2) {
          if (y > 80 && y < 400 && x > 15 && x < 305) { 
            int taskY = y - 90; 
            int taskIndex = taskY / 60; 
            int yOffset = taskY % 60; 
            
            if (taskIndex >= 0 && taskIndex < app.tasks.size() && yOffset < 50) { 
                // Job: Toggle Task State
                toggleTask(app.tasks[taskIndex].id, app.tasks[taskIndex].done);
            }
          }
      }

      // C) Face Tap (View 0)
      else if (app.viewIndex == 0) {
          app.mood = "EXCITED"; 
          app.notifTime = millis();
      }

    } // End of wasTouched == false block

    // D) Eye Tracking Target Update (Follow touch immediately regardless of button)
    eyeTargetX = x;
    eyeTargetY = y;
}


if (!touched && wasTouched) { // Touch lift off
    wasTouched = false;
    
    // Release nav button effect
    if(activeNavButton != -1) {
        activeNavButton = -1;
        drawNavBar(app.viewIndex);
    }
    
    // Default Eye Target (Look at center or an area of interest when not touched)
    if(app.viewIndex == 0) { 
      eyeTargetX = 160; 
      eyeTargetY = 180;
    }
}


// 4. Network Task (Every 10 seconds)
if (millis() - lastNetCheck > 10000) {
fetchData();
lastNetCheck = millis();
}
delay(10); // Frame Rate Control (Approx 100 FPS max for animation budget)
}