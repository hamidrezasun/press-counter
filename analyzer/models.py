from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime

class DeviceList(BaseModel):
    devices: List[str]

class DeviceInfo(BaseModel):
    timestamp: str
    ip: str
    rssi: int
    mac: str

class DeviceInfoList(BaseModel):
    records: List[DeviceInfo]

class PressCounterRecord(BaseModel):
    timestamp: str
    count: int
    duration: int
    temperature: float

class PressCounterList(BaseModel):
    records: List[PressCounterRecord]

class WorkSession(BaseModel):
    session_id: str
    device: str
    personnel_id: str
    start_time: str
    end_time: Optional[str] = None
    duration_seconds: Optional[float] = None
    missing_counts: List[int]
    average_press_interval_seconds: float
    average_temperature: float
    average_press_duration_ms: float
    total_count: int
    die_id: Optional[str] = None
    end_reason: Optional[str] = None

class WorkSessionList(BaseModel):
    sessions: List[WorkSession]

class PressCounterRecord(BaseModel):
    timestamp: str
    count: int
    duration: int
    temperature: float

class PressCounterList(BaseModel):
    records: List[PressCounterRecord]

class TotalCountResponse(BaseModel):
    device: str
    date: str
    total_count: int

class ErrorLogEntry(BaseModel):
    timestamp: str
    message: str

class ErrorLogList(BaseModel):
    logs: List[ErrorLogEntry]