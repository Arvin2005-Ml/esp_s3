/**
 * ============================================================================
 *  Project: Ultimate Kawaii Plant Monitor (ESP-NOW Receiver)
 *  Hardware: ESP32-S3 + ILI9488 + XPT2046
 *  Input: Receives Light Data via ESP-NOW from another ESP
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <esp_now.h>

// --- تنظیمات رنگ ---
#define C_BG        0xF7F9 
#define C_FACE_BG   0xFFFA 
#define C_CARD      0xFFFF 
#define C_TEXT      0x39E7 
#define C_ACCENT    0xFB56 
#define C_WATER     0x4D19 
#define C_TEMP      0xFC00 
#define C_HUMID     0x867F 
#define C_LIGHT     0xFE60 

// --- درایور نمایشگر (بدون تغییر) ---
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
LGFX_Sprite sprFace(&tft);
LGFX_Sprite sprData(&tft);

// --- متغیرهای دیتا ---
// ساختار دیتای دریافتی (باید با فرستنده یکی باشد)
typedef struct struct_message {
  float lux;
} struct_message;

struct_message incomingData;

struct PlantData {
  float moisture = 50; // مقدار پیش‌فرض چون سنسور رطوبت نداریم
  float temp = 25.0;   // مقدار پیش‌فرض
  int humidity = 40;   // مقدار پیش‌فرض
  float light = 0;       // این از GY-30 میاد
  String mood = "HAPPY";
};
PlantData pData;

// --- متغیرهای انیمیشن ---
unsigned long lastBlink = 0;
bool isBlinking = false;
float breathVal = 0;

// ==========================================
// توابع ESP-NOW
// ==========================================
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  // بروزرسانی مقدار نور
  pData.light = incomingData.lux;
  
  // منطق ساده برای تغییر Mood بر اساس نور (چون سرور نداریم)
  if (pData.light < 10) {
    pData.mood = "SLEEPY";
  } else if (pData.light > 1000) {
    pData.mood = "EXCITED";
  } else {
    pData.mood = "HAPPY";
  }
}

// ==========================================
// توابع گرافیک (مشابه قبل با تغییرات جزئی)
// ==========================================
float lerp(float start, float end, float t) {
  return start + (end - start) * t;
}

void drawIcon(LGFX_Sprite *spr, int x, int y, int type, uint16_t color) {
  if (type == 0) { // قطره آب
    spr->fillCircle(x, y+4, 6, color);
    spr->fillTriangle(x-6, y+4, x+6, y+4, x, y-6, color);
  } else if (type == 1) { // دما
    spr->fillCircle(x, y+4, 5, color);
    spr->fillRect(x-2, y-6, 4, 10, color);
  } else if (type == 2) { // خورشید
    spr->fillCircle(x, y, 5, color);
    for(int i=0; i<8; i++) {
      float a = i * 45 * 0.01745;
      spr->drawLine(x, y, x + cos(a)*10, y + sin(a)*10, color);
    }
  } else if (type == 3) { // ابر (رطوبت)
    spr->fillCircle(x-4, y, 4, color);
    spr->fillCircle(x+4, y, 5, color);
    spr->fillCircle(x, y-3, 5, color);
  }
}

void updateFaceAnimation() {
  if (!isBlinking && millis() - lastBlink > random(2000, 5000)) {
    isBlinking = true; lastBlink = millis();
  }
  if (isBlinking && millis() - lastBlink > 150) {
    isBlinking = false; lastBlink = millis();
  }
  breathVal = sin(millis() / 500.0) * 3.0;
}

void drawFaceSprite() {
  sprFace.fillSprite(C_FACE_BG);
  int cx = 90; 
  int cy = 160 + (int)breathVal;
  
  sprFace.fillEllipse(cx, cy + 90, 60, 10, 0xE71C);
  sprFace.fillEllipse(cx, cy, 80, 75, 0xFFFF);
  sprFace.drawEllipse(cx, cy, 80, 75, C_TEXT);

  uint16_t eyeColor = C_TEXT;
  if (pData.mood == "SAD") {
    sprFace.fillEllipse(cx - 30, cy, 10, 15, eyeColor);
    sprFace.fillEllipse(cx + 30, cy, 10, 15, eyeColor);
    sprFace.fillCircle(cx - 35, cy + 10, 4, C_WATER);
  } 
  else if (pData.mood == "EXCITED") {
    sprFace.drawString(">", cx - 40, cy - 10, 4);
    sprFace.drawString("<", cx + 20, cy - 10, 4);
  }
  else if (isBlinking || pData.mood == "SLEEPY") {
    sprFace.drawArc(cx - 30, cy, 15, 12, 180, 360, eyeColor);
    sprFace.drawArc(cx + 30, cy, 15, 12, 180, 360, eyeColor);
    if(pData.mood == "SLEEPY") sprFace.drawString("Zzz", cx + 40, cy - 40);
  } 
  else { 
    sprFace.fillEllipse(cx - 30, cy, 10, 15, eyeColor);
    sprFace.fillEllipse(cx + 30, cy, 10, 15, eyeColor);
    sprFace.fillCircle(cx - 32, cy - 4, 3, 0xFFFF);
    sprFace.fillCircle(cx + 28, cy - 4, 3, 0xFFFF);
  }

  sprFace.fillEllipse(cx - 45, cy + 15, 10, 6, 0xFDB8);
  sprFace.fillEllipse(cx + 45, cy + 15, 10, 6, 0xFDB8);

  if (pData.mood == "SAD") {
    sprFace.drawArc(cx, cy + 40, 10, 8, 180, 360, eyeColor);
  } else if (pData.mood == "EXCITED") {
    sprFace.fillEllipse(cx, cy + 30, 8, 10, C_ACCENT);
  } else {
    sprFace.drawArc(cx, cy + 25, 10, 8, 0, 180, eyeColor);
  }
  
  sprFace.fillEllipse(cx, cy - 80 + breathVal, 5, 10, C_WATER);
  sprFace.fillEllipse(cx + 8, cy - 85 + breathVal, 15, 8, C_HUMID);

  sprFace.pushSprite(0, 0);
}

void drawInfoCard(int x, int y, String title, String val, uint16_t color, int iconType) {
  sprData.fillSmoothRoundRect(x, y, 130, 65, 10, C_CARD);
  sprData.fillSmoothCircle(x + 20, y + 32, 14, color);
  drawIcon(&sprData, x + 20, y + 32, iconType, 0xFFFF);
  
  sprData.setTextColor(0x9CD3);
  sprData.setFont(&fonts::FreeSans9pt7b);
  sprData.setTextDatum(TL_DATUM);
  sprData.drawString(title, x + 45, y + 10);
  
  sprData.setTextColor(C_TEXT);
  sprData.setFont(&fonts::FreeSansBold12pt7b);
  sprData.drawString(val, x + 45, y + 30);
}

void drawDataSprite() {
  sprData.fillSprite(C_BG);
  
  sprData.setTextColor(C_TEXT);
  sprData.setFont(&fonts::FreeSansBold12pt7b);
  sprData.setTextDatum(TL_DATUM);
  sprData.drawString("Plant Status", 10, 10);
  
  // مقادیر ثابت (چون سنسور نداریم)
  drawInfoCard(10, 50, "Soil", "--", C_WATER, 0);
  drawInfoCard(150, 50, "Temp", "--", C_TEMP, 1);
  
  // مقدار نور که از GY-30 می‌آید (Lux)
  String lightStr = String((int)pData.light);
  if(pData.light > 999) lightStr = String((int)pData.light / 1000) + "k"; // فرمت کردن اعداد بزرگ
  
  drawInfoCard(10, 125, "Lux", lightStr, C_LIGHT, 2);
  drawInfoCard(150, 125, "Air", "--", C_HUMID, 3);
  
  // Health Bar (فعلا ثابت)
  sprData.fillSmoothRoundRect(10, 210, 270, 20, 10, 0xE71C);
  sprData.fillSmoothRoundRect(10, 210, 200, 20, 10, C_ACCENT);
  
  sprData.setTextColor(0xFFFF);
  sprData.setFont(&fonts::FreeSansBold9pt7b);
  sprData.setTextDatum(MC_DATUM);
  sprData.drawString("GY-30 ACTIVE", 145, 221);
  
  // دکمه پایین
  sprData.fillSmoothRoundRect(10, 250, 270, 50, 15, C_TEXT);
  sprData.drawString("NO SERVER", 145, 275);

  sprData.pushSprite(180, 0);
}

void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(C_BG);
  
  sprFace.createSprite(180, 320);
  sprData.createSprite(300, 320);
  
  tft.setTextColor(C_TEXT);
  tft.drawString("ESP-NOW Init...", 200, 160);
  
  // راه‌اندازی وای‌فای در حالت Station (الزامی برای ESP-NOW)
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    tft.drawString("Error ESP-NOW", 200, 180);
    return;
  }
  
  // ثبت تابع دریافت
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // تاچ اسکرین (فقط برای افکت بصری، چون سروری نیست)
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (x > 190 && y > 250 && y < 310) {
       sprData.fillSmoothRoundRect(10, 250, 270, 50, 15, C_ACCENT);
       sprData.pushSprite(180, 0);
       pData.mood = "EXCITED"; // تغییر مود با کلیک
       delay(200); 
    }
  }

  updateFaceAnimation();
  drawFaceSprite();
  drawDataSprite();
}