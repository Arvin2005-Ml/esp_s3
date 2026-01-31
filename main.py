# server.py
import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime
from typing import Optional

app = FastAPI(title="Plant Bridge Server", version="2.0.0")


# ---------- مدل‌های داده ----------
class SensorPayload(BaseModel):
    moisture: float = Field(..., ge=0, le=100, description="Percent moisture 0-100")
    temp: float = Field(..., ge=-40, le=80, description="Temperature in °C")
    humidity: float = Field(..., ge=0, le=100, description="Relative humidity 0-100")
    light: float = Field(..., ge=0, le=1000, description="Light level (arbitrary scale 0-1000)")
    mood: Optional[str] = Field(None, description="Optional pre-computed plant mood by sensor")


class TaskIdx(BaseModel):
    index: int


class Action(BaseModel):
    action: str


# ---------- وضعیت در حافظه ----------
data_store = {
    "moisture": None,
    "temp": None,
    "humidity": None,
    "light": None,
    "mood": "UNKNOWN",
    "last_update": None,
    "tasks": [
        {"name": "Water Plants", "done": False},
        {"name": "Check Bugs", "done": True},
        {"name": "Mist Leaves", "done": False},
        {"name": "Talk to Plant", "done": False},
    ],
}


# ---------- توابع کمکی ----------
def compute_mood(moisture: float, temp: float) -> str:
    if moisture < 30:
        return "SAD"
    if temp > 30:
        return "HOT"
    return "HAPPY"


# ---------- آندپوینت‌ها ----------
@app.post("/update-data", summary="دریافت داده از ESP سنسور")
async def update_data(payload: SensorPayload):
    data_store["moisture"] = round(payload.moisture, 2)
    data_store["temp"] = round(payload.temp, 2)
    data_store["humidity"] = round(payload.humidity, 2)
    data_store["light"] = round(payload.light, 2)

    if payload.mood:
        data_store["mood"] = payload.mood.upper()
    else:
        data_store["mood"] = compute_mood(payload.moisture, payload.temp)

    data_store["last_update"] = datetime.utcnow().isoformat() + "Z"

    return {
        "status": "accepted",
        "stored": {
            "moisture": data_store["moisture"],
            "temp": data_store["temp"],
            "humidity": data_store["humidity"],
            "light": data_store["light"],
            "mood": data_store["mood"],
            "last_update": data_store["last_update"],
        },
    }


@app.get("/get-data", summary="ارسال آخرین داده برای ESP-S3")
async def get_data():
    if data_store["last_update"] is None:
        raise HTTPException(status_code=404, detail="No sensor data received yet.")
    return data_store


@app.post("/toggle-task", summary="تغییر وضعیت کارها")
async def toggle_task(item: TaskIdx):
    idx = item.index
    if not (0 <= idx < len(data_store["tasks"])):
        raise HTTPException(status_code=400, detail="Task index out of range.")
    data_store["tasks"][idx]["done"] = not data_store["tasks"][idx]["done"]
    return {"tasks": data_store["tasks"]}


@app.post("/send-touch", summary="دستور لمس/آب‌دهی از سمت کاربر")
async def send_action(act: Action):
    if act.action.upper() == "WATER":
        data_store["moisture"] = 100.0
        data_store["mood"] = "EXCITED"
        data_store["last_update"] = datetime.utcnow().isoformat() + "Z"
        return {"status": "ok", "message": "Moisture set to 100"}
    return {"status": "ignored", "message": f"Action {act.action} not recognized."}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
