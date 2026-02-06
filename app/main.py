from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app.routers.plant import router as plant_router
from app.routers.tasks import router as tasks_router
from app.routers.view import router as view_router
app = FastAPI(title="Kawaii Plant Monitor")

# تنظیمات CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# اضافه کردن روترها (Routers)
app.include_router(plant_router)
app.include_router(tasks_router)
app.include_router(view_router)