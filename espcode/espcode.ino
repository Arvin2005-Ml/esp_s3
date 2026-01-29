/**
 * ============================================================================
 *  Project: ESP32-S3 FastAPI Client (Kawaii Style)
 *  Hardware: ESP32-S3 + ILI9488 (16-bit Parallel) + XPT2046
 *  Library: LovyanGFX + ArduinoJson
 *  Author: Arvin (Modified for Server Connection)
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==========================================
// 1. تنظیمات شبکه و سرور
// ==========================================

const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 

// آدرس IP کامپیوتر خود را جایگزین کنید (از دستور ipconfig در ویندوز استفاده کنید)
const char* SERVER_GET_URL  = "http://10.156.45.197:8000/get-data";
const char* SERVER_POST_URL = "http://10.156.45.197:8000/send-touch";

// ==========================================
// 2. پالت رنگی (همان استایل Kawaii)
// ==========================================
namespace Palette {
  const uint16_t BG_MAIN      = 0xFFDF; // صورتی خیلی ملایم
  const uint16_t CARD_BG      = 0xFFFF; // سفید
  const uint16_t TEXT_DARK    = 0x4A69; // خاکستری تیره
  const uint16_t GREEN_BTN    = 0x4E72; // سبز آبی
  const uint16_t RED_LED_ON   = 0xF800; // قرمز روشن
  const uint16_t GREY_LED_OFF = 0xBDF7; // طوسی
  const uint16_t BLUE_HEADER  = 0x8E7F; // آبی ملایم
}

// ==========================================
// 3. درایور نمایشگر (دقیقا طبق کد سخت‌افزاری شما)
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
      // پین‌های دیتا طبق نمونه کد شما
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
      // پین‌های تاچ طبق نمونه کد شما
      cfg.pin_sclk = 47; cfg.pin_mosi = 48; cfg.pin_miso = 20; cfg.pin_cs = 2;         
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

MyDisplay tft;
LGFX_Sprite statusSprite(&tft); // بافر برای جلوگیری از چشمک زدن متن

// ==========================================
// 4. متغیرهای برنامه
// ==========================================
struct ServerData {
  String message = "Waiting...";
  bool ledOn = false;
  String lastTouchStatus = "No Touch Yet";
};
ServerData appData;

unsigned long lastNetworkCheck = 0;

// ==========================================
// 5. توابع گرافیکی (GUI)
// ==========================================

void drawCard(int x, int y, int w, int h) {
  tft.fillSmoothRoundRect(x+4, y+4, w, h, 15, 0xE73C); // سایه
  tft.fillSmoothRoundRect(x, y, w, h, 15, Palette::CARD_BG); // بدنه
}

void updateUI() {
  // رسم پس زمینه کارت (فقط یکبار یا اگر نیاز به رفرش کامل بود)
  // اینجا ما فقط محتوا را آپدیت می‌کنیم روی Sprite

  statusSprite.fillSprite(Palette::CARD_BG);
  
  // 1. نمایش پیام سرور
  statusSprite.setTextColor(Palette::TEXT_DARK);
  statusSprite.setFont(&fonts::FreeSansBold12pt7b);
  statusSprite.setTextDatum(TL_DATUM);
  statusSprite.drawString("Server Says:", 10, 10);
  
  statusSprite.setTextColor(Palette::BLUE_HEADER);
  statusSprite.drawString(appData.message, 10, 40);

  // 2. نمایش وضعیت LED مجازی
  statusSprite.setTextColor(Palette::TEXT_DARK);
  statusSprite.drawString("Virtual LED:", 10, 90);
  
  uint16_t ledColor = appData.ledOn ? Palette::RED_LED_ON : Palette::GREY_LED_OFF;
  statusSprite.fillSmoothCircle(200, 100, 15, ledColor);
  
  if(appData.ledOn) {
     statusSprite.setTextColor(Palette::RED_LED_ON);
     statusSprite.drawString("ON", 230, 90);
  } else {
     statusSprite.setTextColor(Palette::GREY_LED_OFF);
     statusSprite.drawString("OFF", 230, 90);
  }

  // 3. نمایش وضعیت آخرین ارسال
  statusSprite.setTextColor(Palette::GREEN_BTN);
  statusSprite.setFont(&fonts::FreeSans9pt7b);
  statusSprite.drawString(appData.lastTouchStatus, 10, 140);

  // انتقال اسپرایت به صفحه اصلی
  statusSprite.pushSprite(20, 80);
}

// ==========================================
// 6. توابع شبکه (Network)
// ==========================================

void sendTouchToServer(int x, int y) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_POST_URL);
    http.addHeader("Content-Type", "application/json");

    // ساخت JSON: {"x": 100, "y": 200}
    StaticJsonDocument<200> doc;
    doc["x"] = x;
    doc["y"] = y;
    String requestBody;
    serializeJson(doc, requestBody);

    appData.lastTouchStatus = "Sending...";
    updateUI(); // آپدیت فوری رابط کاربری

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      appData.lastTouchStatus = "Sent: OK (" + String(httpResponseCode) + ")";
    } else {
      appData.lastTouchStatus = "Error: " + String(httpResponseCode);
    }
    http.end();
  } else {
    appData.lastTouchStatus = "WiFi Disconnected!";
  }
  updateUI();
}

void getDataFromServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_GET_URL);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      // پارس کردن JSON: {"message": "Hello", "led_on": true}
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        String msg = doc["message"];
        appData.message = msg;
        appData.ledOn = doc["led_on"];
        updateUI(); // آپدیت گرافیک با دیتای جدید
      }
    }
    http.end();
  }
}

// ==========================================
// 7. بدنه اصلی (Setup & Loop)
// ==========================================

void setup() {
  Serial.begin(115200);

  // راه اندازی نمایشگر
  tft.init();
  tft.setRotation(0); // عمودی
  tft.fillScreen(Palette::BG_MAIN);
  
  // ساخت هدر ثابت
  tft.fillSmoothRoundRect(10, 10, 300, 50, 10, Palette::BLUE_HEADER);
  tft.setTextColor(Palette::CARD_BG);
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FastAPI Client", 160, 35);

  // رسم کادر اصلی دیتا
  drawCard(20, 80, 280, 180);
  
  // راه اندازی بافر گرافیکی (Sprite)
  statusSprite.createSprite(280, 180);
  
  // اتصال به وای فای
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(Palette::TEXT_DARK);
  tft.drawString("Connecting WiFi...", 20, 300);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  tft.fillRect(20, 290, 200, 30, Palette::BG_MAIN); // پاک کردن متن وای فای
  tft.setTextColor(Palette::GREEN_BTN);
  tft.drawString("WiFi Connected!", 20, 300);
  tft.drawString(WiFi.localIP().toString(), 20, 320);
  
  updateUI();
}

void loop() {
  // 1. بررسی تاچ
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    // رسم دایره کوچک زیر انگشت برای فیدبک بصری
    tft.fillCircle(x, y, 5, Palette::GREEN_BTN);
    
    // ارسال به سرور
    sendTouchToServer(x, y);
    
    // پاک کردن دایره بعد از کمی تاخیر (اختیاری)
    delay(100); 
    tft.fillCircle(x, y, 5, Palette::BG_MAIN); // یا رفرش کامل صفحه اگر روی کارت نبود
  }

  // 2. دریافت دیتا از سرور (هر 2 ثانیه)
  if (millis() - lastNetworkCheck > 2000) {
    getDataFromServer();
    lastNetworkCheck = millis();
  }
}