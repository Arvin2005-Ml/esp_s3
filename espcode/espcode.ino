/**
 * ============================================================================
 *  Project: Smart Plant Monitor (Client)
 *  Hardware: ESP32-S3 + ILI9488 (Parallel) + XPT2046
 *  Server: Python FastAPI (Simulated Data)
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==========================================
// 1. تنظیمات شبکه
// ==========================================
const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 

// آدرس‌های سرور طبق درخواست شما
const char* SERVER_GET_URL  = "http://10.156.45.197:8000/get-data";
const char* SERVER_POST_URL = "http://10.156.45.197:8000/send-touch";

// ==========================================
// 2. پالت رنگ (Modern UI)
// ==========================================
#define C_BG       0xEF7D // خاکستری خیلی روشن مایل به آبی
#define C_CARD     0xFFFF // سفید
#define C_TEXT     0x2124 // مشکی ذغالی
#define C_WATER    0x4416 // آبی تیره
#define C_SUN      0xFD20 // نارنجی خورشیدی
#define C_TEMP     0xF800 // قرمز
#define C_GREEN    0x2589 // سبز گیاهی

// ==========================================
// 3. درایور نمایشگر (هماهنگ با سخت‌افزار شما)
// ==========================================
class MyDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  MyDisplay(void) {
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

MyDisplay tft;
LGFX_Sprite spr(&tft); // بافر گرافیکی برای جلوگیری از پرپر زدن

// ==========================================
// 4. متغیرهای برنامه
// ==========================================
struct PlantData {
  int moisture = 0;
  int sunlight = 0;
  float temp = 0.0;
  String message = "Connecting...";
  String mood = "happy";
};
PlantData myPlant;

unsigned long lastCheck = 0;

// ==========================================
// 5. توابع گرافیکی (UI)
// ==========================================

// رسم نوار پیشرفت (Progress Bar)
void drawBar(int x, int y, int w, int h, int val, uint16_t color, String label) {
  // پس زمینه نوار
  spr.fillSmoothRoundRect(x, y, w, h, 6, 0xE71C); 
  
  // مقدار نوار
  int fillW = map(val, 0, 100, 0, w);
  if(fillW > 0) spr.fillSmoothRoundRect(x, y, fillW, h, 6, color);
  
  // متن و درصد
  spr.setTextColor(C_TEXT);
  spr.setFont(&fonts::FreeSansBold9pt7b);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(label, x, y - 20);
  
  spr.setTextDatum(TR_DATUM);
  spr.drawString(String(val) + "%", x + w, y - 20);
}

// رسم دکمه آب دادن
void drawWaterButton(bool pressed) {
  uint16_t color = pressed ? 0x2410 : C_WATER; // تیره‌تر اگر فشار داده شد
  int y = 380;
  
  spr.fillSmoothRoundRect(40, y, 240, 60, 20, color);
  spr.setTextColor(C_CARD);
  spr.setFont(&fonts::FreeSansBold18pt7b);
  spr.setTextDatum(MC_DATUM);
  spr.drawString("WATER ME", 160, y + 30);
  
  // آیکون قطره (ساده)
  spr.fillCircle(80, y + 30, 8, C_CARD);
  spr.fillTriangle(80-8, y+30, 80+8, y+30, 80, y+15, C_CARD);
}

// تابع اصلی آپدیت صفحه
void updateScreen(bool btnPressed = false) {
  spr.fillSprite(C_BG);

  // --- هدر ---
  spr.fillRect(0, 0, 320, 80, C_CARD);
  spr.setTextColor(C_TEXT);
  spr.setFont(&fonts::FreeSansBold12pt7b);
  spr.setTextDatum(TC_DATUM);
  spr.drawString("Smart Plant Monitor", 160, 15);
  
  // پیام وضعیت
  spr.setFont(&fonts::FreeSans9pt7b);
  if(myPlant.mood == "thirsty") spr.setTextColor(C_TEMP);
  else spr.setTextColor(C_GREEN);
  spr.drawString(myPlant.message, 160, 50);
  
  // --- کارت دما (دایره‌ای) ---
  int cx = 160; int cy = 160;
  spr.fillSmoothCircle(cx, cy, 60, C_CARD); // پس زمینه دایره
  spr.drawArc(cx, cy, 55, 45, 0, 360, 0xE71C); // حلقه خاکستری
  // حلقه دما
  int angle = map((int)myPlant.temp, 0, 50, 0, 360);
  spr.drawArc(cx, cy, 55, 45, 0, angle, C_TEMP);
  
  spr.setTextColor(C_TEXT);
  spr.setFont(&fonts::FreeSansBold18pt7b);
  spr.setTextDatum(MC_DATUM);
  spr.drawFloat(myPlant.temp, 1, cx, cy);
  spr.setFont(&fonts::FreeSans9pt7b);
  spr.drawString("C", cx + 40, cy - 10);

  // --- نوارها ---
  drawBar(30, 260, 260, 15, myPlant.moisture, C_WATER, "Soil Moisture");
  drawBar(30, 320, 260, 15, myPlant.sunlight, C_SUN, "Sunlight");

  // --- دکمه ---
  drawWaterButton(btnPressed);

  spr.pushSprite(0, 0);
}

// ==========================================
// 6. ارتباط با سرور
// ==========================================

void fetchFromServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_GET_URL);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<512> doc;
      deserializeJson(doc, payload);
      
      myPlant.moisture = doc["moisture"];
      myPlant.sunlight = doc["sunlight"];
      myPlant.temp     = doc["temp"];
      myPlant.message  = doc["message"].as<String>();
      myPlant.mood     = doc["mood"].as<String>();
      
      updateScreen();
    }
    http.end();
  }
}

void sendWaterCommand() {
  if (WiFi.status() == WL_CONNECTED) {
    // افکت گرافیکی دکمه
    updateScreen(true);
    
    HTTPClient http;
    http.begin(SERVER_POST_URL);
    http.addHeader("Content-Type", "application/json");
    
    // ارسال جیسون {"action": "WATER"}
    String json = "{\"action\": \"WATER\"}";
    http.POST(json);
    http.end();
    
    delay(200); // تاخیر کوتاه برای دیدن افکت کلیک
    
    // بلافاصله آپدیت کن تا نتیجه (پر شدن آب) دیده شود
    fetchFromServer(); 
  }
}

// ==========================================
// 7. Setup & Loop
// ==========================================

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);
  spr.createSprite(320, 480);
  
  // صفحه اتصال
  tft.fillScreen(C_BG);
  tft.setTextColor(C_TEXT);
  tft.drawString("Connecting to WiFi...", 10, 10);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  tft.drawString("Connected!", 10, 30);
  delay(1000);
  
  fetchFromServer();
}

void loop() {
  // 1. بررسی تاچ
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    // اگر مختصات تاچ روی دکمه پایین بود (تقریبی)
    if (y > 380 && y < 440 && x > 40 && x < 280) {
      sendWaterCommand();
    }
  }

  // 2. آپدیت خودکار هر 2 ثانیه
  if (millis() - lastCheck > 2000) {
    fetchFromServer();
    lastCheck = millis();
  }
}