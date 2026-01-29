# نصب کتابخانه‌ها در ترمینال:
# pip install fastapi uvicorn pydantic

from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn
from datetime import datetime

app = FastAPI()

# --- 1. مدل داده برای دریافت تاچ ---
# این کلاسی است که چک می‌کند دیتای ارسالی از ESP32 درست باشد
class TouchData(BaseModel):
    x: int
    y: int

# --- 2. حافظه وضعیت (State) ---
# این دیکشنری وضعیت فعلی سیستم را نگه می‌دارد
# وقتی سرور را خاموش کنید این اطلاعات پاک می‌شود
system_state = {
    "message": "Ready to Start", # پیامی که روی LCD نمایش داده می‌شود
    "led_on": False,             # وضعیت LED مجازی (قرمز/خاکستری)
    "touch_count": 0             # تعداد کل تاچ‌ها
}

# --- 3. متد ارسال دیتا به ESP32 (GET) ---
# آدرس: http://IP:8000/get-data
@app.get("/get-data")
async def send_data_to_esp32():
    """
    این تابع توسط ESP32 هر 2 ثانیه صدا زده می‌شود.
    آخرین وضعیت سیستم را برمی‌گرداند.
    """
    return system_state

# --- 4. متد دریافت تاچ از ESP32 (POST) ---
# آدرس: http://IP:8000/send-touch
@app.post("/send-touch")
async def receive_touch_from_esp32(data: TouchData):
    """
    این تابع وقتی صفحه لمس شد صدا زده می‌شود.
    مختصات را می‌گیرد و یک کاری انجام می‌دهد (مثلا LED را روشن/خاموش می‌کند).
    """
    print(f"--> Touch Received: X={data.x}, Y={data.y}")
    
    # آپدیت کردن منطق برنامه:
    system_state["touch_count"] += 1
    
    # 1. تغییر وضعیت LED (خاموش/روشن کردن)
    system_state["led_on"] = not system_state["led_on"]
    
    # 2. تغییر پیام بر اساس وضعیت
    if system_state["led_on"]:
        system_state["message"] = f"LED ON (Touch #{system_state['touch_count']})"
    else:
        system_state["message"] = f"LED OFF (Touch #{system_state['touch_count']})"
        
    return {
        "status": "success", 
        "server_time": datetime.now().strftime("%H:%M:%S"),
        "new_led_state": system_state["led_on"]
    }

# --- اجرای برنامه ---
if __name__ == "__main__":
    print("Server is running...")
    print("Make sure your ESP32 connects to your Computer's IP address!")
    # host="0.0.0.0" یعنی سرور در شبکه لوکال قابل دیدن باشد (خیلی مهم)
    uvicorn.run(app, host="0.0.0.0", port=8000)