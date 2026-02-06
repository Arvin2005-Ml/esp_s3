from pydantic import BaseModel

class SensorData(BaseModel):
    temperature: float
    humidity: float
    moisture: float

class TaskCreate(BaseModel):
    title: str