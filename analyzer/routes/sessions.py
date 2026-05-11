import logging
from fastapi import APIRouter, HTTPException, Depends, Query
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi
from typing import List, Optional
from datetime import datetime, timedelta, timezone, date
from pydantic import parse_obj_as
from ..config import get_influxdb_client, get_influxdb_query_api, get_bucket
from ..models import WorkSessionList, WorkSession, PressCounterList, PressCounterRecord, TotalCountResponse, ErrorLogList, ErrorLogEntry
import urllib.parse

# Configure logging with a StreamHandler to capture logs
logging.basicConfig(level=logging.ERROR)
logger = logging.getLogger(__name__)

# In-memory log storage for error logs (for demonstration; consider persistent storage for production)
error_logs = []

class ErrorLogHandler(logging.Handler):
    def emit(self, record):
        log_message = self.format(record)
        if "error" in log_message.lower() or "failed" in log_message.lower():
            error_logs.append({
                "timestamp": datetime.now(tz=timezone.utc).isoformat(),
                "message": log_message
            })

# Add custom handler to logger
logger.addHandler(ErrorLogHandler())

router = APIRouter(
    prefix="/sessions",
    tags=["sessions"],
    responses={404: {"description": "Not found"}}
)

def get_client():
    """Dependency to provide InfluxDB client."""
    return get_influxdb_client()

def get_query_api():
    """Dependency to provide InfluxDB query API."""
    return get_influxdb_query_api()

def get_die_id(device: str, timestamp: datetime, query_api: QueryApi) -> Optional[str]:
    """
    Retrieve the die_id from the most recent nfc_event with tag_data like die:{id}
    before the given timestamp for the specified device.
    """
    bucket = get_bucket()
    die_query = f'''
    from(bucket: "{bucket}")
        |> range(start: 0, stop: {timestamp.isoformat()})
        |> filter(fn: (r) => r["_measurement"] == "nfc_event" and r["device"] == "{device}")
        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
        |> keep(columns: ["_time", "tag_data"])
        |> filter(fn: (r) => r["tag_data"] =~ /^die:/)
        |> sort(columns: ["_time"], desc: true)
        |> limit(n: 1)
    '''
    try:
        die_result = query_api.query(die_query)
        die_events = [
            {"timestamp": row["_time"], "tag_data": row.values.get("tag_data", "")}
            for table in die_result for row in table.records
        ]
        if die_events:
            tag_data = die_events[0]["tag_data"]
            if isinstance(tag_data, str) and tag_data.startswith("die:"):
                die_id = tag_data.split(":")[1] if ":" in tag_data else None
                logger.info(f"Found die_id {die_id} for timestamp {timestamp}")
                return die_id
        logger.info(f"No die_id found for device {device} before {timestamp}")
        return None
    except Exception as e:
        logger.error(f"Die query failed for device {device} at {timestamp}: {str(e)}")
        return None

@router.get("/{device}", response_model=WorkSessionList, summary="List work sessions for a device within the past X days")
async def list_work_sessions(
    device: str,
    days: Optional[int] = Query(None, ge=1, description="Number of past days to filter sessions (optional)"),
    client: InfluxDBClient = Depends(get_client),
    query_api: QueryApi = Depends(get_query_api)
):
    """
    Retrieve work sessions for the specified device, optionally filtered to the past X days, including:
    - Session duration (null for ongoing)
    - Average time between press_counter records
    - Average temperature
    - Missing counts (non-sequential counts in press_counter)
    - Average press duration
    - Total count (last count in press_counter records)
    - Die ID (from most recent nfc_event with tag_data like die:{id} before session start)
    - End reason (nfc_event, device_status, timeout, or null for ongoing)
    A session starts with an nfc_event (tag_data like personnel:{id}, relay_state=true) and ends with
    an nfc_event (relay_state=false), device_status (relay=0), or 10 minutes of press_counter inactivity.
    """
    bucket = get_bucket()
    range_filter = f"range(start: -{days}d)" if days else "range(start: 0)"
    flux_query = f'''
    union(tables: [
        from(bucket: "{bucket}")
            |> {range_filter}
            |> filter(fn: (r) => r["_measurement"] == "nfc_event" and r["device"] == "{device}")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> keep(columns: ["_time", "tag_data", "relay_state", "device"]),
        from(bucket: "{bucket}")
            |> {range_filter}
            |> filter(fn: (r) => r["_measurement"] == "device_status" and r["device"] == "{device}")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> keep(columns: ["_time", "relay", "device"])
            |> map(fn: (r) => ({{r with tag_data: "", relay_state: r.relay == 1, end_reason: "device_status"}}))
    ])
    |> sort(columns: ["_time"], desc: false)
    '''
    
    try:
        result = query_api.query(flux_query)
        events = []
        for table in result:
            for row in table.records:
                logger.info(f"FluxRecord values: {row.values}")
                event_device = row.values.get("device", "")
                if event_device != device:
                    continue
                tag_data = row.values.get("tag_data", "")
                if tag_data is None:
                    logger.warning(f"tag_data is None for event at {row['_time']}")
                    tag_data = ""
                end_reason = row.values.get("end_reason", "nfc_event" if tag_data else "device_status")
                events.append({
                    "timestamp": row["_time"],
                    "tag_data": tag_data,
                    "relay_state": row.values.get("relay_state", row.values.get("relay") == 1),
                    "device": event_device,
                    "end_reason": end_reason
                })
        logger.info(f"Retrieved {len(events)} events for device {device} with {range_filter}: {events}")

        if not events:
            logger.warning(f"No events found for device {device} with {range_filter}")
            return {"sessions": []}

        # Process work sessions
        sessions = []
        current_session = None
        PRESS_TIMEOUT = 600_000  # 10 minutes in milliseconds

        for event in events:
            timestamp = event["timestamp"]
            tag_data = event["tag_data"]
            relay_state = event["relay_state"]
            end_reason = event["end_reason"]

            # Start a new session
            if isinstance(tag_data, str) and tag_data.startswith("personnel:") and relay_state:
                if current_session:
                    current_session["end_time"] = timestamp
                    current_session["end_reason"] = "nfc_event"  # Ended by new session start
                    current_session["session_id"] = f"{current_session['device']},{current_session['start_time'].isoformat()},{current_session['end_time'].isoformat() if current_session['end_time'] else 'null'}"
                    sessions.append(current_session)
                personnel_id = tag_data.split(":")[1] if ":" in tag_data else "unknown"
                current_session = {
                    "session_id": None,  # Set later when session ends or at timeout
                    "device": device,
                    "personnel_id": personnel_id,
                    "start_time": timestamp,
                    "end_time": None,
                    "end_reason": None,
                    "press_records": [],
                    "die_id": get_die_id(device, timestamp, query_api)
                }
                logger.info(f"Started session for device {device} at {timestamp}")

            # End a session
            if current_session and not relay_state:
                current_session["end_time"] = timestamp
                current_session["end_reason"] = end_reason
                current_session["session_id"] = f"{current_session['device']},{current_session['start_time'].isoformat()},{current_session['end_time'].isoformat() if current_session['end_time'] else 'null'}"
                sessions.append(current_session)
                logger.info(f"Ended session {current_session['session_id']} with reason {end_reason}")
                current_session = None

        # Check for timeout-based session end and include ongoing sessions
        if current_session:
            press_query = f'''
            from(bucket: "{bucket}")
                |> range(start: {current_session["start_time"].isoformat()})
                |> filter(fn: (r) => r["_measurement"] == "press_counter" and r["device"] == "{device}")
                |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                |> keep(columns: ["_time", "device"])
                |> sort(columns: ["_time"], desc: false)
            '''
            try:
                press_result = query_api.query(press_query)
                press_times = [
                    row["_time"] for table in press_result for row in table.records
                    if row.values.get("device") == device
                ]
                last_press_time = max(press_times, default=None)
                if last_press_time and (datetime.now(tz=timezone.utc) - last_press_time).total_seconds() * 1000 > PRESS_TIMEOUT:
                    current_session["end_time"] = last_press_time + timedelta(milliseconds=PRESS_TIMEOUT)
                    current_session["end_reason"] = "timeout"
                    current_session["session_id"] = f"{current_session['device']},{current_session['start_time'].isoformat()},{current_session['end_time'].isoformat()}"
                    press_query = f'''
                    from(bucket: "{bucket}")
                        |> range(start: {current_session["start_time"].isoformat()}, stop: {current_session["end_time"].isoformat()})
                        |> filter(fn: (r) => r["_measurement"] == "press_counter" and r["device"] == "{device}")
                        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                        |> keep(columns: ["_time", "count", "duration", "temperature", "device"])
                        |> sort(columns: ["_time"], desc: false)
                    '''
                    press_result = query_api.query(press_query)
                    current_session["press_records"] = [
                        {
                            "timestamp": row["_time"],
                            "count": int(row.values.get("count", 0)) if row.values.get("count") is not None else 0,
                            "duration": int(row.values.get("duration", 0)) if row.values.get("duration") is not None else 0,
                            "temperature": float(row.values.get("temperature", 0.0)) if row.values.get("temperature") is not None else 0.0
                        }
                        for table in press_result for row in table.records
                        if row.values.get("device") == device
                    ]
                    sessions.append(current_session)
                    logger.info(f"Closed session {current_session['session_id']} due to timeout: {len(current_session['press_records'])} press records")
                else:
                    # Ongoing session
                    current_session["session_id"] = f"{current_session['device']},{current_session['start_time'].isoformat()},null"
                    press_query = f'''
                    from(bucket: "{bucket}")
                        |> range(start: {current_session["start_time"].isoformat()})
                        |> filter(fn: (r) => r["_measurement"] == "press_counter" and r["device"] == "{device}")
                        |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                        |> keep(columns: ["_time", "count", "duration", "temperature", "device"])
                        |> sort(columns: ["_time"], desc: false)
                    '''
                    press_result = query_api.query(press_query)
                    current_session["press_records"] = [
                        {
                            "timestamp": row["_time"],
                            "count": int(row.values.get("count", 0)) if row.values.get("count") is not None else 0,
                            "duration": int(row.values.get("duration", 0)) if row.values.get("duration") is not None else 0,
                            "temperature": float(row.values.get("temperature", 0.0)) if row.values.get("temperature") is not None else 0.0
                        }
                        for table in press_result for row in table.records
                        if row.values.get("device") == device
                    ]
                    sessions.append(current_session)
                    logger.info(f"Included ongoing session {current_session['session_id']}: {len(current_session['press_records'])} press records")
            except Exception as e:
                logger.error(f"Press query for timeout check failed: {str(e)}")
                current_session["session_id"] = f"{current_session['device']},{current_session['start_time'].isoformat()},null"
                current_session["press_records"] = []
                sessions.append(current_session)
                logger.info(f"Included ongoing session {current_session['session_id']} despite press query failure")

        # Query press_counter records for sessions without them
        for session in sessions:
            if not session["press_records"]:
                start_time = session["start_time"]
                end_time = session["end_time"] or datetime.now(tz=timezone.utc)
                press_query = f'''
                from(bucket: "{bucket}")
                    |> range(start: {start_time.isoformat()}, stop: {end_time.isoformat()})
                    |> filter(fn: (r) => r["_measurement"] == "press_counter" and r["device"] == "{device}")
                    |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
                    |> keep(columns: ["_time", "count", "duration", "temperature", "device"])
                    |> sort(columns: ["_time"], desc: false)
                '''
                try:
                    press_result = query_api.query(press_query)
                    session["press_records"] = [
                        {
                            "timestamp": row["_time"],
                            "count": int(row.values.get("count", 0)) if row.values.get("count") is not None else 0,
                            "duration": int(row.values.get("duration", 0)) if row.values.get("duration") is not None else 0,
                            "temperature": float(row.values.get("temperature", 0.0)) if row.values.get("temperature") is not None else 0.0
                        }
                        for table in press_result for row in table.records
                        if row.values.get("device") == device
                    ]
                    logger.info(f"Session {session['session_id']}: {len(session['press_records'])} press records")
                except Exception as e:
                    logger.error(f"Press query failed for session {session['session_id']}: {str(e)}")
                    session["press_records"] = []

        # Calculate session metrics
        final_sessions = []
        for session in sessions:
            start_time = session["start_time"]
            end_time = session["end_time"]
            duration_seconds = None
            if end_time:
                duration_seconds = (end_time - start_time).total_seconds()
            
            counts = [r["count"] for r in session["press_records"] if r["count"] != 0]
            total_count = max(counts, default=0)  # Last count or 0 if no press records
            expected_counts = list(range(min(counts, default=1), max(counts, default=0) + 1)) if counts else []
            missing_counts = [c for c in expected_counts if c not in counts]
            press_times = [r["timestamp"] for r in session["press_records"]]
            intervals = [
                (press_times[i + 1] - press_times[i]).total_seconds()
                for i in range(len(press_times) - 1)
            ]
            avg_press_interval = sum(intervals) / len(intervals) if intervals else 0.0
            temperatures = [r["temperature"] for r in session["press_records"] if r["temperature"] != 0.0]
            durations = [r["duration"] for r in session["press_records"] if r["duration"] != 0]
            avg_temperature = sum(temperatures) / len(temperatures) if temperatures else 0.0
            avg_press_duration = sum(durations) / len(durations) if durations else 0.0

            final_sessions.append({
                "session_id": session["session_id"],
                "device": session["device"],
                "personnel_id": session["personnel_id"],
                "start_time": start_time.isoformat(),
                "end_time": end_time.isoformat() if end_time else None,
                "duration_seconds": duration_seconds,
                "missing_counts": missing_counts,
                "average_press_interval_seconds": avg_press_interval,
                "average_temperature": avg_temperature,
                "average_press_duration_ms": avg_press_duration,
                "total_count": total_count,
                "die_id": session["die_id"],
                "end_reason": session["end_reason"]
            })

        logger.info(f"Returning {len(final_sessions)} sessions for device {device} with {range_filter}")
        return {"sessions": final_sessions}
    except Exception as e:
        logger.error(f"Error in list_work_sessions for device {device}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Query failed: {str(e)}")

@router.get("/{device}/{begin_date}/{end_date}", response_model=PressCounterList, summary="Get press_counter records for a session")
async def get_session_presses(
    device: str,
    begin_date: str,
    end_date: Optional[str] = None,
    client: InfluxDBClient = Depends(get_client),
    query_api: QueryApi = Depends(get_query_api)
):
    """
    Retrieve press_counter records for the specified session, identified by device, begin_date, and end_date, including:
    - timestamp, count, duration, temperature
    The begin_date and end_date must be in ISO 8601 format; end_date is optional (defaults to now for ongoing sessions).
    """
    bucket = get_bucket()
    try:
        # Decode URL-encoded parameters
        device = urllib.parse.unquote(device)
        begin_date = urllib.parse.unquote(begin_date)
        end_date = urllib.parse.unquote(end_date) if end_date else None
        logger.info(f"Processing session for device {device} from {begin_date} to {end_date or 'now'}")

        # Parse and validate begin_date
        try:
            start_dt = datetime.fromisoformat(begin_date.replace("Z", "+00:00"))
        except ValueError:
            logger.error(f"Invalid begin_date format: {begin_date}")
            raise HTTPException(status_code=400, detail="Invalid begin_date format. Expected ISO 8601")

        # Parse end_date; use current time if not provided (ongoing session)
        if end_date is None:
            end_dt = datetime.now(tz=timezone.utc)
        else:
            try:
                end_dt = datetime.fromisoformat(end_date.replace("Z", "+00:00"))
            except ValueError:
                logger.error(f"Invalid end_date format: {end_date}")
                raise HTTPException(status_code=400, detail="Invalid end_date format. Expected ISO 8601")

        press_query = f'''
        from(bucket: "{bucket}")
            |> range(start: {start_dt.isoformat()}, stop: {end_dt.isoformat()})
            |> filter(fn: (r) => r["_measurement"] == "press_counter" and r["device"] == "{device}")
            |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
            |> keep(columns: ["_time", "count", "duration", "temperature", "device"])
            |> sort(columns: ["_time"], desc: false)
        '''
        press_result = query_api.query(press_query)
        records = [
            {
                "timestamp": row["_time"].isoformat(),
                "count": int(row.values.get("count", 0)) if row.values.get("count") is not None else 0,
                "duration": int(row.values.get("duration", 0)) if row.values.get("duration") is not None else 0,
                "temperature": float(row.values.get("temperature", 0.0)) if row.values.get("temperature") is not None else 0.0
            }
            for table in press_result for row in table.records
            if row.values.get("device") == device
        ]
        logger.info(f"Retrieved {len(records)} press records for device {device} from {begin_date} to {end_date or 'now'}")
        return {"records": records}
    except Exception as e:
        logger.error(f"Press query failed for device {device} from {begin_date} to {end_date or 'now'}: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Query failed: {str(e)}")
