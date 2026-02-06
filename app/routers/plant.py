from fastapi import APIRouter
from datetime import datetime
import random
from app.database import sensor_collection, task_collection
from app.models import SensorData
from app.utils import calculate_mood, serialize

router = APIRouter()

@router.post("/update-data")
async def update_data(data: SensorData):
    mood = calculate_mood(data.moisture)
    doc = {
        "temperature": data.temperature,
        "humidity": data.humidity,
        "moisture": data.moisture,
        "light": random.randint(20, 100),
        "mood": mood,
        "timestamp": datetime.utcnow()
    }
    await sensor_collection.insert_one(doc)
    return {"status": "ok", "mood": mood}

@router.get("/get-data")
async def get_data():
    latest = await sensor_collection.find_one(sort=[("timestamp", -1)])
    if not latest:
        latest = {"temperature": 0, "humidity": 0, "moisture": 0, "light": 0, "mood": "SAD"}
    
    tasks = await task_collection.find().to_list(100)
    return {
        "sensor": serialize(latest),
        "tasks": [serialize(t) for t in tasks]
    }

@router.get("/get-history")
async def get_history():
    cursor = sensor_collection.find().sort("timestamp", -1).limit(20)
    history = await cursor.to_list(length=20)
    return [serialize(doc) for doc in history][::-1]