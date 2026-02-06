from fastapi import APIRouter, Request
from fastapi.templating import Jinja2Templates

router = APIRouter()

# تنظیم پوشه تمپلیت روی دایرکتوری جاری (.) تا بتواند dashboard.html را از ریشه بخواند
templates = Jinja2Templates(directory=".")

@router.get("/dashboard")
async def dashboard(request: Request):
    # به جای open().read() از موتور تمپلیت استفاده می‌کنیم
    return templates.TemplateResponse("dashboard.html", {"request": request})