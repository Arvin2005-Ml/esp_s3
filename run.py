# main.py
from fastapi import FastAPI
from fastapi.responses import HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from motor.motor_asyncio import AsyncIOMotorClient
from bson import ObjectId
from datetime import datetime
import random

app = FastAPI(title="Kawaii Plant Monitor")

# تنظیمات CORS (برای امنیت و جلوگیری از خطا در مرورگر)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# کانکشن دیتابیس
MONGO_URL = "mongodb://localhost:27017"
client = AsyncIOMotorClient(MONGO_URL)
db = client["kawaii_plant"]
sensor_collection = db["sensor_data"]
task_collection = db["tasks"]

# --- Models ---
class SensorData(BaseModel):
    temperature: float
    humidity: float
    moisture: float

class TaskCreate(BaseModel):
    title: str

# --- Utils ---
def serialize(doc):
    if not doc: return None
    doc["_id"] = str(doc["_id"])
    if "timestamp" in doc and isinstance(doc["timestamp"], datetime):
        doc["timestamp"] = doc["timestamp"].isoformat()
    return doc

def calculate_mood(moisture: float):
    if moisture > 80: return "EXCITED"
    elif moisture > 50: return "HAPPY"
    elif moisture > 30: return "SLEEPY"
    else: return "SAD"

# --- Endpoints ---

@app.post("/update-data")
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

@app.get("/get-data")
async def get_data():
    latest = await sensor_collection.find_one(sort=[("timestamp", -1)])
    if not latest:
        latest = {"temperature": 0, "humidity": 0, "moisture": 0, "light": 0, "mood": "SAD"}
    
    tasks = await task_collection.find().to_list(100)
    return {
        "sensor": serialize(latest),
        "tasks": [serialize(t) for t in tasks]
    }

# --- بخش جدید برای نمودار ---
@app.get("/get-history")
async def get_history():
    # دریافت ۵۰ رکورد آخر برای نمودار
    cursor = sensor_collection.find().sort("timestamp", -1).limit(20)
    history = await cursor.to_list(length=20)
    # معکوس کردن لیست تا زمان از چپ به راست باشد
    return [serialize(doc) for doc in history][::-1]

@app.post("/add-task")
async def add_task(task: TaskCreate):
    doc = {"title": task.title, "done": False, "created_at": datetime.utcnow()}
    await task_collection.insert_one(doc)
    return {"status": "task added"}

@app.post("/toggle-task/{task_id}")
async def toggle_task(task_id: str):
    try:
        await task_collection.update_one(
            {"_id": ObjectId(task_id)},
            {"$bit": {"done": {"xor": 1}}}
        )
        return {"status": "toggled"}
    except:
        return {"status": "error"}

@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard():
    with open("dashboard.html", "r", encoding="utf-8") as f:
        return f.read()

# اجرای برنامه با دستور زیر در ترمینال:
# uvicorn main:app --reload