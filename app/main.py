from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app.routers import plant, tasks, view

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
app.include_router(plant.router)
app.include_router(tasks.router)
app.include_router(view.router)