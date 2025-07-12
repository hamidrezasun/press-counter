from influxdb_client import InfluxDBClient
from PyQt6.QtWidgets import QMessageBox
from datetime import datetime
from data_processing.utils import INFLUXDB_URL, INFLUXDB_TOKEN, INFLUXDB_ORG, INFLUXDB_BUCKET, FIELD_NAME, PULSE_DURATION_FIELD, SEND_DURATION_FIELD, MEASUREMENT, RFID_MEASUREMENT, UTC_TZ, IRAN_TZ, escape_flux_string

def populate_device_ids(device_combo):
    """Queries InfluxDB for unique device IDs."""
    flux_query_devices = f'''
    from(bucket: "{INFLUXDB_BUCKET}")
      |> range(start: -30d)
      |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
      |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}")
      |> group(columns: ["device"])
      |> distinct(column: "device")
    '''

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    query_api = client.query_api()

    try:
        result = query_api.query(org=INFLUXDB_ORG, query=flux_query_devices)
        device_ids = []
        for table in result:
            for record in table.records:
                device_id = record.values.get("device")
                if device_id:
                    device_ids.append(device_id)

        device_info = {}
        device_ids.sort()
        device_combo.clear()
        if device_ids:
            device_combo.addItems(device_ids)
            device_combo.setCurrentText(device_ids[0])
            device_combo.setEnabled(True)
        else:
            device_combo.addItem("No devices found")
            device_combo.setEnabled(False)
            QMessageBox.warning(None, "No Devices", "No device IDs found in the database for the last 30 days.")

        return device_info

    except Exception as e:
        QMessageBox.critical(None, "Error", f"Failed to fetch device info: {e}")
        device_combo.clear()
        device_combo.addItem("Error fetching devices")
        device_combo.setEnabled(False)
        return {}

    finally:
        client.close()

def query_rfid_records(device_id, start_date, end_date):
    """Queries RFID tag records (rfid_tag_die and rfid_tag_people) from InfluxDB."""
    try:
        start_dt = datetime.strptime(start_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        end_dt = datetime.strptime(end_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        start_time_utc = start_dt.astimezone(UTC_TZ)
        end_time_utc = end_dt.astimezone(UTC_TZ)
    except ValueError:
        QMessageBox.critical(None, "Invalid Input", "Please enter valid dates in the format shown.")
        return []

    cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
    escaped_device_id = escape_flux_string(cleaned_device_id)

    measurements = ["rfid_tag_die", "rfid_tag_people"]
    rfid_records = []

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    query_api = client.query_api()

    try:
        for measurement in measurements:
            flux_query = f'''
            from(bucket: "{INFLUXDB_BUCKET}")
              |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
              |> filter(fn: (r) => r["_measurement"] == "{measurement}")
              |> filter(fn: (r) => r["_field"] == "uid")
              |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
              |> sort(columns: ["_time"])
            '''
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
            for table in result:
                for record in table.records:
                    uid = record.get_value()
                    if uid:
                        rfid_records.append((record.get_time(), uid, measurement))
        return rfid_records

    except Exception as e:
        QMessageBox.critical(None, "Query Error", f"Error querying RFID records: {e}")
        return []

    finally:
        client.close()

def query_production_data(device_id, start_date, end_date):
    """Queries production data from InfluxDB."""
    try:
        start_dt = datetime.strptime(start_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        end_dt = datetime.strptime(end_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        start_time_utc = start_dt.astimezone(UTC_TZ)
        end_time_utc = end_dt.astimezone(UTC_TZ)
    except ValueError:
        QMessageBox.critical(None, "Invalid Input", "Please enter valid dates in the format shown.")
        return [], [], 0

    cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
    escaped_device_id = escape_flux_string(cleaned_device_id)

    flux_query_production = f'''
    from(bucket: "{INFLUXDB_BUCKET}")
      |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
      |> filter(fn: (r) => r["_measurement"] == "{MEASUREMENT}")
      |> filter(fn: (r) => r["_field"] == "{FIELD_NAME}" or r["_field"] == "{PULSE_DURATION_FIELD}" or r["_field"] == "{SEND_DURATION_FIELD}")
      |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
      |> pivot(rowKey: ["_time", "device"], columnKey: ["_field"], valueColumn: "_value")
      |> sort(columns: ["_time"])
    '''

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    query_api = client.query_api()

    records = []
    skipped_records = 0
    try:
        result = query_api.query(org=INFLUXDB_ORG, query=flux_query_production)
        count_sum = 0
        for table in result:
            for record in table.records:
                count = record.values.get("count", 0)
                pulse_duration = record.values.get("pulse_duration_ms")
                send_duration = record.values.get("send_duration_ms", 0)
                if pulse_duration is None:
                    skipped_records += 1
                    continue
                try:
                    count = int(count) if count is not None else 0
                    pulse_duration = float(pulse_duration)
                    send_duration = float(send_duration) if send_duration is not None else 0
                except (ValueError, TypeError):
                    count = 0
                    pulse_duration = float(pulse_duration)
                    send_duration = 0
                    skipped_records += 1
                records.append((record.get_time(), count, pulse_duration, send_duration))
                count_sum += count

        if skipped_records > 0:
            print(f"Warning: Skipped {skipped_records} records due to missing or invalid pulse_duration_ms values.")

        return records, [], skipped_records

    except Exception as e:
        QMessageBox.critical(None, "Query Error", f"Error querying InfluxDB: {e}")
        return [], [], 0

    finally:
        client.close()

def query_device_ip(device_id):
    """Queries the latest IP address for a given device from InfluxDB."""
    cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
    escaped_device_id = escape_flux_string(cleaned_device_id)

    flux_query = f'''
    from(bucket: "{INFLUXDB_BUCKET}")
      |> range(start: -30d)
      |> filter(fn: (r) => r["_measurement"] == "device_info")
      |> filter(fn: (r) => r["_field"] == "ip")
      |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
      |> last()
    '''

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    query_api = client.query_api()

    try:
        result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
        for table in result:
            for record in table.records:
                ip = record.get_value()
                if ip:
                    return ip
        return None

    except Exception as e:
        QMessageBox.critical(None, "Query Error", f"Error querying device IP: {e}")
        return None

    finally:
        client.close()

def query_rssi_data(device_ids, start_date, end_date):
    """Queries WiFi RSSI data for all devices from InfluxDB."""
    try:
        start_dt = datetime.strptime(start_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        end_dt = datetime.strptime(end_date, "%Y-%m-%d").replace(tzinfo=IRAN_TZ)
        start_time_utc = start_dt.astimezone(UTC_TZ)
        end_time_utc = end_dt.astimezone(UTC_TZ)
    except ValueError:
        QMessageBox.critical(None, "Invalid Input", "Please enter valid dates in the format shown.")
        return []

    client = InfluxDBClient(url=INFLUXDB_URL, token=INFLUXDB_TOKEN, org=INFLUXDB_ORG)
    query_api = client.query_api()

    rssi_data = []
    try:
        for device_id in device_ids:
            cleaned_device_id = device_id.encode('ascii', 'ignore').decode('ascii')
            escaped_device_id = escape_flux_string(cleaned_device_id)
            flux_query = f'''
            from(bucket: "{INFLUXDB_BUCKET}")
              |> range(start: {start_time_utc.isoformat()}, stop: {end_time_utc.isoformat()})
              |> filter(fn: (r) => r["_measurement"] == "device_info")
              |> filter(fn: (r) => r["_field"] == "wifi_signal")
              |> filter(fn: (r) => r["device"] == "{escaped_device_id}")
              |> sort(columns: ["_time"])
            '''
            result = query_api.query(org=INFLUXDB_ORG, query=flux_query)
            for table in result:
                for record in table.records:
                    rssi = record.get_value()
                    if rssi is not None:
                        local_time = record.get_time().astimezone(IRAN_TZ)
                        rssi_data.append((device_id, local_time.strftime("%Y-%m-%d"), local_time.hour, rssi))
        return rssi_data

    except Exception as e:
        QMessageBox.critical(None, "Query Error", f"Error querying RSSI data: {e}")
        return []

    finally:
        client.close()