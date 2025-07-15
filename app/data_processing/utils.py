from datetime import datetime, timedelta, timezone

# --- Configuration ---
INFLUXDB_URL = "http://192.168.1.37:8086"
INFLUXDB_TOKEN = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
INFLUXDB_ORG = "tooswasher"
INFLUXDB_BUCKET = "manufacture"
FIELD_NAME = "count"
PULSE_DURATION_FIELD = "pulse_duration_ms"
SEND_DURATION_FIELD = "send_duration_ms"
MEASUREMENT = "production"
RFID_MEASUREMENT = "rfid_tag"

UTC_TZ = timezone.utc
IRAN_TZ = timezone(timedelta(hours=3, minutes=30))  # UTC+3:30

def get_signal_quality(rssi):
    """Convert RSSI value to human-readable signal quality."""
    if rssi >= -50:
        return "Excellent"
    if rssi >= -60:
        return "Good"
    if rssi >= -70:
        return "Fair"
    return "Weak"

def escape_flux_string(s: str) -> str:
    """Escapes a string for safe inclusion within a Flux query string literal."""
    if s is None:
        return ""
    s_str = str(s)
    return s_str.replace('\\', '\\\\').replace('"', '\\"')