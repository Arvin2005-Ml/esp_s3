import random
from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn

app = FastAPI()

# مدل داده برای دریافت دستور لمس
class ActionData(BaseModel):
    action: str  # مثلا "WATER" یا "SUN"

# وضعیت اولیه گلدان
plant_status = {
    "moisture": 80,      # رطوبت خاک (0-100)
    "sunlight": 60,      # نور (0-100)
    "temp": 24.0,        # دما
    "message": "I'm Happy!",
    "mood": "happy"      # happy, thirsty, hot
}

@app.get("/get-data")
async def get_plant_stats():
    """
    این تابع داده‌های سنسورها را شبیه‌سازی می‌کند
    """
    global plant_status
    
    # 1. شبیه‌سازی تغییرات رندوم دما (بین 20 تا 30)
    change = random.uniform(-0.5, 0.5)
    plant_status["temp"] = round(max(20, min(30, plant_status["temp"] + change)), 1)
    
    # 2. شبیه‌سازی خشک شدن خاک (هر بار کمی کم می‌شود)
    plant_status["moisture"] = max(0, plant_status["moisture"] - random.randint(0, 2))
    
    # 3. شبیه‌سازی نور (رندوم)
    plant_status["sunlight"] = random.randint(30, 100)
    
    # 4. تعیین پیام بر اساس وضعیت
    if plant_status["moisture"] < 30:
        plant_status["message"] = "Water Me Please!"
        plant_status["mood"] = "thirsty"
    elif plant_status["temp"] > 28:
        plant_status["message"] = "It's too HOT!"
        plant_status["mood"] = "hot"
    else:
        plant_status["message"] = "Feeling Good :)"
        plant_status["mood"] = "happy"
        
    return plant_status

@app.post("/send-touch")
async def perform_action(data: ActionData):
    """
    وقتی دکمه روی LCD لمس شود این تابع اجرا می‌شود
    """
    global plant_status
    
    if data.action == "WATER":
        plant_status["moisture"] = 100
        plant_status["message"] = "Yummy Water!"
        plant_status["mood"] = "happy"
        print("--> Plant was watered!")
        
    return {"status": "Updated", "new_level": plant_status["moisture"]}

if __name__ == "__main__":
    # اجرای سرور روی تمام کارت‌های شبکه با پورت 8000
    uvicorn.run(app, host="0.0.0.0", port=8000)