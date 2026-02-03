import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from motor.motor_asyncio import AsyncIOMotorClient
from datetime import datetime
from typing import Optional, List

app = FastAPI(title="Plant Bridge Server", version="2.1.0")

# ---------- MongoDB ----------
client = AsyncIOMotorClient("mongodb://localhost:27017")
db = client["plant_monitor"]

sensor_col = db["sensor_data"]
task_col = db["tasks"]

# ---------- Models ----------
class SensorPayload(BaseModel):
    moisture: float = Field(..., ge=0, le=100)
    temp: float = Field(..., ge=-40, le=80)
    humidity: float = Field(..., ge=0, le=100)
    light: float = Field(..., ge=0, le=1000)
    mood: Optional[str] = None


class Task(BaseModel):
    name: str
    done: bool = False


class TaskIdx(BaseModel):
    index: int


class Action(BaseModel):
    action: str


# ---------- Utils ----------
def compute_mood(moisture: float, temp: float) -> str:
    if moisture < 30:
        return "SAD"
    if temp > 30:
        return "HOT"
    return "HAPPY"


# ---------- Endpoints ----------
@app.post("/update-data")
async def update_data(payload: SensorPayload):
    mood = payload.mood.upper() if payload.mood else compute_mood(payload.moisture, payload.temp)

    doc = {
        "moisture": round(payload.moisture, 2),
        "temp": round(payload.temp, 2),
        "humidity": round(payload.humidity, 2),
        "light": round(payload.light, 2),
        "mood": mood,
        "timestamp": datetime.utcnow(),
    }

    await sensor_col.insert_one(doc)
    return {"status": "stored", "data": doc}


@app.get("/get-data")
async def get_data():
    sensor = await sensor_col.find().sort("timestamp", -1).to_list(1)
    if not sensor:
        raise HTTPException(404, "No sensor data")

    tasks = []
    async for t in task_col.find():
        t["id"] = str(t["_id"])
        t.pop("_id")
        tasks.append(t)

    sensor[0].pop("_id")
    return {
        "sensor": sensor[0],
        "tasks": tasks
    }


@app.post("/toggle-task")
async def toggle_task(item: TaskIdx):
    tasks = await task_col.find().to_list(None)
    if not (0 <= item.index < len(tasks)):
        raise HTTPException(400, "Index out of range")

    task = tasks[item.index]
    await task_col.update_one(
        {"_id": task["_id"]},
        {"$set": {"done": not task["done"]}}
    )
    return {"status": "updated"}


@app.post("/add-task")
async def add_task(task: Task):
    await task_col.insert_one(task.dict())
    return {"status": "task added"}


@app.post("/send-touch")
async def send_action(act: Action):
    if act.action.upper() == "WATER":
        doc = {
            "moisture": 100.0,
            "temp": None,
            "humidity": None,
            "light": None,
            "mood": "EXCITED",
            "timestamp": datetime.utcnow(),
            "manual": True
        }
        await sensor_col.insert_one(doc)
        return {"status": "ok", "message": "Plant watered"}
    return {"status": "ignored"}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
