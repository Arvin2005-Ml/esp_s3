from fastapi import APIRouter
from bson import ObjectId
from datetime import datetime
from app.database import task_collection
from app.models import TaskCreate

router = APIRouter()

@router.post("/add-task")
async def add_task(task: TaskCreate):
    doc = {"title": task.title, "done": False, "created_at": datetime.utcnow()}
    await task_collection.insert_one(doc)
    return {"status": "task added"}

@router.post("/toggle-task/{task_id}")
async def toggle_task(task_id: str):
    try:
        # نکته: عملگر bit xor برای اعداد صحیح است. 
        # اگر done بولین است بهتر است از روش دیگری استفاده شود، اما طبق کد اصلی شما:
        await task_collection.update_one(
            {"_id": ObjectId(task_id)},
            {"$bit": {"done": {"xor": 1}}}
        )
        return {"status": "toggled"}
    except:
        return {"status": "error"}