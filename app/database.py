from motor.motor_asyncio import AsyncIOMotorClient

MONGO_URL = "mongodb://localhost:27017"
DB_NAME = "plant_db"

client = AsyncIOMotorClient(MONGO_URL)
db = client[DB_NAME]

sensor_collection = db["sensor_data"]
task_collection   = db["tasks"]
control_collection = db["control"]
mood_collection = db["mood_events"]
