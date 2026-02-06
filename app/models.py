from pydantic import BaseModel

class SensorData(BaseModel):
    temperature: int
    humidity: int
    moisture: int

class TaskCreate(BaseModel):
    title: str