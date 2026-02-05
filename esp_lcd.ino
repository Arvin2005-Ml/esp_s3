/**
 * ============================================================================
 *  Project: EMO Style Plant Monitor (Cyclic Data + Dynamic Eyes)
 *  Hardware: ESP32-S3 + ILI9488 (Parallel) + XPT2046 (SPI3)
 *  Date: 1404/11/16
 * ============================================================================
 */

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

// --- تنظیمات وای‌فای و سرور ---
const char* WIFI_SSID     = "arvinshokouhi";      
const char* WIFI_PASS     = "Arvin1384"; 
const char* SERVER_BASE   = "http://10.156.45.78:8000"; 

// --- رنگ‌های استایل EMO ---
#define C_BLACK     0x0000
#define C_WHITE     0xFFFF
#define C_EMO_BLUE  0x07FF  // Cyan Neon
#define C_EMO_GREEN 0x07E0  // Happy
#define C_EMO_RED   0xF800  // Angry/Alert
#define C_EMO_PURPLE 0xF81F // Sleepy/Confused
#define C_BG_TASKS  0x2124  // Dark Gray

// --- درایور نمایشگر (دقیقاً طبق سخت‌افزار شما) ---
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
    
    // کانفیگ تاچ
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;   cfg.x_max      = 319;
      cfg.y_min      = 0;   cfg.y_max      = 479;
      cfg.pin_int    = 1;   
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.spi_host = SPI3_HOST; 
      cfg.freq     = 1000000;   
      cfg.pin_sclk = 47;        
      cfg.pin_mosi = 48;        
      cfg.pin_miso = 20;        
      cfg.pin_cs   = 2;         
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

KawaiiDisplay tft;
LGFX_Sprite spr(&tft); // اسپریت اصلی برای جلوگیری از پرپر زدن (Flicker)

// --- ساختار داده ---
struct TaskItem { String id; String name; bool done; };

struct AppData {
  float moisture = 0; 
  float temp = 0; 
  int humidity = 0; 
  int light = 0;
  String mood = "NEUTRAL"; 
  std::vector<TaskItem> tasks;
  bool isWatering = false;
};
AppData appData;

// --- وضعیت صفحات ---
enum ScreenMode { SCREEN_MAIN, SCREEN_TASKS }; // Stats حذف شد چون در صفحه اصلی ادغام شده
ScreenMode currentScreen = SCREEN_MAIN;

// --- متغیرهای انیمیشن و چرخش اطلاعات ---
unsigned long lastBlink = 0;
bool isBlinking = false;
int eyeHeight = 80;

// چرخش اطلاعات
enum InfoState { INFO_NONE, INFO_TEMP, INFO_HUMID, INFO_SOIL, INFO_LIGHT };
InfoState currentInfo = INFO_NONE;
unsigned long lastInfoChange = 0;
const int INFO_INTERVAL = 3500; // هر 3.5 ثانیه اطلاعات عوض شود

// ==========================================
// 1. شبکه (Network)
// ==========================================
void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(SERVER_BASE) + "/get-data");
  http.setTimeout(1500); // کاهش تایم‌اوت برای روان‌تر شدن UI
  int code = http.GET();
  
  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject sensor = doc["sensor"];
      if (!sensor.isNull()) {
        if (appData.mood != "EXCITED") {
            appData.moisture = sensor["moisture"] | 0;
            appData.mood     = sensor["mood"].as<String>(); 
        } else {
             if (sensor["moisture"].as<int>() > 90) { 
                appData.mood = sensor["mood"].as<String>(); 
             }
        }
        appData.temp     = sensor["temperature"] | 0;
        appData.humidity = sensor["humidity"] | 0;
        appData.light    = sensor["light"] | 0;
      }
      // تسک‌ها فقط اگر نیاز بود آپدیت شوند (برای جلوگیری از لگ)
      if (currentScreen == SCREEN_TASKS) {
          appData.tasks.clear();
          JsonArray tArr = doc["tasks"];
          for(JsonObject v : tArr) {
            appData.tasks.push_back({v["_id"].as<String>(), v["title"].as<String>(), v["done"]});
          }
      }
    }
  }
  http.end();
}

void toggleTaskServer(int index) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (index < 0 || index >= appData.tasks.size()) return;
  HTTPClient http;
  String url = String(SERVER_BASE) + "/toggle-task/" + appData.tasks[index].id;
  http.begin(url);
  http.POST(""); 
  http.end();
}

void localWaterEffect() {
  appData.moisture = 100;
  appData.mood = "EXCITED";
  appData.isWatering = true;
}

// ==========================================
// 2. گرافیک (UI & EMO Face)
// ==========================================

// تابع کمکی برای رسم چشم ایمو
void drawEmoEye(int x, int y, int w, int h, uint16_t color, String mood) {
    // حالت پلک زدن
    if (isBlinking) {
        spr.fillRect(x, y + h/2 - 2, w, 4, color);
        return;
    }

    // رسم بر اساس مود
    if (mood == "HAPPY") {
        // چشم‌های قوسی (لبخند معکوس)
        spr.fillArc(x + w/2, y + h/2 + 20, w/2, w/2 - 10, 220, 320, color);
    } 
    else if (mood == "EXCITED") {
        // ستاره یا لوزی
        spr.fillTriangle(x + w/2, y, x, y + h/2, x + w, y + h/2, color); // بالا
        spr.fillTriangle(x + w/2, y + h, x, y + h/2, x + w, y + h/2, color); // پایین
    }
    else if (mood == "SAD") {
        // مستطیل پایین افتاده
        spr.fillSmoothRoundRect(x, y + 20, w, h - 20, 15, color);
        // اشک
        if (x < 160) spr.fillCircle(x + 10, y + h + 10, 5, C_EMO_BLUE);
    }
    else if (mood == "SLEEPY") {
        // خط‌های باریک
        spr.fillSmoothRoundRect(x, y + h/2 - 5, w, 10, 3, color);
        spr.drawString("Zzz...", 160, 50);
    }
    else {
        // حالت عادی (NEUTRAL) - مستطیل گرد بزرگ
        spr.fillSmoothRoundRect(x, y, w, h, 20, color);
    }
}

void drawMainScreen() {
  // 1. پاک کردن صفحه
  spr.fillSprite(C_BLACK);

  // 2. مدیریت انیمیشن پلک زدن
  if (!isBlinking && millis() - lastBlink > 3500 + random(2000)) { 
      isBlinking = true; lastBlink = millis(); 
  }
  if (isBlinking && millis() - lastBlink > 150) { 
      isBlinking = false; lastBlink = millis(); 
  }

  // 3. تعیین رنگ و شکل بر اساس مود
  uint16_t eyeColor = C_EMO_BLUE;
  if (appData.mood == "HAPPY") eyeColor = C_EMO_GREEN;
  if (appData.mood == "SAD") eyeColor = C_EMO_PURPLE;
  if (appData.mood == "EXCITED") eyeColor = C_EMO_RED;
  if (appData.mood == "SLEEPY") eyeColor = C_WHITE;

  // مختصات چشم‌ها
  int eyeW = 90;
  int eyeH = 120;
  int eyeY = 100;
  
  // لرزش خفیف اگر تشنه است
  if (appData.mood == "SAD" || appData.moisture < 20) {
      eyeY += random(-2, 3);
  }

  // رسم چشم چپ و راست
  drawEmoEye(40, eyeY, eyeW, eyeH, eyeColor, appData.mood);   // Left
  drawEmoEye(190, eyeY, eyeW, eyeH, eyeColor, appData.mood);  // Right

  // 4. مدیریت چرخش اطلاعات (Cyclic Info)
  if (millis() - lastInfoChange > INFO_INTERVAL) {
      currentInfo = (InfoState)((currentInfo + 1) % 5); // چرخش بین 0 تا 4
      lastInfoChange = millis();
  }

  // رسم اطلاعات در پایین صفحه
  spr.setTextDatum(MC_DATUM); // مرکز
  int infoY = 320;
  
  // کادر محو زیر اطلاعات
  if (currentInfo != INFO_NONE) {
      spr.fillSmoothRoundRect(60, infoY - 25, 200, 50, 10, 0x18E3); // خاکستری تیره
  }

  spr.setFont(&fonts::FreeSansBold12pt7b);
  spr.setTextColor(C_WHITE);

  switch (currentInfo) {
      case INFO_TEMP:
          spr.setTextColor(0xFFE0); // زرد
          spr.drawString("TEMP: " + String(appData.temp, 1) + " C", 160, infoY);
          break;
      case INFO_HUMID:
          spr.setTextColor(0x07FF); // آبی
          spr.drawString("HUMID: " + String(appData.humidity) + " %", 160, infoY);
          break;
      case INFO_SOIL:
          spr.setTextColor(0x07E0); // سبز
          spr.drawString("SOIL: " + String((int)appData.moisture) + " %", 160, infoY);
          break;
      case INFO_LIGHT:
          spr.setTextColor(0xF800); // قرمز/نارنجی
          spr.drawString("LIGHT: " + String(appData.light) + " %", 160, infoY);
          break;
      case INFO_NONE:
          // نمایش Mood یا پیام خالی
          spr.setFont(&fonts::FreeSans9pt7b);
          spr.setTextColor(0x9CD3);
          spr.drawString(appData.mood, 160, infoY);
          break;
  }

  // راهنما پایین
  spr.setFont(&fonts::FreeSans9pt7b);
  spr.setTextColor(0x52AA); // خاکستری
  spr.drawString("v  TASKS  v", 160, 460);
  
  // دکمه مخفی آب دادن (اگر مود Excited نیست)
  if (appData.mood != "EXCITED") {
     spr.drawRect(140, 400, 40, 40, C_BLACK); // ناحیه لمسی مخفی
  } else {
     spr.setTextColor(C_EMO_GREEN);
     spr.drawString("YUMMY!", 160, 380);
  }

  // پوش کردن اسپریت به نمایشگر
  spr.pushSprite(0, 0);
}

void drawTaskScreen() {
  spr.fillSprite(C_BG_TASKS);
  
  spr.setTextColor(C_WHITE, C_BG_TASKS);
  spr.setTextDatum(TC_DATUM);
  spr.setFont(&fonts::FreeSansBold12pt7b);
  spr.drawString("MY TASKS", 160, 20);
  
  spr.setFont(&fonts::FreeSans9pt7b);
  spr.setTextColor(C_EMO_BLUE, C_BG_TASKS);
  spr.drawString("^ Swipe UP to Face ^", 160, 50);
  
  int y = 80;
  spr.setTextDatum(TL_DATUM);
  
  for (int i = 0; i < appData.tasks.size(); i++) {
    if (y > 450) break;

    uint16_t cardColor = 0x3186; // خاکستری روشن‌تر
    spr.fillSmoothRoundRect(20, y, 280, 50, 10, cardColor); 
    
    if (appData.tasks[i].done) {
      spr.fillSmoothCircle(45, y+25, 12, C_EMO_GREEN);
      spr.setTextColor(C_BLACK, C_EMO_GREEN); 
      spr.drawString("v", 40, y+17);
    } else {
      spr.drawCircle(45, y+25, 12, C_WHITE);
    }
    
    spr.setTextColor(C_WHITE, cardColor); 
    spr.setFont(&fonts::FreeSans12pt7b);
    spr.drawString(appData.tasks[i].name, 75, y+15);
    y += 60;
  }
  spr.pushSprite(0, 0);
}

// ==========================================
// 3. ورودی (Touch)
// ==========================================
int16_t startY = -1;
unsigned long lastClick = 0;

void checkInput() {
  int32_t x, y;
  bool touched = tft.getTouch(&x, &y);
  
  if (touched) {
    if (startY == -1) startY = y; 
  } else {
    startY = -1;
  }
  
  if (touched && startY != -1) {
    int dist = startY - y; 
    
    // Swipe UP (برو به بالا - برگشت به صورت)
    if (dist > 50 && currentScreen == SCREEN_TASKS) {
      currentScreen = SCREEN_MAIN;
      startY = -1; delay(150); return;
    }
    
    // Swipe DOWN (برو به پایین - لیست تسک‌ها)
    if (dist < -50 && currentScreen == SCREEN_MAIN) {
      currentScreen = SCREEN_TASKS;
      startY = -1; delay(150); return;
    }
    
    // Click (تپ کردن)
    if (abs(dist) < 10 && (millis() - lastClick > 400)) {
      
      // در صفحه اصلی: دکمه مخفی یا تعامل با صورت
      if (currentScreen == SCREEN_MAIN) {
          // اگر وسط پایین صفحه کلیک شد (آب دادن دستی)
          if (y > 350 && x > 100 && x < 220) {
              localWaterEffect();
              lastClick = millis();
          }
          // اگر روی چشم‌ها کلیک شد (چشمک بزند)
          else if (y < 250) {
              isBlinking = true;
              lastBlink = millis() + 500; // کمی بیشتر نگه داره
          }
      }
      
      // در صفحه تسک‌ها: تیک زدن
      else if (currentScreen == SCREEN_TASKS) {
          int idx = (y - 80) / 60;
          if (idx >= 0 && idx < appData.tasks.size()) {
              appData.tasks[idx].done = !appData.tasks[idx].done;
              toggleTaskServer(idx);
              drawTaskScreen(); // رفرش فوری
              lastClick = millis();
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
  
  // راه اندازی LCD
  tft.init();
  tft.setRotation(1); // ممکن است برای شما 1 یا 3 باشد
  tft.fillScreen(C_BLACK);
  
  // ایجاد Sprite تمام صفحه برای انیمیشن روان
  spr.setColorDepth(16);
  spr.createSprite(320, 480); 

  // اتصال وای‌فای
  spr.drawString("CONNECTING...", 110, 220);
  spr.pushSprite(0,0);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 10) {
    delay(500); retry++;
    Serial.print(".");
  }
  
  fetchData(); // دریافت اولیه
}

unsigned long lastNetCheck = 0;

void loop() {
  checkInput();

  // آپدیت شبکه هر 5 ثانیه (بدون بلاک کردن انیمیشن)
  if (millis() - lastNetCheck > 5000) {
    fetchData();
    lastNetCheck = millis();
  }

  // مدیریت تایمر آب خوردن
  if (appData.isWatering && millis() % 200 == 0) {
     // افکت لرزش رنگ هنگام آب خوردن
  }

  // رسم صفحه فعال
  if (currentScreen == SCREEN_MAIN) {
    drawMainScreen();
  } 
  else if (currentScreen == SCREEN_TASKS) {
    // تسک‌ها استاتیک هستند مگر اینکه تاچ شوند، اما برای سادگی در لوپ می‌گذاریم
    // برای پرفورمنس بهتر می‌توان شرط گذاشت
    drawTaskScreen();
  }
  
  delay(10); // تاخیر کوچک برای ثبات
}
