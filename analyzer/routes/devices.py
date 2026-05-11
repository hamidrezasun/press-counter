from fastapi import APIRouter, HTTPException, Depends
from influxdb_client import InfluxDBClient
from influxdb_client.client.query_api import QueryApi
from typing import List
from ..config import get_influxdb_client, get_influxdb_query_api, get_bucket
from ..models import DeviceList, DeviceInfoList

router = APIRouter(
    prefix="/devices",
    tags=["devices"],
    responses={404: {"description": "Not found"}}
)

def get_client():
    """Dependency to provide InfluxDB client."""
    return get_influxdb_client()

def get_query_api():
    """Dependency to provide InfluxDB query API."""
    return get_influxdb_query_api()

@router.get("/", response_model=DeviceList, summary="List all unique devices from the past 365 days")
async def list_devices(client: InfluxDBClient = Depends(get_client), query_api: QueryApi = Depends(get_query_api)):
    """
    Retrieve a list of all unique devices that have sent device_info in the past 365 days.
    """
    bucket = get_bucket()
    flux_query = f'''
    from(bucket: "{bucket}")
      |> range(start: -365d)
      |> filter(fn: (r) => r["_measurement"] == "device_info")
      |> keep(columns: ["device"])
      |> group()
      |> distinct(column: "device")
      |> sort()
    '''
    
    try:
        result = query_api.query(flux_query)
        devices = [row.get_value() for table in result for row in table.records]
        return {"devices": devices}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Query failed: {str(e)}")

@router.get("/{device}/info", response_model=DeviceInfoList, summary="Get all device info records for a specific device")
async def get_device_info(device: str, client: InfluxDBClient = Depends(get_client), query_api: QueryApi = Depends(get_query_api)):
    """
    Retrieve all device_info records for the specified device.
    Note: This retrieves all historical records, not limited to 365 days.
    """
    bucket = get_bucket()
    flux_query = f'''
    from(bucket: "{bucket}")
      |> range(start: 0)  // All time
      |> filter(fn: (r) => r["_measurement"] == "device_info" and r["device"] == "{device}")
      |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")
      |> keep(columns: ["_time", "ip", "rssi", "mac"])
      |> rename(columns: {{_time: "timestamp"}})
      |> sort(columns: ["timestamp"], desc: false)
    '''
    
    try:
        result = query_api.query(flux_query)
        records = []
        for table in result:
            for row in table.records:
                records.append({
                    "timestamp": row["timestamp"].isoformat(),
                    "ip": row["ip"],
                    "rssi": row["rssi"],
                    "mac": row["mac"]
                })
        if not records:
            raise HTTPException(status_code=404, detail="No records found for this device")
        return {"records": records}
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Query failed: {str(e)}")