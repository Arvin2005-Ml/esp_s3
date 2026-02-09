from pydantic import BaseModel, Field
from typing import Optional

# این مدل باید دقیقاً با چیزی که ESP می‌فرستد یکی باشد
class SensorData(BaseModel):
    temperature: float = 0.0
    humidity: float = 0.0
    moisture: float = 0.0  # قبلا int بود، باید float باشد
    light: float = 0.0     # قبلا int بود، باید float باشد
    gas: float = 0.0       # این فیلد را نداشتید یا int بود
    water_level: float = 0.0
    touch: int = 0
    pump_state: bool = False # بهتر است bool باشد

class TaskCreate(BaseModel):
    title: str

class PumpCommand(BaseModel):
    pump_state: bool

class MoodEvent(BaseModel):
    mood: str
