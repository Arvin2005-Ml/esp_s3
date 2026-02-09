from fastapi import APIRouter
from datetime import datetime
from app.database import sensor_collection, task_collection,control_collection
from app.models import SensorData, PumpCommand, MoodEvent
from app.utils import calculate_mood, serialize

router = APIRouter()

@router.post("/update-data")
async def update_data(data: SensorData):
    mood = calculate_mood(data.moisture)

    doc = {
        "temperature": data.temperature,
        "humidity": data.humidity,
        "moisture": data.moisture,
        "light": data.light,
        "gas": data.gas,
        "water_level": data.water_level,
        "touch": data.touch,
        "pump_state": data.pump_state,
        "mood": mood,
        "timestamp": datetime.utcnow()
    }

    await sensor_collection.insert_one(doc)
    return {"status": "ok", "mood": mood}

from datetime import datetime, timezone

@router.get("/get-data")
async def get_data():
    latest = await sensor_collection.find_one(sort=[("timestamp", -1)])
    if not latest:
        latest = {
            "temperature": 0,
            "humidity": 0,
            "moisture": 0,
            "light": 0,
            "gas": 0,
            "water_level": 0,
            "touch": 0,
            "pump_state": 0,
            "mood": "SAD",
            "timestamp": datetime.utcnow()
        }

    tasks = await task_collection.find().to_list(100)
    pump = await control_collection.find_one({"_id": "pump"}) or {"pump_state": False}

    return {
        "sensor": serialize(latest),
        "tasks": [serialize(t) for t in tasks],
        "server_time": datetime.now(timezone.utc).isoformat(),
        "pump_state": pump.get("pump_state", False)
    }

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

@router.post("/log-mood")
async def log_mood(event: MoodEvent):
    doc = {
        "mood": event.mood,
        "timestamp": datetime.utcnow()
    }
    await mood_collection.insert_one(doc)
    return {"status": "logged"}
