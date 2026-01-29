import random
from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn

app = FastAPI()

class TaskIdx(BaseModel):
    index: int
class Action(BaseModel):
    action: str

# دیتابیس
data_store = {
    "moisture": 80, "temp": 24.5, "humidity": 45, "light": 70,
    "mood": "HAPPY",
    "tasks": [
        {"name": "Water Plants", "done": False},
        {"name": "Check Bugs", "done": True},
        {"name": "Mist Leaves", "done": False},
        {"name": "Talk to Plant", "done": False}
    ]
}

@app.get("/get-data")
async def get_data():
    global data_store
    data_store["moisture"] = max(0, data_store["moisture"] - 0.1)
    data_store["temp"] = round(random.uniform(22.0, 28.0), 1)
    if data_store["moisture"] < 30: data_store["mood"] = "SAD"
    elif data_store["temp"] > 30: data_store["mood"] = "HOT"
    else: data_store["mood"] = "HAPPY"
    return data_store

@app.post("/toggle-task")
async def toggle_task(item: TaskIdx):
    idx = item.index
    if 0 <= idx < len(data_store["tasks"]):
        data_store["tasks"][idx]["done"] = not data_store["tasks"][idx]["done"]
    return {"tasks": data_store["tasks"]}

@app.post("/send-touch")
async def send_action(act: Action):
    if act.action == "WATER":
        data_store["moisture"] = 100
        data_store["mood"] = "EXCITED"
    return {"status": "ok"}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)