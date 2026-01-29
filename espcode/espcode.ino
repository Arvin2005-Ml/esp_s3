/**
 * ============================================================================
 *  Project: Ultimate Kawaii Plant Monitor (Smooth & Info Rich)
 *  Hardware: ESP32-S3 + ILI9488 + XPT2046
 *  Features: Smooth Animations, 4-Data Grid, Modern UI
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- تنظیمات ---
const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 
const char* SERVER_BASE   = "http://10.156.45.197:8000"; // IP سرور خود را چک کنید

// --- رنگ‌های پاستلی مدرن ---
#define C_BG        0xF7F9 // سفید مایل به آبی
#define C_FACE_BG   0xFFFA // صورتی خیلی محو
#define C_CARD      0xFFFF // سفید مطلق
#define C_TEXT      0x39E7 // خاکستری تیره
#define C_ACCENT    0xFB56 // صورتی جیغ (برای هایلایت)
#define C_WATER     0x4D19 // آبی اقیانوسی
#define C_TEMP      0xFC00 // نارنجی
#define C_HUMID     0x867F // بنفش ملایم
#define C_LIGHT     0xFE60 // زرد

// --- درایور نمایشگر ---
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
// دو اسپیرایت جداگانه برای مدیریت بهتر رم
LGFX_Sprite sprFace(&tft); // اسپیرایت صورت (چپ)
LGFX_Sprite sprData(&tft); // اسپیرایت دیتا (راست)

// --- متغیرهای دیتا ---
struct PlantData {
  float moisture = 0; float targetMoisture = 0;
  float temp = 0;
  int humidity = 0;
  int light = 0;
  String mood = "HAPPY";
};
PlantData pData;

// --- متغیرهای انیمیشن ---
unsigned long lastBlink = 0;
bool isBlinking = false;
float breathVal = 0; // برای بالا پایین رفتن صورت

// ==========================================
// توابع کمکی گرافیک
// ==========================================

// تابع Lerp برای حرکت نرم اعداد (Linear Interpolation)
float lerp(float start, float end, float t) {
  return start + (end - start) * t;
}

// رسم آیکون‌های ساده
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

// ==========================================
// انیمیشن و رسم صورت
// ==========================================
void updateFaceAnimation() {
  // 1. منطق پلک زدن
  if (!isBlinking && millis() - lastBlink > random(2000, 5000)) {
    isBlinking = true;
    lastBlink = millis();
  }
  if (isBlinking && millis() - lastBlink > 150) {
    isBlinking = false;
    lastBlink = millis();
  }

  // 2. منطق نفس کشیدن (محاسبه Y Offset)
  breathVal = sin(millis() / 500.0) * 3.0; // نوسان نرم بین -3 تا +3
}

void drawFaceSprite() {
  sprFace.fillSprite(C_FACE_BG);
  
  int cx = 90; // مرکز صورت در اسپیرایت
  int cy = 160 + (int)breathVal; // اعمال انیمیشن تنفس
  
  // سایه زیر
  sprFace.fillEllipse(cx, cy + 90, 60, 10, 0xE71C);
  
  // صورت اصلی
  sprFace.fillEllipse(cx, cy, 80, 75, 0xFFFF);
  sprFace.drawEllipse(cx, cy, 80, 75, C_TEXT);

  // چشم‌ها
  uint16_t eyeColor = C_TEXT;
  if (pData.mood == "SAD") {
    sprFace.fillEllipse(cx - 30, cy, 10, 15, eyeColor);
    sprFace.fillEllipse(cx + 30, cy, 10, 15, eyeColor);
    sprFace.fillCircle(cx - 35, cy + 10, 4, C_WATER); // اشک
  } 
  else if (pData.mood == "EXCITED") {
    // چشم ستاره‌ای
    sprFace.drawString(">", cx - 40, cy - 10, 4);
    sprFace.drawString("<", cx + 20, cy - 10, 4);
  }
  else if (isBlinking || pData.mood == "SLEEPY") {
    // چشم بسته
    sprFace.drawArc(cx - 30, cy, 15, 12, 180, 360, eyeColor);
    sprFace.drawArc(cx + 30, cy, 15, 12, 180, 360, eyeColor);
    if(pData.mood == "SLEEPY") sprFace.drawString("Zzz", cx + 40, cy - 40);
  } 
  else { // HAPPY NORMAL
    sprFace.fillEllipse(cx - 30, cy, 10, 15, eyeColor);
    sprFace.fillEllipse(cx + 30, cy, 10, 15, eyeColor);
    sprFace.fillCircle(cx - 32, cy - 4, 3, 0xFFFF); // برق چشم
    sprFace.fillCircle(cx + 28, cy - 4, 3, 0xFFFF);
  }

  // گونه‌ها
  sprFace.fillEllipse(cx - 45, cy + 15, 10, 6, 0xFDB8);
  sprFace.fillEllipse(cx + 45, cy + 15, 10, 6, 0xFDB8);

  // دهان
  if (pData.mood == "SAD") {
    sprFace.drawArc(cx, cy + 40, 10, 8, 180, 360, eyeColor);
  } else if (pData.mood == "EXCITED") {
    sprFace.fillEllipse(cx, cy + 30, 8, 10, C_ACCENT);
  } else {
    sprFace.drawArc(cx, cy + 25, 10, 8, 0, 180, eyeColor);
  }
  
  // برگ روی سر
  sprFace.fillEllipse(cx, cy - 80 + breathVal, 5, 10, C_WATER);
  sprFace.fillEllipse(cx + 8, cy - 85 + breathVal, 15, 8, C_HUMID);

  sprFace.pushSprite(0, 0); // رسم سمت چپ صفحه
}

// ==========================================
// رسم پنل اطلاعات (سمت راست)
// ==========================================

void drawInfoCard(int x, int y, String title, String val, uint16_t color, int iconType) {
  // پس زمینه کارت
  sprData.fillSmoothRoundRect(x, y, 130, 65, 10, C_CARD);
  
  // آیکون
  sprData.fillSmoothCircle(x + 20, y + 32, 14, color);
  drawIcon(&sprData, x + 20, y + 32, iconType, 0xFFFF);
  
  // متن
  sprData.setTextColor(0x9CD3); // خاکستری روشن
  sprData.setFont(&fonts::FreeSans9pt7b);
  sprData.setTextDatum(TL_DATUM);
  sprData.drawString(title, x + 45, y + 10);
  
  sprData.setTextColor(C_TEXT);
  sprData.setFont(&fonts::FreeSansBold12pt7b);
  sprData.drawString(val, x + 45, y + 30);
}

void drawDataSprite() {
  sprData.fillSprite(C_BG);
  
  // عنوان
  sprData.setTextColor(C_TEXT);
  sprData.setFont(&fonts::FreeSansBold12pt7b);
  sprData.setTextDatum(TL_DATUM);
  sprData.drawString("Plant Status", 10, 10);
  
  // شبکه 4 تایی اطلاعات
  // 1. Soil (رطوبت خاک)
  drawInfoCard(10, 50, "Soil", String((int)pData.moisture) + "%", C_WATER, 0);
  
  // 2. Temp (دما)
  drawInfoCard(150, 50, "Temp", String(pData.temp, 1) + "c", C_TEMP, 1);
  
  // 3. Light (نور)
  drawInfoCard(10, 125, "Light", String(pData.light) + "%", C_LIGHT, 2);
  
  // 4. Humidity (رطوبت هوا)
  drawInfoCard(150, 125, "Air", String(pData.humidity) + "%", C_HUMID, 3);
  
  // نوار وضعیت کلی (Health Bar)
  sprData.fillSmoothRoundRect(10, 210, 270, 20, 10, 0xE71C);
  int healthW = map((int)pData.moisture, 0, 100, 0, 270);
  // افکت انیمیشن نرم برای نوار
  static float smoothHealth = 0;
  smoothHealth = lerp(smoothHealth, healthW, 0.1);
  
  sprData.fillSmoothRoundRect(10, 210, (int)smoothHealth, 20, 10, C_ACCENT);
  
  sprData.setTextColor(0xFFFF);
  sprData.setFont(&fonts::FreeSansBold9pt7b);
  sprData.setTextDatum(MC_DATUM);
  sprData.drawString("OVERALL HEALTH", 145, 221);
  
  // دکمه بزرگ پایین
  sprData.fillSmoothRoundRect(10, 250, 270, 50, 15, C_TEXT);
  sprData.drawString("WATER ME!", 145, 275);

  sprData.pushSprite(180, 0); // رسم سمت راست صفحه
}

// ==========================================
// شبکه و منطق
// ==========================================
void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/get-data");
  if (http.GET() > 0) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, payload);
    
    // ذخیره هدف برای انیمیشن نرم
    pData.targetMoisture = doc["moisture"];
    pData.temp = doc["temp"];
    pData.humidity = doc["humidity"];
    pData.light = doc["light"];
    pData.mood = doc["mood"].as<String>();
  }
  http.end();
}

void sendWater() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/send-touch");
  http.addHeader("Content-Type", "application/json");
  http.POST("{\"action\": \"WATER\"}");
  http.end();
  
  // فیدبک فوری
  pData.mood = "EXCITED";
  pData.targetMoisture = 100;
}

// ==========================================
// Setup & Loop
// ==========================================
void setup() {
  Serial.begin(115200);
  
  tft.init();
  tft.setRotation(1); // افقی Landscape
  tft.fillScreen(C_BG);
  
  // ساخت اسپیرایت‌ها (سایز بهینه شده برای رم)
  // کل عرض 480 است: 180 پیکسل چپ (صورت) + 300 پیکسل راست (دیتا)
  sprFace.createSprite(180, 320);
  sprData.createSprite(300, 320);
  
  // صفحه لودینگ
  tft.setTextColor(C_TEXT);
  tft.drawString("Connecting...", 220, 160);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  fetchData();
}

unsigned long lastNetCheck = 0;

void loop() {
  // 1. منطق تاچ (سریع)
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    // دکمه آب در مختصات خاصی قرار دارد
    // Sprite دیتا از x=180 شروع می‌شود. دکمه در Sprite در y=250 است.
    // پس روی صفحه اصلی: x > 190 و y > 250
    if (x > 190 && y > 250 && y < 310) {
       sprData.fillSmoothRoundRect(10, 250, 270, 50, 15, C_ACCENT); // تغییر رنگ دکمه
       sprData.pushSprite(180, 0);
       sendWater();
       delay(200); // مکث کوتاه برای حس کلیک
    }
  }

  // 2. انیمیشن مقادیر (نرم شدن تغییر عدد رطوبت)
  pData.moisture = lerp(pData.moisture, pData.targetMoisture, 0.05);

  // 3. آپدیت گرافیک (سریع - 30 فریم بر ثانیه)
  updateFaceAnimation();
  drawFaceSprite();
  drawDataSprite();

  // 4. دریافت اطلاعات از سرور (کند - هر 2 ثانیه)
  if (millis() - lastNetCheck > 2000) {
    fetchData();
    lastNetCheck = millis();
  }
}