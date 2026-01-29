/**
 * ============================================================================
 *  Project: Kawaii Plant Monitor (Server Connected)
 *  Style: Milky Pastel & Anime Eyes + FastAPI Backend
 *  Hardware: ESP32-S3 + ILI9488 + XPT2046
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

// آدرس سرور پایتون
const char* SERVER_GET  = "http://10.156.45.197:8000/get-data";
const char* SERVER_POST = "http://10.156.45.197:8000/send-touch";

// ==========================================
// 2. پالت رنگی (همان استایل زیبا)
// ==========================================
namespace Palette {
  const uint16_t BG_DAY       = 0xFFDF; // صورتی خیلی ملایم
  const uint16_t BG_NIGHT     = 0x2965; // سرمه‌ای ملایم
  const uint16_t WHITE        = 0xFFFF; 
  const uint16_t PINK_HOT     = 0xFB56; 
  const uint16_t PINK_SOFT    = 0xFDB8; 
  const uint16_t BLUE_SKY     = 0x8E7F; 
  const uint16_t TEAL_WATER   = 0x4E72; 
  const uint16_t TEXT_MAIN    = 0x4A69; 
}

// ==========================================
// 3. درایور نمایشگر (کانفیگ سخت‌افزاری شما)
// ==========================================
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
LGFX_Sprite faceSprite(&tft);

// ==========================================
// 4. دیتای دریافت شده از سرور
// ==========================================
struct ServerState {
  int moisture = 50;
  int sunlight = 50;
  float temp = 25.0;
  String mood = "HAPPY"; // HAPPY, SAD, EXCITED, SLEEPY, LOVE, HOT
};
ServerState currentData;
unsigned long lastFetch = 0;

// ==========================================
// 5. توابع گرافیکی (رسم صورت و کارت‌ها)
// ==========================================

void drawKawaiiCard(int x, int y, int w, int h, String title, float val, uint16_t color) {
  tft.fillSmoothRoundRect(x+3, y+3, w, h, 20, 0xE73C); // Shadow
  tft.fillSmoothRoundRect(x, y, w, h, 20, Palette::WHITE); // Body
  
  // Icon Circle
  tft.fillSmoothCircle(x + 25, y + 25, 12, color);
  
  // Title
  tft.setTextColor(Palette::TEXT_MAIN, Palette::WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFont(&fonts::FreeSansBold9pt7b);
  tft.drawString(title, x + 50, y + 15);
  
  // Progress Bar
  int barW = w - 30;
  int barY = y + h - 25;
  tft.fillSmoothRoundRect(x+15, barY, barW, 10, 5, 0xF7BE); // Back
  int fillW = map((int)val, 0, 100, 0, barW);
  if (fillW > 4) tft.fillSmoothRoundRect(x+15, barY, fillW, 10, 5, color);
  
  // Text Value
  tft.setTextDatum(TR_DATUM);
  tft.drawString(String((int)val) + "%", x + w - 15, y + 15);
}

void drawStar(int x, int y, uint16_t c) {
  faceSprite.drawLine(x-4, y, x+4, y, c);
  faceSprite.drawLine(x, y-4, x, y+4, c);
}

void drawFace() {
  // رنگ پس زمینه بر اساس مود (شب/روز یا مود خاص)
  uint16_t bg = (currentData.mood == "SLEEPY") ? Palette::BG_NIGHT : Palette::BG_DAY;
  faceSprite.fillSprite(bg);
  
  // انیمیشن تنفس
  static float t = 0; t += 0.1;
  float yOff = sin(t) * 2.0;
  int cx = 110; int cy = 110 + (int)yOff;
  
  // بدنه صورت
  faceSprite.fillEllipse(cx, cy + 85, 60, 10, 0xE699); // سایه زیر
  faceSprite.fillEllipse(cx, cy, 95, 85, Palette::WHITE); // صورت
  faceSprite.drawEllipse(cx, cy, 95, 85, Palette::TEXT_MAIN); // خط دور

  // گونه‌ها
  faceSprite.fillEllipse(cx - 55, cy + 15, 14, 8, Palette::PINK_SOFT);
  faceSprite.fillEllipse(cx + 55, cy + 15, 14, 8, Palette::PINK_SOFT);

  uint16_t eyeC = Palette::TEXT_MAIN;
  String m = currentData.mood;

  // --- رسم چشم‌ها بر اساس مود ---
  if (m == "SLEEPY") {
    faceSprite.drawArc(cx-40, cy, 15, 12, 190, 350, eyeC);
    faceSprite.drawArc(cx+40, cy, 15, 12, 190, 350, eyeC);
    faceSprite.setTextColor(Palette::BLUE_SKY);
    faceSprite.drawString("Zzz...", cx+60, cy-40);
  } 
  else if (m == "LOVE") {
     faceSprite.fillSmoothCircle(cx - 40, cy, 10, Palette::PINK_HOT);
     faceSprite.fillSmoothCircle(cx + 40, cy, 10, Palette::PINK_HOT);
  }
  else if (m == "EXCITED") {
     drawStar(cx-40, cy, eyeC);
     drawStar(cx+40, cy, eyeC);
  }
  else if (m == "SAD") {
     // چشم گریان
     faceSprite.fillEllipse(cx - 40, cy, 12, 10, eyeC);
     faceSprite.fillEllipse(cx + 40, cy, 12, 10, eyeC);
     // قطره اشک
     faceSprite.fillCircle(cx - 40, cy + 15, 4, Palette::BLUE_SKY);
  }
  else if (m == "HOT") {
     // چشم گیج
     faceSprite.drawLine(cx-45, cy-5, cx-35, cy+5, eyeC);
     faceSprite.drawLine(cx-35, cy-5, cx-45, cy+5, eyeC);
     faceSprite.drawLine(cx+45, cy-5, cx+35, cy+5, eyeC);
     faceSprite.drawLine(cx+35, cy-5, cx+45, cy+5, eyeC);
     // قطره عرق
     faceSprite.fillCircle(cx + 70, cy - 20, 6, Palette::BLUE_SKY);
  }
  else { // HAPPY
    faceSprite.fillEllipse(cx - 40, cy, 12, 16, eyeC);
    faceSprite.fillEllipse(cx + 40, cy, 12, 16, eyeC);
    faceSprite.fillCircle(cx - 44, cy - 6, 4, Palette::WHITE); 
    faceSprite.fillCircle(cx + 36, cy - 6, 4, Palette::WHITE); 
  }

  // --- رسم دهان ---
  if (m == "SAD") {
    faceSprite.drawArc(cx, cy + 40, 10, 8, 200, 340, eyeC); // ناراحت
  } else if (m == "EXCITED" || m == "LOVE") {
    faceSprite.fillEllipse(cx, cy + 30, 8, 12, Palette::PINK_HOT); // دهان باز
  } else {
    faceSprite.drawArc(cx - 5, cy + 30, 6, 5, 20, 160, eyeC); // لبخند
    faceSprite.drawArc(cx + 5, cy + 30, 6, 5, 20, 160, eyeC);
  }
  
  // برگ روی سر
  faceSprite.fillEllipse(cx, cy - 90, 5, 10, Palette::TEAL_WATER);
  faceSprite.fillEllipse(cx + 10, cy - 95, 15, 8, 0x5F68);

  faceSprite.pushSprite(50, 40);
}

// ==========================================
// 6. ارتباط با سرور
// ==========================================

void syncWithServer() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(SERVER_GET);
  int code = http.GET();
  
  if (code > 0) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    
    if (!err) {
      currentData.moisture = doc["moisture"];
      currentData.sunlight = doc["sunlight"];
      currentData.temp = doc["temp"];
      const char* m = doc["mood"];
      currentData.mood = String(m);
      
      // تغییر رنگ پس زمینه کل صفحه اگر مود خواب بود
      static String lastMood = "";
      if (currentData.mood != lastMood) {
        if (currentData.mood == "SLEEPY") tft.fillScreen(Palette::BG_NIGHT);
        else if (lastMood == "SLEEPY") tft.fillScreen(Palette::BG_DAY);
        
        // آپدیت کارت‌های پایین (چون بک گراند ممکن است پاکشان کرده باشد)
        drawKawaiiCard(15, 290, 140, 90, "Water", currentData.moisture, Palette::TEAL_WATER);
        drawKawaiiCard(165, 290, 140, 90, "Light", currentData.sunlight, 0xFED0);
        drawKawaiiCard(15, 395, 290, 70, "Temp", currentData.temp * 3, Palette::PINK_HOT); // *3 just for scaling on bar
        
        lastMood = currentData.mood;
      } else {
        // فقط آپدیت مقادیر بدون پاک کردن کل صفحه
        drawKawaiiCard(15, 290, 140, 90, "Water", currentData.moisture, Palette::TEAL_WATER);
        drawKawaiiCard(165, 290, 140, 90, "Light", currentData.sunlight, 0xFED0);
        drawKawaiiCard(15, 395, 290, 70, "Temp", currentData.temp * 3, Palette::PINK_HOT);
      }
    }
  }
  http.end();
}

void sendAction(String actionType) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(SERVER_POST);
  http.addHeader("Content-Type", "application/json");
  String json = "{\"action\": \"" + actionType + "\"}";
  http.POST(json);
  http.end();
  
  // بلافاصله سینک کن تا تغییر حس رو ببینیم
  syncWithServer();
}

// ==========================================
// 7. Setup & Loop
// ==========================================

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(Palette::BG_DAY);
  
  faceSprite.createSprite(220, 220); // بافر برای انیمیشن روان
  
  tft.drawString("Connecting...", 10, 10);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  tft.fillScreen(Palette::BG_DAY);
  syncWithServer();
}

void loop() {
  // 1. بررسی تاچ
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    // کارت سمت چپ (آب)
    if (y > 290 && y < 380 && x < 160) {
       sendAction("WATER");
       faceSprite.fillSprite(Palette::WHITE); // فلش سفید برای بازخورد
       faceSprite.pushSprite(50, 40);
    }
    // کارت پایین (Love/Temp)
    else if (y > 390) {
       sendAction("LOVE");
    }
  }

  // 2. انیمیشن صورت (سریع)
  drawFace();

  // 3. دریافت دیتا از سرور (کندتر - هر 1 ثانیه)
  if (millis() - lastFetch > 1000) {
    syncWithServer();
    lastFetch = millis();
  }
}