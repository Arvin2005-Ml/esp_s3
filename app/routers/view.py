from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter()

@router.get("/dashboard", response_class=HTMLResponse)
async def dashboard():
    # فرض بر این است که فایل dashboard.html در پوشه ریشه پروژه کنار run.py است
    try:
        with open("dashboard.html", "r", encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return "<h1>Dashboard file not found</h1>"