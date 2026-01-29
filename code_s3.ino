/**
 * ============================================================================
 *  Project: Kawaii Plant Monitor (FINAL COMPLETE VERSION)
 *  Features: 
 *    1. Stable WiFi & RAM (Small Sprite)
 *    2. Working Task Manager (Swipe Up)
 *    3. Direct Drawing for UI (No Flicker)
 *    4. Smooth Face Animation
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- تنظیمات ---
const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 
// **مهم**: IP سیستم خود را جایگزین کنید
const char* SERVER_BASE   = "http://10.156.45.197:8000"; 

// --- رنگ‌ها ---
#define C_BG_PLANT  0xF7F9 
#define C_BG_TASK   0xE0F7 
#define C_FACE_BG   0xF7F9 
#define C_CARD      0xFFFF 
#define C_TEXT      0x39E7 
#define C_ACCENT    0xFB56 
#define C_CHECK     0x2E8B 

// --- درایور نمایشگر (ILI9488 Parallel) ---
class KawaiiDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  KawaiiDisplay(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 20000000; 
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
      cfg.pin_cs = 40; cfg.pin_rst = 41; cfg.pin_busy = -1;
      cfg.panel_width = 320; cfg.panel_height = 480;
      cfg.readable = true; cfg.dlen_16bit = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 300; cfg.x_max = 3800;
      cfg.y_min = 200; cfg.y_max = 3700;
      cfg.bus_shared = true;
      cfg.spi_host = SPI2_HOST; cfg.freq = 2500000;
      cfg.pin_sclk = 47; cfg.pin_mosi = 48; cfg.pin_miso = 20; cfg.pin_cs = 2;         
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

KawaiiDisplay tft;
LGFX_Sprite sprFace(&tft); // اسپیرایت کوچک برای صورت (RAM Safe)

// --- متغیرها ---
struct TaskItem { String name; bool done; };
struct AppData {
  float moisture; float temp; int humidity; int light;
  String mood = "HAPPY";
  std::vector<TaskItem> tasks;
};
AppData appData;

enum ScreenMode { SCREEN_PLANT, SCREEN_TASKS };
ScreenMode currentScreen = SCREEN_PLANT;

float breathVal = 0;
bool isBlinking = false;
unsigned long lastBlink = 0;

// ==========================================
// 1. توابع شبکه (Network)
// ==========================================
void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/get-data");
  int code = http.GET();
  if (code > 0) {
    String payload = http.getString();
    // بافر کوچک برای جلوگیری از کرش
    StaticJsonDocument<1500> doc;
    deserializeJson(doc, payload);
    
    appData.moisture = doc["moisture"];
    appData.temp = doc["temp"];
    appData.humidity = doc["humidity"];
    appData.light = doc["light"];
    appData.mood = doc["mood"].as<String>();
    
    appData.tasks.clear();
    JsonArray tArr = doc["tasks"];
    for(JsonObject v : tArr) {
      appData.tasks.push_back({v["name"].as<String>(), v["done"]});
    }
  }
  http.end();
}

void toggleTaskServer(int index) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/toggle-task");
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"index\": " + String(index) + "}");
  http.end();
  fetchData(); 
}

void sendWater() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/send-touch");
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"action\": \"WATER\"}");
  http.end();
  appData.moisture = 100;
  appData.mood = "EXCITED";
}

// ==========================================
// 2. توابع گرافیکی (UI Drawing)
// ==========================================

void drawFace() {
  sprFace.fillSprite(C_FACE_BG);
  int cx = 70; // مرکز اسپیرایت 140 پیکسلی
  int cy = 70 + (int)breathVal;
  
  // بدنه صورت
  sprFace.fillEllipse(cx, cy + 50, 40, 6, 0xE71C); // سایه
  sprFace.fillEllipse(cx, cy, 60, 55, 0xFFFF);
  sprFace.drawEllipse(cx, cy, 60, 55, C_TEXT);

  // چشم‌ها
  uint16_t ec = C_TEXT;
  if(appData.mood == "SAD") {
     sprFace.fillEllipse(cx-20, cy, 8, 12, ec); sprFace.fillEllipse(cx+20, cy, 8, 12, ec);
     sprFace.drawArc(cx, cy+30, 8, 6, 180, 360, ec);
     sprFace.fillCircle(cx-25, cy+10, 3, 0x001F);
  } else if (appData.mood == "EXCITED") {
     sprFace.drawString(">", cx-30, cy-5, 2); sprFace.drawString("<", cx+15, cy-5, 2);
     sprFace.fillEllipse(cx, cy+25, 6, 8, C_ACCENT);
  } else if (isBlinking || appData.mood == "SLEEPY") {
     sprFace.drawArc(cx-20, cy, 12, 10, 180, 360, ec); sprFace.drawArc(cx+20, cy, 12, 10, 180, 360, ec);
  } else {
     sprFace.fillEllipse(cx-20, cy, 8, 12, ec); sprFace.fillEllipse(cx+20, cy, 8, 12, ec);
     sprFace.fillCircle(cx-22, cy-3, 2, 0xFFFF); sprFace.fillCircle(cx+18, cy-3, 2, 0xFFFF);
     sprFace.drawArc(cx, cy+20, 8, 6, 0, 180, ec);
  }
  
  // انیمیشن تنفس
  breathVal = sin(millis()/500.0)*2.0;
  if (!isBlinking && millis()-lastBlink > 3000) { isBlinking=true; lastBlink=millis(); }
  if (isBlinking && millis()-lastBlink > 150) { isBlinking=false; lastBlink=millis(); }

  // رسم در سمت چپ صفحه
  sprFace.pushSprite(20, 100);
}

void drawPlantData() {
  int bx = 180; // شروع پنل دیتا
  
  // رسم 4 کارت (مستقیم روی صفحه)
  struct CardInfo { int x; int y; String t; String v; uint16_t c; };
  CardInfo cards[] = {
    {bx+10, 50, "Soil", String((int)appData.moisture)+"%", 0x4D19},
    {bx+150, 50, "Temp", String(appData.temp,1), 0xFC00},
    {bx+10, 125, "Light", String(appData.light)+"%", 0xFE60},
    {bx+150, 125, "Air", String(appData.humidity)+"%", 0x867F}
  };
  
  for(auto& c : cards) {
    tft.fillSmoothRoundRect(c.x, c.y, 130, 65, 10, C_CARD);
    tft.fillSmoothCircle(c.x+20, c.y+32, 14, c.c);
    tft.setTextColor(0x9CD3); tft.setFont(&fonts::FreeSans9pt7b);
    tft.drawString(c.t, c.x+45, c.y+10);
    tft.setTextColor(C_TEXT); tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.drawString(c.v, c.x+45, c.y+30);
  }
  
  // دکمه آب
  tft.fillSmoothRoundRect(bx+10, 250, 270, 50, 15, C_TEXT);
  tft.setTextColor(0xFFFF); tft.setTextDatum(MC_DATUM);
  tft.drawString("WATER ME!", bx+145, 275);
  
  // متن راهنما برای Swipe
  tft.setTextColor(0x9CD3); tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("^ Swipe UP for Tasks ^", 240, 315);
  tft.setTextDatum(TL_DATUM);
}

void drawTaskScreen() {
  tft.setTextColor(C_TEXT);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.drawString("My Tasks", 20, 20);
  
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.drawString("v Swipe DOWN to Back v", 20, 50);
  
  int y = 80;
  for (int i = 0; i < appData.tasks.size(); i++) {
    tft.fillSmoothRoundRect(20, y, 440, 50, 10, C_CARD);
    if (appData.tasks[i].done) {
      tft.fillSmoothRoundRect(30, y+10, 30, 30, 5, C_CHECK);
      tft.setTextColor(C_CARD); tft.drawString("v", 38, y+15);
    } else {
      tft.drawRoundRect(30, y+10, 30, 30, 5, C_TEXT);
    }
    tft.setTextColor(C_TEXT); tft.setFont(&fonts::FreeSans12pt7b);
    tft.drawString(appData.tasks[i].name, 75, y+15);
    y += 60;
  }
}

// انیمیشن پرده‌ای برای تغییر صفحه
void animateSwitch(ScreenMode target) {
  uint16_t targetColor = (target == SCREEN_TASKS) ? C_BG_TASK : C_BG_PLANT;
  int step = 60; 
  
  // افکت Wipe
  if (target == SCREEN_TASKS) {
    for (int y = 320; y >= 0; y -= step) {
      tft.fillRect(0, y, 480, step, targetColor);
      delay(10);
    }
  } else {
    for (int y = 0; y < 320; y += step) {
      tft.fillRect(0, y, 480, step, targetColor);
      delay(10);
    }
  }
  
  tft.fillScreen(targetColor);
  
  // رسم محتوای صفحه جدید
  if (target == SCREEN_TASKS) {
    drawTaskScreen();
  } else {
    tft.setTextColor(C_TEXT); tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.drawString("Plant Status", 190, 10);
    drawPlantData(); 
  }
  currentScreen = target;
}

// ==========================================
// 3. ورودی و کنترل (Input Logic)
// ==========================================
int16_t startY = -1;

void checkSwipeAndTouch() {
  int32_t x, y;
  bool touched = tft.getTouch(&x, &y);
  
  if (touched) {
    if (startY == -1) startY = y; // شروع لمس
  } else {
    // انگشت برداشته شد
    if (startY != -1) {
       // اینجا میتونیم کلیک رو تشخیص بدیم ولی بهتره در لحظه تاچ تشخیص بدیم
    }
    startY = -1; // ریست
  }
  
  // اگر انگشت روی صفحه است
  if (touched && startY != -1) {
    int dist = startY - y; // مثبت = حرکت به بالا
    
    // تشخیص Swipe
    if (dist > 50 && currentScreen == SCREEN_PLANT) {
      animateSwitch(SCREEN_TASKS);
      startY = -1; // ریست تا وقتی دست برداشته شه
      return;
    }
    if (dist < -50 && currentScreen == SCREEN_TASKS) {
      animateSwitch(SCREEN_PLANT);
      startY = -1;
      return;
    }
    
    // تشخیص کلیک (اگر حرکت کم بود)
    if (abs(dist) < 10) {
       // برای جلوگیری از کلیک رگباری، یک فلگ ساده
       static unsigned long lastClick = 0;
       if (millis() - lastClick > 300) {
          // کلیک در صفحه گیاه
          if (currentScreen == SCREEN_PLANT) {
              if (x > 190 && y > 250 && y < 310) {
                  sendWater();
                  tft.fillSmoothRoundRect(190, 250, 270, 50, 15, C_ACCENT);
                  delay(150);
                  drawPlantData();
                  lastClick = millis();
              }
          }
          // کلیک در صفحه تسک‌ها
          else if (currentScreen == SCREEN_TASKS) {
              int idx = (y - 80) / 60;
              if (idx >= 0 && idx < appData.tasks.size()) {
                  // تیک زدن گرافیکی فوری
                  appData.tasks[idx].done = !appData.tasks[idx].done;
                  drawTaskScreen();
                  // ارسال به سرور
                  toggleTaskServer(idx);
                  lastClick = millis();
              }
          }
       }
    }
  }
}

// ==========================================
// Setup & Loop
// ==========================================
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG_PLANT);
  
  // اسپیرایت 140x140 برای صورت (رم امن)
  sprFace.createSprite(140, 140);
  
  tft.setTextColor(C_TEXT);
  tft.drawString("Connecting...", 220, 160);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!");
  
  fetchData();
  
  // رسم اولیه
  tft.fillScreen(C_BG_PLANT);
  tft.setTextColor(C_TEXT);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.drawString("Plant Status", 190, 10);
  drawPlantData();
}

unsigned long lastNetCheck = 0;
unsigned long lastDrawCheck = 0;

void loop() {
  checkSwipeAndTouch(); // همیشه ورودی را چک کن
  
  // فقط اگر در صفحه اصلی هستیم صورت را انیمیت کن
  if (currentScreen == SCREEN_PLANT) {
    drawFace(); 
    
    // آپدیت دیتا هر 2 ثانیه (فقط گرافیک)
    if (millis() - lastDrawCheck > 2000) {
      drawPlantData();
      lastDrawCheck = millis();
    }
  }
  
  // سینک با سرور هر 2 ثانیه
  if (millis() - lastNetCheck > 2000) {
    fetchData();
    // اگر در صفحه تسک هستیم و دیتا عوض شد، رفرش کن
    if (currentScreen == SCREEN_TASKS) {
       // اینجا میتونیم رفرش کنیم اگر لازم بود
       // drawTaskScreen(); 
    }
    lastNetCheck = millis();
  }
}