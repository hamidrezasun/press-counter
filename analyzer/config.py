from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi

# InfluxDB configuration
INFLUXDB_URL = "http://192.168.1.37:8086"
INFLUXDB_TOKEN = "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
INFLUXDB_ORG = "tooswasher"
INFLUXDB_BUCKET = "manufacture"

# Initialize InfluxDB client and query API as singletons
_client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
_query_api = _client.query_api()

def get_influxdb_client() -> InfluxDBClient:
    """Return the initialized InfluxDB client."""
    return _client

def get_influxdb_query_api() -> QueryApi:
    """Return the initialized InfluxDB query API."""
    return _query_api

def get_bucket() -> str:
    """Return InfluxDB bucket name."""
    return INFLUXDB_BUCKET