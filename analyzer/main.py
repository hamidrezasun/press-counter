from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from .config import get_influxdb_client
from .routes.devices import router as devices_router
from .routes.sessions import router as sessions_router
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pathlib import Path

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    client = get_influxdb_client()
    client.close()

app = FastAPI(
    title="InfluxDB Device API",
    description="RESTful API to list devices, retrieve device info records, and analyze work sessions from InfluxDB",
    version="1.0.0",
    lifespan=lifespan
)

# ✅ اضافه کردن CORS
origins = [
    "*"
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,  # می‌تونی ["*"] هم بذاری برای همه
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(devices_router)
app.include_router(sessions_router)

frontend_path = Path(__file__).parent / "dist"
app.mount("/assets", StaticFiles(directory=frontend_path / "assets"), name="assets")

@app.get("/{full_path:path}")
async def serve_react(full_path: str):
    # Make sure API routes are not overridden
    if full_path.startswith("devices") or full_path.startswith("sessions") or full_path.startswith("api"):
        return {"error": "API route not found"}  # FastAPI will handle API 404
    index_file = frontend_path / "index.html"
    if index_file.exists():
        return FileResponse(index_file)
    return {"error": "React build not found"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
