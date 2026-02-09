from fastapi import APIRouter, Request
from app.database import sensor_collection, task_collection, control_collection, mood_collection
from app.models import SensorData, TaskCreate, PumpCommand
from app.utils import serialize, calculate_mood
from datetime import datetime, timezone

router = APIRouter()

@router.post("/update-data")
async def update_data(data: SensorData):
    # لاگ کردن برای اطمینان از دریافت داده
    print(f"Received from ESP: {data.dict()}")
    
    mood = calculate_mood(data.moisture)
    
    # ساخت داکیومنت کامل برای دیتابیس
    doc = data.dict()
    doc["mood"] = mood
    doc["timestamp"] = datetime.now(timezone.utc)

    await sensor_collection.insert_one(doc)
    return {"status": "ok", "mood": mood}

@router.get("/get-data")
async def get_data():
    # 1. گرفتن آخرین داده ثبت شده
    latest = await sensor_collection.find_one(sort=[("timestamp", -1)])
    
    # 2. گرفتن وضعیت پمپ
    pump_doc = await control_collection.find_one({"_id": "pump"})
    current_pump_state = pump_doc.get("pump_state", False) if pump_doc else False

    # 3. داده‌های پیش‌فرض اگر دیتابیس خالی بود
    data = {
        "temperature": 0, "humidity": 0, "moisture": 0, "light": 0,
        "gas": 0, "water_level": 0, "touch": 0, "mood": "Unknown"
    }

    # 4. اگر داده جدید در دیتابیس بود، جایگزین کن
    if latest:
        serialized_latest = serialize(latest)
        data.update(serialized_latest) # این خط کلیدهای سنسور را به data اضافه می‌کند

    # 5. اضافه کردن اطلاعات سیستمی و پمپ
    data["pump_state"] = current_pump_state
    data["server_time"] = datetime.now(timezone.utc).isoformat()
    
    # نکته مهم: اگر تسک‌ها را هم نیاز دارید، این خط را فعال کنید، اما فعلا برای سادگی حذف کردم
    # data["tasks"] = ... 

    return data

@router.get("/get-history")
async def get_history():
    cursor = sensor_collection.find().sort("timestamp", -1).limit(20)
    history = await cursor.to_list(length=20)
    return [serialize(doc) for doc in history][::-1]

@router.post("/set-pump")
async def set_pump(cmd: PumpCommand):
    await control_collection.update_one(
        {"_id": "pump"},
        {"$set": {"pump_state": cmd.pump_state, "updated_at": datetime.utcnow()}},
        upsert=True
    )
    return {"status": "ok", "pump_state": cmd.pump_state}

@router.get("/get-pump")
async def get_pump():
    ctrl = await control_collection.find_one({"_id": "pump"}) or {"pump_state": False}
    return {"pump_state": ctrl.get("pump_state", False)}