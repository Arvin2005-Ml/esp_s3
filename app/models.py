from pydantic import BaseModel
from typing import Optional

class SensorData(BaseModel):
    temperature: float
    humidity: float
    moisture: int
    light: int
    gas: int
    water_level: int
    touch: int
    pump_state: int

class TaskCreate(BaseModel):
    title: str

class PumpCommand(BaseModel):
    pump_state: bool
